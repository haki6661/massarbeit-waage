#include "TftDisplay.h"
#include "BoardConfig.h"
#include <math.h>

namespace {
    constexpr uint16_t COLOR_MUTED = 0x7BEF; // grau
}

void TftDisplay::begin() {
    // Peripherie-Power: ohne das bleibt das Display tot (auch am USB-Kabel,
    // siehe offizielles T-Display-S3-README).
    pinMode(Pins::POWER_ON, OUTPUT);
    digitalWrite(Pins::POWER_ON, HIGH);

    pinMode(Pins::LCD_BL, OUTPUT);
    digitalWrite(Pins::LCD_BL, HIGH);

    // 8-Bit-Parallelbus (Intel-8080-Timing) - Konstruktor-Reihenfolge und
    // -Werte 1:1 aus LilyGOs Arduino_GFXDemo uebernommen.
    bus_ = new Arduino_ESP32PAR8Q(
        Pins::LCD_DC, Pins::LCD_CS, Pins::LCD_WR, Pins::LCD_RD,
        Pins::LCD_D0, Pins::LCD_D1, Pins::LCD_D2, Pins::LCD_D3,
        Pins::LCD_D4, Pins::LCD_D5, Pins::LCD_D6, Pins::LCD_D7);

    gfx_ = new Arduino_ST7789(
        bus_, Pins::LCD_RST, /*rotation=*/0, /*IPS=*/true,
        /*width=*/170, /*height=*/320,
        /*col offset1=*/35, /*row offset1=*/0,
        /*col offset2=*/35, /*row offset2=*/0);

    gfx_->begin();
    // Querformat, 320x170. Rotation 3 statt 1 - beide sind Querformat
    // (width/height vertauscht gegenueber Portrait), aber 180 Grad
    // zueinander gedreht (siehe Arduino_GFX::setRotation()) - je nachdem,
    // wie das Board eingebaut/gehalten wird.
    gfx_->setRotation(3);
    gfx_->fillScreen(BLACK);
    gfx_->setTextColor(WHITE);

    emberColor_ = gfx_->color565(0xD9, 0x72, 0x3A);
    zestColor_  = gfx_->color565(0x9F, 0xBF, 0x3F);
    creamColor_ = gfx_->color565(0xFD, 0xF0, 0xD9);
    coralColor_ = gfx_->color565(0xC8, 0x5A, 0x3F);
}

namespace {
    // Waehlt SCHWARZ oder WEISS als Textfarbe auf einem gegebenen RGB565-
    // Hintergrund, je nach wahrgenommener Helligkeit (Luma) - die 8
    // Spieler-Farben (siehe PLAYER_COLOR_PALETTE im App-Repo) reichen von
    // sehr hell (Gold, Lime) bis mittel-dunkel (Coral, Violet), ein fest
    // verdrahtetes WEISS waere auf den hellen Farben kaum lesbar.
    uint16_t contrastTextColor(uint16_t bg565) {
        uint8_t r5 = (bg565 >> 11) & 0x1F;
        uint8_t g6 = (bg565 >> 5) & 0x3F;
        uint8_t b5 = bg565 & 0x1F;
        uint16_t r8 = (r5 * 527 + 23) >> 6;
        uint16_t g8 = (g6 * 259 + 33) >> 6;
        uint16_t b8 = (b5 * 527 + 23) >> 6;
        uint32_t luma = (299u * r8 + 587u * g8 + 114u * b8) / 1000u;
        return luma > 150 ? BLACK : WHITE;
    }
}

