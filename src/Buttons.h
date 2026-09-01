#pragma once

// Ersetzt WeighMyBru2s kapazitive Touch-Sensoren (TouchSensor.h/.cpp) durch
// die zwei physischen Taster des T-Display S3. Nutzt die OneButton-Library
// (liegt LilyGOs eigenem T-Display-S3-Repo bei) statt eigenem
// Debounce-/Long-Press-Code.
//
// Belegung:
//   Taste 1 (GPIO0/BOOT): kurzer Klick -> Geraete-Spielauswahl: naechstes Spiel
//                         langer Druck (2s) -> Deep Sleep
//   Taste 2 (GPIO14):     kurzer Klick -> Geraete-Spielauswahl: bestaetigen
//                          langer Druck (1,5s) -> Kalibrierroutine starten
//
// Tara gibt es als eigene Tastenfunktion nicht mehr - laeuft nur noch
// automatisch (Auto-Zero-Nachfuehrung, siehe Scale.cpp) oder ueber die App
// (BLE-Kommando 0x01).
//
// Aufwachen aus dem Deep Sleep passiert NICHT ueber Taste 1, sondern immer
// ueber Taste 2 (siehe main.cpp) - GPIO0/Taste 1 ist ein Strapping-Pin
// (BOOT-Auswahl), der beim Boot-Vorgang nach dem Aufwachen versehentlich in
// den Flash-Download-Modus fuehren kann, wenn er dabei gedrueckt gehalten
// wird.

#include <OneButton.h>

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
