#include "TftDisplay.h"
#include "BoardConfig.h"
#include "FreeSansBold10pt7b.h"
#include <SPIFFS.h>
#include <math.h>

namespace {
    // Geraete-Spielauswahl (siehe TftDisplay::pickerNext()) - Reihenfolge +
    // Namen bewusst 1:1 zu GAME_REGISTRY in gameRegistry.ts (App-Repo)
    // gepflegt. Manuell synchron halten statt per BLE zu uebertragen - haelt
    // den ersten Wurf einfach, auf Kosten davon, dass ein neues Spiel im
    // App-Repo hier von Hand nachgezogen werden muss (siehe ROADMAP.md,
    // "App-Sync: Geraete-Spielauswahl schaltet die App mit um").
    struct PickerGame {
        GameKind kind;
        const char* name;
    };
    constexpr PickerGame PICKER_GAMES[] = {
        { GameKind::Golf, "Golf" },
        { GameKind::Dart, "Dart" },
        { GameKind::Blackjack, "Blackjack" },
        { GameKind::Tower, "Wackelturm" },
        { GameKind::Boxen, "Boxen" },
        { GameKind::Scale, "Scale" },
    };
    constexpr uint8_t PICKER_GAME_COUNT = sizeof(PICKER_GAMES) / sizeof(PICKER_GAMES[0]);

    // Feste Anzeigedauer je Sprite-Frame (siehe playBootSprite()) - macht die
    // Abspielgeschwindigkeit unabhaengig davon, wie lange SPIFFS-Lesen +
    // Zeichnen + stepInit() in diesem konkreten Frame gebraucht haben (das
    // schwankt spuerbar, u.a. je nachdem ob gerade ein init-Schritt wie
    // bleService.begin() im selben Frame mitlief). 100ms = die Framerate des
    // Quell-GIFs (10fps, siehe BOOT_SPRITE_ASSETS.md) - jeder gezeigte Frame
    // ersetzt ja 3 Original-Frames (STEP=3), laeuft aber weiter in Original-
    // Geschwindigkeit statt gerafft. Gilt bewusst als eigene, benannte
    // Konstante (nicht direkt in playBootSprite() vergraben): weitere
    // Sprite-Animationen sind denkbar (spaeter ggf. von SD statt SPIFFS
    // geladen) - eine davon abweichende Zieldauer laesst sich dann
    // pro Animation einfach als eigene Konstante danebenstellen, das
    // Pacing-Muster selbst (Frame-Start merken, Restzeit am Ende abwarten)
    // bleibt 1:1 wiederverwendbar.
    constexpr uint16_t SPRITE_FRAME_DURATION_MS = 100;
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

    // Exakt dieselben Hex-Werte wie die neuen App-Design-Tokens (siehe
    // src/styles/globals.css im App-Repo, an Accios UI-Manifest angelehnt) -
    // einmal hier auf RGB565 umgerechnet statt in jeder Render-Funktion neu.
    // Ersetzt die vorherige Café-Kreide-Palette (ember/zest/cream/coral).
    accentColor_  = gfx_->color565(0xF2, 0x8A, 0x4A); // --color-accent
    successColor_ = gfx_->color565(0x66, 0xBB, 0x6A); // --color-success
    dangerColor_  = gfx_->color565(0xEF, 0x53, 0x50); // --color-danger
    warningColor_ = gfx_->color565(0xFF, 0xA7, 0x26); // --color-warning
    textColor_    = gfx_->color565(0xF2, 0xF4, 0xF7); // --color-text
    mutedColor_   = gfx_->color565(0x9A, 0xA1, 0xAD); // --color-text-muted

    gfx_->setTextColor(textColor_);
    // Kraeftige, geometrische Bold-Schrift statt des klassischen 5x7-Bitmap-
    // Fonts fuer die grossen, oft gelesenen Texte (Spielname, Cue-Titel wie
    // "BEREIT!") - naeher an der Outfit-Schriftfamilie der App als der
    // vorherige technische Pixel-Font. Kleine Hinweistexte/Labels bleiben
    // bewusst beim eingebauten Font (siehe printCentered()/direkte
    // setCursor()-Aufrufe ohne vorheriges setFont()) - ein Bold-Font in
    // sehr kleiner Groesse wuerde nur zu einem verwaschenen Klumpen.
    gfx_->setFont(&FreeSansBold10pt7b);
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

void TftDisplay::update(bool hx711Connected, bool bleConnected) {
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
            forceRedraw_ = true;
        } else {
            if (!forceRedraw_ && (now - lastRenderMs_ < 150)) return;
            lastRenderMs_ = now;
            forceRedraw_ = false;
            renderRemoteCueScreen(remoteCue_);
            return;
        }
    }

    bool bleChanged = (bleConnected != lastBleConnected_);
    lastBleConnected_ = bleConnected;
    bool hx711Changed = (hx711Connected != lastHx711Connected_);
    lastHx711Connected_ = hx711Connected;

    // Anders als beim frueheren Live-Gewicht (das sich fast jeden Tick
    // aenderte, siehe altes fullRedraw-Kommentar) laeuft in keinem der
    // Ruhezustaende (Spielauswahl/Bestaetigt/Warteschirm) irgendeine
    // laufende Animation - ohne aenderungsbasierte Bedingung wuerde hier
    // trotzdem alle ~150ms ein kompletter fillScreen(BLACK) samt Neuzeichnen
    // passieren, obwohl sich nichts geaendert hat (sichtbarer Schwarz-Blitz,
    // "Flackern"). Nur noch bei einer TATSAECHLICHEN Aenderung neu zeichnen,
    // kein periodischer Zwangs-Redraw mehr.
    if (!bleChanged && !hx711Changed && !forceRedraw_) {
        return;
    }

    lastRenderMs_ = now;
    forceRedraw_ = false;

    // Prioritaet: Spieler-Warteschirm (ein Zug ist gerade aktiv, aber
    // zwischen zwei RemoteCues) > Geraete-Spielauswahl (Ruhezustand).
    if (hasActivePlayer_) {
        renderWaitingForTurnScreen(hx711Connected, bleConnected);
    } else if (localScreen_ == LocalScreen::GameConfirmed) {
        renderGameConfirmedScreen(hx711Connected, bleConnected);
    } else {
        renderGamePickerScreen(hx711Connected, bleConnected);
    }
}

