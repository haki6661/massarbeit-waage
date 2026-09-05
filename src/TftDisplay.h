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

// LocalScreen/RemoteCue/GameKind sind nicht TFT-spezifisch, sondern die
// gemeinsame Zustandssprache beider Geraetevarianten (die Basis bildet sie
// auf LED-Muster ab) - deshalb ausgelagert.
#include "DeviceUiTypes.h"

class LedRing;

class TftDisplay {
public:
    void begin();

    // Optionaler WS2812B-Lichtring im Deckel (siehe LedRing.h) - laeuft
    // parallel zum TFT und bekommt von hier dieselben Cue-/Spielerwechsel
    // weitergereicht. Ohne Aufruf (oder mit nullptr) aendert sich am
    // Display-Verhalten nichts.
    void attachLedRing(LedRing* ring) { ledRing_ = ring; }

    // In loop() aufrufen. Zeichnet intern gedrosselt (~alle 150ms) neu, um
    // nicht bei jedem Loop-Durchlauf den ganzen Bildschirm neu zu fuellen.
    // Zeigt je nach Zustand: RemoteCue (laueft ein Spiel-Ritual) > Spieler-
    // Warteschirm (Spieler am Zug, aber gerade kein Cue) > Geraete-
    // Spielauswahl (Ruhezustand) - siehe LocalScreen. Kein Live-Gewicht
    // mehr auf dem Display (war nur Debug-Hilfsmittel) - fuer den seltenen
    // Fall, dass der Rohwert doch mal gebraucht wird, bleibt er ueber
    // Serial (main.cpp) einsehbar.
    void update(bool hx711Connected, bool bleConnected);

    // Von main.cpp bei Taste 1 kurz (Geraete-Spielauswahl) aufgerufen -
    // schaltet zur naechsten Spiel-Option durch bzw. oeffnet die Auswahl
    // erneut, falls gerade GameConfirmed angezeigt wird. Ohne Wirkung,
    // waehrend ein RemoteCue/Spieler-Zug laeuft (Taste 1 waere sonst
    // waehrend eines echten Spiels missverstaendlich belegt).
    void pickerNext();

    // Von main.cpp bei Taste 2 kurz aufgerufen - bestaetigt die aktuell
    // hervorgehobene Auswahl (zeigt "X ausgewaehlt - jetzt in der App
    // oeffnen"). Kein automatisches Umschalten der App (siehe ROADMAP.md,
    // "App-Sync: Geraete-Spielauswahl schaltet die App mit um") - reine
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

    // Gemeinsamer Einstiegspunkt beider Varianten fuer die Startsequenz -
    // main.cpp ruft nur noch diesen, egal ob TFT oder Status-LED dahinter
    // steht. Auf der Vision ist das genau die Sprite-Animation; der
    // Name playBootSprite() bleibt bewusst bestehen, weil die komplette
    // Sprite-Doku (data/README.md, BOOT_SPRITE_ASSETS.md) darauf verweist.
    void runBootSequence(bool (*stepInit)()) { playBootSprite(stepInit); }

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

    // Gegenstueck zu LedStatusUi::prepareForSleep(). Auf der Vision
    // nichts zu tun: main.cpp schaltet direkt danach POWER_ON ab, was Display
    // und Hintergrundbeleuchtung ohnehin komplett stromlos macht.
    void prepareForSleep() {}

    // Wandelt 8-Bit-RGB (wie von der App per BLE geschickt, siehe
    // COMMAND_PLAYER_TURN in Config.h) in den RGB565-Wert um, den Arduino_GFX
    // erwartet - eigene Methode, weil `gfx_` (und damit color565()) privat ist.
    uint16_t color565FromRgb(uint8_t r, uint8_t g, uint8_t b) const { return gfx_->color565(r, g, b); }

private:
    LedRing* ledRing_ = nullptr;

    Arduino_DataBus* bus_ = nullptr;
    Arduino_GFX* gfx_ = nullptr;

    // Akzentfarben 1:1 aus dem neuen App-Design uebernommen (src/styles/
    // globals.css im App-Repo, an Accios UI-Manifest angelehnt: dunkler
    // Grund, ein Akzent "Sonnenuntergang", Status-Farben success/danger/
    // warning) - einmal in begin() aus den exakt gleichen Hex-Werten
    // berechnet statt in jeder Render-Funktion neu, damit Handy-App und
    // Waagen-Display exakt dieselbe Farbsprache tragen. Ersetzt die
    // frueheren Café-Kreide-Namen (ember/zest/cream/coral).
    uint16_t accentColor_ = 0;   // --color-accent (#f28a4a)
    uint16_t successColor_ = 0;  // --color-success (#66bb6a)
    uint16_t dangerColor_ = 0;   // --color-danger (#ef5350)
    uint16_t warningColor_ = 0;  // --color-warning (#ffa726)
    uint16_t textColor_ = 0;     // --color-text (#f2f4f7)
    uint16_t mutedColor_ = 0;    // --color-text-muted (#9aa1ad)