void TftDisplay::update(DisplayMode mode, float weight, float rawReading, bool hx711Connected,
                         bool bleConnected, float batteryVoltage) {
    uint32_t now = millis();

    if (remoteCue_ != RemoteCue::None) {
        // Sicherheitsnetz: "Bereit"/"Away" duerfen theoretisch laenger stehen
        // (Spieler braucht Zeit, das Glas aufzustellen bzw. zu trinken),
        // Ergebnis-Anzeigen sollen sich von selbst wieder aufloesen, falls
        // die App das explizite Zuruecksetzen vergisst oder die Verbindung
        // abbricht.
        bool isLongLived = (remoteCue_ == RemoteCue::Ready || remoteCue_ == RemoteCue::Away);
        uint32_t timeoutMs = isLongLived ? 20000 : 4000;
        if (now - remoteCueSetMs_ > timeoutMs) {
            remoteCue_ = RemoteCue::None;
        } else {
            if (!forceRedraw_ && (now - lastRenderMs_ < 150)) return;
            lastRenderMs_ = now;
            forceRedraw_ = false;
            renderRemoteCueScreen(remoteCue_);
            return;
        }
    }

    bool modeChanged = (mode != lastMode_);
    bool bleChanged = (bleConnected != lastBleConnected_);
    lastBleConnected_ = bleConnected;

    if (!modeChanged && !bleChanged && !forceRedraw_ && (now - lastRenderMs_ < 150)) {
        return;
    }

    // Voller Redraw (fillScreen + alle Elemente inkl. Titel/Badge/Fusszeile)
    // nur, wenn sich etwas Strukturelles geaendert hat (Modus-Wechsel,
    // BLE-Verbindungsstatus, oder ein explizites forceRedraw_ z.B. durch
    // setActivePlayer()) - sonst nur die Gewichtszahl selbst neu zeichnen.
    // Grund: bei ~150ms-Refresh und einer (vor der Kalibrierung besonders)
    // stark schwankenden Zahl sah das volle fillScreen()-Blinken bei jedem
    // Tick wie starkes Flackern aus, obwohl Titel/Badge/Fusszeile sich gar
    // nicht aendern.
    bool fullRedraw = modeChanged || bleChanged || forceRedraw_;

    lastRenderMs_ = now;
    lastMode_ = mode;
    forceRedraw_ = false;

    switch (mode) {
        case DisplayMode::Weight:
            renderWeightScreen(weight, hx711Connected, bleConnected, fullRedraw);
            break;
        case DisplayMode::Status:
            renderStatusScreen(rawReading, hx711Connected, bleConnected, batteryVoltage);
            break;
    }
}

void TftDisplay::renderWeightScreen(float weight, bool hx711Connected, bool bleConnected, bool fullRedraw) {
    if (fullRedraw) {
        gfx_->fillScreen(BLACK);

        // Kopfzeile: Spieler-Badge (Name/Farbe/Spiel-Icon), solange jemand
        // am Zug ist (siehe setActivePlayer()), sonst der schlichte Titel.
        if (hasActivePlayer_) {
            renderPlayerBadge(6, 5);
        } else {
            gfx_->setTextColor(WHITE);
            gfx_->setTextSize(2);
            gfx_->setCursor(8, 8);
            gfx_->print("Massarbeit Waage");
        }

        gfx_->setTextColor(COLOR_MUTED);
        gfx_->setTextSize(1);
        gfx_->setCursor(8, 152);
        gfx_->print("Taste1: Tara   Taste2: Status");
    } else {
        // Nur den dynamischen Bereich (Gewichtszahl + BLE-Status) loeschen
        // und neu zeichnen statt den kompletten Screen - deutlich weniger
        // sichtbares Flackern, Titel/Badge/Fusszeile bleiben unberuehrt.
        gfx_->fillRect(4, 58, 300, 44, BLACK);
        gfx_->fillRect(4, 118, 300, 14, BLACK);
    }

    gfx_->setTextSize(4);
    gfx_->setCursor(8, 62);
    if (!hx711Connected) {
        gfx_->setTextColor(coralColor_);
        gfx_->print("HX711 Fehler");
    } else {
        gfx_->setTextColor(zestColor_);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f g", weight);
        gfx_->print(buf);
    }

    // Kleines BLE-Statuslicht statt eigener Textzeile - macht oben Platz
    // fuers Spieler-Badge, ohne dass der Verbindungsstatus ganz verschwindet.
    // Rohwert-Debug ist auf den Status-Screen (Taste 2 kurz) umgezogen.
    uint16_t bleColor = bleConnected ? zestColor_ : COLOR_MUTED;
    gfx_->fillCircle(13, 128, 4, bleColor);
    gfx_->setTextSize(1);
    gfx_->setTextColor(bleColor);
    gfx_->setCursor(24, 124);
    gfx_->print(bleConnected ? "Verbunden" : "Wartet auf Verbindung ...");
}

