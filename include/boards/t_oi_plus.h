#pragma once

// Board-Profil "Basis": LilyGO T-OI Plus V1.3 (Silkscreen "mini D1 PLUS") -
// die abgespeckte Waage OHNE Display. Sie wiegt und funkt, sonst nichts; das
// gesamte Spielgeschehen laeuft ohnehin auf dem Handy. Rueckmeldung am Geraet
// gibt es nur ueber die Onboard-LED (siehe LedStatusUi) und Serial.
//
// Board: ESP32-C3 (RISC-V, NICHT Xtensa wie der S3), 4MB Flash, CH340C als
// USB-Serial-Wandler (also KEIN nativer USB -> Serial laeuft ueber UART0),
// Halter fuer eine 16340-Zelle mit Laderegler, Power-Schalter, Reset-Taster.
// PlatformIO-Boardprofil: `ttgo-t-oi-plus` (liegt der Plattform bei).
//
// Quellen fuer alles hier - nicht raten, nicht von einem generischen
// C3-Devkit uebernehmen:
//   Pinmap-Bild  https://github.com/Xinyuan-LilyGO/LilyGo-T-OI-PLUS
//                (image/new_TOI_Plus.png)
//   Arduino-Variante framework-arduinoespressif32/variants/ttgo-t-oi-plus
//   Beispiele    example/battery_voltage, example/simple im selben Repo
//
// Wird nie direkt eingebunden, immer ueber include/BoardConfig.h.

#include <Arduino.h>

// --- Faehigkeits-Steckbrief ------------------------------------------------
// Steuert die #if-Zweige im restlichen Code UND wird per BLE an die App
// gemeldet, damit sie sich zur Laufzeit auf das Modell einstellen kann
// (siehe BleWeightService::buildDeviceInfoJson()).
#define MASSARBEIT_MODEL_ID        "t-oi-plus"
#define MASSARBEIT_MODEL_NAME      "Massarbeit"
// Eigener BLE-Name, damit die zwei Modelle im Bluetooth-Dialog des Handys
// auseinanderzuhalten sind. Am Kommandoprotokoll (0x01, 0x10-0x15) aendert
// sich nichts - die Basis rendert dieselben Kommandos als LED-Muster statt
// als Vollbild-Animation, die App muss davon nichts wissen.
#define MASSARBEIT_BLE_NAME        "Massarbeit"
#define MASSARBEIT_HAS_TFT         0
#define MASSARBEIT_HAS_STATUS_LED  1
// Einfarbige Onboard-LED, keine WS2812 - jede "Farbe" wird deshalb auf ein
// eigenes Blink-/Helligkeitsmuster abgebildet (siehe LedStatusUi.cpp). Die
// NeoPixel-Beispiele in LilyGOs Repo gehoeren zum aufsteckbaren RGB-Shield
// (7 Pixel, Datenpin ueber DIP-Schalter waehlbar), nicht zum Board selbst.
#define MASSARBEIT_STATUS_LED_RGB  0
// WS2812B-Lichtring im Deckel: vorbereitet, aber AUS - die Firmware bringt
// die komplette Lichtlogik schon mit (src/LedRing.h), es fehlt nur die
// Hardware. Auf 1 setzen, sobald ein Ring an Pins::LED_RING_DATA haengt UND
// die NeoPixel-Abhaengigkeit in platformio.ini aktiviert ist; dann auch
// MASSARBEIT_LED_RING_COUNT auf die tatsaechliche LED-Zahl korrigieren.
// Fuer die Basis waere der Ring die erste echte Farbanzeige ueberhaupt - die
// einfarbige Onboard-LED laeuft unveraendert weiter, beide ergaenzen sich.
#define MASSARBEIT_HAS_LED_RING    0
#define MASSARBEIT_LED_RING_COUNT  16
#define MASSARBEIT_HAS_BATTERY     1
#define MASSARBEIT_BUTTON_COUNT    1
#define MASSARBEIT_HAS_POWER_ON    0
// Kein Entwicklungs-OTA per WLAN auf der Basis: die Aktivierung haengt am
// zweiten Taster (den es hier nicht gibt), und Firmware-Updates laufen
// ohnehin ueber BLE aus der App (OtaUpdater). Spart nebenbei den kompletten
// WLAN-/ArduinoOTA-Code im Binary.
#define MASSARBEIT_HAS_DEV_OTA     0

