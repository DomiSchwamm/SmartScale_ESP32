#ifndef WAAGE_STATE_H
#define WAAGE_STATE_H

// Verwaltet alle Variablen und Zustände, die sich zur Laufzeit ändern.

#include "waage_config.h"

typedef struct {
    char name[32];
    float khWert;   // Kohlenhydrate pro 100g oder pro Stück
    bool istStueck; // false = pro 100g (wiegen), true = pro Stück (zählen)
} Lebensmittel;

enum Modus {
    BUCHSTABEN_WAHL,
    SELEKTION,
    MANUELLE_EINGABE,
    STUECK_ZAEHLEN,
    WIEGEN,
    ABFRAGE_WEITER,
    PUMPEN_ERGEBNIS
};

// Lebensmittelliste
extern Lebensmittel lebensmittelListe[MAX_LEBENSMITTEL];
extern int anzahlLebensmittel;
extern int aktuellerIndex;

// Buchstaben-Filter ('?' + A-Z + '#')
extern char verfuegbareBuchstaben[28];
extern int anzahlBuchstaben;
extern int buchstabenIndex;

// Bedienung
extern Modus aktuellerModus;
extern float manuellKhPro100g;
extern int stueckAnzahl;
extern int abfrageAuswahl;

// Mahlzeit
extern float gesamtMahlzeitKH;
extern float aktuellesGewicht;

// Flag wird vom Webserver gesetzt. Das Zeichnen erfolgt ausschließlich im Loop, 
// um konkurrierende Zugriffe auf das Display zu verhindern.
extern volatile bool displayNeuZeichnen;

// Zentrale Berechnungslogik für konsistente Werte in Anzeige und Mahlzeitensumme.

// Bei manuellem Eintrag wird der individuell eingestellte Wert verwendet.
inline float khPro100gAktuell() {
    const Lebensmittel& item = lebensmittelListe[aktuellerIndex];
    if (strcmp(item.name, MANUELL_NAME) == 0) return manuellKhPro100g;
    return item.khWert;
}

// Negative Gewichtsangaben werden als 0 g Kohlenhydrate gewertet.
inline float berechneKH(float gewichtG) {
    if (gewichtG <= 0.0f) return 0.0f;
    return (gewichtG * khPro100gAktuell()) / 100.0f;
}

// Berechnet die Kohlenhydrate basierend auf der Stückzahl.
inline float berechneKHStueck() {
    return lebensmittelListe[aktuellerIndex].khWert * stueckAnzahl;
}

#endif