void TftDisplay::renderStatusScreen(float rawReading, bool hx711Connected, bool bleConnected, float batteryVoltage) {
    gfx_->fillScreen(BLACK);

    gfx_->setTextColor(WHITE);
    gfx_->setTextSize(2);
    gfx_->setCursor(8, 8);
    gfx_->print("Status");

    gfx_->setTextSize(1);
    gfx_->setCursor(8, 45);
    gfx_->setTextColor(hx711Connected ? GREEN : RED);
    gfx_->print(hx711Connected ? "HX711: OK" : "HX711: FEHLER");

    gfx_->setCursor(8, 60);
    gfx_->setTextColor(COLOR_MUTED);
    if (hx711Connected) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Rohwert: %.0f", rawReading);
        gfx_->print(buf);
    } else {
        gfx_->print("Rohwert: -- (DT/SCK/VCC pruefen)");
    }

    gfx_->setCursor(8, 80);
    gfx_->setTextColor(bleConnected ? GREEN : YELLOW);
    gfx_->print(bleConnected ? "BLE: verbunden" : "BLE: wartet auf Verbindung");

    gfx_->setCursor(8, 100);
    gfx_->setTextColor(WHITE);
    if (batteryVoltage > 0.1f) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Akku: %.2f V", batteryVoltage);
        gfx_->print(buf);
    } else {
        gfx_->print("Akku: n/a (USB gesteckt)");
    }

    gfx_->setTextColor(COLOR_MUTED);
    gfx_->setCursor(8, 152);
    gfx_->print("Taste2: zurueck zur Waage");
}

void TftDisplay::playBootAnimation(bool hx711Connected, float batteryVoltage, bool bleStarted) {
    constexpr int16_t pivotX = 70;
    constexpr int16_t pivotY = 55;
    constexpr int16_t beamLen = 40;
    constexpr int16_t standBottomY = 105;

    // Phase 1 (~1,4s): eine Balkenwaage pendelt sich gedaempft ein, statt
    // eines starren Logos - passt zum Produkt (Praezisionswaage) und
    // braucht keine Sprite-/Bitmap-Assets, nur Linien/Kreise wie die
    // "Ball fliegt"-Animation in renderRemoteCueScreen().
    uint32_t phaseStart = millis();
    constexpr uint32_t swingDurationMs = 1400;
    while (millis() - phaseStart <= swingDurationMs) {
        float t = (millis() - phaseStart) / 1000.0f;
        float angle = 0.55f * expf(-2.6f * t) * sinf(2.0f * PI * 2.1f * t);

        gfx_->fillScreen(BLACK);

        gfx_->drawFastVLine(pivotX, pivotY, standBottomY - pivotY, creamColor_);
        gfx_->fillTriangle(pivotX - 14, standBottomY, pivotX + 14, standBottomY, pivotX, standBottomY - 14, creamColor_);

        float dx = cosf(angle) * beamLen;
        float dy = sinf(angle) * beamLen;
        int16_t leftX  = pivotX - (int16_t)dx, leftY  = pivotY - (int16_t)dy;
        int16_t rightX = pivotX + (int16_t)dx, rightY = pivotY + (int16_t)dy;
        gfx_->drawLine(leftX, leftY, rightX, rightY, creamColor_);
        gfx_->fillCircle(pivotX, pivotY, 3, creamColor_);

        gfx_->drawFastVLine(leftX, leftY, 8, COLOR_MUTED);
        gfx_->drawFastVLine(rightX, rightY, 8, COLOR_MUTED);
        gfx_->drawCircle(leftX, leftY + 8, 8, emberColor_);
        gfx_->drawCircle(rightX, rightY + 8, 8, zestColor_);

        gfx_->setTextSize(2);
        gfx_->setTextColor(WHITE);
        gfx_->setCursor(140, 40);
        gfx_->print("Massarbeit");
        gfx_->setCursor(140, 62);
        gfx_->print("Waage");

        delay(20); // ~50fps
    }

    // Phase 2: Boot-Checks (HX711/Akku/BLE) einzeln einblenden statt als
    // fertigen Textblock - jede Zeile bekommt einen kleinen farbigen Chip
    // statt eines reinen Textpraefixes, angelehnt an die "Chunky"-UI-Sprache
    // der Handy-App.
    gfx_->fillScreen(BLACK);
    gfx_->setTextSize(2);
    gfx_->setTextColor(WHITE);
    gfx_->setCursor(8, 8);
    gfx_->print("Massarbeit Waage");

    char battBuf[16];
    if (batteryVoltage > 0.1f) {
        snprintf(battBuf, sizeof(battBuf), "%.2f V", batteryVoltage);
    } else {
        snprintf(battBuf, sizeof(battBuf), "USB");
    }

    struct Check { const char* label; bool ok; const char* value; };
    const Check checks[] = {
        { "HX711", hx711Connected, hx711Connected ? "OK" : "Fehler" },
        { "Akku",  true,           battBuf },
        { "BLE",   bleStarted,     bleStarted ? "gestartet" : "Fehler" },
    };

    int16_t y = 40;
    for (const Check& check : checks) {
        uint16_t color = check.ok ? zestColor_ : RED;
        gfx_->fillRoundRect(8, y, 10, 10, 2, color);
        gfx_->setTextSize(1);
        gfx_->setTextColor(WHITE);
        gfx_->setCursor(24, y + 1);
        gfx_->print(check.label);
        gfx_->setTextColor(color);
        gfx_->setCursor(90, y + 1);
        gfx_->print(check.value);
        y += 18;
        delay(220);
    }

    delay(500);

    // Sofort sichtbar, nicht erst nach dem 150ms-Drossel-Timer des naechsten
    // update()-Aufrufs.
    forceRedraw_ = true;
}

