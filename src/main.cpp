// Küchenwaage mit ESP32 zur direkten Umrechnung des Gewichts in Kohlenhydrate (KH).
// Die Bedienung erfolgt über einen Drehencoder, während die Verwaltung der Lebensmittelliste 
// über einen integrierten Webserver im eigenen WLAN stattfindet. Hobbyprojekt, kein Medizinprodukt.

#include <ESP32Encoder.h>

#include "waage_config.h"
#include "waage_state.h"
#include "waage_scale.h"
#include "waage_storage.h"
#include "waage_display.h"
#include "waage_power.h"
#include "waage_webserver.h"

// Globale Objekte und Systemvariablen
HX711 scale;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AsyncWebServer server(80);
SemaphoreHandle_t listeMutex = NULL;

Lebensmittel lebensmittelListe[MAX_LEBENSMITTEL];
int anzahlLebensmittel = 0;
int aktuellerIndex = 0;

char verfuegbareBuchstaben[28];
int anzahlBuchstaben = 0;
int buchstabenIndex = 0;

Modus aktuellerModus = BUCHSTABEN_WAHL;
float manuellKhPro100g = MANUELL_STARTWERT_KH;
int stueckAnzahl = 1;
int abfrageAuswahl = 0;

float gesamtMahlzeitKH = 0.0;
float aktuellesGewicht = 0.0;

volatile bool displayNeuZeichnen = false;

// Lokale Variablen für die Hauptdatei
static ESP32Encoder encoder;
static int64_t alterZaehlerstand = 0;

static unsigned long letzteAktivitaet = 0;  // Zeitstempel für den Inaktivitäts Timeout

static bool tasteWarGedrueckt = false;
static unsigned long drueckStartZeit = 0;

static unsigned long letzteDrehung = 0;  // Zur Bestimmung der Drehadaption bei der KH Eingabe


// Verarbeitet Drehbewegungen des Encoders abhängig vom aktuellen Programmmodus
static void verarbeiteDrehung() {
    // Vier Zählschritte pro Rastpunkt, daher Division durch 4
    int64_t neuerZaehlerstand = encoder.getCount() / 4;
    if (neuerZaehlerstand == alterZaehlerstand) return;

    letzteAktivitaet = millis();
    int differenz = (int)(neuerZaehlerstand - alterZaehlerstand);
    alterZaehlerstand = neuerZaehlerstand;

    switch (aktuellerModus) {
        case BUCHSTABEN_WAHL:
            // Zyklischer Überlauf am Anfang und Ende des Alphabets
            if (anzahlBuchstaben > 0) {
                if (differenz > 0) {
                    buchstabenIndex++;
                    if (buchstabenIndex >= anzahlBuchstaben) buchstabenIndex = 0;
                } else {
                    buchstabenIndex--;
                    if (buchstabenIndex < 0) buchstabenIndex = anzahlBuchstaben - 1;
                }
            }
            break;

        case SELEKTION:
            // Zyklisches Navigieren durch die Lebensmittelliste
            if (differenz > 0) {
                aktuellerIndex++;
                if (aktuellerIndex >= anzahlLebensmittel) aktuellerIndex = 0;
            } else {
                aktuellerIndex--;
                if (aktuellerIndex < 0) aktuellerIndex = anzahlLebensmittel - 1;
            }
            break;

        case MANUELLE_EINGABE: {
            // Dynamische Schrittweite: Langsames Drehen in 0,1 Schritten, schnelles Drehen in 1 Schritten
            unsigned long jetzt = millis();
            bool schnell = (jetzt - letzteDrehung < SCHNELL_DREHEN_MS) || abs(differenz) > 1;
            letzteDrehung = jetzt;

            float neu = manuellKhPro100g + differenz * (schnell ? 1.0f : 0.1f);
            neu = roundf(neu * 10.0f) / 10.0f;  // Bereinigung von Fließkomma Rundungsfehlern
            manuellKhPro100g = constrain(neu, 0.0f, 100.0f);
            break;
        }

        case STUECK_ZAEHLEN:
            stueckAnzahl = constrain(stueckAnzahl + differenz, 1, 10);
            break;

        case ABFRAGE_WEITER:
            abfrageAuswahl = (differenz > 0) ? 1 : 0;
            break;

        default:
            break;
    }

    displayNeuZeichnen = true;
}

// Aktiviert den Wiege Modus und setzt Messpuffer sowie Gewicht zurück
static void starteWiegen() {
    aktuellerModus = WIEGEN;
    aktuellesGewicht = 0.0;
    tareWaage();
}

