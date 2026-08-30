#include "BleWeightService.h"
#include "Config.h"

BleWeightService::BleWeightService(Scale& scale, TftDisplay& display, Battery& battery)
    : scale_(scale), display_(display), battery_(battery) {}

void BleWeightService::begin() {
    Serial.println("[BLE] Initialisiere NimBLE...");

    NimBLEDevice::init(BLE_DEVICE_NAME);

    // Groesseres MTU anfragen, bevor irgendetwas verbindet - der Firmware-
    // Update-Start-Befehl (21 Byte) und vor allem die Firmware-Chunks selbst
    // brauchen mehr als das Default-MTU (23, davon nur 20 Nutzlast). Moderne
    // Telefone handeln beim Verbinden ohnehin meist ein hohes MTU aus, das
    // hier gesetzte ist nur die vom Server erlaubte Obergrenze dafuer.
    // MUSS NACH init() passieren: setMTU() ruft intern ble_att_set_preferred_mtu()
    // auf, das auf NimBLE-Host-Mutexe zugreift, die erst init() anlegt - davor
    // aufgerufen crasht zuverlaessig mit "assert failed: npl_freertos_mutex_pend
    // ... (mu->handle)" (Boot-Loop, im Serial-Log zu sehen).
    NimBLEDevice::setMTU(247);

    server_ = NimBLEDevice::createServer();
    server_->setCallbacks(this);

    NimBLEService* service = server_->createService(BLE_SERVICE_UUID);

    weightChar_ = service->createCharacteristic(
        BLE_WEIGHT_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    commandChar_ = service->createCharacteristic(
        BLE_COMMAND_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    commandChar_->setCallbacks(this);

    batteryChar_ = service->createCharacteristic(
        BLE_BATTERY_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    versionChar_ = service->createCharacteristic(
        BLE_VERSION_CHAR_UUID,
        NIMBLE_PROPERTY::READ);
    versionChar_->setValue(FIRMWARE_VERSION);

    // Firmware-Update per BLE (siehe OtaUpdater.h) - WRITE statt WRITE_NR fuer
    // beide Update-Characteristics: Chunks duerfen beim Flashen nicht
    // stillschweigend verloren gehen, "mit Response" gibt dem Uebertragungs-
    // Code auf App-Seite die noetige Zustellbestaetigung fuer Flusskontrolle.
    NimBLECharacteristic* otaControlChar = service->createCharacteristic(
        BLE_OTA_CONTROL_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE);
    NimBLECharacteristic* otaDataChar = service->createCharacteristic(
        BLE_OTA_DATA_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE);
    NimBLECharacteristic* otaStatusChar = service->createCharacteristic(
        BLE_OTA_STATUS_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    ota_.begin(otaControlChar, otaDataChar, otaStatusChar);

    service->start();

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(BLE_SERVICE_UUID);
    advertising->setMinPreferred(0x06); // 7.5ms
    advertising->setMaxPreferred(0x12); // 22.5ms
    advertising->start();

    Serial.printf("[BLE] Advertising als \"%s\" gestartet.\n", BLE_DEVICE_NAME);
}

void BleWeightService::update() {
    uint32_t now = millis();

    if (!connected_ && wasConnected_) {
        // Client ist weg - nach kurzer Pause wieder sichtbar machen.
        delay(200);
        server_->startAdvertising();
        wasConnected_ = false;
        Serial.println("[BLE] Advertising nach Verbindungsabbruch neu gestartet.");
    }

    if (connected_ && !wasConnected_) {
        wasConnected_ = true;
        Serial.println("[BLE] Client verbunden.");
    }

    if (connected_ && (now - lastNotifyMs_ >= BLE_WEIGHT_NOTIFY_INTERVAL_MS)) {
        lastNotifyMs_ = now;
        sendWeight(scale_.getCurrentWeight());
    }

    if (connected_ && (now - lastBatteryNotifyMs_ >= BLE_BATTERY_NOTIFY_INTERVAL_MS)) {
        lastBatteryNotifyMs_ = now;
        sendBattery(battery_.readPercent());
    }

    ota_.update();
}

void BleWeightService::sendWeight(float grams) {
    if (!weightChar_) return;

    union {
        float value;
        uint8_t bytes[4];
    } payload;
    payload.value = grams;

    weightChar_->setValue(payload.bytes, 4);
    weightChar_->notify();
}

void BleWeightService::sendBattery(int8_t percent) {
    if (!batteryChar_) return;

    // -1 (kein verlaesslicher Wert, siehe Battery::readPercent()) wird als
    // 0xFF gesendet - die App zeigt dann bewusst keinen Prozentwert an,
    // statt eine erfundene Zahl zu raten.
    uint8_t value = (percent < 0) ? 0xFF : static_cast<uint8_t>(percent);
    batteryChar_->setValue(&value, 1);
    batteryChar_->notify();
}

void BleWeightService::onConnect(NimBLEServer* /*server*/) {
    connected_ = true;
}

void BleWeightService::onDisconnect(NimBLEServer* /*server*/) {
    connected_ = false;
    ota_.onClientDisconnected();
}

void BleWeightService::onWrite(NimBLECharacteristic* characteristic) {
    std::string value = characteristic->getValue();
    if (value.empty()) return;

    uint8_t command = static_cast<uint8_t>(value[0]);
    switch (command) {
        case COMMAND_TARE:
            Serial.println("[BLE] Tare-Kommando empfangen.");
            scale_.tare();
            break;

        case COMMAND_DISPLAY_IDLE:
            display_.setRemoteCue(RemoteCue::None);
            break;

        case COMMAND_DISPLAY_READY:
            display_.setRemoteCue(RemoteCue::Ready);
            break;

        case COMMAND_DISPLAY_AWAY: {
            // <gameId> optional dran - aeltere/vereinfachte Aufrufer ohne
            // das Byte bekommen die generische (Golf-)Animation, siehe
            // TftDisplay::renderRemoteCueScreen().
            GameKind game = value.size() >= 2 ? static_cast<GameKind>(value[1]) : GameKind::None;
            display_.setRemoteCue(RemoteCue::Away, game);
            break;
        }

        case COMMAND_DISPLAY_RESULT: {
            if (value.size() < 2) {
                Serial.println("[BLE] DISPLAY_RESULT-Kommando ohne Guete-Byte, ignoriert.");
                break;
            }
            uint8_t quality = static_cast<uint8_t>(value[1]);
            RemoteCue cue = quality >= 2 ? RemoteCue::ResultPerfect
                          : quality == 1 ? RemoteCue::ResultClose
                                         : RemoteCue::ResultMiss;
            display_.setRemoteCue(cue);
            break;
        }

        case COMMAND_PLAYER_TURN: {
            // Layout: <gameId><r><g><b><nameLen><name...>, siehe Config.h.
            if (value.size() < 6) {
                Serial.println("[BLE] PLAYER_TURN-Kommando zu kurz, ignoriert.");
                break;
            }
            GameKind game = static_cast<GameKind>(value[1]);
            uint16_t color565 = display_.color565FromRgb(
                static_cast<uint8_t>(value[2]), static_cast<uint8_t>(value[3]), static_cast<uint8_t>(value[4]));
            uint8_t nameLen = static_cast<uint8_t>(value[5]);
            String name;
            size_t available = value.size() - 6;
            size_t take = nameLen < available ? nameLen : available;
            for (size_t i = 0; i < take; i++) {
                name += value[6 + i];
            }
            display_.setActivePlayer(game, color565, name);
            break;
        }

        case COMMAND_PLAYER_CLEAR:
            display_.clearActivePlayer();
            break;

        default:
            Serial.printf("[BLE] Unbekanntes Kommando: 0x%02X\n", command);
    }
}