void TftDisplay::setRemoteCue(RemoteCue cue, GameKind game) {
    remoteCue_ = cue;
    remoteCueGame_ = game;
    remoteCueSetMs_ = millis();
    forceRedraw_ = true; // sofort anzeigen, nicht auf den 150ms-Drossel-Timer warten
}

void TftDisplay::renderRemoteCueScreen(RemoteCue cue) {
    gfx_->fillScreen(BLACK);

    if (cue == RemoteCue::Away) {
        // Welches Spiel gerade laeuft, bestimmt die Animation - jedes Spiel
        // hat sein eigenes kleines Warte-Motiv (analog zu den AwayMoment-
        // Components im App-Repo: GolfBallFlight/DartThrow/BlackjackDraw/
        // TowerPull), damit die Waage nicht bei jedem Spiel denselben Ball
        // zeigt. Ohne bekanntes Spiel (None/Scale) faellt es auf die
        // Golf-Ball-Animation zurueck - bewusst nicht ganz leer.
        switch (remoteCueGame_) {
            case GameKind::Dart:
                renderAwayDart();
                break;
            case GameKind::Blackjack:
                renderAwayBlackjack();
                break;
            case GameKind::Tower:
                renderAwayTower();
                break;
            case GameKind::Golf:
            case GameKind::Scale:
            case GameKind::None:
            default:
                renderAwayGolf();
                break;
        }
        return;
    }

    // Farbwelt + kleiner "Burst" je Ergebnis-Guete statt reinem Text -
    // Haken fuer Bereit/Volltreffer, Kreuz fuer Daneben, gefuellter Kreis
    // fuer "nah dran". Rein prozedural (Linien/Kreise), kein Sprite noetig.
    const char* title;
    const char* subtitle;
    uint16_t color;
    bool showCheck = false;
    bool showCross = false;
    bool showRing = false;
    switch (cue) {
        case RemoteCue::Ready:
            title = "BEREIT!";
            subtitle = hasActivePlayer_ ? activePlayerName_.c_str() : "Jetzt trinken";
            color = hasActivePlayer_ ? activePlayerColor565_ : zestColor_;
            showCheck = true;
            break;
        case RemoteCue::ResultPerfect:
            title = "VOLLTREFFER!";
            subtitle = "";
            color = zestColor_;
            showCheck = true;
            break;
        case RemoteCue::ResultClose:
            title = "NAH DRAN";
            subtitle = "";
            color = emberColor_;
            showRing = true;
            break;
        case RemoteCue::ResultMiss:
        default:
            title = "DANEBEN";
            subtitle = "";
            color = coralColor_;
            showCross = true;
            break;
    }

    // Badge mittig oben, Text darunter zentriert (per getTextBounds, wie im
    // Original) - so kollidiert das Badge nie mit unterschiedlich langen
    // Titeln ("DANEBEN" vs. "VOLLTREFFER!"), die Breite ist beim Zentrieren
    // schon beruecksichtigt.
    constexpr int16_t badgeCx = 160;
    constexpr int16_t badgeCy = 42;
    constexpr int16_t badgeR = 26;
    // Sanfter Puls, wie beim BEREIT-Zustand der App (glow-zest) - simpler
    // Sinus auf dem Radius statt einer echten Glow-Textur.
    float pulse = 0.5f + 0.5f * sinf((float)millis() / 220.0f);
    int16_t glowR = badgeR + 3 + (int16_t)(pulse * 3);
    gfx_->drawCircle(badgeCx, badgeCy, glowR, color);
    gfx_->fillCircle(badgeCx, badgeCy, badgeR, color);

    uint16_t markColor = contrastTextColor(color);
    if (showCheck) {
        gfx_->drawLine(badgeCx - 11, badgeCy, badgeCx - 3, badgeCy + 9, markColor);
        gfx_->drawLine(badgeCx - 11, badgeCy + 1, badgeCx - 3, badgeCy + 10, markColor);
        gfx_->drawLine(badgeCx - 3, badgeCy + 9, badgeCx + 13, badgeCy - 10, markColor);
        gfx_->drawLine(badgeCx - 3, badgeCy + 10, badgeCx + 13, badgeCy - 9, markColor);
    } else if (showCross) {
        gfx_->drawLine(badgeCx - 10, badgeCy - 10, badgeCx + 10, badgeCy + 10, markColor);
        gfx_->drawLine(badgeCx - 10, badgeCy - 9, badgeCx + 10, badgeCy + 11, markColor);
        gfx_->drawLine(badgeCx - 10, badgeCy + 10, badgeCx + 10, badgeCy - 10, markColor);
        gfx_->drawLine(badgeCx - 10, badgeCy + 11, badgeCx + 10, badgeCy - 9, markColor);
    } else if (showRing) {
        gfx_->drawCircle(badgeCx, badgeCy, 11, markColor);
        gfx_->drawCircle(badgeCx, badgeCy, 10, markColor);
    }

    int16_t x1, y1;
    uint16_t w, h;
    gfx_->setTextSize(3);
    gfx_->setTextColor(color);
    gfx_->getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    gfx_->setCursor((320 - w) / 2, 88);
    gfx_->print(title);

    if (subtitle[0] != '\0') {
        gfx_->setTextSize(2);
        gfx_->setTextColor(WHITE);
        gfx_->getTextBounds(subtitle, 0, 0, &x1, &y1, &w, &h);
        gfx_->setCursor((320 - w) / 2, 122);
        gfx_->print(subtitle);
    }
}

