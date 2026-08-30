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
    gfx_->setRotation(1); // Querformat, 320x170
    gfx_->fillScreen(BLACK);
    gfx_->setTextColor(WHITE);
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

    if (!modeChanged && !forceRedraw_ && (now - lastRenderMs_ < 150)) {
        return;
    }

    lastRenderMs_ = now;
    lastMode_ = mode;
    forceRedraw_ = false;

    switch (mode) {
        case DisplayMode::Weight:
            renderWeightScreen(weight, rawReading, hx711Connected, bleConnected);
            break;
        case DisplayMode::Status:
            renderStatusScreen(hx711Connected, bleConnected, batteryVoltage);
            break;
    }
}

void TftDisplay::renderWeightScreen(float weight, float rawReading, bool hx711Connected, bool bleConnected) {
    gfx_->fillScreen(BLACK);

    gfx_->setTextColor(WHITE);
    gfx_->setTextSize(2);
    gfx_->setCursor(8, 8);
    gfx_->print("Massarbeit Waage");

    gfx_->setTextSize(4);
    gfx_->setCursor(8, 55);
    if (!hx711Connected) {
        gfx_->setTextColor(RED);
        gfx_->print("HX711 Fehler");
    } else {
        gfx_->setTextColor(GREEN);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f g", weight);
        gfx_->print(buf);
    }

    // Debug-Platzhalter: unkalibrierter Rohwert, damit man ohne Kalibrierung
    // schon sieht, dass sich beim Draufstellen ueberhaupt etwas aendert.
    // TODO entfernen bzw. ersetzen, sobald der Screen final gestaltet ist.
    gfx_->setTextSize(1);
    gfx_->setTextColor(COLOR_MUTED);
    gfx_->setCursor(8, 95);
    if (hx711Connected) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Rohwert: %.0f", rawReading);
        gfx_->print(buf);
    } else {
        gfx_->print("Rohwert: -- (DT/SCK/VCC pruefen)");
    }

    gfx_->setTextSize(2);
    gfx_->setTextColor(bleConnected ? GREEN : COLOR_MUTED);
    gfx_->setCursor(8, 115);
    gfx_->print(bleConnected ? "BLE: verbunden" : "BLE: wartet...");

    gfx_->setTextSize(1);
    gfx_->setTextColor(COLOR_MUTED);
    gfx_->setCursor(8, 152);
    gfx_->print("Taste1: Tara   Taste2: Status");
}

