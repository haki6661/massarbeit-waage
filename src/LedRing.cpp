#include "LedRing.h"

#include <math.h>

#if MASSARBEIT_HAS_LED_RING
// Nur im scharfgeschalteten Build vorhanden - im ausgeschalteten Build ist
// die Bibliothek gar keine Abhaengigkeit (siehe platformio.ini).
#include <Adafruit_NeoPixel.h>
#endif

namespace {

// Farben 1:1 aus derselben Palette wie TftDisplay (App-Design, src/styles/
// globals.css im App-Repo) - Ring und Display sollen dieselbe Farbsprache
// sprechen, sonst blinkt der Deckel in anderen Toenen als der Bildschirm
// direkt daneben.
constexpr uint8_t ACCENT_R = 0xf2, ACCENT_G = 0x8a, ACCENT_B = 0x4a; // #f28a4a
constexpr uint8_t SUCCESS_R = 0x66, SUCCESS_G = 0xbb, SUCCESS_B = 0x6a; // #66bb6a
constexpr uint8_t DANGER_R = 0xef, DANGER_G = 0x53, DANGER_B = 0x50; // #ef5350
constexpr uint8_t WARNING_R = 0xff, WARNING_G = 0xa7, WARNING_B = 0x26; // #ffa726
constexpr uint8_t INFO_R = 0x4a, INFO_G = 0x90, INFO_B = 0xf2; // BLE-Blau

// Cue-Timeouts exakt wie bei TftDisplay/LedStatusUi - der Ring darf nicht
// laenger auf einem Zustand haengen bleiben als die Hauptanzeige, sonst
// widersprechen sich beide.
constexpr uint32_t CUE_TIMEOUT_LONG_MS = 20000;
constexpr uint32_t CUE_TIMEOUT_RESULT_MS = 4000;

#if MASSARBEIT_HAS_LED_RING
Adafruit_NeoPixel strip(MASSARBEIT_LED_RING_COUNT, Pins::LED_RING_DATA, NEO_GRB + NEO_KHZ800);

// Wahrgenommene Helligkeit waechst nicht linear mit dem PWM-Wert - ohne
// diese Korrektur sieht jeder Verlauf so aus, als bliebe der Ring die meiste
// Zeit einfach hell (gleiche Ueberlegung wie die Quadrierung in
// LedStatusUi::setLevel(), hier je Farbkanal).
uint8_t gamma8(uint8_t value) {
    uint32_t v = value;
    return static_cast<uint8_t>((v * v + 127) / 255);
}
#endif

// min()/max() sind auf dem Arduino-Core je nach Version Makro ODER Template -
// beides hier bewusst vermeiden, das kostet nur eine Zeile.
float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

uint8_t addChannel(uint8_t base, uint8_t added) {
    uint16_t sum = static_cast<uint16_t>(base) + added;
    return static_cast<uint8_t>(sum > 255 ? 255 : sum);
}

} // namespace

float LedRing::breathe(uint32_t elapsedMs, uint16_t periodMs) {
    if (periodMs == 0) return 1.0f;
    float phase = (elapsedMs % periodMs) / static_cast<float>(periodMs);
    return 0.5f * (1.0f - cosf(2.0f * PI * phase));
}

LedRing::Rgb LedRing::scaled(Rgb color, float factor) {
    if (factor <= 0.0f) return {0, 0, 0};
    if (factor > 1.0f) factor = 1.0f;
    return {static_cast<uint8_t>(color.r * factor),
            static_cast<uint8_t>(color.g * factor),
            static_cast<uint8_t>(color.b * factor)};
}