void TftDisplay::setActivePlayer(GameKind game, uint16_t color565, const String& name) {
    hasActivePlayer_ = true;
    activeGame_ = game;
    activePlayerColor565_ = color565;
    // Auf ca. 10 Zeichen kuerzen - mehr passt bei Textsize 2 neben dem
    // Farb-Badge nicht mehr lesbar hin (siehe renderPlayerBadge()).
    activePlayerName_ = name.length() > 10 ? name.substring(0, 10) : name;
    forceRedraw_ = true;
}

void TftDisplay::clearActivePlayer() {
    hasActivePlayer_ = false;
    forceRedraw_ = true;
}

void TftDisplay::renderPlayerBadge(int16_t x, int16_t y) {
    constexpr int16_t r = 10;
    int16_t ccx = x + r + 1;
    int16_t ccy = y + r + 1;

    gfx_->fillCircle(ccx, ccy, r, activePlayerColor565_);
    gfx_->drawCircle(ccx, ccy, r, creamColor_);

    char c = activePlayerName_.length() > 0 ? activePlayerName_[0] : '?';
    if (c >= 'a' && c <= 'z') c -= 32; // Grossbuchstabe, ohne <ctype.h>
    char initial[2] = { c, '\0' };

    gfx_->setTextSize(1);
    gfx_->setTextColor(contrastTextColor(activePlayerColor565_));
    int16_t tx1, ty1;
    uint16_t tw, th;
    gfx_->getTextBounds(initial, 0, 0, &tx1, &ty1, &tw, &th);
    gfx_->setCursor(ccx - tw / 2, ccy - th / 2 - 1);
    gfx_->print(initial);

    int16_t textX = x + 2 * r + 12;
    gfx_->setTextSize(2);
    gfx_->setTextColor(WHITE);
    gfx_->setCursor(textX, y);
    gfx_->print(activePlayerName_);

    gfx_->setTextSize(1);
    gfx_->setTextColor(COLOR_MUTED);
    gfx_->setCursor(textX, y + 17);
    gfx_->print("ist dran");

    renderGameIcon(activeGame_, 300, ccy, 16, activePlayerColor565_);
}

