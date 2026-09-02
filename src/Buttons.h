#pragma once

// Ersetzt WeighMyBru2s kapazitive Touch-Sensoren (TouchSensor.h/.cpp) durch
// die physischen Taster des Boards. Nutzt die OneButton-Library (liegt
// LilyGOs eigenem T-Display-S3-Repo bei) statt eigenem Debounce-/
// Long-Press-Code.
//
// Belegung Vision (zwei Taster):
//   Taste 1 (GPIO0/BOOT): kurzer Klick -> Geraete-Spielauswahl: naechstes Spiel
//                         langer Druck (2s) -> Deep Sleep
//   Taste 2 (GPIO14):     kurzer Klick -> Geraete-Spielauswahl: bestaetigen
//                          langer Druck (1,5s) -> Kalibrierroutine starten
//
// Belegung Basis (ein Taster, keine Geraete-Spielauswahl mangels Display):
//   Taste 1 (GPIO33):     kurzer Klick -> Tara
//                         langer Druck (2s) -> Deep Sleep
//                         Doppelklick -> Kalibrierroutine starten
//
// Auf der Vision gibt es Tara als eigene Tastenfunktion nicht - dort
// laeuft es automatisch (Auto-Zero-Nachfuehrung, siehe Scale.cpp) oder ueber
// die App (BLE-Kommando 0x01); der kurze Klick wird fuer die Spielauswahl
// gebraucht. Auf der Basis ist der Klick frei und uebernimmt Tara.
//
// Aufwachen aus dem Deep Sleep passiert ueber Pins::WAKEUP_BUTTON (siehe
// Board-Profil) - auf der Vision bewusst NICHT ueber Taste 1: GPIO0
// ist ein Strapping-Pin (BOOT-Auswahl), der beim Boot-Vorgang nach dem
// Aufwachen versehentlich in den Flash-Download-Modus fuehren kann, wenn er
// dabei gedrueckt gehalten wird.

#include <OneButton.h>

#include "BoardConfig.h"

class Buttons {
public:
    Buttons();

    void begin();
    void update(); // in loop() aufrufen

    void onButton1Click(void (*callback)());
    void onSleepLongPress(void (*callback)());

#if MASSARBEIT_BUTTON_COUNT >= 2
    void onButton2Click(void (*callback)());
    void onCalibrationLongPress(void (*callback)());
#else
    // Auf der Basis ist der lange Druck schon mit Deep Sleep belegt - die
    // Kalibrierung haengt deshalb am Doppelklick. Nebenwirkung von OneButton:
    // sobald ein Doppelklick-Callback gesetzt ist, meldet sich der einfache
    // Klick erst nach Ablauf des Doppelklick-Fensters (~400ms). Fuer "Tara"
    // ist das unkritisch.
    void onCalibrationDoubleClick(void (*callback)());
#endif

private:
    OneButton button1_;
#if MASSARBEIT_BUTTON_COUNT >= 2
    OneButton button2_;
#endif
};
