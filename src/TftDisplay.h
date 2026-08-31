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

// Lokaler Anzeige-Zustand, wenn WEDER ein RemoteCue laeuft NOCH ein Spieler
// am Zug ist (siehe hasActivePlayer_) - die Waage ist "zwischen Spielen".
// Ersetzt die bisherige reine Live-Gewichtsanzeige (war nur zum Debuggen
// gedacht, siehe ROADMAP.md Punkt 1) durch die Geraete-Spielauswahl: Taste 1
// kurz schaltet durch die Spieleliste, Taste 2 kurz bestaetigt.
enum class LocalScreen {
    GamePicker,     // "<Icon> <Name>", Taste 1/2-Hinweis
    GameConfirmed,  // "<Name> ausgewaehlt - jetzt in der App oeffnen"
};

// Von der App per BLE ferngesteuerte Anzeige-Hinweise (siehe Config.h fuer
// das Kommando-Protokoll) - ueberlagern LocalScreen voruebergehend, z.B.
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
    // Zeigt je nach Zustand: RemoteCue (laueft ein Spiel-Ritual) > Spieler-
    // Warteschirm (Spieler am Zug, aber gerade kein Cue) > Geraete-
    // Spielauswahl (Ruhezustand) - siehe LocalScreen. Kein Live-Gewicht
    // mehr auf dem Display (war nur Debug-Hilfsmittel, siehe ROADMAP.md
    // Punkt 1) - fuer den seltenen Fall, dass der Rohwert doch mal
    // gebraucht wird, bleibt er ueber Serial (main.cpp) einsehbar.
    void update(bool hx711Connected, bool bleConnected);

    // Von main.cpp bei Taste 1 kurz (Geraete-Spielauswahl) aufgerufen -
    // schaltet zur naechsten Spiel-Option durch bzw. oeffnet die Auswahl
    // erneut, falls gerade GameConfirmed angezeigt wird. Ohne Wirkung,
    // waehrend ein RemoteCue/Spieler-Zug laeuft (Taste 1 waere sonst
    // waehrend eines echten Spiels missverstaendlich belegt).
    void pickerNext();

    // Von main.cpp bei Taste 2 kurz aufgerufen - bestaetigt die aktuell
    // hervorgehobene Auswahl (zeigt "X ausgewaehlt - jetzt in der App
    // oeffnen"). Kein automatisches Umschalten der App (siehe ROADMAP.md
    // Punkt 1, "App-Sync" bewusst fuers Erste zurueckgestellt) - reine
    // lokale Anzeige, die Bestaetigung in der App passiert weiter von Hand.
    void pickerConfirm();

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
    // ein kleines Namens-/Farb-Badge (+ Spiel-Icon) auf dem Warteschirm
    // (renderWaitingForTurnScreen()), solange ein Spieler am Zug ist - siehe
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

    // Geraete-Spielauswahl (siehe pickerNext()/pickerConfirm()) - Index in
    // PICKER_GAMES (TftDisplay.cpp), Reihenfolge/Namen bewusst 1:1 synchron
    // zu GAME_REGISTRY in gameRegistry.ts (App-Repo) gepflegt statt per BLE
    // uebertragen (siehe ROADMAP.md Punkt 1, "App-Sync" zurueckgestellt).
    LocalScreen localScreen_ = LocalScreen::GamePicker;
    uint8_t pickerIndex_ = 0;

    void renderGamePickerScreen(bool hx711Connected, bool bleConnected);
    void renderGameConfirmedScreen(bool hx711Connected, bool bleConnected);
    void renderWaitingForTurnScreen(bool hx711Connected, bool bleConnected);
    void renderIdleFooter(bool hx711Connected, bool bleConnected);
    void renderRemoteCueScreen(RemoteCue cue);
    void renderPlayerBadge(int16_t x, int16_t y);
    void renderGameIcon(GameKind game, int16_t cx, int16_t cy, int16_t size, uint16_t color);

    void renderAwayGolf();
    void renderAwayDart();
    void renderAwayBlackjack();
    void renderAwayTower();
};