// Gegenstueck zu DeviceUi::color565FromRgb(): die Spielerfarbe kommt per BLE
// als 8-Bit-RGB, wird fuer das Display auf RGB565 gepackt und hier fuer den
// Ring wieder ausgepackt. Der Rueckweg ist verlustbehaftet (5/6/5 Bit), das
// faellt bei einer Leuchtfarbe aber nicht auf - und er spart eine zweite,
// parallel zu pflegende Uebergabe durch die halbe Klassenkette.
LedRing::Rgb LedRing::fromColor565(uint16_t color565) {
    uint8_t r = static_cast<uint8_t>((color565 >> 11) & 0x1F);
    uint8_t g = static_cast<uint8_t>((color565 >> 5) & 0x3F);
    uint8_t b = static_cast<uint8_t>(color565 & 0x1F);
    // Obere Bits nach unten kopieren, damit 0x1F wirklich 255 wird und nicht 248.
    return {static_cast<uint8_t>((r << 3) | (r >> 2)),
            static_cast<uint8_t>((g << 2) | (g >> 4)),
            static_cast<uint8_t>((b << 3) | (b >> 2))};
}

// Kennfarbe je Spiel, damit ein Spiel auf Ring, Display UND App an derselben
// Farbe erkennbar ist (siehe TftDisplay::gameAccentColor(), gleiche
// Hex-Werte wie `game.accent.solid` im App-Repo).
LedRing::Rgb LedRing::gameColor(GameKind game) {
    switch (game) {
        case GameKind::Golf:      return {0x4c, 0xaf, 0x50};
        case GameKind::Dart:      return {0xef, 0x53, 0x50};
        case GameKind::Blackjack: return {0x42, 0xa5, 0xf5};
        case GameKind::Tower:     return {0xab, 0x47, 0xbc};
        case GameKind::Scale:     return {0x26, 0xa6, 0x9a};
        case GameKind::Boxen:     return {0xff, 0x70, 0x43};
        default:                  return {ACCENT_R, ACCENT_G, ACCENT_B};
    }
}

// ============================================================================
// Bausteine
// ============================================================================

void LedRing::clear() {
    for (uint16_t i = 0; i < COUNT; ++i) buffer_[i] = {0, 0, 0};
}

void LedRing::fillAll(Rgb color, float scale) {
    Rgb c = scaled(color, scale);
    for (uint16_t i = 0; i < COUNT; ++i) buffer_[i] = c;
}

void LedRing::setPixel(int16_t index, Rgb color, float scale) {
    if (COUNT == 0) return;
    // Ring: alles rechnet in Umlaeufen, negative und ueberlaufende Indizes
    // sind der Normalfall und werden hier einmal zentral eingefangen.
    int32_t i = index % static_cast<int32_t>(COUNT);
    if (i < 0) i += COUNT;
    buffer_[i] = scaled(color, scale);
}

void LedRing::setPixelBlended(float position, Rgb color, float scale) {
    if (COUNT == 0) return;
    float wrapped = fmodf(position, static_cast<float>(COUNT));
    if (wrapped < 0.0f) wrapped += COUNT;
    int16_t low = static_cast<int16_t>(wrapped);
    float frac = wrapped - low;

    // Additiv statt ueberschreibend: mehrere Punkte duerfen sich ueberlagern
    // (z.B. Komet-Schweif), ohne sich gegenseitig auszuloeschen.
    auto add = [this](int16_t index, Rgb c) {
        int32_t i = index % static_cast<int32_t>(COUNT);
        if (i < 0) i += COUNT;
        buffer_[i].r = addChannel(buffer_[i].r, c.r);
        buffer_[i].g = addChannel(buffer_[i].g, c.g);
        buffer_[i].b = addChannel(buffer_[i].b, c.b);
    };

    add(low, scaled(color, scale * (1.0f - frac)));
    add(low + 1, scaled(color, scale * frac));
}

void LedRing::fillArc(float fraction, Rgb color, float scale) {
    if (COUNT == 0) return;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    float litPixels = fraction * COUNT;
    for (uint16_t i = 0; i < COUNT; ++i) {
        float coverage = litPixels - i; // 1 = ganz an, 0..1 = angebrochene LED
        if (coverage <= 0.0f) continue;
        if (coverage > 1.0f) coverage = 1.0f;
        buffer_[i] = scaled(color, scale * coverage);
    }
}