void TftDisplay::pickerNext() {
    // Waehrend eines echten Spiels (RemoteCue laeuft oder ein Spieler ist am
    // Zug) absichtlich ohne Wirkung - Taste 1 waere sonst waehrend eines
    // laufenden Spiels doppeldeutig belegt.
    if (remoteCue_ != RemoteCue::None || hasActivePlayer_) return;
    pickerIndex_ = (pickerIndex_ + 1) % PICKER_GAME_COUNT;
    localScreen_ = LocalScreen::GamePicker;
    forceRedraw_ = true;
}

void TftDisplay::pickerConfirm() {
    if (remoteCue_ != RemoteCue::None || hasActivePlayer_) return;
    localScreen_ = LocalScreen::GameConfirmed;
    forceRedraw_ = true;
}

// Kleiner BLE-Verbindungspunkt oben rechts - bewusst in ALLEN drei
// Ruhezustands-Screens an derselben Stelle, statt wie frueher als eigene
// Fusszeile (die je nach Screen unterschiedlich viel Platz brauchte).
void TftDisplay::renderIdleFooter(bool hx711Connected, bool bleConnected) {
    (void)hx711Connected; // HX711-Fehler wird vorher schon als eigener Screen abgefangen
    uint16_t bleColor = bleConnected ? successColor_ : mutedColor_;
    gfx_->fillCircle(307, 13, 4, bleColor);
}

void TftDisplay::printCentered(const char* text, int16_t topY, uint16_t color) {
    int16_t x1, y1;
    uint16_t w, h;
    gfx_->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    gfx_->setTextColor(color);
    // y1 ist beim eingebauten Bitmap-Font 0 (Cursor = Textoberkante, Formel
    // bleibt unveraendert `topY`), bei einem eigenen GFXfont dagegen negativ
    // (Cursor = Grundlinie) - das Abziehen von y1 gleicht beide Faelle
    // einheitlich auf "Textoberkante bei topY" aus.
    gfx_->setCursor((320 - (int16_t)w) / 2, topY - y1);
    gfx_->print(text);
}

void TftDisplay::renderGamePickerScreen(bool hx711Connected, bool bleConnected) {
    gfx_->fillScreen(BLACK);

    if (!hx711Connected) {
        gfx_->setTextSize(2);
        printCentered("HX711 Fehler", 60, dangerColor_);
        gfx_->setFont(nullptr);
        gfx_->setTextSize(1);
        gfx_->setCursor(8, 90);
        gfx_->setTextColor(mutedColor_);
        gfx_->print("Verkabelung pruefen");
        gfx_->setFont(&FreeSansBold10pt7b);
        return;
    }

    gfx_->setFont(nullptr);
    gfx_->setTextColor(mutedColor_);
    gfx_->setTextSize(1);
    gfx_->setCursor(8, 12);
    gfx_->print("Spiel waehlen");
    gfx_->setFont(&FreeSansBold10pt7b);

    const PickerGame& game = PICKER_GAMES[pickerIndex_];
    uint16_t gameColor = gameAccentColor(game.kind);
    renderGameIcon(game.kind, 160, 62, 42, gameColor);

    gfx_->setTextSize(2);
    printCentered(game.name, 88, textColor_);

    char posBuf[8];
    snprintf(posBuf, sizeof(posBuf), "%u / %u", pickerIndex_ + 1, PICKER_GAME_COUNT);
    gfx_->setFont(nullptr);
    gfx_->setTextSize(1);
    gfx_->setTextColor(mutedColor_);
    int16_t x1, y1;
    uint16_t w, h;
    gfx_->getTextBounds(posBuf, 0, 0, &x1, &y1, &w, &h);
    gfx_->setCursor((320 - w) / 2, 128);
    gfx_->print(posBuf);

    gfx_->setCursor(8, 152);
    gfx_->print("Taste1: wechseln   Taste2: waehlen");
    gfx_->setFont(&FreeSansBold10pt7b);

    renderIdleFooter(hx711Connected, bleConnected);
}

