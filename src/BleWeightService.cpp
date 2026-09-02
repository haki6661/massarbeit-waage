#include "BleWeightService.h"
#include "Config.h"

BleWeightService::BleWeightService(Scale& scale, DeviceUi& ui, Battery& battery)
    : scale_(scale), ui_(ui), battery_(battery) {}

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

    // Modell + Faehigkeiten als JSON (siehe BLE_DEVICE_INFO_CHAR_UUID in
    // Config.h). Die App liest das beim Verbinden einmal und entscheidet
    // danach, ob sie z.B. Akkuanzeige und display-bezogene Texte zeigt.
    deviceInfoChar_ = service->createCharacteristic(
        BLE_DEVICE_INFO_CHAR_UUID,
        NIMBLE_PROPERTY::READ);
    String deviceInfo = buildDeviceInfoJson();
    deviceInfoChar_->setValue(deviceInfo.c_str());
    Serial.printf("[BLE] Geraete-Info: %s\n", deviceInfo.c_str());

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

    // Waehrend einer Firmware-Uebertragung gehoert die Leitung den Chunks:
    // schnelleres Verbindungsintervall anfordern und die Gewichts-/Akku-
    // Notifies pausieren (20 Notifies pro Sekunde konkurrieren sonst mit den
    // Chunk-Writes um dieselben Verbindungsereignisse, und niemand schaut
    // waehrend eines Updates auf den Gewichtswert).
    const bool otaRunning = ota_.isTransferring();
    if (otaRunning != otaFastLinkActive_) {
        otaFastLinkActive_ = otaRunning;
        applyLinkSpeed(otaRunning);
    }

    if (connected_ && !otaRunning && (now - lastNotifyMs_ >= BLE_WEIGHT_NOTIFY_INTERVAL_MS)) {
        lastNotifyMs_ = now;
        sendWeight(scale_.getCurrentWeight());
    }

    if (connected_ && !otaRunning && (now - lastBatteryNotifyMs_ >= BLE_BATTERY_NOTIFY_INTERVAL_MS)) {
        lastBatteryNotifyMs_ = now;
        sendBattery(battery_.readPercent());
    }

    ota_.update();
}

String BleWeightService::buildDeviceInfoJson() const {
    // Handgeschrieben statt mit einer JSON-Library: der Inhalt ist fest
    // verdrahtet (alles Compile-Zeit-Konstanten aus dem Board-Profil), es gibt
    // nichts zu escapen, und eine Library dafuer ins Binary zu ziehen waere
    // unverhaeltnismaessig. Gelesen wird das JSON nur auf App-Seite.
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"model\":\"%s\",\"name\":\"%s\",\"fw\":\"%s\",\"variant\":\"%s\","
             "\"caps\":{\"display\":%s,\"battery\":%s,\"buttons\":%d,\"led\":%s,\"ota\":true}}",
             MASSARBEIT_MODEL_ID,
             MASSARBEIT_MODEL_NAME,
             FIRMWARE_VERSION,
             MASSARBEIT_MODEL_ID,
             MASSARBEIT_HAS_TFT ? "true" : "false",
             MASSARBEIT_HAS_BATTERY ? "true" : "false",
             MASSARBEIT_BUTTON_COUNT,
             MASSARBEIT_HAS_STATUS_LED ? "true" : "false");
    return String(buf);
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

void BleWeightService::onConnect(NimBLEServer* /*server*/, ble_gap_conn_desc* desc) {
    connHandle_ = desc->conn_handle;
}

void BleWeightService::onDisconnect(NimBLEServer* /*server*/) {
    connected_ = false;
    connHandle_ = BLE_HS_CONN_HANDLE_NONE;
    otaFastLinkActive_ = false;
    ota_.onClientDisconnected();
}

/**
 * Ein Firmware-Chunk wird mit Response geschrieben, kostet also einen
 * kompletten Round-Trip - die Uebertragungsdauer haengt damit fast linear am
 * Verbindungsintervall. Die Advertising-Praeferenz (setMinPreferred/
 * setMaxPreferred in begin()) ist nur ein Hinweis, den Android gern ignoriert
 * und stattdessen ein sparsames Intervall im 30-50ms-Bereich waehlt; eine
 * explizite Parameter-Update-Anfrage wird dagegen praktisch immer akzeptiert.
 * Nur fuer die Dauer der Uebertragung, danach zurueck auf den sparsamen
 * Normalbetrieb - ein dauerhaft schnelles Intervall kostet Akku.
 */
void BleWeightService::applyLinkSpeed(bool fast) {
    if (!server_ || connHandle_ == BLE_HS_CONN_HANDLE_NONE) return;

    if (fast) {
        // 7.5-15ms (Einheit 1.25ms), Timeout 4s (Einheit 10ms).
        server_->updateConnParams(connHandle_, 6, 12, 0, 400);
        // Data Length Extension: bis 251 statt 27 Byte Nutzlast pro Link-
        // Layer-Paket - ein 244-Byte-Chunk passt damit in ein einziges Paket
        // statt in zehn Fragmente.
        server_->setDataLen(connHandle_, 251);
        Serial.printf("[BLE] OTA: schnelles Intervall angefordert (MTU=%u).\n", server_->getPeerMTU(connHandle_));
    } else {
        // Zurueck auf die in begin() beworbene Praeferenz (7.5-22.5ms).
        server_->updateConnParams(connHandle_, 6, 18, 0, 400);
        Serial.println("[BLE] OTA beendet, Verbindungsparameter zurueckgesetzt.");
    }
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
            ui_.setRemoteCue(RemoteCue::None);
            break;

        case COMMAND_DISPLAY_READY:
            ui_.setRemoteCue(RemoteCue::Ready);
            break;

        case COMMAND_DISPLAY_AWAY: {
            // <gameId> optional dran - aeltere/vereinfachte Aufrufer ohne
            // das Byte bekommen die generische (Golf-)Animation, siehe
            // TftDisplay::renderRemoteCueScreen().
            GameKind game = value.size() >= 2 ? static_cast<GameKind>(value[1]) : GameKind::None;
            ui_.setRemoteCue(RemoteCue::Away, game);
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
            ui_.setRemoteCue(cue);
            break;
        }

        case COMMAND_PLAYER_TURN: {
            // Layout: <gameId><r><g><b><nameLen><name...>, siehe Config.h.
            if (value.size() < 6) {
                Serial.println("[BLE] PLAYER_TURN-Kommando zu kurz, ignoriert.");
                break;
            }
            GameKind game = static_cast<GameKind>(value[1]);
            uint16_t color565 = ui_.color565FromRgb(
                static_cast<uint8_t>(value[2]), static_cast<uint8_t>(value[3]), static_cast<uint8_t>(value[4]));
            uint8_t nameLen = static_cast<uint8_t>(value[5]);
            String name;
            size_t available = value.size() - 6;
            size_t take = nameLen < available ? nameLen : available;
            for (size_t i = 0; i < take; i++) {
                name += value[6 + i];
            }
            ui_.setActivePlayer(game, color565, name);
            break;
        }

        case COMMAND_PLAYER_CLEAR:
            ui_.clearActivePlayer();
            break;

        default:
            Serial.printf("[BLE] Unbekanntes Kommando: 0x%02X\n", command);
    }
}
