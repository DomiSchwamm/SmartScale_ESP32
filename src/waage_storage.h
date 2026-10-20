// Verwaltet die Lebensmittelliste im RAM sowie die Persistenz als JSON Datei im Flash Speicher mittels LittleFS.
// Da Webserver und Hauptschleife (loop) gleichzeitig auf die Daten zugreifen, wird ein Mutex eingesetzt.
#ifndef WAAGE_STORAGE_H
#define WAAGE_STORAGE_H

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "waage_config.h"
#include "waage_state.h"

// Der Webserver läuft in einem separaten Task und modifiziert die Liste, während die Hauptschleife sie liest. 
// Ein rekursiver Mutex verhindert gegenseitige Blockierungen bei verschachtelten Funktionsaufrufen.
extern SemaphoreHandle_t listeMutex;

// RAII Wrapper zum automatischen Sperren beim Anlegen und Freigeben beim Verlassen des Gültigkeitsbereichs
struct ListenSperre {
    ListenSperre()  { xSemaphoreTakeRecursive(listeMutex, portMAX_DELAY); }
    ~ListenSperre() { xSemaphoreGiveRecursive(listeMutex); }
};

// Bestimmt den Anfangsbuchstaben für die Navigation ('?' für Manuell, '#' für Sonderzeichen, Ziffern und Umlaute).
inline char anfangsbuchstabe(const char* name) {
    if (strcmp(name, MANUELL_NAME) == 0) return '?';
    char c = name[0];
    if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
    if (c >= 'A' && c <= 'Z') return c;
    return '#';
}

// Vergleichsfunktion für qsort zur alphabetischen Sortierung unabhängig von Groß und Kleinschreibung
inline int vergleicheLebensmittel(const void* a, const void* b) {
    const Lebensmittel* itemA = (const Lebensmittel*)a;
    const Lebensmittel* itemB = (const Lebensmittel*)b;

    // Der manuelle Eintrag soll stets an erster Stelle stehen
    if (strcmp(itemA->name, MANUELL_NAME) == 0) return -1;
    if (strcmp(itemB->name, MANUELL_NAME) == 0) return 1;

    return strcasecmp(itemA->name, itemB->name);
}

// Ermittelt alle verfügbaren Anfangsbuchstaben in der Liste. '?' ist fest integriert, '#' wird am Ende angehängt.
inline void aktualisiereBuchstabenIndex() {
    anzahlBuchstaben = 0;
    verfuegbareBuchstaben[anzahlBuchstaben++] = '?';
    bool hatSonderzeichen = false;

    for (int i = 0; i < anzahlLebensmittel; i++) {
        char erster = anfangsbuchstabe(lebensmittelListe[i].name);
        if (erster == '?') continue;
        if (erster == '#') {
            hatSonderzeichen = true;
            continue;
        }

        bool schonDa = false;
        for (int j = 0; j < anzahlBuchstaben; j++) {
            if (verfuegbareBuchstaben[j] == erster) {
                schonDa = true;
                break;
            }
        }

        if (!schonDa && anzahlBuchstaben < 27) {
            verfuegbareBuchstaben[anzahlBuchstaben++] = erster;
        }
    }

    if (hatSonderzeichen) verfuegbareBuchstaben[anzahlBuchstaben++] = '#';

    // Setzt den Index zurück, falls der gewählte Buchstabe weggefallen ist
    if (buchstabenIndex >= anzahlBuchstaben) buchstabenIndex = 0;
}

inline void sortiereLebensmittel() {
    if (anzahlLebensmittel > 1) {
        qsort(lebensmittelListe, anzahlLebensmittel, sizeof(Lebensmittel), vergleicheLebensmittel);
    }
    aktualisiereBuchstabenIndex();
}

// Merkt sich das aktuell ausgewählte Lebensmittel namentlich. 
// Verhindert falsche Indexverweise nach Neusortierungen, falls über das Webinterface während des Wiegevorgangs Änderungen erfolgen.
struct AuswahlMerker {
    char name[32];
    bool gueltig;

