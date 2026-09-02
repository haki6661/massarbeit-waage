#pragma once

// Interaktive Kalibrierung, angelehnt an WeighMyBru2s Web-API-Endpunkt
// "/api/calibrate" (Faktor = Rohwert / bekanntes Gewicht), nur eben ueber
// Serial+Taster+Geraeteanzeige statt eines Webformulars.
//
// Bewusst BLOCKIEREND: Kalibrieren ist ein seltener, bewusster Wartungs-
// schritt, bei dem sowieso jemand direkt am Geraet mit offenem Serial
// Monitor steht - auf der Light ist Serial ohnehin der einzige Kanal, sie hat
// kein Display, auf dem die Aufforderungen stehen koennten (siehe
// LedStatusUi::showMessage()). Waehrend run() laufen BLE/Anzeige/Taster-
// Polling der Hauptschleife nicht weiter - das ist hier in Ordnung und
// deutlich einfacher als eine nebenlaeufige State Machine.

#include "DeviceUi.h"
#include "Scale.h"

class CalibrationRoutine {
public:
    CalibrationRoutine(Scale& scale, DeviceUi& ui);

    void run();

private:
    Scale& scale_;
    DeviceUi& ui_;

    // Blockiert, bis eine nicht-leere Zeile ueber Serial ankommt.
    String readSerialLine();

    // Blockiert, bis IRGENDEIN Byte ueber Serial ankommt - fuer reine
    // "weiter"-Bestaetigungen (z.B. "Waage leeren, dann Enter"), bei denen
    // der Inhalt egal ist. Ein blosses Enter druecken reicht hier bewusst,
    // anders als bei readSerialLine() (das fuer Werte wie das Referenz-
    // gewicht mindestens ein Zeichen vor dem Enter braucht).
    void waitForAnyInput();
};
