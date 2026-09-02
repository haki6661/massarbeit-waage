#include "OtaUpdater.h"
#include <Arduino.h>
#include <sdkconfig.h> // CONFIG_IDF_FIRMWARE_CHIP_ID

namespace {
    // Keine neuen Daten trotz laufendem Update -> vermutlich Verbindung tot,
    // ohne dass onClientDisconnected() gefeuert hat (z.B. App im Hintergrund
    // eingefroren) - lieber selbst abbrechen als die Partition unbegrenzt
    // halb beschrieben liegen zu lassen.
    constexpr uint32_t STALL_TIMEOUT_MS = 15000;
    // Zeit zwischen Erfolgs-Notify und ESP.restart() - damit die Notify die
    // BLE-Verbindung noch sicher verlaesst, bevor der Neustart sie kappt.
    constexpr uint32_t REBOOT_DELAY_MS = 1500;

    String md5ToHex(const uint8_t md5[16]) {
        String hex;
        hex.reserve(32);
        for (int i = 0; i < 16; i++) {
            if (md5[i] < 0x10) hex += '0';
            hex += String(md5[i], HEX);
        }
        return hex;
    }
}

void OtaUpdater::begin(NimBLECharacteristic* controlChar, NimBLECharacteristic* dataChar, NimBLECharacteristic* statusChar) {
    controlChar_ = controlChar;
    dataChar_ = dataChar;
    statusChar_ = statusChar;
    controlChar_->setCallbacks(this);
    dataChar_->setCallbacks(this);
}

void OtaUpdater::onWrite(NimBLECharacteristic* characteristic) {
    std::string value = characteristic->getValue();
    if (characteristic == controlChar_) {
        handleControl(value);
    } else if (characteristic == dataChar_) {
        handleData(value);
    }
}

void OtaUpdater::handleControl(std::string& value) {
    if (value.empty()) return;
    uint8_t command = static_cast<uint8_t>(value[0]);

    if (command == 0x01) { // Start
        if (value.size() < 21) {
            Serial.println("[OTA] Start-Kommando zu kurz, ignoriert.");
            return;
        }
        uint32_t totalSize;
        memcpy(&totalSize, value.data() + 1, 4); // little-endian, wie ESP32 selbst rechnet
        uint8_t md5[16];
        memcpy(md5, value.data() + 5, 16);
        beginUpdate(totalSize, md5);
    } else if (command == 0x02) { // Abbrechen
        Serial.println("[OTA] Abbruch von der App angefordert.");
        abortUpdate(ErrorCode::Aborted);
    } else {
        Serial.printf("[OTA] Unbekanntes Kommando: 0x%02X\n", command);
    }
}

void OtaUpdater::beginUpdate(uint32_t totalSize, const uint8_t md5[16]) {
    if (state_ == State::InProgress) {
        Serial.println("[OTA] Neuer Start waehrend laufendem Update - breche das alte ab.");
        Update.abort();
    }

    // Groesser als eine einzelne OTA-Partition kann nicht passen (siehe
    // Board-Partitionstabelle: app0/app1 je 6,5MB) - offensichtlich
    // fehlerhafte/manipulierte Anfrage, gar nicht erst versuchen.
    constexpr uint32_t MAX_PARTITION_SIZE = 0x640000;
    if (totalSize == 0 || totalSize > MAX_PARTITION_SIZE) {
        Serial.printf("[OTA] Ungueltige Groesse: %u\n", (unsigned)totalSize);
        state_ = State::Error;
        error_ = ErrorCode::SizeMismatch;
        sendStatus(true);
        return;
    }

    Serial.printf("[OTA] Start: %u Bytes.\n", (unsigned)totalSize);

    if (!Update.begin(totalSize)) {
        Serial.printf("[OTA] Update.begin() fehlgeschlagen: %s\n", Update.errorString());
        state_ = State::Error;
        error_ = ErrorCode::BeginFailed;
        sendStatus(true);
        return;
    }
    Update.setMD5(md5ToHex(md5).c_str());

    totalSize_ = totalSize;
    bytesWritten_ = 0;
    imageHeaderLen_ = 0; // sonst wuerde ein zweiter Versuch die Chip-Pruefung ueberspringen
    state_ = State::InProgress;
    error_ = ErrorCode::None;
    lastDataMs_ = millis();
    lastNotifiedPercent_ = 0xFF;
    sendStatus(true);
}

/**
 * Jedes ESP-Firmware-Image beginnt mit esp_image_header_t: Byte 0 ist das
 * Magic 0xE9, Byte 12/13 tragen die Chip-Kennung (little endian, 0x0009 =
 * ESP32-S3, 0x0005 = ESP32-C3). CONFIG_IDF_FIRMWARE_CHIP_ID ist genau der
 * Wert, fuer den dieses Binary gebaut wurde.
 *
 * Warum das hier geprueft wird, obwohl der Bootloader dasselbe tut: seit es
 * zwei Geraetevarianten auf VERSCHIEDENEN Architekturen gibt (Xtensa und
 * RISC-V), haengt an der Auswahl der richtigen .bin nur die App - und die
 * entscheidet anhand der Geraete-Info, die sie beim Verbinden gelesen hat.
 * Geht dabei etwas schief (alte App-Version im Browser-Cache, unlesbare
 * Characteristic, Fehler auf dem Weg), landet ein Image der falschen
 * Architektur hier. Es dann erst beim Booten scheitern zu lassen, waere die
 * schlechtere Antwort: das Geraet haette sich schon umgeschaltet, und der
 * Nutzer saehe ein "Update erfolgreich", das keins war. Also lieber sofort
 * ablehnen, solange die alte Firmware noch unangetastet laeuft.
 */
