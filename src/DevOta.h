#pragma once

#include <Arduino.h>

// WLAN + ArduinoOTA - NUR fuer die Entwicklungsphase. Bewusst in einem
// eigenen, isolierten Modul, das nichts von Scale/BLE/Display weiss, damit es
// sich ersatzlos entfernen laesst.
//
// Der eigentliche Update-Weg ist inzwischen BLE-OTA aus der App (siehe
// OtaUpdater.h) und funktioniert auf beiden Varianten. Dieses Modul bleibt
// nur als Werkbank-Bequemlichkeit: waehrend der Entwicklung an der Vision
// spart es das Umstecken des USB-Kabels.
//
// Aktivierung nur, wenn Taste 2 beim Booten gehalten wird - im normalen
// Party-Betrieb bleibt WLAN dadurch komplett aus (Akkuschonung, und die
// Waage muss beim Trinkspiel nicht in Reichweite eines bekannten WLANs sein).
//
// Nur auf der Vision (MASSARBEIT_HAS_DEV_OTA): die Basis hat gar keinen
// Taster, ueber den sich das aktivieren liesse, und Updates laufen dort
// ohnehin per BLE aus der App. Dort sind alle Methoden leere Attrappen
// (siehe DevOta.cpp), damit main.cpp ohne #if auskommt.
class DevOta {
public:
    // Frueh in setup() aufrufen (vor allem anderen, das GPIO14 anfasst).
    static bool bootHeld();

    void begin(); // verbindet WLAN + startet ArduinoOTA; blockiert bis zu ~15s
    void update(); // in loop() aufrufen
    bool isActive() const { return active_; }
    String statusLine() const;

private:
    bool active_ = false;
};
