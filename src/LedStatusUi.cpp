#include "LedStatusUi.h"

#include <math.h>

#include "BoardConfig.h"

namespace {

// LEDC-Kanal fuer die Status-LED. Sonst nutzt im Projekt niemand LEDC, also
// reicht der erste Kanal; 5kHz sind weit oberhalb der Flimmergrenze, 8 Bit
// Aufloesung genug fuer weiche Atem-Verlaeufe.
constexpr uint8_t LED_PWM_CHANNEL = 0;
constexpr uint32_t LED_PWM_FREQ_HZ = 5000;
constexpr uint8_t LED_PWM_RESOLUTION_BITS = 8;

// Cue-Timeouts exakt wie auf dem TFT (TftDisplay::update()): "Bereit"/"Away"
// duerfen lange stehen (der Spieler braucht Zeit), Ergebnis-Rueckmeldungen
// loesen sich von selbst auf, falls die App das Zuruecksetzen vergisst oder
// die Verbindung mittendrin abbricht.
constexpr uint32_t CUE_TIMEOUT_LONG_MS = 20000;
constexpr uint32_t CUE_TIMEOUT_RESULT_MS = 4000;

const char* gameName(GameKind game) {
    switch (game) {
        case GameKind::Golf:      return "Golf";
        case GameKind::Dart:      return "Dart";
        case GameKind::Blackjack: return "Blackjack";
        case GameKind::Tower:     return "Wackelturm";
        case GameKind::Scale:     return "Scale";
        case GameKind::Boxen:     return "Boxen";
        default:                  return "-";
    }
}

} // namespace

void LedStatusUi::begin() {
#if MASSARBEIT_HAS_STATUS_LED
    ledcSetup(LED_PWM_CHANNEL, LED_PWM_FREQ_HZ, LED_PWM_RESOLUTION_BITS);
    ledcAttachPin(Pins::STATUS_LED, LED_PWM_CHANNEL);
    ready_ = true;
    setLevel(0);
#endif
    Serial.println("[UI] Status-LED statt Display (Variante Basis).");
    signalStartMs_ = millis();
}

void LedStatusUi::setLevel(uint8_t level) {
#if MASSARBEIT_HAS_STATUS_LED
    if (!ready_ || lastLevel_ == static_cast<int16_t>(level)) return;
    lastLevel_ = static_cast<int16_t>(level);
    // Wahrgenommene Helligkeit waechst nicht linear mit dem Tastverhaeltnis -
    // ohne diese Quadrierung wirkt ein Atem-Verlauf, als bliebe die LED die
    // meiste Zeit einfach hell.
    uint32_t duty = (static_cast<uint32_t>(level) * level) / 255u;
#if MASSARBEIT_STATUS_LED_ACTIVE_LOW
    duty = 255u - duty;
#endif
    ledcWrite(LED_PWM_CHANNEL, duty);
#else
    (void)level;
#endif
}

void LedStatusUi::prepareForSleep() {
    setLevel(0);
#if MASSARBEIT_HAS_STATUS_LED
    // LEDC laeuft im Deep Sleep nicht weiter; den Pin trotzdem definiert
    // hinterlassen, damit die LED nicht je nach Beschaltung schwach
    // weiterglimmt.
    lastLevel_ = -1;
    ledcDetachPin(Pins::STATUS_LED);
    pinMode(Pins::STATUS_LED, OUTPUT);
    digitalWrite(Pins::STATUS_LED, MASSARBEIT_STATUS_LED_ACTIVE_LOW ? HIGH : LOW);
    ready_ = false;
#endif
}

/**
 * Zustand -> Signal. Die LED ist einfarbig, also traegt allein das Zeitmuster
 * die Information - die Muster sind deshalb bewusst grob verschieden gewaehlt
 * (Anzahl Blitze, Tempo, Atmen vs. Blinken vs. Dauerlicht) und nicht nur um
 * ein paar Millisekunden versetzt:
 *
 *   HX711-Fehler       3 schnelle Blitze, dann Pause   (hoechste Prioritaet)
 *   Bereit (0x11)      schnelles, gleichmaessiges Blinken
 *   Away (0x13)        langsames Atmen
 *   Volltreffer (0x12) 2s Dauerlicht
 *   Nah dran (0x12)    2 lange Blitze
 *   Daneben (0x12)     ein langer, gedimmter Blitz
 *   Spieler am Zug     schwaches Dauerlicht
 *   keine App-Verbindung  ein kurzer Blitz alle 2s
 *   verbunden, Leerlauf   Doppel-"Herzschlag" alle 3s
 */
