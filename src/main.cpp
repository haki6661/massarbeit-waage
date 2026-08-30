#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

#include "BoardConfig.h"
#include "Config.h"
#include "Scale.h"
#include "BleWeightService.h"
#include "Buttons.h"
#include "TftDisplay.h"
#include "Battery.h"
#include "CalibrationRoutine.h"
#include "DevOta.h"

Scale scale(Pins::HX711_DOUT, Pins::HX711_SCK, DEFAULT_CALIBRATION_FACTOR);
TftDisplay display;
Battery battery;
BleWeightService bleService(scale, display, battery);
Buttons buttons;
CalibrationRoutine calibration(scale, display);
DevOta devOta;

DisplayMode currentMode = DisplayMode::Weight;
bool calibrationRequested = false;
bool sleepRequested = false;
bool devOtaActive = false;

// Fuer den Auto-Sleep-Timer: letzter Zeitpunkt mit "Aktivitaet" (Gewichts-
// aenderung oder Tastendruck). Bei Ueberschreiten von AUTO_SLEEP_TIMEOUT_MS
// ohne neue Aktivitaet geht die Waage automatisch schlafen (siehe loop()).
unsigned long lastActivityMs = 0;
float lastActivityWeight = 0.0f;
bool activityBaselineSet = false;

void onTareClick() {
    Serial.println("[Button] Taste 1: Tare");
    scale.tare();
    lastActivityMs = millis();
}

void onModeClick() {
    currentMode = (currentMode == DisplayMode::Weight) ? DisplayMode::Status : DisplayMode::Weight;
    Serial.printf("[Button] Taste 2: Modus -> %s\n",
                  currentMode == DisplayMode::Weight ? "Gewicht" : "Status");
    lastActivityMs = millis();
}

void onCalibrationLongPress() {
    calibrationRequested = true;
    lastActivityMs = millis();
}

void onSleepLongPress() {
    Serial.println("[Button] Taste 1 lang: Deep Sleep angefordert.");
    sleepRequested = true;
}

// Versetzt die Waage in Deep Sleep (~wenige µA statt 60-150+ mA aktiv).
// Aufwachen NUR ueber Taste 2 (GPIO14) - bewusst nicht Taste 1/GPIO0, weil
// das der BOOT-Strapping-Pin ist: waere er beim Aufwach-Boot noch gedrueckt,
// koennte der Chip statt der Firmware in den Flash-Download-Modus starten.
// Nach dem Aufwachen laeuft die komplette Firmware (setup()) neu durch -
// es gibt keinen speziellen "Resume"-Pfad, das ist bei ESP32-Deep-Sleep
// so vorgesehen.
void enterDeepSleep() {
    Serial.println("[Power] Gehe in Deep Sleep. Taste 2 zum Aufwecken.");
    display.showMessage("Gute Nacht", "Taste 2 zum\nAufwecken");
    delay(1200);

    digitalWrite(Pins::POWER_ON, LOW); // Peripherie (Display etc.) stromlos schalten

    rtc_gpio_pullup_en((gpio_num_t)Pins::BUTTON_2);
    rtc_gpio_pulldown_dis((gpio_num_t)Pins::BUTTON_2);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)Pins::BUTTON_2, 0); // LOW = Taste 2 gedrueckt

    esp_deep_sleep_start();
    // Wird nie erreicht.
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    uint32_t waitStart = millis();
    while (!Serial && millis() - waitStart < 3000) {
        delay(10);
    }

    Serial.println();
    Serial.println("==================================================");
    Serial.println(" Massarbeit Waage - LilyGO T-Display S3");
    Serial.println("==================================================");
    Serial.printf("Chip: %s rev %d, Free heap: %u bytes\n",
                   ESP.getChipModel(), ESP.getChipRevision(), ESP.getFreeHeap());

    esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
    if (wakeupCause == ESP_SLEEP_WAKEUP_EXT0) {
        Serial.println("[Power] Aufgewacht aus Deep Sleep (Taste 2).");
    }

    // Muss VOR allem anderen geprueft werden, das Taste 2 anfasst.
    bool devOtaRequested = DevOta::bootHeld();
    devOtaActive = devOtaRequested;

    display.begin();
    display.showMessage("Massarbeit Waage", "Starte...");

    battery.begin();

    buttons.begin();
    buttons.onTare(onTareClick);
    buttons.onSleepLongPress(onSleepLongPress);
    buttons.onModeClick(onModeClick);
    buttons.onCalibrationLongPress(onCalibrationLongPress);

    bool hx711Ok = scale.begin();
    if (!hx711Ok) {
        display.showMessage("Fehler", "HX711 antwortet\nnicht. Verkabelung\npruefen.");
        Serial.println("[Setup] WARNUNG: Waage laeuft ohne HX711 weiter (liefert 0g).");
        delay(2000);
    }

    bleService.begin();

    if (devOtaRequested) {
        devOta.begin();
    } else {
        Serial.println("[Setup] Taste 2 beim Boot nicht gehalten - Dev-OTA/WLAN bleibt aus.");
    }

    Serial.println("[Setup] Bereit.");
    Serial.println("Taste 1: Tara (kurz) / Deep Sleep (2s halten)");
    Serial.println("Taste 2: Status-Anzeige (kurz) / Kalibrierung (lang halten)");
    Serial.printf("[Power] Auto-Sleep nach %lu Minuten Inaktivitaet.\n", AUTO_SLEEP_TIMEOUT_MS / 60000UL);

    // Animierte Startsequenz (wackelnde Balkenwaage + Boot-Checks), bevor es
    // in die normale Gewichtsanzeige uebergeht - blockierend, siehe
    // TftDisplay::playBootAnimation().
    display.playBootAnimation(hx711Ok, battery.readVoltage(), /*bleStarted=*/true);

    lastActivityMs = millis();
}

void loop() {
    buttons.update();
    devOta.update();

    if (sleepRequested) {
        sleepRequested = false;
        enterDeepSleep();
    }

    float weight = scale.getWeight();
    bleService.update();

    // Inaktivitaets-Timer: jede spuerbare Gewichtsaenderung zaehlt als
    // Aktivitaet (Tastendruck wird schon direkt in den Button-Callbacks
    // erfasst, siehe oben). Waehrend Dev-OTA nie automatisch schlafen -
    // wuerde sonst mitten in einem Firmware-Update den Stecker ziehen.
    if (!activityBaselineSet) {
        lastActivityWeight = weight;
        activityBaselineSet = true;
    }
    if (fabsf(weight - lastActivityWeight) > SLEEP_ACTIVITY_THRESHOLD_G) {
        lastActivityWeight = weight;
        lastActivityMs = millis();
    }
    if (!devOtaActive && millis() - lastActivityMs > AUTO_SLEEP_TIMEOUT_MS) {
        Serial.println("[Power] Auto-Sleep: keine Aktivitaet seit 10 Minuten.");
        enterDeepSleep();
    }

    if (calibrationRequested) {
        calibrationRequested = false;
        calibration.run();
    }

    display.update(currentMode, weight, scale.getLastRawReading(), scale.isHX711Connected(),
                    bleService.isConnected(), battery.readVoltage());

    delay(5);
}