// Führt kontextabhängige Aktionen basierend auf Tastendruck und Modus aus
static void verarbeiteKlick(bool lang) {
    switch (aktuellerModus) {
        case BUCHSTABEN_WAHL:
            if (!lang && anzahlBuchstaben > 0) {
                char gewaehlt = verfuegbareBuchstaben[buchstabenIndex];

                // Sucht den ersten passenden Eintrag mit dem gewählten Anfangsbuchstaben
                for (int i = 0; i < anzahlLebensmittel; i++) {
                    if (anfangsbuchstabe(lebensmittelListe[i].name) == gewaehlt) {
                        aktuellerIndex = i;
                        break;
                    }
                }
                aktuellerModus = SELEKTION;
            }
            break;

        case SELEKTION:
            if (lang) {
                aktuellerModus = BUCHSTABEN_WAHL;
            } else if (strcmp(lebensmittelListe[aktuellerIndex].name, MANUELL_NAME) == 0) {
                manuellKhPro100g = MANUELL_STARTWERT_KH;
                aktuellerModus = MANUELLE_EINGABE;
            } else if (lebensmittelListe[aktuellerIndex].istStueck) {
                aktuellerModus = STUECK_ZAEHLEN;
                stueckAnzahl = 1;
            } else {
                starteWiegen();
            }
            break;

        case MANUELLE_EINGABE:
            starteWiegen();
            break;

        case STUECK_ZAEHLEN:
            if (lang) {
                aktuellerModus = SELEKTION;
            } else {
                gesamtMahlzeitKH += berechneKHStueck();
                aktuellerModus = ABFRAGE_WEITER;
                abfrageAuswahl = 0;
            }
            break;

        case WIEGEN:
            if (lang) {
                tareWaage();
            } else if (aktuellesGewicht > 0.0) {
                gesamtMahlzeitKH += berechneKH(aktuellesGewicht);
                aktuellerModus = ABFRAGE_WEITER;
                abfrageAuswahl = 0;
            } else {
                aktuellerModus = SELEKTION;
            }
            break;

        case ABFRAGE_WEITER:
            aktuellerModus = (abfrageAuswahl == 0) ? BUCHSTABEN_WAHL : PUMPEN_ERGEBNIS;
            break;

        case PUMPEN_ERGEBNIS:
            gesamtMahlzeitKH = 0.0;
            aktuellerModus = BUCHSTABEN_WAHL;
            break;
    }

    displayNeuZeichnen = true;
}

// Fragt den Taster ab, führt eine Entprellung durch und unterscheidet zwischen kurzem und langem Druck
static void verarbeiteKnopf() {
    bool istGedrueckt = (digitalRead(ENCODER_SW) == LOW);  // Active Low durch Pull up Beschaltung

    if (istGedrueckt && !tasteWarGedrueckt) {
        tasteWarGedrueckt = true;
        drueckStartZeit = millis();
        letzteAktivitaet = millis();
    } else if (!istGedrueckt && tasteWarGedrueckt) {
        tasteWarGedrueckt = false;
        unsigned long drueckDauer = millis() - drueckStartZeit;

        if (drueckDauer >= ENTPRELL_MS) {
            verarbeiteKlick(drueckDauer > KLICK_LANG_MS);
        }
    }
}


void setup(void) {
    Serial.begin(115200);

    // Initialisierung des Mutex zur Thread Absicherung der Liste
    listeMutex = xSemaphoreCreateRecursiveMutex();

    pinMode(ENCODER_SW, INPUT_PULLUP);

    setupScale();

    // Aktivierung interner Pull up Widerstände für den Drehencoder
    ESP32Encoder::useInternalWeakPullResistors = puType::UP;
    encoder.attachFullQuad(ENCODER_DT, ENCODER_CLK);
    encoder.clearCount();

    if (!displayStarten()) {
        Serial.println(F("OLED nicht gefunden! Neustart in 3s..."));
        delay(3000);
        ESP.restart();
    }

    // Initialisiert das Dateisystem und formatiert bei Fehlern automatisch
    if (LittleFS.begin(true)) {
        ladeLebensmittel();
    } else {
        stelleSicherManuellExistiert();
        sortiereLebensmittel();
    }

    setupWebserver();

    zeigeBegruessung();

    letzteAktivitaet = millis();
    updateDisplay();
}

void loop(void) {
    if (anzahlLebensmittel == 0) return;

    // Verhindert den Deep Sleep, solange aktive Clients mit dem Webserver verbunden sind
    if (WiFi.softAPgetStationNum() > 0) {
        letzteAktivitaet = millis();
    }

    if (millis() - letzteAktivitaet > TIMEOUT_MS) {
        geheInDeepSleep();
    }

    // Sperrt die Lebensmittelliste für die Dauer dieses Schleifendurchlaufs
    ListenSperre sperre;

    if (aktuellerModus == WIEGEN) {
        float neuesGewicht = liesGewicht(aktuellesGewicht);

        // Aktualisiert die Anzeige nur bei relevanten Änderungen in der ersten Nachkommastelle
        if (roundf(neuesGewicht * 10.0f) != roundf(aktuellesGewicht * 10.0f)) {
            displayNeuZeichnen = true;
        }
        aktuellesGewicht = neuesGewicht;
    }

    verarbeiteDrehung();
    verarbeiteKnopf();

    if (displayNeuZeichnen) {
        displayNeuZeichnen = false;

        // Fängt ungültige Indizes ab, falls Datensätze über die Webschnittstelle gelöscht wurden
        if (aktuellerIndex >= anzahlLebensmittel) aktuellerIndex = 0;

        updateDisplay();
    }
}