void TftDisplay::renderStatusScreen(bool hx711Connected, bool bleConnected, float batteryVoltage) {
    gfx_->fillScreen(BLACK);

    gfx_->setTextColor(WHITE);
    gfx_->setTextSize(2);
    gfx_->setCursor(8, 8);
    gfx_->print("Status");

    gfx_->setTextSize(1);
    gfx_->setCursor(8, 45);
    gfx_->setTextColor(hx711Connected ? GREEN : RED);
    gfx_->print(hx711Connected ? "HX711: OK" : "HX711: FEHLER");

    gfx_->setCursor(8, 65);
    gfx_->setTextColor(bleConnected ? GREEN : YELLOW);
    gfx_->print(bleConnected ? "BLE: verbunden" : "BLE: wartet auf Verbindung");

    gfx_->setCursor(8, 85);
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
    // Café-Kreide-Akzentfarben der Handy-App (src/styles/globals.css im
    // App-Repo) angenaehert auf RGB565 - kein exakter Treffer moeglich
    // (16-Bit-Farbtiefe), aber der Wiedererkennungswert bleibt.
    const uint16_t emberColor = gfx_->color565(0xD9, 0x72, 0x3A);
    const uint16_t zestColor  = gfx_->color565(0x9F, 0xBF, 0x3F);
    const uint16_t creamColor = gfx_->color565(0xFD, 0xF0, 0xD9);

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

        gfx_->drawFastVLine(pivotX, pivotY, standBottomY - pivotY, creamColor);
        gfx_->fillTriangle(pivotX - 14, standBottomY, pivotX + 14, standBottomY, pivotX, standBottomY - 14, creamColor);

        float dx = cosf(angle) * beamLen;
        float dy = sinf(angle) * beamLen;
        int16_t leftX  = pivotX - (int16_t)dx, leftY  = pivotY - (int16_t)dy;
        int16_t rightX = pivotX + (int16_t)dx, rightY = pivotY + (int16_t)dy;
        gfx_->drawLine(leftX, leftY, rightX, rightY, creamColor);
        gfx_->fillCircle(pivotX, pivotY, 3, creamColor);

        gfx_->drawFastVLine(leftX, leftY, 8, COLOR_MUTED);
        gfx_->drawFastVLine(rightX, rightY, 8, COLOR_MUTED);
        gfx_->drawCircle(leftX, leftY + 8, 8, emberColor);
        gfx_->drawCircle(rightX, rightY + 8, 8, zestColor);

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
        uint16_t color = check.ok ? zestColor : RED;
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

void TftDisplay::setRemoteCue(RemoteCue cue) {
    remoteCue_ = cue;
    remoteCueSetMs_ = millis();
    forceRedraw_ = true; // sofort anzeigen, nicht auf den 150ms-Drossel-Timer warten
}

void TftDisplay::renderRemoteCueScreen(RemoteCue cue) {
    gfx_->fillScreen(BLACK);

    if (cue == RemoteCue::Away) {
        // "Ball fliegt durch die Luft" - Endlosschleife, waehrend das Glas
        // von der Waage gehoben ist und getrunken wird (keine Messung
        // moeglich, reine Warte-Animation). Rein prozedural (Kreis + Linien)
        // statt Sprite/Bitmap - Grundgeruest, siehe Projekt-Chat fuer den
        // Plan, spaeter durch echte Pixel-Art-Frames zu ersetzen.
        constexpr int16_t groundY = 130;
        gfx_->drawFastHLine(20, groundY, 280, COLOR_MUTED);
        gfx_->drawFastVLine(280, groundY - 40, 40, WHITE);
        gfx_->fillTriangle(280, groundY - 40, 280, groundY - 28, 300, groundY - 34, GREEN);

        float t = fmodf((float)millis(), 1400.0f) / 1400.0f; // 0..1, Endlosschleife
        float x = 30.0f + t * 240.0f;
        float y = groundY - sinf(t * PI) * 70.0f; // Parabelbogen
        gfx_->fillCircle((int16_t)x, (int16_t)y, 6, WHITE);

        gfx_->setTextSize(2);
        gfx_->setTextColor(COLOR_MUTED);
        int16_t x1, y1;
        uint16_t w, h;
        const char* label = "...";
        gfx_->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
        gfx_->setCursor((320 - w) / 2, 148);
        gfx_->print(label);
        return;
    }

    // Grundgeruest-Darstellung fuer die uebrigen Zustaende: nur Text + Farbe.
    // Spaeter laut Vorgabe durch echte Animationen ersetzen (unterschiedliche
    // Animation je Ergebnis-Guete statt nur Text/Farbe hier).
    const char* title;
    const char* subtitle;
    uint16_t color;
    switch (cue) {
        case RemoteCue::Ready:
            title = "BEREIT!";
            subtitle = "Jetzt trinken";
            color = GREEN;
            break;
        case RemoteCue::ResultPerfect:
            title = "VOLLTREFFER!";
            subtitle = "";
            color = GREEN;
            break;
        case RemoteCue::ResultClose:
            title = "NAH DRAN";
            subtitle = "";
            color = YELLOW;
            break;
        case RemoteCue::ResultMiss:
        default:
            title = "DANEBEN";
            subtitle = "";
            color = RED;
            break;
    }

    gfx_->setTextSize(3);
    gfx_->setTextColor(color);
    int16_t x1, y1;
    uint16_t w, h;
    gfx_->getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    gfx_->setCursor((320 - w) / 2, 60);
    gfx_->print(title);

    if (subtitle[0] != '\0') {
        gfx_->setTextSize(2);
        gfx_->setTextColor(WHITE);
        gfx_->getTextBounds(subtitle, 0, 0, &x1, &y1, &w, &h);
        gfx_->setCursor((320 - w) / 2, 100);
        gfx_->print(subtitle);
    }
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