void LedRing::show() {
#if MASSARBEIT_HAS_LED_RING
    for (uint16_t i = 0; i < COUNT; ++i) {
        // Helligkeitsdeckel: WS2812B ziehen bei Vollweiss bis ~60mA je LED -
        // an einem 16340-Zellchen ist das nicht nebensaechlich. Der Deckel
        // sitzt bewusst HIER, an der einzigen Stelle, die tatsaechlich Strom
        // schaltet, und nicht in den einzelnen Mustern: so kann kein spaeter
        // ergaenztes Muster ihn versehentlich umgehen.
        uint32_t cap = LED_RING_MAX_BRIGHTNESS;
        uint8_t r = static_cast<uint8_t>(gamma8(buffer_[i].r) * cap / 255);
        uint8_t g = static_cast<uint8_t>(gamma8(buffer_[i].g) * cap / 255);
        uint8_t b = static_cast<uint8_t>(gamma8(buffer_[i].b) * cap / 255);
        strip.setPixelColor(i, strip.Color(r, g, b));
    }
    strip.show();
#endif
}

// ============================================================================
// Lebenszyklus
// ============================================================================

void LedRing::begin() {
    if (!ENABLED) {
        // Genau eine Zeile im Log, damit beim Debuggen am Geraet sofort klar
        // ist, warum der angeloetete Ring dunkel bleibt.
        Serial.println("[Ring] WS2812B-Ring vorbereitet, aber nicht aktiviert "
                       "(MASSARBEIT_HAS_LED_RING = 0 im Board-Profil).");
        return;
    }
#if MASSARBEIT_HAS_LED_RING
    strip.begin();
    strip.clear();
    strip.show();
    Serial.printf("[Ring] WS2812B-Ring aktiv: %u LEDs an GPIO%u, Helligkeit max. %u/255.\n",
                  (unsigned)COUNT, (unsigned)Pins::LED_RING_DATA, (unsigned)LED_RING_MAX_BRIGHTNESS);
#endif
    ready_ = true;
    stateStartMs_ = millis();
    lastFrameMs_ = 0;
}

void LedRing::prepareForSleep() {
    if (!ENABLED || !ready_) return;
    clear();
    show();
    ready_ = false;
}

void LedRing::setWeight(float grams) {
    if (!ENABLED) return;
    weightG_ = grams;
}

void LedRing::setRemoteCue(RemoteCue cue, GameKind game) {
    if (!ENABLED) return;
    remoteCue_ = cue;
    remoteCueGame_ = game;
    remoteCueSetMs_ = millis();
    // Ein echter Spiel-Cue beendet eine noch laufende Ampel: kommt "bereit"
    // oder ein Ergebnis herein, ist der Start ohnehin vorbei.
    if (cue != RemoteCue::None) racePhase_ = RacePhase::Idle;
}

void LedRing::setActivePlayer(GameKind game, uint16_t color565) {
    if (!ENABLED) return;
    hasActivePlayer_ = true;
    activeGame_ = game;
    activePlayerColor_ = fromColor565(color565);
}

void LedRing::clearActivePlayer() {
    if (!ENABLED) return;
    hasActivePlayer_ = false;
    activeGame_ = GameKind::None;
}

void LedRing::startRaceLights(uint32_t holdMs) {
    if (!ENABLED) return;
    racePhase_ = RacePhase::LampsOn;
    racePhaseStartMs_ = millis();
    raceHoldMs_ = holdMs;
    remoteCue_ = RemoteCue::None; // Ampel hat waehrend ihres Ablaufs Vorrang
}

void LedRing::raceLightsGreen() {
    if (!ENABLED) return;
    // Auch aus LampsOn heraus zulaessig: wenn die App frueher Gruen gibt als
    // die Lampen brauchen, gewinnt die App - sie fuehrt die Uhr, an der die
    // Reaktionszeit gemessen wird.
    racePhase_ = RacePhase::Go;
    racePhaseStartMs_ = millis();
}

void LedRing::abortRaceLights() {
    if (!ENABLED) return;
    racePhase_ = RacePhase::FalseStart;
    racePhaseStartMs_ = millis();
}

// ============================================================================
// Bildaufbau
// ============================================================================

