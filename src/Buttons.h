#pragma once

// Physische Taster statt der kapazitiven Touch-Sensoren aus WeighMyBru2
// (TouchSensor.h/.cpp). Nutzt die OneButton-Library (liegt LilyGOs eigenem
// T-Display-S3-Repo bei) statt eigenem Debounce-/Long-Press-Code.
//
// Wird NUR fuer Varianten mit Tastern gebaut - aktuell also allein fuer die
// Vision. Die Basis hat keinen Taster mehr: sie wird ueber den
// Schiebeschalter des Boards ein- und ausgeschaltet, alles andere laeuft
// ueber die App (siehe t_oi_plus.h). In ihrem Build ist diese Datei per
// build_src_filter ausgeschlossen (platformio.ini).
//
// Belegung Vision:
//   Taste 1 (GPIO0/BOOT): kurzer Klick -> Geraete-Spielauswahl: naechstes Spiel
//                         langer Druck (2s) -> Deep Sleep
//   Taste 2 (GPIO14):     kurzer Klick -> Geraete-Spielauswahl: bestaetigen
//                         langer Druck (1,5s) -> Kalibrierroutine starten
//
// Tara gibt es bewusst nicht auf einer Taste - es laeuft automatisch
// (Auto-Zero-Nachfuehrung, siehe Scale.cpp) oder ueber die App
// (BLE-Kommando 0x01); der kurze Klick wird fuer die Spielauswahl gebraucht.
//
// Aufwachen aus dem Deep Sleep passiert ueber Pins::WAKEUP_BUTTON, also
// Taste 2 - bewusst NICHT ueber Taste 1: GPIO0 ist ein Strapping-Pin
// (BOOT-Auswahl), der beim Boot-Vorgang nach dem Aufwachen versehentlich in
// den Flash-Download-Modus fuehren kann, wenn er dabei gedrueckt gehalten
// wird.

#include <OneButton.h>

#include "BoardConfig.h"

#if MASSARBEIT_BUTTON_COUNT < 2
#error "Buttons.h ist nur fuer Varianten mit zwei Tastern gedacht - Variante ohne Taster? Dann gehoert Buttons.cpp per build_src_filter aus dem Build (siehe platformio.ini)."
#endif

class Buttons {
public:
    Buttons();

    void begin();
    void update(); // in loop() aufrufen

    void onButton1Click(void (*callback)());
    void onSleepLongPress(void (*callback)());
    void onButton2Click(void (*callback)());
    void onCalibrationLongPress(void (*callback)());

private:
    OneButton button1_;
    OneButton button2_;
};