void TftDisplay::renderGameConfirmedScreen(bool hx711Connected, bool bleConnected) {
    gfx_->fillScreen(BLACK);
    const PickerGame& game = PICKER_GAMES[pickerIndex_];
    uint16_t gameColor = gameAccentColor(game.kind);

    renderGameIcon(game.kind, 160, 52, 36, gameColor);

    gfx_->setTextSize(2);
    printCentered(game.name, 78, successColor_);

    gfx_->setFont(nullptr);
    gfx_->setTextSize(1);
    gfx_->setTextColor(mutedColor_);
    const char* hint = "In der App oeffnen";
    int16_t x1, y1;
    uint16_t w, h;
    gfx_->getTextBounds(hint, 0, 0, &x1, &y1, &w, &h);
    gfx_->setCursor((320 - w) / 2, 122);
    gfx_->print(hint);

    gfx_->setCursor(8, 152);
    gfx_->print("Taste1: anderes Spiel");
    gfx_->setFont(&FreeSansBold10pt7b);

    renderIdleFooter(hx711Connected, bleConnected);
}

// Zwischenzustand: ein Spieler ist am Zug (siehe setActivePlayer()), aber
// gerade laeuft kein RemoteCue (z.B. kurz zwischen zwei Rituale-Schritten).
// Kein Live-Gewicht mehr (war nur ein Debug-Hilfsmittel) - nur noch das
// Badge plus ein schlichter Warte-Hinweis.
void TftDisplay::renderWaitingForTurnScreen(bool hx711Connected, bool bleConnected) {
    gfx_->fillScreen(BLACK);
    renderPlayerBadge(6, 5);

    gfx_->setFont(nullptr);
    gfx_->setTextSize(2);
    gfx_->setCursor(8, 70);
    if (!hx711Connected) {
        gfx_->setTextColor(dangerColor_);
        gfx_->print("HX711 Fehler");
    } else {
        gfx_->setTextColor(mutedColor_);
        gfx_->print("Bereit ...");
    }
    gfx_->setFont(&FreeSansBold10pt7b);

    renderIdleFooter(hx711Connected, bleConnected);
}