void LedRing::update(bool hx711Connected, bool bleConnected) {
    if (!ENABLED || !ready_) return;

    uint32_t now = millis();
    // Fest getaktet statt "so schnell wie loop() laeuft": strip.show()
    // sperrt fuer die Dauer der Uebertragung die Interrupts (~30µs je LED),
    // das soll nicht ungebremst zwischen HX711-Messungen liegen.
    if (now - lastFrameMs_ < LED_RING_FRAME_INTERVAL_MS) return;
    lastFrameMs_ = now;

    renderFrame(now, hx711Connected, bleConnected);
    show();
}

/**
 * Zustand -> Bild. Die Reihenfolge ist die Prioritaet und folgt bewusst
 * derselben Rangfolge wie LedStatusUi::currentSignal(), damit Ring und
 * Hauptanzeige nie Widerspruechliches zeigen - mit der Startampel als
 * einzigem Zusatz ganz oben (sie ist zeitkritisch, ein Fehlstart haengt
 * daran):
 *
 *   Startampel (Formel 1)  fuenf rote Lampen -> aus -> gruener Umlauf
 *   HX711-Fehler           drei rote Blitze rundum
 *   Bereit (0x11)          gruener Puls
 *   Away (0x13)            spielabhaengige Bewegung (siehe renderAway())
 *   Volltreffer (0x12)     gruener Umlauf auf hellem Grund
 *   Nah dran (0x12)        zwei gelbe Blitze
 *   Daneben (0x12)         ein langsamer roter Abfall
 *   Gewicht auf der Waage  Balken in Spieler-/Spielfarbe, waechst mit dem Gewicht
 *   Spieler am Zug         langsames Atmen in der Spielerfarbe
 *   keine App-Verbindung   einzelner blauer Punkt kreist
 *   verbunden, Leerlauf    sehr schwaches Atmen im Akzentton
 */
void LedRing::renderFrame(uint32_t now, bool hx711Connected, bool bleConnected) {
    const char* stateName = nullptr;

    if (racePhase_ != RacePhase::Idle) {
        renderRaceLights(now);
        stateName = "Startampel";
    } else if (!hx711Connected) {
        renderHx711Error(now);
        stateName = "HX711-Fehler";
    } else {
        // Cue-Timeout: dieselbe Logik wie in der Hauptanzeige - "bereit"/
        // "away" duerfen lange stehen, Ergebnisse loesen sich von selbst auf,
        // falls die App das Zuruecksetzen vergisst oder die Verbindung
        // mitten im Ritual abbricht.
        if (remoteCue_ != RemoteCue::None) {
            bool isLongLived = (remoteCue_ == RemoteCue::Ready || remoteCue_ == RemoteCue::Away);
            uint32_t timeoutMs = isLongLived ? CUE_TIMEOUT_LONG_MS : CUE_TIMEOUT_RESULT_MS;
            if (now - remoteCueSetMs_ > timeoutMs) remoteCue_ = RemoteCue::None;
        }

        if (remoteCue_ != RemoteCue::None) {
            renderCue(now, remoteCue_);
            stateName = "Cue";
        } else if (renderWeighing(now)) {
            stateName = "Wiegen";
        } else if (hasActivePlayer_) {
            renderActivePlayer(now);
            stateName = "Spieler am Zug";
        } else if (!bleConnected) {
            renderWaitingForApp(now);
            stateName = "wartet auf App";
        } else {
            renderIdle(now);
            stateName = "Leerlauf";
        }
    }

    if (stateName != lastStateName_) {
        lastStateName_ = stateName;
        stateStartMs_ = now;
        Serial.printf("[Ring] %s\n", stateName);
    }
}