// Der C3 kennt KEIN ext0-Wakeup (das gibt es nur auf ESP32/S3). Aufgeweckt
// wird stattdessen ueber esp_deep_sleep_enable_gpio_wakeup(), siehe
// enterDeepSleep() in main.cpp.
#define MASSARBEIT_WAKEUP_USES_EXT0 0

// HX711_SCK ist hier GPIO7 - RTC-faehig sind am C3 nur GPIO0-5, der Pegel
// laesst sich ueber den Deep Sleep also NICHT einfrieren.
//
// Folge fuers Stromsparen: Scale::powerDown() schickt den HX711 zwar auch
// hier in den Standby (~0.3µA statt ~1.4-1.6mA), aber sobald der Chip
// schlaeft, wird PD_SCK hochohmig - ob der HX711 dann im Standby bleibt,
// haengt allein an Leckstroemen und ist nicht garantiert. Wer die
// Standby-Zeit der Basis wirklich braucht, loetet einen Pullup (~10k) von
// HX711_SCK nach 3V3: dann haelt der Pegel auch ohne aktiven Treiber, und
// im Betrieb zieht der GPIO ihn ohne Weiteres gegen den Widerstand nach
// unten. Ohne diesen Widerstand einfach nachmessen, bevor man sich auf eine
// Laufzeit verlaesst.
#define MASSARBEIT_HX711_SCK_CAN_HOLD 0

// Spannungsteiler vor dem Batterie-ADC: Faktor 2, exakt wie in LilyGOs
// eigenem example/battery_voltage (`readADC_Cal(analogRead(BAT_ADC)) * 2`).
#define MASSARBEIT_BATTERY_DIVIDER 2

// Polaritaet der Onboard-LED. 0 = HIGH schaltet ein. Aus den offiziellen
// Quellen nicht belegt (die Arduino-Variante nennt nur den Pin) - leuchtet
// die LED nach dem Flashen genau invers zu dem, was das Serial-Log sagt,
// hier auf 1 aendern, sonst aendert sich nichts.
#define MASSARBEIT_STATUS_LED_ACTIVE_LOW 0

