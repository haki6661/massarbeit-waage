#pragma once

#include <Arduino.h>

// WLAN + ArduinoOTA - NUR fuer die Entwicklungsphase, wie besprochen.
// Bewusst in einem eigenen, isolierten Modul, das nichts von Scale/BLE/
// Display weiss: der geplante Ersatz ist spaeter BLE-OTA ueber die Handy-
// Web-App, und dieser Block soll sich dann ersatzlos entfernen lassen.
//
// Aktivierung nur, wenn Taste 2 beim Booten gehalten wird - im normalen
// Party-Betrieb bleibt WLAN dadurch komplett aus (Akkuschonung, und die
// Waage muss beim Trinkspiel nicht in Reichweite eines bekannten WLANs sein).
//
// Nur auf der grossen Waage (MASSARBEIT_HAS_DEV_OTA): der Light fehlt der
// zweite Taster fuer die Aktivierung, und Updates laufen dort ohnehin per
// BLE aus der App. Dort sind alle Methoden leere Attrappen (siehe
// DevOta.cpp), damit main.cpp ohne #if auskommt.
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