    uint32_t lastRenderMs_ = 0;
    bool forceRedraw_ = true;
    bool lastBleConnected_ = false;
    bool lastHx711Connected_ = true;

    RemoteCue remoteCue_ = RemoteCue::None;
    GameKind remoteCueGame_ = GameKind::None; // nur fuer RemoteCue::Away relevant
    uint32_t remoteCueSetMs_ = 0;

    // Flacker-Fix fuer renderRemoteCueScreen()/renderAway*(): diese Screens
    // laufen als Endlosanimation (Puls-Glow bzw. bewegtes Motiv) und werden
    // alle ~150ms neu gezeichnet, solange der Cue aktiv ist. Ein volles
    // fillScreen(BLACK) + Neuzeichnen bei JEDEM dieser Frames verursacht ein
    // sichtbares Schwarz-Aufblitzen (das gemeldete "Flackern") - deshalb
    // werden unveraenderliche Teile (Titel/Badge-Fuellung bzw. Fairway/
    // Dartscheibe/Kartenstapel/Restturm/Sack) nur EINMAL beim Betreten des
    // Cues gezeichnet, und pro Frame nur noch die tatsaechlich bewegte
    // Flaeche an ihrer alten Position schwarz uebermalt ("Dirty Rect") statt
    // den ganzen Bildschirm zu leeren.
    RemoteCue lastCueRendered_ = RemoteCue::None;
    int16_t lastGlowR_ = -1;
    GameKind awayStaticGame_ = GameKind::None; // welches Spiel die statischen Away-Elemente zuletzt zeigte
    int16_t prevGolfBallX_ = -1000, prevGolfBallY_ = -1000;
    int16_t prevDartMinX_ = -1000, prevDartMinY_ = 0, prevDartMaxX_ = 0, prevDartMaxY_ = 0;
    int16_t prevCardLeft_ = -1000, prevCardW_ = 0;
    int16_t prevTowerX_ = -1000, prevTowerY_ = 0;
    int16_t prevGloveX_ = -1000, prevGloveY_ = 0;
    int16_t prevBagX_ = -1000;

    bool hasActivePlayer_ = false;
    GameKind activeGame_ = GameKind::None;
    uint16_t activePlayerColor565_ = 0;
    String activePlayerName_;

    // Geraete-Spielauswahl (siehe pickerNext()/pickerConfirm()) - Index in
    // PICKER_GAMES (TftDisplay.cpp), Reihenfolge/Namen bewusst 1:1 synchron
    // zu GAME_REGISTRY in gameRegistry.ts (App-Repo) gepflegt statt per BLE
    // uebertragen (siehe ROADMAP.md, "App-Sync: Geraete-Spielauswahl
    // schaltet die App mit um").
    LocalScreen localScreen_ = LocalScreen::GamePicker;
    uint8_t pickerIndex_ = 0;

    void renderGamePickerScreen(bool hx711Connected, bool bleConnected);
    void renderGameConfirmedScreen(bool hx711Connected, bool bleConnected);
    void renderWaitingForTurnScreen(bool hx711Connected, bool bleConnected);
    void renderIdleFooter(bool hx711Connected, bool bleConnected);
    void renderRemoteCueScreen(RemoteCue cue);
    void renderPlayerBadge(int16_t x, int16_t y);
    void renderGameIcon(GameKind game, int16_t cx, int16_t cy, int16_t size, uint16_t color);

    // Kategoriale Kennfarbe je Spiel - exakt dieselben Hex-Werte wie
    // `game.accent.solid` je GamePlugin im App-Repo (GolfGame.tsx etc.), auf
    // RGB565 umgerechnet. Ersetzt die vorherige Einheitsfarbe (immer Zest)
    // fuer Auswahl-Icon/Bestaetigt-Titel, damit ein Spiel auf Waage UND App
    // an derselben Farbe erkennbar ist.
    uint16_t gameAccentColor(GameKind game) const;

    // Zentriert `text` horizontal, mit der Textoberkante bei `topY` -
    // funktioniert unveraendert fuer den eingebauten Bitmap-Font (Cursor-Y
    // = Textoberkante) UND fuer einen eigenen GFXfont wie FreeSansBold10pt7b
    // (Cursor-Y = Grundlinie, deshalb Korrektur um y1 aus getTextBounds()).
    void printCentered(const char* text, int16_t topY, uint16_t color);

    void renderAwayGolf();
    void renderAwayDart();
    void renderAwayBlackjack();
    void renderAwayTower();
    void renderAwayBoxen();
};
