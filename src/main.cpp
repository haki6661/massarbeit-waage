#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>

#include "BoardConfig.h"
#include "Config.h"
#include "Scale.h"
#include "BleWeightService.h"
#if MASSARBEIT_BUTTON_COUNT >= 1
#include "Buttons.h"
#endif
#include "DeviceUi.h"
#include "LedRing.h"
#include "Battery.h"
#include "CalibrationRoutine.h"
#include "DevOta.h"

Scale scale(Pins::HX711_DOUT, Pins::HX711_SCK, DEFAULT_CALIBRATION_FACTOR);
// TFT (Vision) oder Status-LED (Basis) - siehe DeviceUi.h.
DeviceUi ui;
// WS2812B-Ring im Deckel: laeuft PARALLEL zur Hauptanzeige mit, kein Ersatz
// fuer sie. Standardmaessig im Board-Profil ausgeschaltet - dann sind alle
// Aufrufe hier leer und der Ring-Code faellt beim Kompilieren weg
// (siehe src/LedRing.h).
LedRing ledRing;
Battery battery;
BleWeightService bleService(scale, ui, battery);
#if MASSARBEIT_BUTTON_COUNT >= 1
Buttons buttons;
#endif
CalibrationRoutine calibration(scale, ui);
DevOta devOta;

bool calibrationRequested = false;
#if MASSARBEIT_BUTTON_COUNT >= 1
bool sleepRequested = false;
#endif
bool devOtaActive = false;

// Fuer runNextBootStep() (siehe unten) - muss ausserhalb von setup() stehen,
// weil DeviceUi::runBootSequence() zwischen den Frames/Blinkschritten darauf
// zugreift, waehrend setup() selbst noch "pausiert" (in playBootSprite()).
bool bootDevOtaRequested = false;
bool bootHx711Ok = true;

// Fuer den Auto-Sleep-Timer: letzter Zeitpunkt mit "Aktivitaet" (Gewichts-
// aenderung oder Tastendruck). Bei Ueberschreiten von AUTO_SLEEP_TIMEOUT_MS
// ohne neue Aktivitaet geht die Waage automatisch schlafen (siehe loop()).
// Auf Varianten ohne Taster gibt es keinen Deep Sleep, dort laeuft der Timer
// zwar mit, loest aber nichts aus (AUTO_SLEEP_TIMEOUT_MS ist 0).
unsigned long lastActivityMs = 0;
float lastActivityWeight = 0.0f;
bool activityBaselineSet = false;

#if MASSARBEIT_BUTTON_COUNT >= 2

// Taste 1 kurz: Geraete-Spielauswahl, naechste Option. Tara gibt es auf der
// Vision als eigene Tastenfunktion nicht mehr - es laeuft automatisch
// (Auto-Zero-Nachfuehrung, siehe Scale.cpp) oder ueber die App
// (BLE-Kommando 0x01).
void onButton1Click() {
    ui.pickerNext();
    lastActivityMs = millis();
}

// Taste 2 kurz: Geraete-Spielauswahl bestaetigen. Rein lokale Anzeige, kein
// automatisches Umschalten der App (siehe ROADMAP.md, "App-Sync:
// Geraete-Spielauswahl schaltet die App mit um").
void onButton2Click() {
    ui.pickerConfirm();
    lastActivityMs = millis();
}

#endif

void onCalibrationRequested() {
    calibrationRequested = true;
    lastActivityMs = millis();
}

// "cal" + Enter im Serial Monitor startet die Kalibrierroutine. Auf der
// Vision der Ersatz fuer den Taster-Langdruck, auf der Basis (ohne Taster)
// neben dem BLE-Kommando 0x20/0x21 der einzige Weg - zum Kalibrieren haengt
// ohnehin ein USB-Kabel dran. Liest zeichenweise ueber mehrere
// loop()-Durchlaeufe in einen statischen Puffer, bis eine Zeile fertig ist;
// laeuft nur ausserhalb von calibration.run() (das liest Serial ja selbst
// blockierend), es gibt also keine Ueberschneidung beim Byte-Verbrauch.
void checkSerialCalibrationTrigger() {
    static String buf;
    while (Serial.available()) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            buf.trim();
            if (buf.length() > 0) {
                if (buf.equalsIgnoreCase("cal")) {
                    Serial.println("[Serial] 'cal' empfangen - starte Kalibrierung.");
                    calibrationRequested = true;
                    lastActivityMs = millis();
                } else {
                    Serial.printf("[Serial] Unbekannter Befehl: '%s' (kalibrieren mit 'cal')\n", buf.c_str());
                }
            }
            buf = "";
        } else {
            buf += c;
        }
    }
}