// --- Startampel (Formel 1) --------------------------------------------------
// Fuenf Lampenpaare gleichmaessig ueber den Ring verteilt, wie die fuenf
// Lichtbruecken ueber der Startgeraden: alle LED_RING_RACE_LAMP_INTERVAL_MS
// kommt eine dazu. Sind alle fuenf an, bleiben sie stehen - entweder
// raceHoldMs_ lang (Waage lost aus) oder unbegrenzt, bis raceLightsGreen()
// kommt (App lost aus, Standardfall heute: `greenAtMs` in formel1State.ts).
// "Lights out" ist wie im Original das AUSGEHEN, nicht ein Farbwechsel;
// der kurze gruene Umlauf danach ist nur die Bestaetigung fuer den, der
// gerade nicht hingesehen hat.
void LedRing::renderRaceLights(uint32_t now) {
    uint32_t elapsed = now - racePhaseStartMs_;
    constexpr uint8_t LAMP_COUNT = 5;

    switch (racePhase_) {
        case RacePhase::LampsOn: {
            uint8_t lit = static_cast<uint8_t>(elapsed / LED_RING_RACE_LAMP_INTERVAL_MS) + 1;
            if (lit >= LAMP_COUNT) {
                lit = LAMP_COUNT;
                if (elapsed >= static_cast<uint32_t>(LAMP_COUNT) * LED_RING_RACE_LAMP_INTERVAL_MS) {
                    racePhase_ = RacePhase::Hold;
                    racePhaseStartMs_ = now;
                }
            }
            clear();
            for (uint8_t lamp = 0; lamp < lit; ++lamp) {
                float center = (static_cast<float>(lamp) / LAMP_COUNT) * COUNT;
                setPixelBlended(center, {DANGER_R, 0, 0});
                setPixelBlended(center + 1.0f, {DANGER_R, 0, 0}, 0.55f);
            }
            break;
        }

        case RacePhase::Hold: {
            clear();
            for (uint8_t lamp = 0; lamp < LAMP_COUNT; ++lamp) {
                float center = (static_cast<float>(lamp) / LAMP_COUNT) * COUNT;
                setPixelBlended(center, {DANGER_R, 0, 0});
                setPixelBlended(center + 1.0f, {DANGER_R, 0, 0}, 0.55f);
            }
            // raceHoldMs_ == 0 heisst "die App gibt Gruen" - dann hier
            // bewusst kein Timeout, sonst waere die Ampel bei einer
            // laengeren Haltezeit vorzeitig aus.
            if (raceHoldMs_ > 0 && elapsed >= raceHoldMs_) {
                racePhase_ = RacePhase::Go;
                racePhaseStartMs_ = now;
            }
            break;
        }

        case RacePhase::Go: {
            clear();
            if (elapsed < LED_RING_RACE_GO_DARK_MS) {
                // Bewusst nichts zeichnen: das schlagartige Dunkelwerden IST
                // das Startsignal.
                break;
            }
            uint32_t sinceGo = elapsed - LED_RING_RACE_GO_DARK_MS;
            if (sinceGo >= LED_RING_RACE_GO_SWEEP_MS) {
                racePhase_ = RacePhase::Idle;
                break;
            }
            float progress = sinceGo / static_cast<float>(LED_RING_RACE_GO_SWEEP_MS);
            fillArc(progress, {SUCCESS_R, SUCCESS_G, SUCCESS_B});
            break;
        }

        case RacePhase::FalseStart: {
            if (elapsed >= LED_RING_RACE_ABORT_MS) {
                racePhase_ = RacePhase::Idle;
                clear();
                break;
            }
            bool on = ((elapsed / 150) % 2) == 0;
            if (on) {
                fillAll({DANGER_R, DANGER_G, DANGER_B});
            } else {
                clear();
            }
            break;
        }

        case RacePhase::Idle:
            clear();
            break;
    }
}

// --- Fehler / Cues ----------------------------------------------------------

void LedRing::renderHx711Error(uint32_t now) {
    // Drei schnelle Blitze, dann Pause - dasselbe Zeitmuster wie die
    // Status-LED der Basis, nur in Rot und rundum.
    uint32_t pos = (now - stateStartMs_) % 1440;
    bool on = (pos < 540) && ((pos % 180) < 60);
    if (on) {
        fillAll({DANGER_R, DANGER_G, DANGER_B});
    } else {
        clear();
    }
}

