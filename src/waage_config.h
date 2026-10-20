// Zentrale Konfigurationsdatei für Pin Belegung, Kalibrierung, WLAN und System Limits.
#ifndef WAAGE_CONFIG_H
#define WAAGE_CONFIG_H

#include <Arduino.h>

// HX711 Wägezelle
#define LOADCELL_DOUT_PIN 23
#define LOADCELL_SCK_PIN  15

// Ermittelter Kalibrierfaktor (Rohwert pro Gramm) mittels Referenzgewicht
constexpr float KALIBRIERFAKTOR = 193.6f;

// Toleranzbereich zur Vermeidung von Schwankungen im Nullpunkt (+/- Gramm)
constexpr float TOTBEREICH_G = 0.6f;

// OLED Display (SSD1306)
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
#define OLED_SDA       21
#define OLED_SCL       22

// Drehgeber (Encoder)
#define ENCODER_CLK 18
#define ENCODER_DT  19
#define ENCODER_SW  4         // Taster Pin, unterstützt auch das Aufwecken aus dem Deep Sleep

// WLAN Access Point Einstellungen
constexpr const char* AP_SSID     = "Waage";
constexpr const char* AP_PASSWORD = "selbst waehlen";

// UI und Bedienungs Timings sowie Standardwerte
constexpr unsigned long KLICK_LANG_MS     = 800;   
constexpr unsigned long ENTPRELL_MS       = 30;    
constexpr unsigned long SCHNELL_DREHEN_MS = 80;    
constexpr float MANUELL_STARTWERT_KH      = 25.0f;

// System Limits und Dateipfade
constexpr unsigned long TIMEOUT_MS = 100000;  // Inaktivitätszeit bis zum Übergang in den Deep Sleep
constexpr size_t BACKUP_MAX_BYTES  = 65536;   // Maximale Dateigröße für JSON Sicherungsdateien (64 KB)
constexpr float KH_MAX             = 100.0f;  // Obergrenze für Kohlenhydrat Eingaben über das Webinterface
#define MAX_LEBENSMITTEL 500
#define MANUELL_NAME     "? Manuell"
#define DATEI_PFAD       "/lebensmittel.json"

#endif