// Kleine, abstrakte Icons statt Sprites - je Spiel ein simples, sofort
// erkennbares Motiv (Fahne/Scheibe/Karte/Bloecke), passend zur jeweiligen
// Away-Animation (renderAway*()) und im gleichen "nur Formen"-Stil wie der
// Rest der Firmware-UI.
void TftDisplay::renderGameIcon(GameKind game, int16_t cx, int16_t cy, int16_t size, uint16_t color) {
    int16_t half = size / 2;
    switch (game) {
        case GameKind::Golf:
            gfx_->drawFastVLine(cx - half / 2, cy - half, size, color);
            gfx_->fillTriangle(cx - half / 2, cy - half, cx - half / 2, cy - half / 3, cx + half / 2, cy - half * 2 / 3, color);
            gfx_->drawFastHLine(cx - half, cy + half, size, color);
            break;
        case GameKind::Dart:
            gfx_->drawCircle(cx, cy, half, color);
            gfx_->drawCircle(cx, cy, (half * 2) / 3, color);
            gfx_->fillCircle(cx, cy, half / 3, color);
            break;
        case GameKind::Blackjack:
            gfx_->drawRoundRect(cx - half / 2, cy - half, half + 2, size, 2, color);
            gfx_->fillCircle(cx, cy, 2, color);
            break;
        case GameKind::Tower: {
            int16_t blockH = size / 3;
            for (int8_t i = 0; i < 3; i++) {
                int16_t offset = (i % 2 == 0) ? -2 : 2;
                gfx_->fillRect(cx - half + offset, cy + half - (i + 1) * blockH, size - 3, blockH - 1, color);
            }
            break;
        }
        case GameKind::Scale:
        case GameKind::None:
        default:
            gfx_->drawFastHLine(cx - half, cy, size, color);
            gfx_->fillTriangle(cx, cy, cx - half / 2, cy + half, cx + half / 2, cy + half, color);
            break;
    }
}

// "Ball fliegt Richtung Loch" - analog zu GolfBallFlight.tsx im App-Repo:
// Fairway-Linie + Fahne rechts, Ball fliegt in Endlosschleife eine Parabel.
void TftDisplay::renderAwayGolf() {
    constexpr int16_t groundY = 130;
    gfx_->drawFastHLine(20, groundY, 280, COLOR_MUTED);
    gfx_->drawFastVLine(280, groundY - 40, 40, creamColor_);
    gfx_->fillTriangle(280, groundY - 40, 280, groundY - 28, 300, groundY - 34, zestColor_);

    float t = fmodf((float)millis(), 1400.0f) / 1400.0f; // 0..1, Endlosschleife
    float x = 30.0f + t * 240.0f;
    float y = groundY - sinf(t * PI) * 70.0f; // Parabelbogen
    gfx_->fillCircle((int16_t)x, (int16_t)y, 6, creamColor_);

    gfx_->setTextSize(2);
    gfx_->setTextColor(COLOR_MUTED);
    int16_t x1, y1;
    uint16_t w, h;
    const char* label = "Ball fliegt ...";
    gfx_->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
    gfx_->setCursor((320 - w) / 2, 20);
    gfx_->print(label);
}

