#pragma once

// Nimmt eine neue Firmware in Chunks per BLE entgegen und flasht sie in die
// inaktive OTA-Partition (Update.h/esp_ota - Standard-A/B-Partitionsschema,
// bringt das LilyGO-T-Display-S3-Boardprofil bereits mit: app0+app1 je
// 6,5MB, siehe Board-Partitionstabelle). Ersetzt fuer den Partybetrieb den
// bisherigen reinen Entwicklungs-OTA-Weg (WLAN+ArduinoOTA, siehe DevOta.h) -
// hier ueberträgt die App die Firmware direkt per Bluetooth, kein WLAN am
// Partyort noetig.
//
// Sicherheitsargument: schlaegt eine Uebertragung fehl (Groessen-/MD5-
// Mismatch, Verbindungsabbruch, Flash-Fehler), wird die neue Partition NIE
// als bootfaehig markiert - Update.end(true) prueft das selbst, bevor es
// umschaltet. Die Waage bootet beim naechsten Start einfach unveraendert die
// bisherige Firmware weiter. Das ist der eingebaute Mechanismus der
// ESP32-OTA-Partitionierung, kein Extra-Code noetig - ein abgebrochenes
// Update kann die Waage nicht "bricken".

#include <NimBLECharacteristic.h>
#include <Update.h>

class OtaUpdater : public NimBLECharacteristicCallbacks {
public:
    // Registriert sich selbst als Callback-Handler fuer control/data.
    void begin(NimBLECharacteristic* controlChar, NimBLECharacteristic* dataChar, NimBLECharacteristic* statusChar);

    // In loop() aufrufen: erkennt haengengebliebene Uebertragungen (Timeout
    // ohne neue Daten) und fuehrt den verzoegerten Neustart nach einem
    // erfolgreichen Update aus (Verzoegerung, damit die letzte Status-Notify
    // noch rausgeht, bevor der Neustart die BLE-Verbindung kappt).
    void update();

    // Von BleWeightService::onDisconnect() aufgerufen - eine waehrend der
    // Uebertragung verlorene Verbindung soll das halbfertige Update nicht
    // unbegrenzt offen stehen lassen.
    void onClientDisconnected();

    // NimBLECharacteristicCallbacks - fuer sowohl control als auch data
    // registriert, unterscheidet ueber Pointer-Vergleich in onWrite().
    void onWrite(NimBLECharacteristic* characteristic) override;

    // Laeuft gerade eine Uebertragung? BleWeightService haelt der dann die
    // Leitung frei (schnelles Verbindungsintervall, keine Gewichts-Notifies).
    bool isTransferring() const { return state_ == State::InProgress; }

private:
    enum class State : uint8_t { Idle = 0, InProgress = 1, Success = 2, Error = 3 };
    enum class ErrorCode : uint8_t {
        None = 0,
        BeginFailed = 1,
        SizeMismatch = 2,
        Md5Mismatch = 3,
        WriteFailed = 4,
        Aborted = 5,
    };

    NimBLECharacteristic* controlChar_ = nullptr;
    NimBLECharacteristic* dataChar_ = nullptr;
    NimBLECharacteristic* statusChar_ = nullptr;

    State state_ = State::Idle;
    ErrorCode error_ = ErrorCode::None;
    uint32_t totalSize_ = 0;
    uint32_t bytesWritten_ = 0;
    uint32_t lastDataMs_ = 0;
    uint32_t successAtMs_ = 0;
    uint8_t lastNotifiedPercent_ = 0xFF;

    void handleControl(std::string& value);
    void handleData(std::string& value);
    void beginUpdate(uint32_t totalSize, const uint8_t md5[16]);
    void finishUpdate();
    void abortUpdate(ErrorCode reason);
    void sendStatus(bool force);
};
