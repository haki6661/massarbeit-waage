#pragma once

// Minimale TFT-Textausgabe fuer den T-Display S3. Bewusst ohne Animationen -
// die kommen laut Vorgabe erst spaeter. Ersetzt WeighMyBru2s
// Display.h/.cpp (Adafruit_SSD1306 an I2C) komplett, weil der T-Display S3
// sein ST7789-Display ueber einen 8-Bit-Parallelbus ansteuert, nicht I2C.
//
// Bus-/Panel-Konstruktion 1:1 aus LilyGOs eigenem Beispiel uebernommen
// (Xinyuan-LilyGO/T-Display-S3, examples/Arduino_GFXDemo) - nicht geraten.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

enum class DisplayMode {
    Weight,
    Status,
};

// Von der App per BLE ferngesteuerte Anzeige-Hinweise (siehe Config.h fuer
// das Kommando-Protokoll) - ueberlagern DisplayMode voruebergehend, z.B.
// waehrend des Turn-Readiness-Rituals eines Spiels. Grundgeruest: aktuell
// nur Text/Farbe je Zustand, spaeter geplant sind echte Animationen
// (Abschlag-Animation, unterschiedliche Animation je Ergebnis-Guete).
enum class RemoteCue {
    None,
    Ready,          // "Bereit, jetzt trinken"
    Away,           // Glas komplett von der Waage gehoben ("Abschlag") - laeuft
                    // als Endlosschleife weiter, bis Ergebnis/Idle kommt
    ResultMiss,     // Schluck lag daneben
    ResultClose,    // Schluck lag nah am Ziel
    ResultPerfect,  // Schluck hat exakt getroffen
};

class TftDisplay {
public:
    void begin();

    // In loop() aufrufen. Zeichnet intern gedrosselt (~alle 150ms) neu, um
    // nicht bei jedem Loop-Durchlauf den ganzen Bildschirm neu zu fuellen.
    // rawReading: letzter unkalibrierter Messwert (Scale::getLastRawReading())
    // - reine Debug-Info, um ohne Kalibrierung zu sehen, dass sich die
    // Waegezelle ueberhaupt aendert.
    void update(DisplayMode mode, float weight, float rawReading, bool hx711Connected,
                bool bleConnected, float batteryVoltage);

    // Sofortiger, vollflaechiger Text-Screen - fuer Start- und
    // Kalibrierungs-Prompts, unabhaengig vom Drossel-Timer von update().
    void showMessage(const String& title, const String& body = "");

    // Animierte Startsequenz: eine gedaempft schwingende, sich einpendelnde
    // Balkenwaage (rein prozedural, kein Sprite/Bitmap noetig - siehe
    // renderRemoteCueScreen()'s Ball-Flug-Animation fuer denselben Ansatz),
    // danach die Boot-Checks (HX711/Akku/BLE) einzeln eingeblendet statt als
    // einmaliger Textblock. Blockierend (~2,5s), einmalig in setup() vor der
    // ersten normalen Anzeige - siehe main.cpp.
    void playBootAnimation(bool hx711Connected, float batteryVoltage, bool bleStarted);

    // Von BleWeightService bei einem 0x10/0x11/0x12-Kommando aufgerufen.
    // Ueberlagert die naechsten update()-Aufrufe, bis entweder explizit
    // RemoteCue::None gesetzt wird oder das interne Timeout ablaeuft
    // (Sicherheitsnetz, falls die App das Zuruecksetzen vergisst/die BLE-
    // Verbindung mitten im Ritual abbricht).
    void setRemoteCue(RemoteCue cue);

private:
    Arduino_DataBus* bus_ = nullptr;
    Arduino_GFX* gfx_ = nullptr;

    DisplayMode lastMode_ = DisplayMode::Weight;
    uint32_t lastRenderMs_ = 0;
    bool forceRedraw_ = true;

    RemoteCue remoteCue_ = RemoteCue::None;
    uint32_t remoteCueSetMs_ = 0;

    void renderWeightScreen(float weight, float rawReading, bool hx711Connected, bool bleConnected);
    void renderStatusScreen(bool hx711Connected, bool bleConnected, float batteryVoltage);
    void renderRemoteCueScreen(RemoteCue cue);
};