// "Pfeil fliegt zur Scheibe" - analog zu DartThrow.tsx: Dartscheibe rechts
// (konzentrische Ringe + Bullseye), Pfeil fliegt in flacher Kurve heran und
// richtet sich dabei auf die Scheibe aus (Linie + Pfeilspitze, kein echtes
// Sprite noetig - die Ausrichtung ergibt sich aus der Blickrichtung).
void TftDisplay::renderAwayDart() {
    constexpr int16_t boardCx = 270;
    constexpr int16_t boardCy = 85;
    gfx_->drawCircle(boardCx, boardCy, 42, COLOR_MUTED);
    gfx_->drawCircle(boardCx, boardCy, 30, coralColor_);
    gfx_->drawCircle(boardCx, boardCy, 16, emberColor_);
    gfx_->fillCircle(boardCx, boardCy, 6, zestColor_);

    float t = fmodf((float)millis(), 1100.0f) / 1100.0f; // 0..1, Endlosschleife
    float startX = 30.0f, startY = 100.0f;
    float x = startX + t * (boardCx - 14 - startX);
    float y = startY + sinf(t * PI) * -18.0f - t * (startY - boardCy); // leichte Kuruve, landet auf Bullseye-Hoehe

    // Flugrichtung aus der Ableitung der Bahn schaetzen (kleine Zeitdifferenz),
    // damit die Pfeilspitze sich sichtbar Richtung Scheibe ausrichtet.
    float t2 = fminf(1.0f, t + 0.02f);
    float x2 = startX + t2 * (boardCx - 14 - startX);
    float y2 = startY + sinf(t2 * PI) * -18.0f - t2 * (startY - boardCy);
    float angle = atan2f(y2 - y, x2 - x);
    float tipX = x + cosf(angle) * 10.0f, tipY = y + sinf(angle) * 10.0f;
    float tailX = x - cosf(angle) * 10.0f, tailY = y - sinf(angle) * 10.0f;

    gfx_->drawLine((int16_t)tailX, (int16_t)tailY, (int16_t)tipX, (int16_t)tipY, creamColor_);
    float backAngle1 = angle + 2.6f, backAngle2 = angle - 2.6f;
    gfx_->drawLine((int16_t)tipX, (int16_t)tipY, (int16_t)(tipX + cosf(backAngle1) * 6.0f), (int16_t)(tipY + sinf(backAngle1) * 6.0f), creamColor_);
    gfx_->drawLine((int16_t)tipX, (int16_t)tipY, (int16_t)(tipX + cosf(backAngle2) * 6.0f), (int16_t)(tipY + sinf(backAngle2) * 6.0f), creamColor_);

    gfx_->setTextSize(2);
    gfx_->setTextColor(COLOR_MUTED);
    int16_t x1, y1;
    uint16_t w, h;
    const char* label = "Pfeil fliegt ...";
    gfx_->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
    gfx_->setCursor((320 - w) / 2, 20);
    gfx_->print(label);
}

