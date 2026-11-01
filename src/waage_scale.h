#ifndef WAAGE_SCALE_H
#define WAAGE_SCALE_H

#include "HX711.h"
#include "waage_config.h"

extern HX711 scale;

// Anzahl der Messwerte für die gleitende Mittelwertbildung. Der HX711 liefert etwa 10 Werte pro Sekunde, 
// wodurch bei 8 Werten eine Verzögerung von rund 0.8 Sekunden entsteht, die Anzeige jedoch stabil bleibt.
constexpr int GEWICHT_MITTELWERT = 8;

// Ringpuffer für die Messwerte
static float gewichtPuffer[GEWICHT_MITTELWERT];
static int gewichtPufferAnzahl = 0;
static int gewichtPufferPos = 0;

inline void gewichtPufferLeeren() {
    gewichtPufferAnzahl = 0;
    gewichtPufferPos = 0;
}

// Initialisiert die Wägezelle
inline void setupScale() {
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    scale.set_scale(KALIBRIERFAKTOR);
}

// Liest das Gewicht in Gramm aus. Blockiert nicht, sondern verwendet bei fehlenden Neugewichten den letzten Wert, 
// damit die Hauptschleife für die Tasterabfrage reaktionsfähig bleibt.
inline float liesGewicht(float letzterWert) {
    if (scale.is_ready()) {
        gewichtPuffer[gewichtPufferPos] = scale.get_units(1);
        gewichtPufferPos = (gewichtPufferPos + 1) % GEWICHT_MITTELWERT;
        if (gewichtPufferAnzahl < GEWICHT_MITTELWERT) gewichtPufferAnzahl++;
    }

    if (gewichtPufferAnzahl == 0) return letzterWert;

    float summe = 0;
    for (int i = 0; i < gewichtPufferAnzahl; i++) summe += gewichtPuffer[i];
    float gewicht = summe / gewichtPufferAnzahl;

    // Filtert Rauschen im Nullpunktbereich heraus 
    if (gewicht > -TOTBEREICH_G && gewicht < TOTBEREICH_G) return 0.0f;
    return gewicht;
}

inline void tareWaage() {
    scale.tare();
    gewichtPufferLeeren();  // Setzt den Puffer zurück, da alte Messwerte nach dem Tarieren ungültig sind 
}

#endif