void TftDisplay::playBootSprite(bool (*stepInit)()) {
    constexpr int16_t SPRITE_W = 320;
    constexpr int16_t SPRITE_H = 170;
    constexpr uint8_t FRAME_COUNT = 47;

    // Fehlende Palette/Frames (z.B. nach einem BLE-Update von einer alten
    // Firmware, die noch kein data/ auf SPIFFS geschrieben hat - ein
    // App-BLE-Update ueberträgt nur die App-Partition, nie die SPIFFS-
    // Partition, siehe README "Firmware-Update per BLE") duerfen den Boot
    // NICHT aufhalten - die Schleife unten faengt das pro Frame ab (leerer
    // Screen statt Animation), stepInit() laeuft trotzdem normal durch.
    if (!SPIFFS.begin(true)) {
        Serial.println("[Boot] SPIFFS-Mount fehlgeschlagen - Sprite-Animation uebersprungen.");
        while (stepInit()) {
            // trotzdem alle Initialisierungsschritte durchlaufen, sonst
            // bleibt die Waage ohne Anzeige haengen.
        }
        return;
    }

    // Auf dem Heap statt `static` - beides zusammen sind ~55KB, die nur
    // waehrend der Bootanimation gebraucht werden. `static` wuerde sie fuer
    // die gesamte Laufzeit blockieren (BLE/HX711 brauchen den Speicher
    // danach). Ein einfacher lokaler Stack-Array waere dagegen zu gross
    // fuer den Task-Stack (deutlich unter 55KB) - deshalb new[]/delete[].
    uint16_t* palette = new uint16_t[256]();
    uint8_t* frameBuf = new uint8_t[SPRITE_W * SPRITE_H];

    // Palette einmalig laden (512 Byte, 256 Eintraege je 16-Bit RGB565).
    // Bewusst FLACH (kein Unterordner) - SPIFFS' Pfadaufloesung fuer
    // Unterordner ist unzuverlaessig: stat() auf einen Pfad mit Unterordner
    // kann fehlschlagen und dann still auf einen (leeren) Directory-Handle
    // zurueckfallen, der zwar als "geoeffnet" durchgeht, aber bei jedem
    // read() 0 Byte liefert - kein Fehler, nur eine leere/schwarze Palette
    // (so aufgefallen: Boot-Log ueber USB sah komplett fehlerfrei aus, das
    // Display blieb trotzdem schwarz).
    File palFile = SPIFFS.open("/pal.raw", "r");
    if (palFile) {
        size_t got = palFile.read(reinterpret_cast<uint8_t*>(palette), 256 * sizeof(uint16_t));
        palFile.close();
        if (got != 256 * sizeof(uint16_t)) {
            Serial.printf("[Boot] Palette unvollstaendig gelesen: %u/%u Byte.\n", (unsigned)got, (unsigned)(256 * sizeof(uint16_t)));
        }
    } else {
        Serial.println("[Boot] Palette (data/pal.raw) fehlt - `pio run -t uploadfs` vergessen?");
    }

    bool initDone = false;
    uint8_t frameIndex = 0;
    uint32_t framesShown = 0;
    bool loggedMissingFrame = false;

    while (true) {
        uint32_t frameStartMs = millis();

        char path[16];
        snprintf(path, sizeof(path), "/f%03u.raw", frameIndex);
        File frameFile = SPIFFS.open(path, "r");
        if (frameFile) {
            size_t got = frameFile.read(frameBuf, SPRITE_W * SPRITE_H);
            frameFile.close();
            if (got == (size_t)(SPRITE_W * SPRITE_H)) {
                gfx_->drawIndexedBitmap(0, 0, frameBuf, palette, SPRITE_W, SPRITE_H);
            } else if (!loggedMissingFrame) {
                Serial.printf("[Boot] Frame unvollstaendig gelesen: %s (%u/%u Byte).\n", path, (unsigned)got, (unsigned)(SPRITE_W * SPRITE_H));
                loggedMissingFrame = true;
            }
        } else if (!loggedMissingFrame) {
            Serial.printf("[Boot] Frame-Datei fehlt: %s - `pio run -t uploadfs` vergessen?\n", path);
            loggedMissingFrame = true; // nicht bei jedem Frame erneut spammen
        }

        if (!initDone) {
            initDone = !stepInit();
        }

        frameIndex = (frameIndex + 1) % FRAME_COUNT;
        framesShown++;

        // Mindestens einmal komplett durchlaufen, auch wenn stepInit() laengst
        // fertig ist - sonst wurde bei einem schnellen Boot (siehe Kalibrierung/
        // Serial-Log: Init in ~2s fertig) nur der Anfang der Animation gezeigt,
        // nie der "Maßarbeit"-Schriftzug am Ende. Braucht die Initialisierung
        // laenger als ein Durchlauf, laeuft die Animation einfach weiter
        // (Endlosschleife), bis auch das fertig ist.
        if (initDone && framesShown >= FRAME_COUNT) {
            break;
        }

        // Restzeit bis zur festen Frame-Dauer abwarten - SPIFFS-Lesen +
        // Zeichnen + stepInit() brauchen pro Frame unterschiedlich lange
        // (deshalb "mal schneller, mal langsamer"). Dauert ein Frame
        // ausnahmsweise laenger als das Budget (z.B. ein besonders teurer
        // init-Schritt), einfach ohne Wartezeit weiter - aufholen bringt
        // nichts, wuerde die Animation nur ruckeln lassen.
        uint32_t elapsedMs = millis() - frameStartMs;
        if (elapsedMs < SPRITE_FRAME_DURATION_MS) {
            delay(SPRITE_FRAME_DURATION_MS - elapsedMs);
        }
    }

    delete[] palette;
    delete[] frameBuf;

    // Sofort sichtbar, nicht erst nach dem 150ms-Drossel-Timer des naechsten
    // update()-Aufrufs.
    forceRedraw_ = true;
}

void TftDisplay::setRemoteCue(RemoteCue cue, GameKind game) {
    remoteCue_ = cue;
    remoteCueGame_ = game;
    remoteCueSetMs_ = millis();
    forceRedraw_ = true; // sofort anzeigen, nicht auf den 150ms-Drossel-Timer warten

    // Neuer Cue = neuer Render-Kontext: alle "letzte Position"-Merker fuer
    // den Flacker-Fix (siehe TftDisplay.h) zuruecksetzen, damit renderAway*()
    // beim naechsten Aufruf die statischen Elemente frisch zeichnet statt
    // (faelschlich) etwas an einer stehengebliebenen alten Position
    // wegzuradieren, und renderRemoteCueScreen() den Puls-Ring ebenfalls neu
    // aufbaut statt nur den alten Ring zu loeschen.
    lastCueRendered_ = RemoteCue::None;
    lastGlowR_ = -1;
    awayStaticGame_ = GameKind::None;
    prevGolfBallX_ = -1000;
    prevDartMinX_ = -1000;
    prevCardLeft_ = -1000;
    prevTowerX_ = -1000;
    prevGloveX_ = -1000;
    prevBagX_ = -1000;
}