#if MASSARBEIT_BUTTON_COUNT >= 1

void onSleepLongPress() {
    Serial.println("[Button] Langer Druck: Deep Sleep angefordert.");
    sleepRequested = true;
}

// Versetzt die Waage in Deep Sleep (~wenige µA statt 60-150+ mA aktiv).
// Gibt es nur auf Varianten mit Taster - ohne Aufweck-Taster gaebe es keinen
// Weg zurueck, deshalb steht der ganze Block hinter MASSARBEIT_BUTTON_COUNT.
// Aufwachen ueber Pins::WAKEUP_BUTTON (Board-Profil): auf der Vision Taste 2
// (GPIO14). Bewusst KEIN Strapping-Pin - waere so einer beim Aufwach-Boot
// noch gedrueckt, koennte der Chip statt der Firmware in den
// Flash-Download-Modus starten (deshalb scheidet GPIO0 am S3 aus).
//
// Der Weckmechanismus selbst unterscheidet sich: ext0 gibt es nur auf
// ESP32/ESP32-S3, der C3 der Basis kann stattdessen ueber
// esp_deep_sleep_enable_gpio_wakeup() geweckt werden (beides nur mit
// RTC-faehigen Pins, siehe Board-Profile).
// Nach dem Aufwachen laeuft die komplette Firmware (setup()) neu durch -
// es gibt keinen speziellen "Resume"-Pfad, das ist bei ESP32-Deep-Sleep
// so vorgesehen.
void enterDeepSleep() {
    Serial.println("[Power] Gehe in Deep Sleep. Taste 2 zum Aufwecken.");
    ui.showMessage("Gute Nacht", "Taste 2 zum\nAufwecken");
    delay(1200);

    ui.prepareForSleep();
    // Muss ausdruecklich geloescht werden: WS2812B halten ihr letztes Bild
    // auch dann, wenn der Chip schon schlaeft.
    ledRing.prepareForSleep();

    // Groesster Einzelposten im Schlaf: der HX711 zieht ~1.4-1.6mA und laeuft
    // ohne dieses Kommando unbeeindruckt weiter, waehrend der ESP32 nur noch
    // wenige µA braucht. An einer 700mAh-Zelle entscheidet allein das
    // zwischen rund zwei Wochen und mehreren Monaten Standby.
    scale.powerDown();
#if MASSARBEIT_HX711_SCK_CAN_HOLD
    // Der Standby haelt nur, solange PD_SCK HIGH bleibt - im Deep Sleep
    // schaltet der Chip seine normalen GPIO-Treiber aber ab, der Pin wuerde
    // hochohmig werden und der HX711 irgendwann von selbst wieder aufwachen.
    // gpio_hold_en() friert den Pegel im RTC-Bereich ein, gpio_deep_sleep_
    // hold_en() haelt diesen Latch ueber den Schlaf hinweg aktiv.
    gpio_hold_en((gpio_num_t)Pins::HX711_SCK);
    gpio_deep_sleep_hold_en();
#endif

#if MASSARBEIT_HAS_POWER_ON
    digitalWrite(Pins::POWER_ON, LOW); // Peripherie (Display etc.) stromlos schalten
#endif

#if MASSARBEIT_WAKEUP_USES_EXT0
    rtc_gpio_pullup_en((gpio_num_t)Pins::WAKEUP_BUTTON);
    rtc_gpio_pulldown_dis((gpio_num_t)Pins::WAKEUP_BUTTON);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)Pins::WAKEUP_BUTTON, 0); // LOW = Taste gedrueckt
#else
    // Der C3 hat keinen eigenen RTCIO-Mux, die rtc_gpio_*-Funktionen gibt es
    // dort gar nicht - Pullup/Pulldown laufen ueber den normalen GPIO-Treiber.
    gpio_pullup_en((gpio_num_t)Pins::WAKEUP_BUTTON);
    gpio_pulldown_dis((gpio_num_t)Pins::WAKEUP_BUTTON);
    esp_deep_sleep_enable_gpio_wakeup(BIT(Pins::WAKEUP_BUTTON), ESP_GPIO_WAKEUP_GPIO_LOW);