// "Karte wird gezogen" - analog zu BlackjackDraw.tsx: Stapel links (still),
// eine Karte fliegt zur Mitte und "flippt" dabei um (hier simuliert durch
// horizontales Stauchen auf eine duenne Linie in der Flugmitte, statt
// echter 3D-Rotation - danach zeigt die Karte ihre "Vorderseite" mit Pik-
// Symbol statt der leeren Rueckseite).
void TftDisplay::renderAwayBlackjack() {
    constexpr int16_t cardW = 40, cardH = 56;
    constexpr int16_t stackX = 40, cardY = 57;
    gfx_->drawRoundRect(stackX, cardY, cardW, cardH, 6, creamColor_);
    gfx_->drawRoundRect(stackX + 3, cardY - 3, cardW, cardH, 6, COLOR_MUTED);

    float t = fmodf((float)millis(), 1300.0f) / 1300.0f; // 0..1, Endlosschleife
    constexpr int16_t targetX = 160 - cardW / 2;
    float x = stackX + t * (targetX - stackX);
    // Breite pulst zu 0 in der Flugmitte (Flip-Illusion) und wieder zurueck.
    float flip = fabsf(sinf(t * PI));
    int16_t w = (int16_t)(cardW * (0.15f + 0.85f * flip));
    bool showFront = t > 0.5f;

    int16_t cx = (int16_t)x + cardW / 2;
    int16_t left = cx - w / 2;
    if (showFront) {
        gfx_->fillRoundRect(left, cardY, w, cardH, 5, creamColor_);
        if (w > 14) {
            gfx_->fillTriangle(cx, cardY + 10, cx - 8, cardY + 30, cx + 8, cardY + 30, coralColor_);
            gfx_->fillRect(cx - 2, cardY + 28, 4, 14, coralColor_);
        }
    } else {
        gfx_->fillRoundRect(left, cardY, w, cardH, 5, emberColor_);
    }
    gfx_->drawRoundRect(left, cardY, w, cardH, 5, creamColor_);

    gfx_->setTextSize(2);
    gfx_->setTextColor(COLOR_MUTED);
    int16_t x1, y1;
    uint16_t th;
    uint16_t tw;
    const char* label = "Karte wird gezogen ...";
    gfx_->getTextBounds(label, 0, 0, &x1, &y1, &tw, &th);
    gfx_->setCursor((320 - tw) / 2, 20);
    gfx_->print(label);
}

// "Block wird gezogen" - analog zu TowerPull.tsx: Rest-Turm steht mittig,
// ein Block wird seitlich herausgezogen, wandert dann nach oben auf den
// Stapel. Endlosschleife mit vier Phasen (raus, Pause, hoch, Pause) wie im
// App-Original (times: [0, 0.35, 0.65, 1]).
void TftDisplay::renderAwayTower() {
    constexpr int16_t towerCx = 160, blockW = 70, blockH = 14, baseY = 140;
    for (int8_t i = 0; i < 4; i++) {
        gfx_->drawRoundRect(towerCx - blockW / 2, baseY - (i + 1) * (blockH + 2), blockW, blockH, 3, creamColor_);
    }

    float t = fmodf((float)millis(), 1600.0f) / 1600.0f; // 0..1, Endlosschleife
    int16_t pulledX, blockY;
    if (t < 0.35f) {
        float p = t / 0.35f;
        pulledX = towerCx - blockW / 2 - (int16_t)(p * 90.0f);
        blockY = baseY - 2 * (blockH + 2);
    } else if (t < 0.65f) {
        pulledX = towerCx - blockW / 2 - 90;
        blockY = baseY - 2 * (blockH + 2);
    } else {
        float p = (t - 0.65f) / 0.35f;
        pulledX = (int16_t)((towerCx - blockW / 2 - 90) + p * 90.0f);
        blockY = (baseY - 2 * (blockH + 2)) - (int16_t)(p * 4.0f * (blockH + 2));
    }
    gfx_->fillRoundRect(pulledX, blockY, blockW, blockH, 3, zestColor_);
    gfx_->drawRoundRect(pulledX, blockY, blockW, blockH, 3, creamColor_);

    gfx_->setTextSize(2);
    gfx_->setTextColor(COLOR_MUTED);
    int16_t x1, y1;
    uint16_t w, h;
    const char* label = "Block wird gezogen ...";
    gfx_->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
    gfx_->setCursor((320 - w) / 2, 20);
    gfx_->print(label);
}

void TftDisplay::showMessage(const String& title, const String& body) {
    gfx_->fillScreen(BLACK);

    gfx_->setTextColor(WHITE);
    gfx_->setTextSize(2);
    gfx_->setCursor(8, 8);
    gfx_->print(title);

    gfx_->setTextSize(1);
    gfx_->setCursor(8, 45);
    gfx_->print(body);

    // Naechster update()-Aufruf soll sofort neu zeichnen statt auf den
    // Drossel-Timer zu warten.
    forceRedraw_ = true;
}