bool OtaUpdater::imageMatchesThisChip() const {
    if (imageHeader_[0] != 0xE9) return false; // kein ESP-Image
    uint16_t chipId = static_cast<uint16_t>(imageHeader_[12]) |
                      (static_cast<uint16_t>(imageHeader_[13]) << 8);
    return chipId == CONFIG_IDF_FIRMWARE_CHIP_ID;
}

void OtaUpdater::handleData(std::string& value) {
    if (state_ != State::InProgress) return; // kein Update.write() ohne vorheriges begin()
    if (value.empty()) return;

    // Kopfdaten mitschneiden, bis 16 Byte beisammen sind, und dann einmalig
    // pruefen. Ueber mehrere Chunks gesammelt, damit die Pruefung auch bei
    // sehr kleinen Chunks greift - in der Praxis kommt der Kopf komplett im
    // allerersten.
    if (imageHeaderLen_ < sizeof(imageHeader_)) {
        size_t take = sizeof(imageHeader_) - imageHeaderLen_;
        if (take > value.size()) take = value.size();
        memcpy(imageHeader_ + imageHeaderLen_, value.data(), take);
        imageHeaderLen_ += take;

        if (imageHeaderLen_ == sizeof(imageHeader_) && !imageMatchesThisChip()) {
            uint16_t chipId = static_cast<uint16_t>(imageHeader_[12]) |
                              (static_cast<uint16_t>(imageHeader_[13]) << 8);
            Serial.printf("[OTA] Firmware passt nicht zu diesem Chip "
                          "(Image 0x%04X, erwartet 0x%04X) - abgelehnt.\n",
                          chipId, (unsigned)CONFIG_IDF_FIRMWARE_CHIP_ID);
            abortUpdate(ErrorCode::ChipMismatch);
            return;
        }
    }

    size_t written = Update.write(reinterpret_cast<uint8_t*>(&value[0]), value.size());
    if (written != value.size()) {
        Serial.printf("[OTA] Schreibfehler: %s\n", Update.errorString());
        abortUpdate(ErrorCode::WriteFailed);
        return;
    }

    bytesWritten_ += written;
    lastDataMs_ = millis();

    if (bytesWritten_ >= totalSize_) {
        finishUpdate();
    } else {
        sendStatus(false);
    }
}

void OtaUpdater::finishUpdate() {
    if (Update.end(true)) {
        Serial.println("[OTA] Erfolgreich abgeschlossen, starte in Kuerze neu...");
        state_ = State::Success;
        error_ = ErrorCode::None;
        successAtMs_ = millis();
    } else {
        Serial.printf("[OTA] Update.end() fehlgeschlagen: %s\n", Update.errorString());
        state_ = State::Error;
        error_ = (Update.getError() == UPDATE_ERROR_MD5) ? ErrorCode::Md5Mismatch : ErrorCode::WriteFailed;
    }
    sendStatus(true);
}

void OtaUpdater::abortUpdate(ErrorCode reason) {
    if (state_ == State::InProgress) {
        Update.abort();
    }
    state_ = (reason == ErrorCode::Aborted) ? State::Idle : State::Error;
    error_ = reason;
    totalSize_ = 0;
    bytesWritten_ = 0;
    sendStatus(true);
}

void OtaUpdater::onClientDisconnected() {
    if (state_ == State::InProgress) {
        Serial.println("[OTA] Verbindung waehrend Update verloren - breche ab.");
        abortUpdate(ErrorCode::Aborted);
    }
}

void OtaUpdater::update() {
    if (state_ == State::InProgress && millis() - lastDataMs_ > STALL_TIMEOUT_MS) {
        Serial.println("[OTA] Timeout - lange keine Daten mehr erhalten, breche ab.");
        abortUpdate(ErrorCode::WriteFailed);
    }

    if (state_ == State::Success && millis() - successAtMs_ > REBOOT_DELAY_MS) {
        ESP.restart();
    }
}

void OtaUpdater::sendStatus(bool force) {
    if (!statusChar_) return;

    uint8_t percent = (totalSize_ > 0) ? (uint8_t)(((uint64_t)bytesWritten_ * 100) / totalSize_) : 0;
    if (!force && percent == lastNotifiedPercent_) return; // nicht bei jedem einzelnen Chunk feuern, nur bei sichtbarem Fortschritt
    lastNotifiedPercent_ = percent;

    uint8_t payload[3] = { static_cast<uint8_t>(state_), percent, static_cast<uint8_t>(error_) };
    statusChar_->setValue(payload, 3);
    statusChar_->notify();
}
