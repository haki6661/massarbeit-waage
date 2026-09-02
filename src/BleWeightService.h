#pragma once

// BLE-Gewichtsservice fuer die kuenftige Handy-Web-App.
//
// Bewusst reduziert auf das, was WeighMyBru2 (siehe include/Config.h fuer
// die UUID-Herkunft) "Bean Conqueror"-Format nennt: eine Service-UUID, eine
// Notify-Characteristic mit einem simplen 4-Byte-Little-Endian-Float
// (Gramm), eine Write-Characteristic fuer ein Tare-Kommando. Das
// GaggiMate-Protokoll (20-Byte-Pakete, Heartbeats, Maschinensteuerung) aus
// dem Original ist absichtlich nicht mit uebernommen.
//
// NimBLE-Arduino statt des in arduino-esp32 eingebauten Bluedroid-Stacks -
// gleiche Wahl wie WeighMyBru2, spart Flash/RAM.

#include <NimBLEDevice.h>
#include <NimBLEServer.h>

#include "Battery.h"
#include "DeviceUi.h"
#include "OtaUpdater.h"
#include "Scale.h"

class BleWeightService : public NimBLEServerCallbacks, public NimBLECharacteristicCallbacks {
public:
    // ui: fuer die 0x10-0x15-Anzeige-Kommandos (siehe Config.h) - je nach
    // Variante das TFT oder die Status-LED der Light (siehe DeviceUi.h) -
    // die Waage kennt kein eigenes Spielkonzept, sie leitet nur weiter, was
    // die App ihr sagt. battery: fuer die Akkustand-Characteristic (deutlich
    // seltener aktualisiert als das Gewicht, siehe update()).
    BleWeightService(Scale& scale, DeviceUi& ui, Battery& battery);

    void begin();
    void update(); // in loop() aufrufen: sendet Gewicht, haelt Advertising am Laufen
    bool isConnected() const { return connected_; }

    // NimBLEServerCallbacks
    void onConnect(NimBLEServer* server) override;
    // Zweite Overload-Variante: NimBLE ruft beide auf (siehe NimBLEServer.cpp,
    // BLE_GAP_EVENT_CONNECT) - nur ueber diese kommen wir an den Conn-Handle,
    // den updateConnParams()/setDataLen() brauchen.
    void onConnect(NimBLEServer* server, ble_gap_conn_desc* desc) override;
    void onDisconnect(NimBLEServer* server) override;

    // NimBLECharacteristicCallbacks (Command-Characteristic)
    void onWrite(NimBLECharacteristic* characteristic) override;

private:
    static const uint8_t COMMAND_TARE = 0x01;
    static const uint8_t COMMAND_DISPLAY_IDLE = 0x10;
    static const uint8_t COMMAND_DISPLAY_READY = 0x11;
    static const uint8_t COMMAND_DISPLAY_RESULT = 0x12;
    static const uint8_t COMMAND_DISPLAY_AWAY = 0x13;
    static const uint8_t COMMAND_PLAYER_TURN = 0x14;
    static const uint8_t COMMAND_PLAYER_CLEAR = 0x15;

    Scale& scale_;
    DeviceUi& ui_;
    Battery& battery_;
    NimBLEServer* server_ = nullptr;
    NimBLECharacteristic* weightChar_ = nullptr;
    NimBLECharacteristic* commandChar_ = nullptr;
    NimBLECharacteristic* batteryChar_ = nullptr;
    NimBLECharacteristic* versionChar_ = nullptr;
    NimBLECharacteristic* deviceInfoChar_ = nullptr;
    OtaUpdater ota_;

    volatile bool connected_ = false;
    bool wasConnected_ = false;
    uint32_t lastNotifyMs_ = 0;
    uint32_t lastBatteryNotifyMs_ = 0;
    uint16_t connHandle_ = BLE_HS_CONN_HANDLE_NONE;
    bool otaFastLinkActive_ = false;

    // Baut den JSON-Steckbrief fuer BLE_DEVICE_INFO_CHAR_UUID aus den
    // MASSARBEIT_*-Makros des Board-Profils. Einmal in begin() gesetzt -
    // der Inhalt kann sich zur Laufzeit nicht aendern.
    String buildDeviceInfoJson() const;

    void sendWeight(float grams);
    void sendBattery(int8_t percent);
    // Schaltet die Verbindung fuer die Dauer einer Firmware-Uebertragung auf
    // Durchsatz statt auf Sparsamkeit (siehe Implementierung).
    void applyLinkSpeed(bool fast);
};