void TftDisplay::renderRemoteCueScreen(RemoteCue cue) {
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
            case GameKind::Boxen:
                renderAwayBoxen();
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
            color = hasActivePlayer_ ? activePlayerColor565_ : successColor_;
            showCheck = true;
            break;
        case RemoteCue::ResultPerfect:
            title = "VOLLTREFFER!";
            subtitle = "";
            color = successColor_;
            showCheck = true;
            break;
        case RemoteCue::ResultClose:
            title = "NAH DRAN";
            subtitle = "";
            color = accentColor_;
            showRing = true;
            break;
        case RemoteCue::ResultMiss:
        default:
            title = "DANEBEN";
            subtitle = "";
            color = dangerColor_;
            showCross = true;
            break;
    }

    constexpr int16_t badgeCx = 160;
    constexpr int16_t badgeCy = 42;
    constexpr int16_t badgeR = 26;

    // Flacker-Fix: Badge-Fuellung, Marker (Haken/Kreuz/Ring) und Texte sind
    // pro Cue vollkommen statisch - nur der duenne Puls-Glow um das Badge
    // aendert seinen Radius von Frame zu Frame. Ein voller fillScreen(BLACK)
    // + Neuzeichnen alle ~150ms (bis zu 20s lang beim BEREIT-Cue) erzeugte
    // dabei ein sichtbares Schwarz-Aufblitzen. Deshalb: den statischen Teil
    // nur EINMAL zeichnen, wenn sich der Cue tatsaechlich geaendert hat -
    // in jedem weiteren Frame desselben Cues nur noch den alten Glow-Ring
    // uebermalen und den neuen zeichnen.
    bool freshCue = (cue != lastCueRendered_);
    if (freshCue) {
        gfx_->fillScreen(BLACK);
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

        gfx_->setTextSize(2);
        printCentered(title, 78, color);

        if (subtitle[0] != '\0') {
            gfx_->setFont(nullptr);
            gfx_->setTextSize(2);
            gfx_->setTextColor(textColor_);
            int16_t x1, y1;
            uint16_t w, h;
            gfx_->getTextBounds(subtitle, 0, 0, &x1, &y1, &w, &h);
            gfx_->setCursor((320 - w) / 2, 122);
            gfx_->print(subtitle);
            gfx_->setFont(&FreeSansBold10pt7b);
        }

        lastCueRendered_ = cue;
        lastGlowR_ = -1; // erzwingt unten: kein Erase-Versuch fuer den allerersten Ring
    }

    // Sanfter Puls, wie beim BEREIT-Zustand der App (Sweet-Spot-Glow) -
    // simpler Sinus auf dem Radius statt einer echten Glow-Textur.
    float pulse = 0.5f + 0.5f * sinf((float)millis() / 220.0f);
    int16_t glowR = badgeR + 3 + (int16_t)(pulse * 3);
    if (lastGlowR_ >= 0 && lastGlowR_ != glowR) {
        gfx_->drawCircle(badgeCx, badgeCy, lastGlowR_, BLACK);
    }
    gfx_->drawCircle(badgeCx, badgeCy, glowR, color);
    lastGlowR_ = glowR;
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
    gfx_->drawCircle(ccx, ccy, r, textColor_);

    char c = activePlayerName_.length() > 0 ? activePlayerName_[0] : '?';
    if (c >= 'a' && c <= 'z') c -= 32; // Grossbuchstabe, ohne <ctype.h>
    char initial[2] = { c, '\0' };

    gfx_->setFont(nullptr);
    gfx_->setTextSize(1);
    gfx_->setTextColor(contrastTextColor(activePlayerColor565_));
    int16_t tx1, ty1;
    uint16_t tw, th;
    gfx_->getTextBounds(initial, 0, 0, &tx1, &ty1, &tw, &th);
    gfx_->setCursor(ccx - tw / 2, ccy - th / 2 - 1);
    gfx_->print(initial);

    int16_t textX = x + 2 * r + 12;
    gfx_->setTextSize(2);
    gfx_->setTextColor(textColor_);
    gfx_->setCursor(textX, y);
    gfx_->print(activePlayerName_);

    gfx_->setTextSize(1);
    gfx_->setTextColor(mutedColor_);
    gfx_->setCursor(textX, y + 17);
    gfx_->print("ist dran");
    gfx_->setFont(&FreeSansBold10pt7b);

    renderGameIcon(activeGame_, 300, ccy, 16, activePlayerColor565_);
}

