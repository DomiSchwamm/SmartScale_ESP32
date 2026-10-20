// Stellt einen WLAN Hotspot mit einem integrierten Webserver zur Verfügung, um die Lebensmittelliste zu verwalten.
#ifndef WAAGE_WEBSERVER_H
#define WAAGE_WEBSERVER_H

#include <WiFi.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "waage_config.h"
#include "waage_state.h"
#include "waage_storage.h"
#include "waage_webpage.h"

extern AsyncWebServer server;

// Speichert das Ergebnis des letzten Datei Uploads für den Request Handler
static bool restoreErfolgreich = false;

// Öffnet den WLAN Hotspot und registriert alle HTTP Routen. 
// Die Handler verändern ausschließlich die Daten und setzen ein Flag, während das eigentliche Zeichnen im Hauptloop erfolgt.
inline void setupWebserver() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    // Liefert die im Flash Speicher abgelegte Webseite aus
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html", index_html);
    });

    // Liefert die Lebensmittelliste direkt aus der JSON Datei
    server.on("/api/lebensmittel", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists(DATEI_PFAD)) {
            request->send(LittleFS, DATEI_PFAD, "application/json");
        } else {
            request->send(200, "application/json", "[]");
        }
    });

    // Fügt ein neues Lebensmittel hinzu oder aktualisiert es. Parameter sind bereits decodiert
    server.on("/api/add", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (!(request->hasParam("name", true) && request->hasParam("kh", true) && request->hasParam("typ", true))) {
            request->send(400, "text/plain", "Felder fehlen");
            return;
        }

        String name = request->getParam("name", true)->value();
        float kh    = request->getParam("kh", true)->value().toFloat();
        String typ  = request->getParam("typ", true)->value();

        if (fuegeLebensmittelHinzu(name, kh, typ == "stueck")) {
            displayNeuZeichnen = true;
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Ungueltige Eingabe");
        }
    });

    // Löscht einen spezifischen Eintrag aus der Liste
    server.on("/api/delete", HTTP_DELETE, [](AsyncWebServerRequest *request) {
        if (request->hasParam("name") && loescheLebensmittel(request->getParam("name")->value())) {
            displayNeuZeichnen = true;
            request->send(200, "text/plain", "OK");
        } else {
            request->send(404, "text/plain", "Nicht gefunden");
        }
    });

    // Ermöglicht den Download der Sicherungsdatei als JSON
    server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (LittleFS.exists(DATEI_PFAD)) {
            AsyncWebServerResponse *response = request->beginResponse(LittleFS, DATEI_PFAD, "application/json", true);
            response->addHeader("Content-Disposition", "attachment; filename=waage_lebensmittel_backup.json");
            request->send(response);
        } else {
            request->send(404, "text/plain", "Keine Datei vorhanden");
        }
    });

    // Verarbeitet hochgeladene Sicherungsdateien durch Zwischenspeicherung in einer temporären Datei und anschließende Validierung
    server.on("/api/restore", HTTP_POST,
        [](AsyncWebServerRequest *request) {
            if (restoreErfolgreich) {
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Ungueltige Datei");
            }
        },
        [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            static File uploadFile;       // Bleibt über alle Datenpakete hinweg erhalten
            static bool zuGross = false;  // Markiert Dateien, die das Limit überschreiten

            if (index == 0) {  // Initialisiert die temporäre Datei beim ersten Paket
                restoreErfolgreich = false;
                zuGross = false;
                uploadFile = LittleFS.open("/restore.tmp", FILE_WRITE);
            }

            // Begrenzt den Upload auf die maximal zulässige Größe
            if (index + len > BACKUP_MAX_BYTES) zuGross = true;

            if (uploadFile && !zuGross) {
                uploadFile.write(data, len);
            }
            if (final) {  // Schließt die Datei und übernimmt die Daten nach erfolgreicher Prüfung
                if (uploadFile) uploadFile.close();
                if (zuGross) {
                    LittleFS.remove("/restore.tmp");
                    restoreErfolgreich = false;
                } else {
                    restoreErfolgreich = uebernehmeBackupDatei("/restore.tmp");
                    displayNeuZeichnen = true;
                }
            }
        }
    );

    server.begin();
}

#endif