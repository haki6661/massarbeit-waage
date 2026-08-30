#include "DevOta.h"
#include "BoardConfig.h"
#include "Config.h"

#include <WiFi.h>
#include <ArduinoOTA.h>

bool DevOta::bootHeld() {
    pinMode(Pins::BUTTON_2, INPUT_PULLUP);
    delay(20); // kurz entprellen
    return digitalRead(Pins::BUTTON_2) == LOW;
}

void DevOta::begin() {
    Serial.println("[DevOTA] Taste 2 beim Boot gehalten - aktiviere WLAN + ArduinoOTA...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(DEV_WIFI_SSID, DEV_WIFI_PASSWORD);

    Serial.printf("[DevOTA] Verbinde mit WLAN '%s' ", DEV_WIFI_SSID);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(300);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[DevOTA] WLAN-Verbindung fehlgeschlagen - OTA bleibt aus, normaler Betrieb laeuft weiter.");
        active_ = false;
        return;
    }

    Serial.printf("[DevOTA] Verbunden. IP-Adresse: %s\n", WiFi.localIP().toString().c_str());

    ArduinoOTA.setHostname(DEV_OTA_HOSTNAME);
    ArduinoOTA.setPassword(DEV_OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        Serial.println("[DevOTA] Update startet...");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("[DevOTA] Update fertig, starte neu.");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[DevOTA] Fortschritt: %u%%\r", (progress * 100) / total);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[DevOTA] Fehler [%u]\n", error);
    });

    ArduinoOTA.begin();
    active_ = true;

    Serial.println("[DevOTA] Bereit. Upload mit:");
    Serial.printf("  pio run -e t-display-s3-ota -t upload --upload-port %s\n",
                  WiFi.localIP().toString().c_str());
}

void DevOta::update() {
    if (active_) {
        ArduinoOTA.handle();
    }
}

String DevOta::statusLine() const {
    if (!active_) return "OTA: aus";
    return "OTA: " + WiFi.localIP().toString();
}