LedStatusUi::Signal LedStatusUi::currentSignal(bool hx711Connected, bool bleConnected) {
    if (!hx711Connected) {
        return {Pattern::Blink, 255, 3, 60, 120, 900, 0, "HX711-Fehler"};
    }

    if (remoteCue_ != RemoteCue::None) {
        bool isLongLived = (remoteCue_ == RemoteCue::Ready || remoteCue_ == RemoteCue::Away);
        uint32_t timeoutMs = isLongLived ? CUE_TIMEOUT_LONG_MS : CUE_TIMEOUT_RESULT_MS;
        if (millis() - remoteCueSetMs_ > timeoutMs) {
            remoteCue_ = RemoteCue::None;
        } else {
            switch (remoteCue_) {
                case RemoteCue::Ready:
                    return {Pattern::Blink, 255, 1, 150, 0, 150, 0, "Bereit"};
                case RemoteCue::Away:
                    return {Pattern::Breathe, 255, 0, 0, 0, 0, 1400, "Away"};
                case RemoteCue::ResultPerfect:
                    return {Pattern::Solid, 255, 0, 0, 0, 0, 0, "Volltreffer"};
                case RemoteCue::ResultClose:
                    return {Pattern::Blink, 200, 2, 350, 200, 300, 0, "Nah dran"};
                case RemoteCue::ResultMiss:
                    return {Pattern::Blink, 70, 1, 900, 0, 400, 0, "Daneben"};
                default:
                    break;
            }
        }
    }

    if (hasActivePlayer_) {
        return {Pattern::Solid, 45, 0, 0, 0, 0, 0, "Spieler am Zug"};
    }

    if (!bleConnected) {
        return {Pattern::Blink, 255, 1, 90, 0, 1900, 0, "wartet auf App"};
    }

    return {Pattern::Blink, 160, 2, 70, 180, 2700, 0, "verbunden"};
}

void LedStatusUi::update(bool hx711Connected, bool bleConnected) {
    uint32_t now = millis();
    Signal signal = currentSignal(hx711Connected, bleConnected);

    if (signal.name != lastSignalName_) {
        lastSignalName_ = signal.name;
        // Muster immer am Zyklusanfang starten lassen, sonst wuerde ein
        // Zustandswechsel mitten in einer Pause als "LED tot" wirken.
        signalStartMs_ = now;
        Serial.printf("[UI] %s\n", signal.name);
    }

    applySignal(signal, now);
}

void LedStatusUi::applySignal(const Signal& signal, uint32_t now) {
    uint32_t elapsed = now - signalStartMs_;

    switch (signal.pattern) {
        case Pattern::Off:
            setLevel(0);
            break;

        case Pattern::Solid:
            setLevel(signal.level);
            break;

        case Pattern::Blink: {
            uint32_t slotMs = static_cast<uint32_t>(signal.onMs) + signal.offMs;
            uint32_t cycleMs = signal.pulses * slotMs + signal.gapMs;
            if (cycleMs == 0) {
                setLevel(signal.level);
                break;
            }
            uint32_t pos = elapsed % cycleMs;
            bool inPulseTrain = pos < signal.pulses * slotMs;
            bool on = inPulseTrain && (pos % slotMs) < signal.onMs;
            setLevel(on ? signal.level : 0);
            break;
        }

        case Pattern::Breathe: {
            uint16_t period = signal.periodMs ? signal.periodMs : 1000;
            float phase = (elapsed % period) / static_cast<float>(period);
            // Kosinus statt Dreieck: kein sichtbarer Knick an den Umkehrpunkten.
            float envelope = 0.5f * (1.0f - cosf(2.0f * PI * phase));
            setLevel(static_cast<uint8_t>(signal.level * envelope));
            break;
        }
    }
}

void LedStatusUi::showMessage(const String& title, const String& body) {
    Serial.printf("[UI] %s\n", title.c_str());
    if (body.length() > 0) {
        // Die TFT-Variante nimmt \n als Zeilenumbruch im Vollbild-Text - auf
        // Serial ergibt das ohne Zutun mehrere Zeilen.
        Serial.println(body);
    }
}

void LedStatusUi::runBootSequence(bool (*stepInit)()) {
#if MASSARBEIT_HAS_STATUS_LED
    // Schnelles Atmen waehrend der Initialisierung. Mindestdauer, damit auch
    // ein sehr schneller Boot als solcher erkennbar ist (das TFT-Pendant
    // spielt aus demselben Grund immer mindestens einen kompletten
    // Sprite-Durchlauf, siehe playBootSprite()).
    constexpr uint16_t BOOT_BREATHE_PERIOD_MS = 700;
    constexpr uint16_t BOOT_MIN_VISIBLE_MS = 700;
#endif

    uint32_t startMs = millis();
    bool initDone = false;

    while (true) {
        if (!initDone) {
            initDone = !stepInit();
        }

        uint32_t elapsed = millis() - startMs;
#if MASSARBEIT_HAS_STATUS_LED
        float phase = (elapsed % BOOT_BREATHE_PERIOD_MS) / static_cast<float>(BOOT_BREATHE_PERIOD_MS);
        setLevel(static_cast<uint8_t>(255.0f * 0.5f * (1.0f - cosf(2.0f * PI * phase))));
        if (initDone && elapsed >= BOOT_MIN_VISIBLE_MS) break;
#else
        if (initDone) break;
#endif
        delay(10);
    }

    setLevel(0);
    signalStartMs_ = millis();
    Serial.printf("[UI] Boot fertig nach %lums.\n", (unsigned long)(millis() - startMs));
}

void LedStatusUi::setRemoteCue(RemoteCue cue, GameKind game) {
    remoteCue_ = cue;
    remoteCueGame_ = game;
    remoteCueSetMs_ = millis();
}

void LedStatusUi::setActivePlayer(GameKind game, uint16_t color565, const String& name) {
    hasActivePlayer_ = true;
    activeGame_ = game;
    activePlayerColor565_ = color565;
    activePlayerName_ = name;
    Serial.printf("[UI] Zug: %s (%s)\n", name.c_str(), gameName(game));
}

void LedStatusUi::clearActivePlayer() {
    hasActivePlayer_ = false;
    activeGame_ = GameKind::None;
    activePlayerName_ = "";
}
