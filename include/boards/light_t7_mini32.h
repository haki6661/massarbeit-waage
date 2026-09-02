#pragma once

// Board-Profil "Light": LilyGO T7 v1.3 Mini32 (Aufdruck "mini D1 PLUS") -
// die abgespeckte Waage OHNE Display. Sie wiegt und funkt, sonst nichts; das
// gesamte Spielgeschehen laeuft ohnehin auf dem Handy. Rueckmeldung am Geraet
// gibt es nur ueber die Onboard-LED (siehe LedStatusUi) und Serial.
//
// Board: ESP32-WROOM-32 (Xtensa LX6, KEIN C3/RISC-V), 4MB Flash, CH340C als
// USB-Serial-Wandler, TP4054-Laderegler mit JST-1.25mm-Akkubuchse.
// PlatformIO-Boardprofil: `ttgo-t7-v13-mini32` (liegt der Plattform bei).
// Arduino-Variante: framework-arduinoespressif32/variants/ttgo-t7-v13-mini32.
//
// Wird nie direkt eingebunden, immer ueber include/BoardConfig.h.

#include <Arduino.h>

// --- Faehigkeits-Steckbrief ------------------------------------------------
// Steuert die #if-Zweige im restlichen Code UND wird per BLE an die App
// gemeldet, damit sie sich zur Laufzeit auf das Modell einstellen kann
// (siehe BleWeightService::buildDeviceInfoJson()).
#define MASSARBEIT_MODEL_ID        "light-t7"
#define MASSARBEIT_MODEL_NAME      "Massarbeit Waage Light"
// Eigener BLE-Name, damit die zwei Modelle im Bluetooth-Dialog des Handys
// auseinanderzuhalten sind. Am Kommandoprotokoll (0x01, 0x10-0x15) aendert
// sich nichts - die Light rendert dieselben Kommandos eben als LED-Muster
// statt als Vollbild-Animation, die App muss davon nichts wissen.
#define MASSARBEIT_BLE_NAME        "Massarbeit-Light"
#define MASSARBEIT_HAS_TFT         0
#define MASSARBEIT_HAS_STATUS_LED  1
// Einfarbige Onboard-LED, keine WS2812 - jede "Farbe" wird deshalb auf ein
// eigenes Blink-/Helligkeitsmuster abgebildet (siehe LedStatusUi.cpp).
#define MASSARBEIT_STATUS_LED_RGB  0
#define MASSARBEIT_HAS_BATTERY     1
#define MASSARBEIT_BUTTON_COUNT    1
#define MASSARBEIT_HAS_POWER_ON    0
// Kein Entwicklungs-OTA per WLAN auf der Light: die Aktivierung haengt am
// zweiten Taster (den es hier nicht gibt), und Firmware-Updates laufen
// ohnehin ueber BLE aus der App (OtaUpdater). Spart nebenbei den kompletten
// WLAN-/ArduinoOTA-Code im Binary.
#define MASSARBEIT_HAS_DEV_OTA     0

// Spannungsteiler vor dem Batterie-ADC (2x 100k -> Faktor 2), wie auf den
// T7-Boards ueblich. Falls die eigene Board-Revision doch keinen Teiler
// bestueckt hat, meldet Battery::readPercent() von selbst "unbekannt"
// (Plausibilitaetsuntergrenze dort) - die App blendet die Akkuanzeige dann
// aus, statt eine Zahl zu raten.
#define MASSARBEIT_BATTERY_DIVIDER 2

// Polaritaet der Onboard-LED. 0 = HIGH schaltet ein (Annahme, deckt sich mit
// dem Tasmota-Template der Schwesterrevision v1.5). Leuchtet die LED nach dem
// Flashen genau invers zu dem, was das Serial-Log sagt: hier auf 1 aendern,
// sonst aendert sich nichts.
#define MASSARBEIT_STATUS_LED_ACTIVE_LOW 0

namespace Pins {

// --- HX711 -------------------------------------------------------------
// GPIO25/26: beide auf die Stiftleiste gefuehrt, kein Strapping-Pin, keine
// Flash-/USB-/UART-Funktion. (Dass es nebenbei die beiden DAC-Ausgaenge sind,
// stoert nicht - die braucht das Projekt nirgends.)
constexpr uint8_t HX711_DOUT = 25;
constexpr uint8_t HX711_SCK  = 26;

// --- Taster ---------------------------------------------------------------
// EIN externer Taster gegen GND, interner Pullup - kein Widerstand noetig.
// Belegung siehe Buttons.h: kurz = Tara, lang (2s) = Deep Sleep,
// Doppelklick = Kalibrierroutine.
//
// GPIO33 ist bewusst gewaehlt und sollte nicht ohne erneute Pruefung
// umsortiert werden:
//   - Der Onboard-BOOT-Taster (GPIO0) taugt NICHT: GPIO0 ist der
//     Strapping-Pin fuer den Flash-Download-Modus. Genau dieser Taster weckt
//     hier aber aus dem Deep Sleep auf, ist beim folgenden Boot also
//     zwangslaeufig noch gedrueckt - der Chip wuerde statt der Firmware den
//     Bootloader starten.
//   - Nur RTC-faehige GPIOs koennen ueberhaupt aus dem Deep Sleep aufwecken
//     (ext0). GPIO33 ist RTC-faehig, GPIO2/12/15 waeren es zwar auch, sind
//     aber ihrerseits Strapping-Pins.
constexpr uint8_t BUTTON_1      = 33;
constexpr uint8_t WAKEUP_BUTTON = BUTTON_1;

// --- Status-LED -----------------------------------------------------------
// Onboard-LED des T7 v1.3, entspricht LED_BUILTIN der Arduino-Variante.
constexpr uint8_t STATUS_LED = 22;

// --- Batterie -------------------------------------------------------------
// GPIO35 = ADC1_CH7, input-only - haengt auf den T7-Boards ueber den
// Spannungsteiler am Akku (siehe MASSARBEIT_BATTERY_DIVIDER).
constexpr uint8_t BATTERY_ADC = 35;

} // namespace Pins