namespace Pins {

// --- HX711 -------------------------------------------------------------
// GPIO6/7 (Silkscreen MTCK/MTDO): auf der Stiftleiste, kein Strapping-Pin,
// keine Flash-/UART-Funktion. Dass es nebenbei JTAG- und FSPI-Pins sind,
// stoert nicht - JTAG-Debugging nutzt das Projekt nicht, und der interne
// Flash haengt am C3 an eigenen, gar nicht herausgefuehrten Leitungen.
constexpr uint8_t HX711_DOUT = 6;
constexpr uint8_t HX711_SCK  = 7;

// --- Taster ---------------------------------------------------------------
// EIN externer Taster gegen GND, interner Pullup - kein Widerstand noetig.
// Belegung siehe Buttons.h: kurz = Tara, lang (2s) = Deep Sleep,
// Doppelklick = Kalibrierroutine. (Das Board selbst hat nur Reset und einen
// Power-Schalter, keinen frei belegbaren Taster.)
//
// GPIO5 ist bewusst gewaehlt und sollte nicht ohne erneute Pruefung
// umsortiert werden - auf dem C3 schraenken drei Dinge die Wahl ein:
//   - Nur GPIO0-5 sind RTC-faehig, und NUR die koennen aus dem Deep Sleep
//     aufwecken. Herausgefuehrt sind davon GPIO2, 4 und 5.
//   - GPIO2 ist Strapping-Pin (und ausserdem schon der Batterie-ADC, s.u.).
//     GPIO8/9 sind ebenfalls Strapping-Pins (GPIO8 traegt auf diesem Board
//     sogar den Aufdruck "Boot") - ein beim Einschalten gedrueckter Taster
//     an so einem Pin koennte den Chip in den Flash-Download-Modus booten
//     statt in die Firmware. Genau das droht hier, weil derselbe Taster das
//     Geraet aufweckt und beim folgenden Boot noch gedrueckt ist.
//   - Bleiben GPIO4 und GPIO5. GPIO5 ist ADC2 (mit Funk unbrauchbar) und
//     damit ohnehin der schlechtere ADC-Pin - also der bessere Taster-Pin,
//     GPIO4 (ADC1) bleibt fuer spaetere Analog-Erweiterungen frei.
constexpr uint8_t BUTTON_1      = 5;
constexpr uint8_t WAKEUP_BUTTON = BUTTON_1;

// --- Status-LED -----------------------------------------------------------
// Onboard-LED, entspricht LED_BUILTIN der Arduino-Variante. GPIO3 ist auf
// keine Stiftleiste herausgefuehrt, kann also gar nichts anderes sein.
constexpr uint8_t STATUS_LED = 3;

// --- WS2812B-Lichtring (vorbereitet, siehe MASSARBEIT_HAS_LED_RING) -------
// GPIO4: der einzige Pin, den die Basis noch komplett frei hat (oben bei
// BUTTON_1 als "bleibt fuer spaetere Erweiterungen frei" vermerkt - hier ist
// die Erweiterung). Kein Strapping-Pin, kein Flash-/UART-Pin, auf die
// Stiftleiste herausgefuehrt. Dass GPIO4 zugleich ADC1 ist, spielt hier
// keine Rolle: als Datenausgang wird der ADC ohnehin nicht genutzt.
//
// Vor dem Festloeten beachten (siehe ROADMAP.md, Punkt zum LED-Ring):
//   - Pegel: der C3 gibt 3.3V aus, WS2812B sind fuer 5V-Logik spezifiziert.
//     Kurze Leitungen laufen meist trotzdem, sicher ist ein Level-Shifter.
//   - 300-500Ohm in die Datenleitung und ein Puffer-Kondensator (~1000µF)
//     ueber die 5V-Versorgung des Rings, sonst kann die erste LED sterben.
//   - Strom: der Ring haengt an 5V, NICHT an der 16340-Zelle - das Board
//     kann ihn nicht mitversorgen (siehe LED_RING_MAX_BRIGHTNESS).
//   - RUHESTROM: jede WS2812B zieht ~0.6-1mA fuer ihren internen Controller,
//     auch wenn sie schwarz ist. Bei 16 LEDs sind das ~10-16mA rund um die
//     Uhr - das leert eine 700mAh-Zelle in unter zwei Tagen und macht den
//     ganzen Deep Sleep zunichte. LedRing::prepareForSleep() hilft dagegen
//     NICHT, es macht die LEDs nur dunkel. Der Ring braucht deshalb einen
//     Schalt-MOSFET (P-Kanal high-side oder Load-Switch) in seiner
//     5V-Zuleitung, der vor dem Schlafen abschaltet - dafuer ist noch kein
//     GPIO vorgesehen - dafuer ist LED_RING_POWER da, siehe unten.
constexpr uint8_t LED_RING_DATA = 4;

// Schaltet die 5V-Zuleitung des Rings ueber einen High-Side-Schalter
// (P-MOSFET mit Gate-Pullup nach 5V + kleiner N-MOSFET als Pegelwandler,
// oder ein fertiger Load-Switch). HIGH = Ring hat Strom, LOW/hochohmig =
// Ring komplett tot.
//
// Diese Polaritaet ist Absicht und nicht beliebig: im Deep Sleep und
// waehrend des Bootens ist der GPIO hochohmig, der Gate-Pullup schaltet den
// P-MOSFET dann von selbst AUS. Der Ruhestrom der LEDs (~0.6-1mA je Stueck,
// auch wenn sie schwarz sind) ist damit sicher weg, ohne dass die Firmware
// im Schlaf noch irgendetwas halten muesste.
//
// GPIO10 ist auf dem T-OI Plus der letzte voellig freie Header-Pin (linke
// Reihe, unterster vor 3V3, Aufdruck "10"): kein Strapping-Pin (das sind am
// C3 GPIO2/8/9), kein UART (GPIO20/21), kein ADC, den wir brauchen. Dass er
// im Pinmap als FSPI_CS0 gefuehrt wird, stoert nicht - der interne Flash des
// C3 haengt an eigenen, gar nicht herausgefuehrten Leitungen (gleiche
// Argumentation wie bei HX711_DOUT/SCK oben). Alternative, falls GPIO10
// anderweitig gebraucht wird: GPIO18/19, die auch am Grove-Stecker liegen.
constexpr uint8_t LED_RING_POWER = 10;

// --- Batterie -------------------------------------------------------------
// GPIO2 = ADC1_CH2, haengt ueber den Onboard-Spannungsteiler an der
// 16340-Zelle (siehe MASSARBEIT_BATTERY_DIVIDER). Identisch mit
// BAT_ADC_PIN der Arduino-Variante und mit LilyGOs battery_voltage-Beispiel.
constexpr uint8_t BATTERY_ADC = 2;

} // namespace Pins