void LedRing::renderCue(uint32_t now, RemoteCue cue) {
    uint32_t elapsed = now - remoteCueSetMs_;

    switch (cue) {
        case RemoteCue::Ready:
            // "Bereit, jetzt trinken": ruhiger gruener Puls, nie ganz aus -
            // eine dunkle Phase koennte im Partylicht als "Ampel aus" und
            // damit als Startsignal missverstanden werden.
            fillAll({SUCCESS_R, SUCCESS_G, SUCCESS_B}, 0.35f + 0.65f * breathe(elapsed, 1100));
            break;

        case RemoteCue::Away:
            renderAway(now, remoteCueGame_);
            break;

        case RemoteCue::ResultPerfect: {
            // Volltreffer: heller gruener Grund, darauf ein schneller Umlauf.
            fillAll({SUCCESS_R, SUCCESS_G, SUCCESS_B}, 0.45f);
            float head = (elapsed / 45.0f);
            for (uint8_t tail = 0; tail < 4; ++tail) {
                setPixelBlended(head - tail, {255, 255, 255}, 1.0f - tail * 0.25f);
            }
            break;
        }

        case RemoteCue::ResultClose: {
            // Nah dran: zwei lange gelbe Blitze, danach ruhig - gleiche
            // Anzahl "Signale" wie das Blinkmuster der Status-LED.
            bool on = (elapsed % 550) < 350 && elapsed < 1650;
            if (on) {
                fillAll({WARNING_R, WARNING_G, WARNING_B});
            } else {
                fillAll({WARNING_R, WARNING_G, WARNING_B}, 0.08f);
            }
            break;
        }

        case RemoteCue::ResultMiss:
            // Daneben: ein einziger roter Abfall, gedimmt - deutlich, aber
            // ohne den Spieler auszulachen.
            fillAll({DANGER_R, DANGER_G, DANGER_B}, 0.55f * (1.0f - clamp01(elapsed / 1600.0f)));
            break;

        default:
            clear();
            break;
    }
}

/**
 * Away ("Glas ist weg") je Spiel - dieselbe Bildidee wie die
 * renderAway*()-Animationen auf dem TFT, auf einen Ring uebersetzt. Laeuft
 * als Endlosschleife, bis Ergebnis oder Idle kommt.
 */
void LedRing::renderAway(uint32_t now, GameKind game) {
    uint32_t elapsed = now - remoteCueSetMs_;
    Rgb color = gameColor(game);
    clear();

    switch (game) {
        case GameKind::Golf: {
            // Ball rollt seine Bahn - ein weisser Punkt mit Schweif auf
            // gruenem Fairway-Untergrund.
            fillAll(color, 0.12f);
            float head = elapsed / 90.0f;
            for (uint8_t tail = 0; tail < 5; ++tail) {
                setPixelBlended(head - tail, {255, 255, 255}, 1.0f - tail * 0.2f);
            }
            break;
        }

        case GameKind::Dart: {
            // Pfeil fliegt aufs Ziel: der Punkt laeuft immer schneller auf
            // die 12-Uhr-Position zu und schlaegt dort als Blitz ein.
            constexpr uint32_t FLIGHT_MS = 900;
            uint32_t phase = elapsed % (FLIGHT_MS + 300);
            if (phase < FLIGHT_MS) {
                float t = phase / static_cast<float>(FLIGHT_MS);
                float eased = t * t; // beschleunigt, wie ein geworfener Pfeil
                setPixelBlended(COUNT * (1.0f - eased) * 0.5f, color);
            } else {
                fillAll(color, 0.6f);
                setPixel(0, {255, 255, 255});
            }
            break;
        }

        case GameKind::Blackjack: {
            // Karte fuer Karte: der Ring fuellt sich in Vierteln, danach
            // wieder von vorn - das Austeilen als Rhythmus statt als Bild.
            uint8_t cards = static_cast<uint8_t>((elapsed / 600) % 5);
            fillArc(cards / 4.0f, color);
            break;
        }

        case GameKind::Tower: {
            // Wackelturm: der Balken waechst, kippt kurz (flackert) und
            // faellt wieder in sich zusammen.
            constexpr uint32_t CYCLE_MS = 2600;
            uint32_t phase = elapsed % CYCLE_MS;
            if (phase < 1800) {
                fillArc(phase / 1800.0f, color);
            } else {
                float wobble = 0.5f + 0.5f * sinf((phase - 1800) / 40.0f);
                fillArc(1.0f - (phase - 1800) / 800.0f, color, 0.3f + 0.7f * wobble);
            }
            break;
        }

        case GameKind::Boxen: {
            // Schlag: kurzer heller Aufprall auf 12 Uhr, danach ausrollen.
            constexpr uint32_t CYCLE_MS = 1200;
            uint32_t phase = elapsed % CYCLE_MS;
            if (phase < 120) {
                fillAll({255, 255, 255});
            } else {
                float fade = 1.0f - (phase - 120) / static_cast<float>(CYCLE_MS - 120);
                fillAll(color, fade * 0.8f);
            }
            break;
        }

        default: {
            // Generisch (auch GameKind::Scale): ruhiger Komet - "es laeuft
            // gerade etwas, wir warten".
            float head = elapsed / 120.0f;
            for (uint8_t tail = 0; tail < 6; ++tail) {
                setPixelBlended(head - tail, color, 1.0f - tail * 0.16f);
            }
            break;
        }
    }
}