#endif

    esp_deep_sleep_start();
    // Wird nie erreicht.
}

#endif // MASSARBEIT_BUTTON_COUNT >= 1

// Wird von DeviceUi::runBootSequence() zwischen jedem angezeigten Frame
// aufgerufen - erledigt EINEN Initialisierungsschritt pro Aufruf und gibt
// true zurueck, solange noch etwas zu tun ist. So laeuft die Bootanimation
// parallel zur echten Initialisierung, statt hinterher eine feste Dauer
// draufzuschlagen.
bool runNextBootStep() {
    static uint8_t step = 0;
    switch (step++) {
        case 0:
            battery.begin();
            return true;
        case 1:
#if MASSARBEIT_BUTTON_COUNT >= 1
            buttons.begin();
            buttons.onButton1Click(onButton1Click);
            buttons.onSleepLongPress(onSleepLongPress);
            buttons.onButton2Click(onButton2Click);
            buttons.onCalibrationLongPress(onCalibrationRequested);
#endif
            return true;
        case 2:
            bootHx711Ok = scale.begin();
            return true;
        case 3:
            bleService.begin();
            return true;
        case 4:
            if (bootDevOtaRequested) {
                devOta.begin();
            } else {
                Serial.println("[Setup] Taste 2 beim Boot nicht gehalten - Dev-OTA/WLAN bleibt aus.");
            }
            return true;
        default:
            return false; // fertig
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    uint32_t waitStart = millis();
    // Nur kurz auf einen bereits offenen Serial Monitor warten (z.B. `pio
    // device monitor`, das kurz vor dem Einschalten schon laeuft) - im
    // normalen Party-Betrieb (Akku, kein USB-Kabel/Monitor) wuerde das
    // bisherige 3s-Timeout jedes Mal ungenutzt verstreichen, bevor ueberhaupt
    // mit der eigentlichen Initialisierung begonnen wird.
    while (!Serial && millis() - waitStart < 300) {
        delay(10);
    }

    Serial.println();
    Serial.println("==================================================");
    Serial.println(" " MASSARBEIT_MODEL_NAME " (" MASSARBEIT_MODEL_ID ")");
    Serial.println("==================================================");
    Serial.printf("Chip: %s rev %d, Free heap: %u bytes\n",
                   ESP.getChipModel(), ESP.getChipRevision(), ESP.getFreeHeap());

    // Beide moeglichen Ursachen pruefen, statt sie per #if auseinanderzu-
    // halten: ext0 meldet der S3, ESP_SLEEP_WAKEUP_GPIO der C3 der Basis.
    esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
    bool wokeFromSleep = wakeupCause == ESP_SLEEP_WAKEUP_EXT0 || wakeupCause == ESP_SLEEP_WAKEUP_GPIO;
    if (wokeFromSleep) {
        Serial.println("[Power] Aufgewacht aus Deep Sleep (Aufweck-Taster).");
    }

#if MASSARBEIT_HX711_SCK_CAN_HOLD
    // Gegenstueck zum Pin-Latch aus enterDeepSleep(): der RTC-Latch ueberlebt
    // den Aufwach-Reset und muss ausdruecklich geloest werden. Ohne das
    // bliebe HX711_SCK dauerhaft HIGH festgenagelt - der HX711 kaeme aus dem
    // Standby nie zurueck und die Waage meldete nach jedem Aufwachen
    // "HX711 antwortet nicht". Unbedingt VOR scale.begin() (Bootschritt 2).
    // Bedingungslos, nicht nur nach einem Aufwachen: nach einem Reset per
    // Taster/USB mitten im Schlaf steht derselbe Latch, ohne dass die
    // Aufweck-Ursache das noch verraet.
    gpio_deep_sleep_hold_dis();
    gpio_hold_dis((gpio_num_t)Pins::HX711_SCK);
#endif

    // Muss VOR allem anderen geprueft werden, das Taste 2 anfasst. NICHT
    // pruefen, wenn dieser Boot ein Aufwachen aus dem Deep Sleep ist: das
    // passiert ja GENAU ueber Taste 2 (siehe enterDeepSleep()/ext0-Wakeup),
    // die ist beim Hochfahren durch den Aufweck-Tastendruck zwangslaeufig
    // noch gehalten (Loslassen braucht laenger als die 20ms-Entprellung in
    // bootHeld()). Ohne diese Ausnahme startete jedes Aufwachen faelschlich
    // DevOta::begin() - blockiert bis zu 15s beim (mangels echter WLAN-
    // Zugangsdaten aussichtslosen) Verbindungsversuch, wodurch die
    // Boot-Sprite-Animation fuer die ganze Zeit eingefroren wirkte.
    bootDevOtaRequested = !wokeFromSleep && DevOta::bootHeld();
    devOtaActive = bootDevOtaRequested;

    ui.begin();
    ledRing.begin();
    // Der Ring sieht dieselben Cue-/Spielerwechsel wie die Hauptanzeige,
    // ohne dass BleWeightService oder das BLE-Protokoll etwas davon wissen
    // muessen (siehe attachLedRing() in TftDisplay.h/LedStatusUi.h).
    ui.attachLedRing(&ledRing);

    // Kein Text-Zwischenscreen mehr davor ("Starte...") - die Startsequenz
    // (Sprite-Animation auf der Vision, atmende Status-LED auf der
    // Basis) laeuft direkt los und erledigt die eigentliche Initialisierung
    // (battery/buttons/scale/BLE, siehe runNextBootStep() oben) parallel
    // dazu, statt hinterher eine feste Dauer draufzuschlagen.
    ui.runBootSequence(runNextBootStep);

    if (!bootHx711Ok) {
        ui.showMessage("Fehler", "HX711 antwortet\nnicht. Verkabelung\npruefen.");
        Serial.println("[Setup] WARNUNG: Waage laeuft ohne HX711 weiter (liefert 0g).");
        delay(2000);
    }

    Serial.println("[Setup] Bereit.");
#if MASSARBEIT_BUTTON_COUNT >= 2
    Serial.println("Taste 1: Spielauswahl weiter (kurz) / Deep Sleep (2s halten)");
    Serial.println("Taste 2: Spielauswahl bestaetigen (kurz) / Kalibrierung (lang halten)");
#else
    Serial.println("Keine Taster - Bedienung komplett ueber die App (Tara 0x01, Kalibrierung 0x20/0x21).");
    Serial.println("Ein/Aus: Schiebeschalter des Boards.");
#endif
    Serial.println("Serial: 'cal' + Enter startet die Kalibrierung.");
#if AUTO_SLEEP_TIMEOUT_MS > 0
    Serial.printf("[Power] Auto-Sleep nach %lu Minuten Inaktivitaet.\n", AUTO_SLEEP_TIMEOUT_MS / 60000UL);
#else
    Serial.println("[Power] Auto-Sleep deaktiviert - Geraet wird ueber den Board-Schalter ausgeschaltet.");
#endif

    lastActivityMs = millis();
}

void loop() {
#if MASSARBEIT_BUTTON_COUNT >= 1
    buttons.update();
    if (sleepRequested) {
        sleepRequested = false;
        enterDeepSleep();
    }
#endif
    devOta.update();
    checkSerialCalibrationTrigger();

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
#if AUTO_SLEEP_TIMEOUT_MS > 0
    if (!devOtaActive && millis() - lastActivityMs > AUTO_SLEEP_TIMEOUT_MS) {
        Serial.println("[Power] Auto-Sleep: keine Aktivitaet seit 10 Minuten.");
        enterDeepSleep();
    }
#endif

    if (calibrationRequested) {
        calibrationRequested = false;
        calibration.run();
    }

    // isSensorOk() statt isHX711Connected(): auch eine fehlende Waegezelle
    // (HX711 antwortet, liefert aber nur Anschlagwerte) ist ein Fehler der
    // Messkette und bekommt dieselbe Fehleranzeige auf TFT/LEDs.
    const bool sensorOk = scale.isSensorOk();
    ui.update(sensorOk, bleService.isConnected());
    // Gleiche Zustaende wie die Hauptanzeige, zusaetzlich das Live-Gewicht
    // fuer den Wiege-Balken - den gibt es weder auf dem TFT noch auf der
    // Status-LED (siehe LedRing::renderWeighing()).
    ledRing.setWeight(weight);
    ledRing.update(sensorOk, bleService.isConnected());

    delay(5);
}