    AuswahlMerker() {
        gueltig = (aktuellerIndex >= 0 && aktuellerIndex < anzahlLebensmittel);
        if (gueltig) {
            strncpy(name, lebensmittelListe[aktuellerIndex].name, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
        }
    }

    void wiederherstellen() {
        if (!gueltig) return;

        for (int i = 0; i < anzahlLebensmittel; i++) {
            if (strcmp(lebensmittelListe[i].name, name) != 0) continue;

            aktuellerIndex = i;
            // Korrigiert den Modus bei Typänderungen über das Webinterface
            bool stueck = lebensmittelListe[i].istStueck;
            if ((aktuellerModus == WIEGEN && stueck) ||
                (aktuellerModus == STUECK_ZAEHLEN && !stueck)) {
                aktuellerModus = SELEKTION;
            }
            return;
        }

        // Fällt auf die Auswahl zurück, falls der Eintrag gelöscht wurde
        aktuellerIndex = 0;
        if (aktuellerModus == MANUELLE_EINGABE || aktuellerModus == STUECK_ZAEHLEN ||
            aktuellerModus == WIEGEN) {
            aktuellerModus = SELEKTION;
        }
    }
};

// Der manuelle Eintrag existiert ausschließlich im RAM und wird nicht dauerhaft gespeichert
inline void stelleSicherManuellExistiert() {
    for (int i = 0; i < anzahlLebensmittel; i++) {
        if (strcmp(lebensmittelListe[i].name, MANUELL_NAME) == 0) return;
    }

    if (anzahlLebensmittel < MAX_LEBENSMITTEL) {
        strcpy(lebensmittelListe[anzahlLebensmittel].name, MANUELL_NAME);
        lebensmittelListe[anzahlLebensmittel].khWert = 0;
        lebensmittelListe[anzahlLebensmittel].istStueck = false;
        anzahlLebensmittel++;
    }
}

// Schreibt die Liste ohne den manuellen Systemdatensatz in den Flash Speicher (erfordert aktive Listensperre)
inline void speichereInDatei() {
    File file = LittleFS.open(DATEI_PFAD, FILE_WRITE);
    if (!file) return;

    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    for (int i = 0; i < anzahlLebensmittel; i++) {
        if (strcmp(lebensmittelListe[i].name, MANUELL_NAME) == 0) continue;

        JsonObject obj = array.add<JsonObject>();
        obj["name"]   = lebensmittelListe[i].name;
        obj["kh"]     = lebensmittelListe[i].khWert;
        obj["stueck"] = lebensmittelListe[i].istStueck;
    }

    serializeJson(doc, file);
    file.close();
}

// Lädt die Lebensmittelliste aus dem Flash Speicher und behält bei beschädigten Dateien den vorherigen Zustand bei
inline void ladeLebensmittel() {
    ListenSperre sperre;
    AuswahlMerker auswahl;  

    // Erstellt beim allerersten Systemstart eine Standarddatei mit Beispieldaten
    if (!LittleFS.exists(DATEI_PFAD)) {
        File file = LittleFS.open(DATEI_PFAD, FILE_WRITE);
        if (file) {
            JsonDocument doc;
            JsonArray array = doc.to<JsonArray>();

            JsonObject obj1 = array.add<JsonObject>();
            obj1["name"] = "Apfel";
            obj1["kh"] = 12.0;
            obj1["stueck"] = false;

            JsonObject obj2 = array.add<JsonObject>();
            obj2["name"] = "Milchschnitte";
            obj2["kh"] = 15.0;
            obj2["stueck"] = true;

            serializeJson(doc, file);
            file.close();
        }
    }

    File file = LittleFS.open(DATEI_PFAD, FILE_READ);
    if (!file) return;

    JsonDocument doc;
    DeserializationError fehler = deserializeJson(doc, file);
    file.close();

    if (fehler || !doc.is<JsonArray>()) return;

    anzahlLebensmittel = 0;
    for (JsonObject obj : doc.as<JsonArray>()) {
        if (anzahlLebensmittel >= MAX_LEBENSMITTEL) break;

        Lebensmittel& l = lebensmittelListe[anzahlLebensmittel];

        // Fallback Standardwerte bei fehlenden JSON Feldern
        const char* name = obj["name"] | "Unbekannt";
        strncpy(l.name, name, sizeof(l.name) - 1);
        l.name[sizeof(l.name) - 1] = '\0';

        l.khWert    = obj["kh"] | 0.0f;
        l.istStueck = obj["stueck"] | false;
        anzahlLebensmittel++;
    }

    stelleSicherManuellExistiert();
    sortiereLebensmittel();
    auswahl.wiederherstellen();
}

// Fügt ein neues Lebensmittel hinzu oder aktualisiert bestehende Einträge (unabhängig von Groß und Kleinschreibung)
inline bool fuegeLebensmittelHinzu(const String& rohName, float kh, bool istStueck) {
    String name = rohName;
    name.trim();
    if (name.length() == 0 || name == MANUELL_NAME) return false;
    if (isnan(kh) || kh < 0.0f || kh > KH_MAX) return false;

    char neuerName[sizeof(Lebensmittel::name)];
    strncpy(neuerName, name.c_str(), sizeof(neuerName) - 1);
    neuerName[sizeof(neuerName) - 1] = '\0';

    ListenSperre sperre;
    AuswahlMerker auswahl;

    Lebensmittel* ziel = nullptr;
    for (int i = 0; i < anzahlLebensmittel; i++) {
        if (strcasecmp(lebensmittelListe[i].name, neuerName) == 0) {
            ziel = &lebensmittelListe[i];
            break;
        }
    }

    if (ziel == nullptr) {
        if (anzahlLebensmittel >= MAX_LEBENSMITTEL) return false;
        ziel = &lebensmittelListe[anzahlLebensmittel];
        strcpy(ziel->name, neuerName);
        anzahlLebensmittel++;
    }
    ziel->khWert    = kh;
    ziel->istStueck = istStueck;

    stelleSicherManuellExistiert();
    sortiereLebensmittel();
    auswahl.wiederherstellen();
    speichereInDatei();
    return true;
}

// Löscht ein Lebensmittel anhand des Namens (der manuelle Eintrag ist geschützt)
inline bool loescheLebensmittel(const String& name) {
    if (name == MANUELL_NAME) return false;

    ListenSperre sperre;
    AuswahlMerker auswahl;

    int gefunden = -1;
    for (int i = 0; i < anzahlLebensmittel; i++) {
        if (String(lebensmittelListe[i].name) == name) {
            gefunden = i;
            break;
        }
    }
    if (gefunden == -1) return false;

    // Verschiebt nachfolgende Elemente im Array nach vorne
    for (int i = gefunden; i < anzahlLebensmittel - 1; i++) {
        lebensmittelListe[i] = lebensmittelListe[i + 1];
    }
    anzahlLebensmittel--;

    stelleSicherManuellExistiert();
    sortiereLebensmittel();
    auswahl.wiederherstellen();
    speichereInDatei();
    return true;
}

// Überprüft hochgeladene Backup Dateien vor dem Ersetzen, um Datenverlust durch fehlerhafte Formate zu verhindern
inline bool uebernehmeBackupDatei(const char* tempPfad) {
    File file = LittleFS.open(tempPfad, FILE_READ);
    if (!file) return false;

    JsonDocument doc;
    DeserializationError fehler = deserializeJson(doc, file);
    file.close();

    if (fehler || !doc.is<JsonArray>()) {
        LittleFS.remove(tempPfad);
        return false;
    }

    ListenSperre sperre;
    LittleFS.remove(DATEI_PFAD);
    if (!LittleFS.rename(tempPfad, DATEI_PFAD)) return false;

    ladeLebensmittel();
    return true;
}

#endif