uint16_t TftDisplay::gameAccentColor(GameKind game) const {
    switch (game) {
        case GameKind::Golf:      return gfx_->color565(0x4A, 0xDE, 0x80); // #4ade80
        case GameKind::Dart:      return gfx_->color565(0xFB, 0x71, 0x85); // #fb7185
        case GameKind::Blackjack: return gfx_->color565(0xE8, 0xC1, 0x4D); // #e8c14d
        case GameKind::Tower:     return gfx_->color565(0xC9, 0x9A, 0x5C); // #c99a5c
        case GameKind::Boxen:     return gfx_->color565(0xA7, 0x8B, 0xFA); // #a78bfa
        case GameKind::Scale:     return gfx_->color565(0x38, 0xBD, 0xF8); // #38bdf8
        case GameKind::None:
        default:                  return accentColor_;
    }
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
        case GameKind::Boxen:
            gfx_->fillCircle(cx - half / 4, cy, half * 2 / 3, color);
            gfx_->fillRoundRect(cx + half / 6, cy - half / 2, half, size - 2, 2, color);
            break;
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
//
// Flacker-Fix: Boden/Fahne/Label sind ueber die gesamte Cue-Dauer statisch
// und werden nur beim allerersten Frame (awayStaticGame_ wechselt) gezeichnet.
// Danach wird pro Frame nur der Ball an seiner alten Position schwarz
// uebermalt, bevor er an der neuen Position neu erscheint - kein
// fillScreen(BLACK) mehr noetig.
void TftDisplay::renderAwayGolf() {
    constexpr int16_t groundY = 130;
    constexpr int16_t ballR = 6;

    if (awayStaticGame_ != GameKind::Golf) {
        gfx_->fillScreen(BLACK);
        gfx_->drawFastHLine(20, groundY, 280, mutedColor_);
        gfx_->drawFastVLine(280, groundY - 40, 40, textColor_);
        gfx_->fillTriangle(280, groundY - 40, 280, groundY - 28, 300, groundY - 34, gameAccentColor(GameKind::Golf));

        gfx_->setFont(nullptr);
        gfx_->setTextSize(2);
        int16_t x1, y1;
        uint16_t w, h;
        const char* label = "Ball fliegt ...";
        gfx_->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
        gfx_->setTextColor(mutedColor_);
        gfx_->setCursor((320 - w) / 2, 20);
        gfx_->print(label);
        gfx_->setFont(&FreeSansBold10pt7b);

        awayStaticGame_ = GameKind::Golf;
        prevGolfBallX_ = -1000; // erzwingt unten: kein Erase-Versuch fuer den allerersten Ball
    }

    float t = fmodf((float)millis(), 1400.0f) / 1400.0f; // 0..1, Endlosschleife
    float x = 30.0f + t * 240.0f;
    float y = groundY - sinf(t * PI) * 70.0f; // Parabelbogen
    int16_t bx = (int16_t)x, by = (int16_t)y;

    if (prevGolfBallX_ > -1000) {
        gfx_->fillCircle(prevGolfBallX_, prevGolfBallY_, ballR, BLACK);
    }
    gfx_->fillCircle(bx, by, ballR, textColor_);
    prevGolfBallX_ = bx;
    prevGolfBallY_ = by;
}

// "Pfeil fliegt zur Scheibe" - analog zu DartThrow.tsx: Dartscheibe rechts
// (konzentrische Ringe + Bullseye), Pfeil fliegt in flacher Kurve heran und
// richtet sich dabei auf die Scheibe aus (Linie + Pfeilspitze, kein echtes
// Sprite noetig - die Ausrichtung ergibt sich aus der Blickrichtung).
//
// Flacker-Fix wie renderAwayGolf(): Scheibe + Label nur einmal, pro Frame
// nur die Bounding-Box des alten Pfeils schwarz uebermalen.
void TftDisplay::renderAwayDart() {
    constexpr int16_t boardCx = 270;
    constexpr int16_t boardCy = 85;

    if (awayStaticGame_ != GameKind::Dart) {
        gfx_->fillScreen(BLACK);
        gfx_->drawCircle(boardCx, boardCy, 42, mutedColor_);
        gfx_->drawCircle(boardCx, boardCy, 30, dangerColor_);
        gfx_->drawCircle(boardCx, boardCy, 16, accentColor_);
        gfx_->fillCircle(boardCx, boardCy, 6, gameAccentColor(GameKind::Dart));

        gfx_->setFont(nullptr);
        gfx_->setTextSize(2);
        int16_t x1, y1;
        uint16_t w, h;
        const char* label = "Pfeil fliegt ...";
        gfx_->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
        gfx_->setTextColor(mutedColor_);
        gfx_->setCursor((320 - w) / 2, 20);
        gfx_->print(label);
        gfx_->setFont(&FreeSansBold10pt7b);

        awayStaticGame_ = GameKind::Dart;
        prevDartMinX_ = -1000;
    }

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
    float backAngle1 = angle + 2.6f, backAngle2 = angle - 2.6f;
    float back1X = tipX + cosf(backAngle1) * 6.0f, back1Y = tipY + sinf(backAngle1) * 6.0f;
    float back2X = tipX + cosf(backAngle2) * 6.0f, back2Y = tipY + sinf(backAngle2) * 6.0f;

    // Bounding-Box aus allen vier Punkten des Pfeils (Schwanz/Spitze/zwei
    // Feder-Striche) plus 2px Rand - deckt die alte Pfeil-Zeichnung sicher
    // komplett ab, ohne die (weiter rechts liegende) Dartscheibe zu beruehren.
    int16_t minX = (int16_t)fminf(fminf(tailX, tipX), fminf(back1X, back2X)) - 2;
    int16_t maxX = (int16_t)fmaxf(fmaxf(tailX, tipX), fmaxf(back1X, back2X)) + 2;
    int16_t minY = (int16_t)fminf(fminf(tailY, tipY), fminf(back1Y, back2Y)) - 2;
    int16_t maxY = (int16_t)fmaxf(fmaxf(tailY, tipY), fmaxf(back1Y, back2Y)) + 2;

    if (prevDartMinX_ > -1000) {
        gfx_->fillRect(prevDartMinX_, prevDartMinY_, prevDartMaxX_ - prevDartMinX_, prevDartMaxY_ - prevDartMinY_, BLACK);
    }

    gfx_->drawLine((int16_t)tailX, (int16_t)tailY, (int16_t)tipX, (int16_t)tipY, textColor_);
    gfx_->drawLine((int16_t)tipX, (int16_t)tipY, (int16_t)back1X, (int16_t)back1Y, textColor_);
    gfx_->drawLine((int16_t)tipX, (int16_t)tipY, (int16_t)back2X, (int16_t)back2Y, textColor_);

    prevDartMinX_ = minX;
    prevDartMinY_ = minY;
    prevDartMaxX_ = maxX;
    prevDartMaxY_ = maxY;
}

// "Karte wird gezogen" - analog zu BlackjackDraw.tsx: Stapel links (still),
// eine Karte fliegt zur Mitte und "flippt" dabei um (hier simuliert durch
// horizontales Stauchen auf eine duenne Linie in der Flugmitte, statt
// echter 3D-Rotation - danach zeigt die Karte ihre "Vorderseite" mit
// Symbol statt der leeren Rueckseite).
//
// Flacker-Fix wie renderAwayGolf(): Stapel + Label nur einmal, pro Frame
// nur das alte Karten-Rechteck (inkl. 3px Rand fuer die Rundung) schwarz
// uebermalen - Y/Hoehe der fliegenden Karte sind konstant, nur X/Breite
// aendern sich, deshalb reicht ein simples fillRect als Radiergummi.
void TftDisplay::renderAwayBlackjack() {
    constexpr int16_t cardW = 40, cardH = 56;
    constexpr int16_t stackX = 40, cardY = 57;

    if (awayStaticGame_ != GameKind::Blackjack) {
        gfx_->fillScreen(BLACK);
        gfx_->drawRoundRect(stackX, cardY, cardW, cardH, 6, textColor_);
        gfx_->drawRoundRect(stackX + 3, cardY - 3, cardW, cardH, 6, mutedColor_);

        gfx_->setFont(nullptr);
        gfx_->setTextSize(2);
        int16_t x1, y1;
        uint16_t w, h;
        const char* label = "Karte wird gezogen ...";
        gfx_->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
        gfx_->setTextColor(mutedColor_);
        gfx_->setCursor((320 - w) / 2, 20);
        gfx_->print(label);
        gfx_->setFont(&FreeSansBold10pt7b);

        awayStaticGame_ = GameKind::Blackjack;
        prevCardLeft_ = -1000;
    }

    float t = fmodf((float)millis(), 1300.0f) / 1300.0f; // 0..1, Endlosschleife
    constexpr int16_t targetX = 160 - cardW / 2;
    float x = stackX + t * (targetX - stackX);
    // Breite pulst zu 0 in der Flugmitte (Flip-Illusion) und wieder zurueck.
    float flip = fabsf(sinf(t * PI));
    int16_t w = (int16_t)(cardW * (0.15f + 0.85f * flip));
    bool showFront = t > 0.5f;

    int16_t cx = (int16_t)x + cardW / 2;
    int16_t left = cx - w / 2;

    if (prevCardLeft_ > -1000) {
        gfx_->fillRect(prevCardLeft_ - 2, cardY - 2, prevCardW_ + 4, cardH + 4, BLACK);
    }

    uint16_t gameColor = gameAccentColor(GameKind::Blackjack);
    if (showFront) {
        gfx_->fillRoundRect(left, cardY, w, cardH, 5, textColor_);
        if (w > 14) {
            gfx_->fillTriangle(cx, cardY + 10, cx - 8, cardY + 30, cx + 8, cardY + 30, dangerColor_);
            gfx_->fillRect(cx - 2, cardY + 28, 4, 14, dangerColor_);
        }
    } else {
        gfx_->fillRoundRect(left, cardY, w, cardH, 5, gameColor);
    }
    gfx_->drawRoundRect(left, cardY, w, cardH, 5, textColor_);

    prevCardLeft_ = left;
    prevCardW_ = w;
}

// "Block wird gezogen" - analog zu TowerPull.tsx: Rest-Turm steht mittig,
// ein Block wird seitlich herausgezogen, wandert dann nach oben auf den
// Stapel. Endlosschleife mit vier Phasen (raus, Pause, hoch, Pause) wie im
// App-Original (times: [0, 0.35, 0.65, 1]).
//
// Flacker-Fix wie renderAwayGolf(): Rest-Turm + Label nur einmal, pro Frame
// nur das alte Block-Rechteck (Groesse konstant, nur x/y aendern sich)
// schwarz uebermalen.
void TftDisplay::renderAwayTower() {
    constexpr int16_t towerCx = 160, blockW = 70, blockH = 14, baseY = 140;

    if (awayStaticGame_ != GameKind::Tower) {
        gfx_->fillScreen(BLACK);
        for (int8_t i = 0; i < 4; i++) {
            gfx_->drawRoundRect(towerCx - blockW / 2, baseY - (i + 1) * (blockH + 2), blockW, blockH, 3, textColor_);
        }

        gfx_->setFont(nullptr);
        gfx_->setTextSize(2);
        int16_t x1, y1;
        uint16_t w, h;
        const char* label = "Block wird gezogen ...";
        gfx_->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
        gfx_->setTextColor(mutedColor_);
        gfx_->setCursor((320 - w) / 2, 20);
        gfx_->print(label);
        gfx_->setFont(&FreeSansBold10pt7b);

        awayStaticGame_ = GameKind::Tower;
        prevTowerX_ = -1000;
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

    if (prevTowerX_ > -1000) {
        gfx_->fillRect(prevTowerX_, prevTowerY_, blockW, blockH, BLACK);
    }
    gfx_->fillRoundRect(pulledX, blockY, blockW, blockH, 3, gameAccentColor(GameKind::Tower));
    gfx_->drawRoundRect(pulledX, blockY, blockW, blockH, 3, textColor_);
    prevTowerX_ = pulledX;
    prevTowerY_ = blockY;
}

// "Boxhandschuh trifft Sack" - analog zu BoxenAwayMoment.tsx: Boxsack haengt
// rechts, ein Handschuh fliegt in einer Hin-und-her-Bewegung heran und
// wieder zurueck (Ping-Pong statt Parabel/Kreisbahn wie bei den anderen
// Spielen - passt besser zu einem Jab). Der Sack wackelt staerker, je naeher
// der Handschuh gerade dran ist - kein echter Treffer-Trigger noetig, das
// Wackeln folgt einfach der Ping-Pong-Position selbst.
//
// Flacker-Fix wie renderAwayGolf(): nur das Label ist wirklich statisch -
// Sack UND Handschuh bewegen sich (der Sack nur minimal, das Wackeln),
// deshalb bekommen beide ihre eigene Dirty-Rect-Radierung statt eines
// fillScreen(BLACK).
void TftDisplay::renderAwayBoxen() {
    constexpr int16_t bagTopY = 55;
    constexpr int16_t bagW = 32, bagH = 55;
    constexpr int16_t bagCx = 270;

    if (awayStaticGame_ != GameKind::Boxen) {
        gfx_->fillScreen(BLACK);

        gfx_->setFont(nullptr);
        gfx_->setTextSize(2);
        int16_t x1, y1;
        uint16_t w, h;
        const char* label = "Schlag wird gesetzt ...";
        gfx_->getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
        gfx_->setTextColor(mutedColor_);
        gfx_->setCursor((320 - w) / 2, 20);
        gfx_->print(label);
        gfx_->setFont(&FreeSansBold10pt7b);

        awayStaticGame_ = GameKind::Boxen;
        prevBagX_ = -1000;
        prevGloveX_ = -1000;
    }

    float t = fmodf((float)millis(), 1300.0f) / 1300.0f; // 0..1, Endlosschleife
    float pp = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;   // 0 -> 1 -> 0 (hin und zurueck)
    float wobble = sinf((float)millis() / 120.0f) * 2.0f * pp;

    int16_t bagX = bagCx + (int16_t)wobble;
    if (prevBagX_ > -1000) {
        gfx_->fillRect(prevBagX_ - bagW / 2 - 1, 20, bagW + 2, (bagTopY - 20) + bagH + 5, BLACK);
    }
    gfx_->drawFastVLine(bagX, 20, bagTopY - 20, textColor_);
    gfx_->fillRoundRect(bagX - bagW / 2, bagTopY, bagW, bagH, 10, mutedColor_);
    prevBagX_ = bagX;

    float gloveX = 30.0f + pp * (float)(bagX - bagW / 2 - 30 - 14);
    int16_t gloveY = bagTopY + bagH / 2;
    int16_t gx = (int16_t)gloveX;
    if (prevGloveX_ > -1000) {
        gfx_->fillRect(prevGloveX_ - 11, prevGloveY_ - 12, 27, 24, BLACK);
    }
    uint16_t gloveColor = gameAccentColor(GameKind::Boxen);
    gfx_->fillCircle(gx, gloveY, 10, gloveColor);
    gfx_->fillRoundRect(gx + 6, gloveY - 7, 9, 14, 3, gloveColor);
    prevGloveX_ = gx;
    prevGloveY_ = gloveY;
}

void TftDisplay::showMessage(const String& title, const String& body) {
    gfx_->fillScreen(BLACK);

    gfx_->setFont(nullptr);
    gfx_->setTextColor(textColor_);
    gfx_->setTextSize(2);
    gfx_->setCursor(8, 8);
    gfx_->print(title);

    gfx_->setTextSize(1);
    gfx_->setTextColor(mutedColor_);
    gfx_->setCursor(8, 45);
    gfx_->print(body);
    gfx_->setFont(&FreeSansBold10pt7b);

    // Naechster update()-Aufruf soll sofort neu zeichnen statt auf den
    // Drossel-Timer zu warten.
    forceRedraw_ = true;
}