// --- Wiegen -----------------------------------------------------------------

/**
 * Der Ring als Fuellstandsanzeige: je mehr auf der Waage steht, desto weiter
 * laeuft der Balken herum (Vollausschlag bei LED_RING_WEIGH_FULL_SCALE_G).
 * Das ist das eine Muster, das es weder auf dem TFT noch auf der Status-LED
 * gibt - beim Einschenken schaut man auf das Glas, nicht auf ein Display,
 * und ein wachsender Lichtbogen im Deckel liegt genau im Blickfeld.
 *
 * Gibt false zurueck, wenn (fast) nichts drauf steht - dann bleibt der Ring
 * fuer die darunterliegenden Zustaende (Spieler/Leerlauf) frei, statt eine
 * dauerhaft leere Anzeige zu belegen.
 */
bool LedRing::renderWeighing(uint32_t now) {
    if (weightG_ < LED_RING_WEIGH_MIN_G) return false;

    float fraction = weightG_ / LED_RING_WEIGH_FULL_SCALE_G;
    Rgb color = hasActivePlayer_ ? activePlayerColor_ : gameColor(activeGame_);

    if (fraction >= 1.0f) {
        // Ueber Vollausschlag: kompletter Ring, langsam pulsierend - der
        // Balken kann nicht weiter wachsen, die Bewegung zeigt trotzdem an,
        // dass die Waage lebt.
        fillAll(color, 0.55f + 0.45f * breathe(now - stateStartMs_, 1500));
        return true;
    }

    clear();
    fillArc(fraction, color);
    return true;
}

// --- Ruhezustaende ----------------------------------------------------------

void LedRing::renderActivePlayer(uint32_t now) {
    // Spielerfarbe, langsam atmend: "du bist dran" soll praesent sein, ohne
    // ueber einen ganzen Zug hinweg zu nerven.
    fillAll(activePlayerColor_, 0.25f + 0.45f * breathe(now - stateStartMs_, 2600));
}

void LedRing::renderWaitingForApp(uint32_t now) {
    // Einzelner blauer Punkt kreist - dieselbe Aussage wie der einsame Blitz
    // der Status-LED ("Waage laeuft, aber niemand ist verbunden").
    clear();
    setPixelBlended((now - stateStartMs_) / 160.0f, {INFO_R, INFO_G, INFO_B});
}

void LedRing::renderIdle(uint32_t now) {
    // Sehr schwaches Atmen im Akzentton. Bewusst gedaempft: der Leerlauf ist
    // der mit Abstand haeufigste Zustand, hier entscheidet sich, wie viel
    // Akku der Ring ueber einen Abend kostet.
    fillAll({ACCENT_R, ACCENT_G, ACCENT_B}, 0.06f + 0.10f * breathe(now - stateStartMs_, 4000));
}
