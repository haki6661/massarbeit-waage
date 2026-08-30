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

// Welches Spiel gerade laeuft - bestimmt, welche Away-Animation gezeigt wird
// (Ball/Pfeil/Karte/Block, siehe renderAway*() in TftDisplay.cpp) und das
// kleine Icon im Spieler-Badge. Werte muessen exakt zum <gameId>-Byte im
// BLE-Protokoll passen, siehe Config.h.
enum class GameKind : uint8_t {
    None = 0,
    Golf = 1,
    Dart = 2,
    Blackjack = 3,
    Tower = 4,
    Scale = 5,
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

    // Animierte Startsequenz: spielt die Pixel-Art-Sprite-Animation aus
    // data/ (SPIFFS, siehe platformio.ini) - kein Text/Logo davor,
    // die Animation selbst zeigt am Ende den Maßarbeit-Schriftzug. Ruft
    // zwischen jedem gezeigten Frame `stepInit()` auf, das den naechsten
    // Initialisierungsschritt erledigen und false zurueckgeben soll, sobald
    // nichts mehr zu tun ist - die Animation laeuft dadurch parallel zur
    // echten Initialisierung statt eine feste Dauer aufzuschlagen. Spielt
    // dabei IMMER mindestens einen kompletten Durchlauf aller Frames ab
    // (bis zum Schriftzug), auch wenn stepInit() laengst vorher fertig ist -
    // sonst waere bei einem schnellen Boot nur der Anfang zu sehen. Braucht
    // die Initialisierung laenger als ein Durchlauf, laeuft die Animation
    // einfach weiter (Endlosschleife), bis auch das fertig ist. Blockierend,
    // einmalig in setup() - siehe main.cpp.
    void playBootSprite(bool (*stepInit)());

    // Von BleWeightService bei einem 0x10/0x11/0x12/0x13-Kommando aufgerufen.
    // Ueberlagert die naechsten update()-Aufrufe, bis entweder explizit
    // RemoteCue::None gesetzt wird oder das interne Timeout ablaeuft
    // (Sicherheitsnetz, falls die App das Zuruecksetzen vergisst/die BLE-
    // Verbindung mitten im Ritual abbricht). `game` waehlt bei Away die
    // passende Animation, sonst ohne Effekt.
    void setRemoteCue(RemoteCue cue, GameKind game = GameKind::None);

    // Von BleWeightService bei einem 0x14/0x15-Kommando aufgerufen: zeigt
    // ein kleines Namens-/Farb-Badge (+ Spiel-Icon) oben auf dem normalen
    // Gewichtsscreen, solange ein Spieler am Zug ist - siehe
    // renderPlayerBadge() in TftDisplay.cpp. `name` wird auf ca. 10 Zeichen
    // gekuerzt (mehr passt auf dem 320x170-Display ohnehin nicht lesbar hin).
    void setActivePlayer(GameKind game, uint16_t color565, const String& name);
    void clearActivePlayer();

    // Wandelt 8-Bit-RGB (wie von der App per BLE geschickt, siehe
    // COMMAND_PLAYER_TURN in Config.h) in den RGB565-Wert um, den Arduino_GFX
    // erwartet - eigene Methode, weil `gfx_` (und damit color565()) privat ist.
    uint16_t color565FromRgb(uint8_t r, uint8_t g, uint8_t b) const { return gfx_->color565(r, g, b); }

private:
    Arduino_DataBus* bus_ = nullptr;
    Arduino_GFX* gfx_ = nullptr;

    // Café-Kreide-Akzentfarben der Handy-App (src/styles/globals.css im
    // App-Repo) angenaehert auf RGB565 - einmal in begin() berechnet statt
    // in jeder Render-Funktion neu (gfx_->color565() ist zwar billig, aber
    // so gibt es nur eine Quelle der Wahrheit fuers Farbschema).
    uint16_t emberColor_ = 0;
    uint16_t zestColor_ = 0;
    uint16_t creamColor_ = 0;
    uint16_t coralColor_ = 0;

    DisplayMode lastMode_ = DisplayMode::Weight;
    uint32_t lastRenderMs_ = 0;
    bool forceRedraw_ = true;
    bool lastBleConnected_ = false;

    RemoteCue remoteCue_ = RemoteCue::None;
    GameKind remoteCueGame_ = GameKind::None; // nur fuer RemoteCue::Away relevant
    uint32_t remoteCueSetMs_ = 0;

    bool hasActivePlayer_ = false;
    GameKind activeGame_ = GameKind::None;
    uint16_t activePlayerColor565_ = 0;
    String activePlayerName_;

    void renderWeightScreen(float weight, bool hx711Connected, bool bleConnected, bool fullRedraw);
    void renderStatusScreen(float rawReading, bool hx711Connected, bool bleConnected, float batteryVoltage);
    void renderRemoteCueScreen(RemoteCue cue);
    void renderPlayerBadge(int16_t x, int16_t y);
    void renderGameIcon(GameKind game, int16_t cx, int16_t cy, int16_t size, uint16_t color);

    void renderAwayGolf();
    void renderAwayDart();
    void renderAwayBlackjack();
    void renderAwayTower();
};
