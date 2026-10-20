// Steuert die OLED Anzeige. Definiert ein Layout pro Programmmodus und dient ausschließlich der grafischen Ausgabe.
#ifndef WAAGE_DISPLAY_H
#define WAAGE_DISPLAY_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "waage_config.h"
#include "waage_state.h"

extern Adafruit_SSD1306 display;

// Gibt false zurück, falls das Display nicht gefunden wird
inline bool displayStarten() {
    Wire.begin(OLED_SDA, OLED_SCL);
    return display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
}

inline void displayLoeschen() {
    display.clearDisplay();
    display.display();
}

// Zeigt den Begrüßungsbildschirm und wartet auf den ersten Tastendruck
inline void zeigeBegruessung() {
    display.clearDisplay();
    display.drawRoundRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, 8, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(46, 12);
    display.println(F("Hallo!"));
    display.setCursor(16, 46);
    display.println(F("Klicke zum Start"));
    display.display();

    while (digitalRead(ENCODER_SW) == HIGH) {
        delay(10);
    }
    delay(200);
}

// Zeichnet die Benutzeroberfläche basierend auf dem aktuellen Modus und verwendet bereits ermittelte Gewichtsdaten.
inline void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    if (aktuellerModus == BUCHSTABEN_WAHL) {
        display.setCursor(0, 0);
        display.println("Anfangsbuchstabe?");
        display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

        if (anzahlBuchstaben == 0) {
            display.setCursor(0, 25);
            display.println("Keine Eintraege!");
        } else {
            display.setTextSize(3);
            display.setCursor(56, 22);
            display.print(verfuegbareBuchstaben[buchstabenIndex]);
            display.setTextSize(1);
        }

        // Fusszeile mit laufender Summe oder Bedienhinweis
        display.setCursor(0, 53);
        if (gesamtMahlzeitKH > 0) {
            display.print("Bisher: ");
            display.print(gesamtMahlzeitKH, 1);
            display.println("g KH");
        } else {
            display.println("[Drehen = Waehlen]");
        }

    } else if (aktuellerModus == SELEKTION) {
        display.setCursor(0, 0);
        display.println("Worauf hast du Lust?");

        if (anzahlLebensmittel == 0) {
            display.setCursor(0, 25);
            display.println("Keine Eintraege!");
        } else {
            display.setCursor(0, 20);
            display.print(">");
            display.println(lebensmittelListe[aktuellerIndex].name);
        }

        display.setCursor(0, 52);
        if (gesamtMahlzeitKH > 0) {
            display.print("Bisher gesamt: ");
            display.print(gesamtMahlzeitKH, 1);
            display.println("g");
        } else {
            display.println("[Lang = Buchstabe]");
        }

    } else if (aktuellerModus == MANUELLE_EINGABE) {
        display.setCursor(0, 0);
        display.println("Freie Eingabe:");
        display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

        display.setCursor(0, 16);
        display.println("KH pro 100g:");

        display.setTextSize(2);
        display.setCursor(0, 32);
        display.print(manuellKhPro100g, 1);
        display.setTextSize(1);
        display.println(" g");

        display.setCursor(0, 53);
        display.println("Drehen & Klick = OK");

    } else if (aktuellerModus == STUECK_ZAEHLEN) {
        const Lebensmittel& item = lebensmittelListe[aktuellerIndex];

        display.setCursor(0, 0);
        display.println(item.name);
        display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

        display.setTextSize(2);
        display.setCursor(0, 18);
        display.print("Anzahl: ");
        display.println(stueckAnzahl);

        display.setTextSize(1);
        display.setCursor(0, 48);
        display.print("= ");
        display.print(berechneKHStueck(), 1);
        display.println(" g KH");

    } else if (aktuellerModus == WIEGEN) {
        const Lebensmittel& item = lebensmittelListe[aktuellerIndex];

        display.setCursor(0, 0);
        display.println(item.name);
        display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

        display.setTextSize(2);
        display.setCursor(0, 13);
        display.print(aktuellesGewicht, 1);
        display.setTextSize(1);
        display.println(" g");

        display.setTextSize(2);
        display.setCursor(0, 33);
        display.print("= ");
        display.print(berechneKH(aktuellesGewicht), 1);
        display.setTextSize(1);
        display.println(" g KH");

        display.setCursor(0, 53);
        display.println("Klick:OK|Lang:Tara");

    } else if (aktuellerModus == ABFRAGE_WEITER) {
        display.setCursor(0, 0);
        display.println("Noch etwas wiegen?");
        display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

        display.setCursor(0, 14);
        display.print("Bisher: ");
        display.print(gesamtMahlzeitKH, 1);
        display.println(" g KH");

        // Markiert die aktuelle Menüauswahl
        display.setCursor(5, 32);
        display.print(abfrageAuswahl == 0 ? "> " : "  ");
        display.println("Weiteres Essen");

        display.setCursor(5, 48);
        display.print(abfrageAuswahl == 1 ? "> " : "  ");
        display.println("Fertig & Pumpe");

    } else if (aktuellerModus == PUMPEN_ERGEBNIS) {
        display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 4, SSD1306_WHITE);

        display.setCursor(12, 6);
        display.println("IN PUMPE EINGEBEN:");

        display.setTextSize(2);
        display.setCursor(15, 24);
        display.print(gesamtMahlzeitKH, 1);
        display.setTextSize(1);
        display.println(" g KH");

        display.setCursor(15, 48);
        display.println("Guten Appetit! <3");
    }

    display.display();
}

#endif