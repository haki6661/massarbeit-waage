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

// Dieselben Verlaufskurven, die die App-Animationen benutzen (framer-motion
// "easeIn"/"easeOut"/"easeInOut") - sonst stimmt zwar der Takt, aber die
// Bewegung beschleunigt an anderen Stellen als auf dem Bildschirm daneben.
float easeIn(float t) { return t * t; }
float easeOut(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }
float easeInOut(float t) { return 0.5f - 0.5f * cosf(PI * t); }

// Position 0..1 innerhalb eines Zyklus von `cycleMs`.
float cyclePhase(uint32_t elapsedMs, uint32_t cycleMs) {
    return (elapsedMs % cycleMs) / static_cast<float>(cycleMs);
}

} // namespace

float LedRing::breathe(uint32_t elapsedMs, uint16_t periodMs) {
    if (periodMs == 0) return 1.0f;
    float phase = (elapsedMs % periodMs) / static_cast<float>(periodMs);
    return 0.5f * (1.0f - cosf(2.0f * PI * phase));
}

float LedRing::travel(uint32_t elapsedMs, uint32_t periodMs) const {
    if (periodMs == 0 || COUNT == 0) return 0.0f;
    float phase = (elapsedMs % periodMs) / static_cast<float>(periodMs); // 0..1
    if (!IS_STRIP) return phase * COUNT;

    // Leiste: hin und zurueck statt rundum. Ein voller Zyklus ist einmal hin
    // UND zurueck, damit die Bewegung bei gleicher periodMs nicht doppelt so
    // hektisch wirkt wie auf dem Ring.
    float span = (COUNT > 1) ? static_cast<float>(COUNT - 1) : 0.0f;
    float p = phase * 2.0f;
    return (p <= 1.0f) ? p * span : (2.0f - p) * span;
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
// Farbe erkennbar ist - gleiche Hex-Werte wie TftDisplay::gameAccentColor()
// und `game.accent.solid` im App-Repo. Bei einem App-Redesign alle drei
// Stellen zusammen nachziehen (hier stand lange noch die alte Palette:
// Blackjack blau statt gold, Boxen orange statt violett).
LedRing::Rgb LedRing::gameColor(GameKind game) {
    switch (game) {
        case GameKind::Golf:      return {0x4a, 0xde, 0x80}; // #4ade80
        case GameKind::Dart:      return {0xfb, 0x71, 0x85}; // #fb7185
        case GameKind::Blackjack: return {0xe8, 0xc1, 0x4d}; // #e8c14d
        case GameKind::Tower:     return {0xc9, 0x9a, 0x5c}; // #c99a5c
        case GameKind::Scale:     return {0x38, 0xbd, 0xf8}; // #38bdf8
        case GameKind::Boxen:     return {0xa7, 0x8b, 0xfa}; // #a78bfa
        // Formel 1 bekommt den Akzentton. Der ist frei geworden, seit der
        // Wiege-Balken weg ist (er war die einzige Stelle, die ihn benutzte),
        // und er beisst sich mit keiner Statusfarbe: rot bleibt dem Fehler,
        // blau der fehlenden Verbindung, weiss dem "bereit". Vorher hatte
        // Formel 1 gar keinen Eintrag und fiel in den default-Zweig - die
        // Away-Animation lief deshalb im generischen Kometen statt in einem
        // eigenen Muster.
        case GameKind::Formel1:   return {ACCENT_R, ACCENT_G, ACCENT_B}; // #f28a4a
        default:                  return {ACCENT_R, ACCENT_G, ACCENT_B};
    }
}

LedRing::Rgb LedRing::hueToRgb(float hue) {
    float h = (hue - floorf(hue)) * 6.0f; // 0..6, ein Sechstel je Farbuebergang
    int sector = static_cast<int>(h);
    uint8_t rise = static_cast<uint8_t>((h - sector) * 255.0f);
    uint8_t fall = static_cast<uint8_t>(255 - rise);
    switch (sector) {
        case 0:  return {255, rise, 0};
        case 1:  return {fall, 255, 0};
        case 2:  return {0, 255, rise};
        case 3:  return {0, fall, 255};
        case 4:  return {rise, 0, 255};
        default: return {255, 0, fall};
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
    float wrapped = position;
    if (!IS_STRIP) {
        wrapped = fmodf(position, static_cast<float>(COUNT));
        if (wrapped < 0.0f) wrapped += COUNT;
    }
    int16_t low = static_cast<int16_t>(floorf(wrapped));
    float frac = wrapped - low;

    // Additiv statt ueberschreibend: mehrere Punkte duerfen sich ueberlagern
    // (z.B. Komet-Schweif), ohne sich gegenseitig auszuloeschen.
    // Auf der Leiste faellt weg, was ueber den Rand hinausragt (ein Schweif
    // laeuft dort aus dem Bild) - auf dem Ring laeuft es rundum weiter.
    auto add = [this](int16_t index, Rgb c) {
        int32_t i = index;
        if (IS_STRIP) {
            if (i < 0 || i >= static_cast<int32_t>(COUNT)) return;
        } else {
            i %= static_cast<int32_t>(COUNT);
            if (i < 0) i += COUNT;
        }
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

void LedRing::drawBlock(float start, uint16_t length, Rgb color, float scale) {
    for (uint16_t i = 0; i < length; ++i) setPixelBlended(start + i, color, scale);
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
#if MASSARBEIT_LED_RING_HAS_POWER_SWITCH
    // Erst Strom auf den Ring, dann der erste Datenrahmen: die WS2812B
    // brauchen einen Moment, bis ihre interne Logik nach dem Einschalten
    // sauber steht - ein Frame in diese Phase hinein kommt bei der ersten
    // LED gern verstuemmelt an.
    pinMode(Pins::LED_RING_POWER, OUTPUT);
    digitalWrite(Pins::LED_RING_POWER, HIGH);
    delay(2);
#endif

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
#if MASSARBEIT_HAS_LED_RING && MASSARBEIT_LED_RING_HAS_POWER_SWITCH
    // Dunkelschalten allein reicht NICHT: jede WS2812B zieht ~0.6-1mA fuer
    // ihren internen Controller, auch wenn sie schwarz ist. Bei 16 LEDs sind
    // das ~10-16mA rund um die Uhr - mehr als alles andere im Deep Sleep
    // zusammen, und genug, um eine 700mAh-Zelle in unter zwei Tagen zu
    // leeren. Deshalb wird der Ring hier komplett stromlos geschaltet.
    //
    // Reihenfolge beachten: erst die Datenleitung auf LOW, dann die
    // Versorgung weg. Ein auf HIGH stehengebliebener Datenpin wuerde sonst
    // ueber die Eingangs-Schutzdioden der ersten LED in den stromlosen Ring
    // zurueckspeisen - die LEDs glimmen dann schwach weiter und ziehen genau
    // den Strom, den das Abschalten sparen soll.
    digitalWrite(Pins::LED_RING_DATA, LOW);
    digitalWrite(Pins::LED_RING_POWER, LOW);
#endif
    // Ohne Schalt-MOSFET (aktueller Aufbau der Basis) bleiben die LEDs zwar
    // dunkel, ziehen aber weiter ihren Controller-Ruhestrom - dort wird das
    // Geraet ohnehin ueber den Schiebeschalter ganz ausgeschaltet statt
    // schlafen gelegt (die Basis hat keinen Taster, siehe Board-Profil).
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
    // Ein Spieler am Zug heisst zwingend: ein Spiel laeuft. Das hier
    // mitzuziehen macht den Lobby-Zustand unabhaengig davon, in welcher
    // Reihenfolge die App 0x14/0x15/0x16 schickt.
    inLobby_ = false;
    hasActivePlayer_ = true;
    activeGame_ = game;
    activePlayerColor_ = fromColor565(color565);
}

void LedRing::clearActivePlayer() {
    if (!ENABLED) return;
    hasActivePlayer_ = false;
    activeGame_ = GameKind::None;
}

void LedRing::setInLobby(bool inLobby) {
    if (!ENABLED) return;
    inLobby_ = inLobby;
}

void LedRing::startRaceLights(uint32_t holdMs, uint16_t lampIntervalMs, uint8_t lampCount) {
    if (!ENABLED) return;
    racePhase_ = RacePhase::Red;
    racePhaseStartMs_ = millis();
    raceHoldMs_ = holdMs;
    raceLampIntervalMs_ = lampIntervalMs ? lampIntervalMs : LED_RING_RACE_LAMP_INTERVAL_MS;
    raceLampCount_ = lampCount ? lampCount : LED_RING_RACE_LAMP_COUNT;
    remoteCue_ = RemoteCue::None; // Ampel hat waehrend ihres Ablaufs Vorrang
}

void LedRing::raceLightsGreen() {
    if (!ENABLED) return;
    // Auch mitten im Aufleuchten zulaessig: wenn die App frueher Gruen gibt als
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
 *   Startampel (Formel 1)  vier rote Lampen je 2 LEDs -> alle gruen
 *   HX711-Fehler           drei rote Blitze rundum
 *   Bereit (0x11)          gruener Puls
 *   Away (0x13)            spielabhaengige Bewegung (siehe renderAway())
 *   Volltreffer (0x12)     gruener Umlauf auf hellem Grund
 *   Nah dran (0x12)        zwei gelbe Blitze
 *   Daneben (0x12)         ein langsamer roter Abfall
 *   Gewicht auf der Waage  Balken in Spieler-/Spielfarbe, waechst mit dem Gewicht
 *   Spieler am Zug         langsames Atmen in der Spielerfarbe
 *   keine App-Verbindung   einzelner blauer Punkt kreist
 *   verbunden, Leerlauf    Regenbogen-Lauf (kein Spieler am Zug, nichts drauf)
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
        // Der Ring zeigt nur noch Zustaende, keine Ergebnisse: die drei
        // Guete-Cues (Volltreffer/nah dran/daneben) sind hier bewusst raus.
        // Auf acht schwach angesteuerten LEDs waren "gruener Puls" und
        // "gruener Umlauf" aus zwei Metern nicht auseinanderzuhalten, und die
        // Zahl steht ohnehin gross in der App - der Ring hat sie nur
        // wiederholt. Das Protokoll kennt sie weiterhin (die Vision zeigt sie
        // auf dem TFT), die Waage ignoriert sie schlicht.
        if (remoteCue_ == RemoteCue::ResultPerfect || remoteCue_ == RemoteCue::ResultClose ||
            remoteCue_ == RemoteCue::ResultMiss) {
            remoteCue_ = RemoteCue::None;
        }

        // Cue-Timeout: "bereit"/"away" duerfen lange stehen, loesen sich aber
        // von selbst auf, falls die App das Zuruecksetzen vergisst oder die
        // Verbindung mitten im Ritual abbricht.
        if (remoteCue_ != RemoteCue::None && now - remoteCueSetMs_ > CUE_TIMEOUT_LONG_MS) {
            remoteCue_ = RemoteCue::None;
        }

        if (remoteCue_ != RemoteCue::None) {
            renderCue(now, remoteCue_);
            stateName = "Cue";
        } else if (hasActivePlayer_) {
            renderActivePlayer(now);
            stateName = "Spieler am Zug";
        } else if (!bleConnected) {
            renderWaitingForApp(now);
            stateName = "wartet auf App";
        } else if (inLobby_) {
            renderIdle(now);
            stateName = "Lobby";
        } else {
            // Verbunden, ein Spiel laeuft, aber gerade ist niemand am Zug
            // (Boxen zwischen zwei Runden, Formel 1 nach dem letzten Lauf).
            // Bewusst dunkel statt Regenbogen: der Regenbogen ist das Signal
            // "hier laeuft nichts, kommt spielen" und stand bisher mitten im
            // Spiel - siehe Live-Log 10.09., wo er waehrend einer laufenden
            // Golf-Partie ansprang. Dunkel heisst hier "von dir wird gerade
            // nichts erwartet" und braucht kein eigenes Muster.
            clear();
            stateName = "Spielpause";
        }
    }

    if (stateName != lastStateName_) {
        lastStateName_ = stateName;
        stateStartMs_ = now;
        Serial.printf("[Ring] %s\n", stateName);
    }
}

// Eine Startlampe = ein zusammenhaengender Block aus COUNT / Lampenzahl
// LEDs - bei vier Lampen auf der 8er-Leiste genau zwei LEDs je Lampe, auf
// einem 16er-Ring vier. Bewusst auf ganze LEDs statt weich eingeblendet: eine
// Ampel muss man zaehlen koennen ("noch eine, dann geht's los"), und eine
// zwischen zwei LEDs liegende Lampe verschmiert genau diese Information.
// Deshalb ist die Lampenzahl so gewaehlt, dass sie die LED-Zahl glatt teilt
// (siehe LED_RING_RACE_LAMP_COUNT); geht die Teilung doch nicht auf, bleiben
// die uebrigen LEDs dunkel und verteilen sich auf beide Enden.
void LedRing::drawRaceLamp(uint8_t lamp, Rgb color, float scale) {
    if (COUNT == 0 || raceLampCount_ == 0 || lamp >= raceLampCount_) return;
    uint16_t perLamp = COUNT / raceLampCount_;
    if (perLamp == 0) {
        // Mehr Lampen als LEDs: eine LED je Lampe, der Rest faellt weg.
        if (lamp < COUNT) setPixel(lamp, color, scale);
        return;
    }
    uint16_t offset = (COUNT - perLamp * raceLampCount_) / 2;
    // Ab drei LEDs je Lampe bleibt die letzte als dunkle Fuge stehen, damit
    // die Lampen auch dann noch einzeln zu sehen sind, wenn alle leuchten.
    // Bei zwei LEDs je Lampe (8er-Leiste) wuerde die Fuge die Lampe halbieren.
    uint16_t litPerLamp = perLamp >= 3 ? perLamp - 1 : perLamp;
    for (uint16_t i = 0; i < litPerLamp; ++i) {
        setPixel(static_cast<int16_t>(offset + lamp * perLamp + i), color, scale);
    }
}

// --- Startampel (Formel 1) --------------------------------------------------
// Gleicher Ablauf und gleicher Takt wie Formel1Lights.tsx in der App:
//
//   - Lampe n (1..Lampenzahl) geht bei n * Takt an - die erste also NICHT
//     sofort, sondern nach einem Takt (App: floor(elapsed / LIGHT_INTERVAL_MS)).
//   - Ab Lampenzahl * Takt stehen alle rot, dann laeuft die Haltezeit -
//     entweder raceHoldMs_ (Waage lost aus, Standardfall heute) oder
//     unbegrenzt, bis raceLightsGreen() kommt (holdMs = 0).
//   - Gruen = alle Lampen schlagen gleichzeitig auf Gruen um. Die App hat
//     sich bewusst gegen das Formel-1-typische Ausgehen entschieden (ein
//     Farbumschlag ist im Augenwinkel schwerer zu uebersehen, siehe
//     Formel1Lights.tsx) - die Waage macht es genauso, sonst hiesse dasselbe
//     Startsignal auf Bildschirm und Waage zweierlei.
//
// Noch nicht angegangene Lampen glimmen schwach, so wie die App die leeren
// Lampen als dunkle Umrisse zeigt: die Ampel ist ab dem Aufstellen als
// "scharf" zu erkennen, auch bevor die erste Lampe angeht.
void LedRing::renderRaceLights(uint32_t now) {
    constexpr float UNLIT_LAMP_SCALE = 0.3f;
    const Rgb red = {DANGER_R, 0, 0};
    const Rgb green = {0, SUCCESS_G, 0};
    uint32_t elapsed = now - racePhaseStartMs_;

    switch (racePhase_) {
        case RacePhase::Red: {
            uint32_t allRedMs = static_cast<uint32_t>(raceLampCount_) * raceLampIntervalMs_;
            if (raceHoldMs_ > 0 && elapsed >= allRedMs + raceHoldMs_) {
                // Auf den geplanten Gruen-Zeitpunkt setzen, nicht auf `now`
                // (bis zu ein Frame spaeter) - die Mindestdauer von Gruen
                // zaehlt sonst ab dem falschen Moment.
                racePhase_ = RacePhase::Go;
                racePhaseStartMs_ += allRedMs + raceHoldMs_;
                renderRaceLights(now);
                return;
            }
            uint32_t lit = elapsed / raceLampIntervalMs_;
            if (lit > raceLampCount_) lit = raceLampCount_;
            clear();
            for (uint8_t lamp = 0; lamp < raceLampCount_; ++lamp) {
                drawRaceLamp(lamp, red, lamp < lit ? 1.0f : UNLIT_LAMP_SCALE);
            }
            break;
        }

        case RacePhase::Go: {
            // Gruen bleibt stehen, bis das Glas abgehoben ist - in der App
            // verschwinden die gruenen Lampen im selben Moment (Wechsel auf
            // die Stoppuhr). Mindestdauer, damit ein Abheben exakt auf Gruen
            // das Signal nicht auf einen Frame zusammenschrumpfen laesst.
            bool lifted = weightG_ < LED_RING_WEIGH_MIN_G;
            if (elapsed >= LED_RING_RACE_GO_MAX_MS || (lifted && elapsed >= LED_RING_RACE_GO_MIN_MS)) {
                racePhase_ = RacePhase::Idle;
                clear();
                break;
            }
            clear();
            for (uint8_t lamp = 0; lamp < raceLampCount_; ++lamp) drawRaceLamp(lamp, green);
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
            // "Bereit, jetzt trinken": ruhiger Puls, nie ganz aus - eine
            // dunkle Phase koennte im Partylicht als "Ampel aus" und damit als
            // Startsignal missverstanden werden.
            //
            // Weiss statt gruen, und zwar wegen der Spielerfarben: die Palette
            // der App (PLAYER_COLOR_PALETTE) enthaelt ein helles Limette, und
            // ein limettefarben ATMENDER Spieler neben einem gruen PULSIERENDEN
            // "bereit" sind auf acht schwach angesteuerten LEDs dasselbe Bild -
            // ausgerechnet in den zwei Zustaenden, die im Ritual unmittelbar
            // aufeinander folgen. Weiss kann kein Spieler haben.
            fillAll({255, 255, 255}, 0.35f + 0.65f * breathe(elapsed, 1100));
            break;

        case RemoteCue::Away:
            renderAway(now, remoteCueGame_);
            break;

        case RemoteCue::ResultPerfect: {
            // Volltreffer: heller gruener Grund, darauf ein schneller heller
            // Laeufer. Auf der Leiste flitzt er hin und her, auf dem Ring rundum.
            fillAll({SUCCESS_R, SUCCESS_G, SUCCESS_B}, 0.45f);
            float head = travel(elapsed, 420);
            for (uint8_t t = 0; t < TAIL; ++t) {
                setPixelBlended(head - t, {255, 255, 255}, 1.0f - t * (0.8f / TAIL));
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
 * Away ("Glas ist weg") je Spiel - dieselbe Bildidee UND derselbe Takt wie
 * die Away-Momente der App (GolfBallFlight, DartThrow, BlackjackDraw,
 * TowerPull, BoxenAwayMoment): dort bewegt sich alles von links nach rechts
 * und faengt danach von vorn an - hier genauso, LED 0 = links. Die App zeigt
 * ihren Away-Moment im selben Moment, in dem sie das Kommando schickt, beide
 * laufen also bis auf die BLE-Latenz im Gleichtakt.
 *
 * Einheit fuer alles, was als Ganzes wandert (Karte, Turmblock, Sandsack),
 * ist wie bei der Startampel ein Viertel der LEDs - auf der 8er-Leiste zwei.
 * Laeuft als Endlosschleife, bis Ergebnis oder Idle kommt.
 */
void LedRing::renderAway(uint32_t now, GameKind game) {
    uint32_t elapsed = now - remoteCueSetMs_;
    Rgb color = gameColor(game);
    const Rgb white = {255, 255, 255};
    const uint16_t unit = (COUNT >= 4) ? COUNT / 4 : 1;
    const float last = (COUNT > 0) ? static_cast<float>(COUNT - 1) : 0.0f;
    clear();

    switch (game) {
        case GameKind::Golf: {
            // App: der Ball fliegt in 1.4s (easeInOut) von links nach rechts
            // und zieht eine Spur hinter sich her. Hier derselbe Flug als
            // weisser Punkt mit Schweif ueber dem gruenen Fairway.
            fillAll(color, 0.3f);
            float head = easeInOut(cyclePhase(elapsed, 1400)) * last;
            for (uint8_t t = 0; t < TAIL; ++t) {
                setPixelBlended(head - t, white, 1.0f - t * (0.85f / TAIL));
            }
            break;
        }

        case GameKind::Dart: {
            // App: der Pfeil fliegt in 1.1s beschleunigt (easeIn) auf die
            // Scheibe rechts. Hier derselbe Flug in 850ms plus 250ms
            // Einschlag-Blitz am Ende - zusammen wieder 1.1s, damit der
            // Einschlag auf der Waage mit dem Ankommen in der App zusammenfaellt.
            constexpr uint32_t CYCLE_MS = 1100;
            constexpr uint32_t FLIGHT_MS = 850;
            uint32_t phase = elapsed % CYCLE_MS;
            if (phase < FLIGHT_MS) {
                float pos = easeIn(phase / static_cast<float>(FLIGHT_MS)) * last;
                setPixelBlended(pos, color);
                // Kurzer Schweif, damit die Bewegung auch bei wenigen LEDs
                // als Flug und nicht als Springen gelesen wird.
                setPixelBlended(pos - 1.0f, color, 0.35f);
            } else {
                fillAll(color, 0.6f);
                setPixel(static_cast<int16_t>(last), white);
            }
            break;
        }

        case GameKind::Blackjack: {
            // App: alle 1.3s fliegt eine Karte von links heran (easeOut, sie
            // kommt bei 55% an) und dreht sich dann einmal um die eigene
            // Achse. Hier: Karte = eine Einheit, fliegt in denselben 55% auf
            // ihren Platz (die Plaetze fuellen sich von rechts), dreht sich -
            // die Helligkeit folgt dem Drehwinkel, die Rueckseite blitzt weiss
            // - und bleibt liegen. Nach vier Karten ist die Leiste voll, dann
            // wird neu gegeben.
            constexpr uint32_t CARD_MS = 1300;
            constexpr uint8_t CARDS = 4;
            constexpr float ARRIVE = 0.55f;
            uint8_t card = static_cast<uint8_t>((elapsed / CARD_MS) % CARDS);
            float t = cyclePhase(elapsed, CARD_MS);
            auto slotStart = [&](uint8_t slot) { return static_cast<float>(COUNT) - (slot + 1) * unit; };

            for (uint8_t dealt = 0; dealt < card; ++dealt) drawBlock(slotStart(dealt), unit, color);
            float target = slotStart(card);
            if (t < ARRIVE) {
                drawBlock(easeOut(t / ARRIVE) * target, unit, color, 0.7f);
            } else {
                // Wie in der App: 0->180 Grad bis 80%, 180->360 Grad bis 100%.
                float angle = (t < 0.8f) ? PI * (t - ARRIVE) / (0.8f - ARRIVE) : PI + PI * (t - 0.8f) / 0.2f;
                float facing = cosf(angle);
                bool backSide = facing < 0.0f;
                drawBlock(target, unit, backSide ? white : color, fabsf(facing) * (backSide ? 0.6f : 1.0f));
            }
            break;
        }

        case GameKind::Tower: {
            // App (TowerPull, 1.6s): ein Block wird aus dem Turm gezogen (bis
            // 35%), angehoben (bis 65%) und oben wieder aufgelegt (bis 100%),
            // der Turm wackelt dabei. Hier liegt der Turm quer - links unten,
            // rechts oben: drei Einheiten Turm, die oberste Einheit ist frei.
            // Der gezogene Block (zweite Einheit von unten) leuchtet auf,
            // wandert nach oben und bleibt dort liegen; waehrend er unterwegs
            // ist, flackert der Rest-Turm.
            float t = cyclePhase(elapsed, 1600);
            const uint16_t gapFrom = unit;
            const uint16_t gapTo = 2 * unit;
            const float from = static_cast<float>(gapFrom);
            const float to = static_cast<float>(COUNT - unit);

            float towerScale = 0.3f;
            if (t >= 0.35f && t < 0.65f) {
                towerScale *= 0.6f + 0.4f * cosf((t - 0.35f) / 0.3f * 4.0f * PI);
            }
            for (uint16_t i = 0; i + unit < COUNT; ++i) {
                if (i >= gapFrom && i < gapTo) continue; // Platz des gezogenen Blocks
                setPixel(static_cast<int16_t>(i), color, towerScale);
            }

            if (t < 0.35f) {
                drawBlock(from, unit, color, 0.3f + 0.7f * easeInOut(t / 0.35f));
            } else if (t < 0.65f) {
                drawBlock(from + (to - from) * easeInOut((t - 0.35f) / 0.3f), unit, color);
            } else {
                drawBlock(to, unit, color);
            }
            break;
        }

        case GameKind::Boxen: {
            // App (BoxenAwayMoment, 1.3s): der Handschuh schnellt bis 40%
            // nach rechts gegen den Sandsack und zieht sich bis 100% wieder
            // zurueck. Hier: Sandsack = oberste Einheit rechts (schwach
            // weiss), Handschuh = Punkt in Spielfarbe; beim Aufprall blitzt
            // der Sack auf und klingt ab.
            constexpr uint32_t CYCLE_MS = 1300;
            constexpr float HIT = 0.4f;
            constexpr float FLASH_MS = 300.0f;
            float t = cyclePhase(elapsed, CYCLE_MS);
            float bagStart = static_cast<float>(COUNT - unit);
            float reach = bagStart - 1.0f; // Handschuh stoppt direkt vor dem Sack
            float glove = (t < HIT) ? easeInOut(t / HIT) * reach
                                    : (1.0f - easeInOut((t - HIT) / (1.0f - HIT))) * reach;
            float sinceHitMs = (t - HIT) * CYCLE_MS;
            float flash = (t >= HIT && sinceHitMs < FLASH_MS) ? 1.0f - sinceHitMs / FLASH_MS : 0.0f;
            drawBlock(bagStart, unit, white, 0.3f + 0.7f * flash);
            setPixelBlended(glove, color);
            break;
        }

        case GameKind::Formel1: {
            // Der Away-Moment IST hier die gestoppte Zeit: das Glas ist oben,
            // die Uhr laeuft. Also ein Wagen, der die Runde faehrt - und
            // schneller wird, je laenger es dauert. Das erzeugt genau den
            // Druck, den das Spiel will, und ist von jedem anderen Muster
            // sofort zu unterscheiden (nichts sonst beschleunigt).
            //
            // Umlaufdauer faellt linear von LAP_START_MS auf LAP_MIN_MS,
            // erreicht nach RAMP_MS. Integriert wird ueber die Zeit statt
            // travel() zu benutzen: bei veraenderlicher Geschwindigkeit muss
            // die zurueckgelegte Strecke aufaddiert werden, sonst springt der
            // Punkt bei jeder Tempoaenderung zurueck.
            constexpr float LAP_START_MS = 2200.0f;
            constexpr float LAP_MIN_MS = 550.0f;
            constexpr float RAMP_MS = 12000.0f;
            float t = elapsed / RAMP_MS;
            if (t > 1.0f) t = 1.0f;
            // Strecke = Integral von 1/Umlaufdauer. Bei linear fallender
            // Dauer ist das analytisch unschoen - eine kleine Naeherung ueber
            // die mittlere Geschwindigkeit reicht fuer eine Lichtanimation
            // vollkommen und bleibt monoton, also sprungfrei.
            float lapNow = LAP_START_MS + (LAP_MIN_MS - LAP_START_MS) * t;
            float lapAvg = (LAP_START_MS + lapNow) * 0.5f;
            float laps = elapsed / lapAvg;
            float head = (laps - floorf(laps)) * COUNT;

            // Schweif als Bremsspur, und der Ring bleibt schwach durchgezeichnet:
            // eine dunkle Strecke waere im Partylicht kaum als Rundkurs lesbar.
            fillAll(color, 0.12f);
            for (uint8_t i = 0; i < TAIL; ++i) {
                setPixelBlended(head - i, {255, 255, 255}, 1.0f - i * (0.8f / TAIL));
            }
            break;
        }

        default: {
            // Generisch (auch GameKind::Scale): ruhiger Komet - "es laeuft
            // gerade etwas, wir warten".
            float head = travel(elapsed, 2000);
            for (uint8_t t = 0; t < TAIL; ++t) {
                setPixelBlended(head - t, color, 1.0f - t * (0.8f / TAIL));
            }
            break;
        }
    }
}


// --- Ruhezustaende ----------------------------------------------------------

void LedRing::renderActivePlayer(uint32_t now) {
    // Spielerfarbe, langsam atmend: "du bist dran" soll praesent sein, ohne
    // ueber einen ganzen Zug hinweg zu nerven. Untergrenze wie im Leerlauf
    // bewusst ueber Null - eine Anzeige, die zwischendurch ganz ausgeht, liest
    // sich als abgeschaltetes Geraet.
    fillAll(activePlayerColor_, 0.30f + 0.45f * breathe(now - stateStartMs_, 2600));
}

void LedRing::renderWaitingForApp(uint32_t now) {
    // Einzelner blauer Punkt wandert - dieselbe Aussage wie der einsame Blitz
    // der Status-LED ("Waage laeuft, aber niemand ist verbunden"). Bewusst
    // deutlich sichtbar: solange keine App dranhaengt, ist das die einzige
    // Rueckmeldung, dass das Geraet ueberhaupt an ist.
    clear();
    uint32_t elapsed = now - stateStartMs_;
    float head = travel(elapsed, 2400);
    setPixelBlended(head, {INFO_R, INFO_G, INFO_B});
    setPixelBlended(head - 1.0f, {INFO_R, INFO_G, INFO_B}, 0.3f);
}

/**
 * Leerlauf = "die Waage ist an und bereit": App verbunden, HX711 und
 * Waegezelle in Ordnung, niemand am Zug, kein Cue, nichts auf der Waage -
 * in der Praxis die Lobby bzw. der Spielekatalog, bevor ein Spiel laeuft.
 * Seit die Basis keinen Auto-Sleep mehr hat (sie wird am Schalter
 * ausgeschaltet), ist das der am haeufigsten zu sehende Zustand: ohne ihn
 * kann niemand sehen, ob das Geraet laeuft.
 *
 * Deshalb ein Regenbogen-Lauf: das Farbrad liegt einmal ueber die ganze
 * Leiste verteilt und wandert langsam von links nach rechts (dieselbe
 * Richtung wie alle Away-Animationen) - unverwechselbar mit jedem
 * Spielzustand, die alle in einer einzelnen Farbe leuchten, und nie dunkel.
 * Die Helligkeit begrenzt wie immer show() (LED_RING_MAX_BRIGHTNESS).
 */
void LedRing::renderIdle(uint32_t now) {
    constexpr uint32_t RAINBOW_CYCLE_MS = 3000; // ein voller Farbumlauf
    if (COUNT == 0) return;
    float shift = cyclePhase(now - stateStartMs_, RAINBOW_CYCLE_MS);
    for (uint16_t i = 0; i < COUNT; ++i) {
        // Minus: das Farbmuster wandert zu hoeheren Indizes, also nach rechts.
        float hue = i / static_cast<float>(COUNT) - shift;
        hue -= floorf(hue); // 0..1
        buffer_[i] = hueToRgb(hue);
    }
}
