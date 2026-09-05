#pragma once

// WS2812B-RGB-LED-Ring (5V, adressierbar) im Deckel - VORBEREITET, aber in
// beiden Geraetevarianten standardmaessig AUSGESCHALTET.
// Siehe ROADMAP.md, "WS2812B-RGB-LED-Ring im Deckel fuer visuelle Cues".
//
// ---------------------------------------------------------------------------
// Was hier schon fertig ist und was noch fehlt
// ---------------------------------------------------------------------------
// Fertig: die komplette Lichtlogik - Zustandsauswahl, Prioritaeten, alle
// Muster (Ampel/Formel 1, Away je Spiel, Ergebnis-Gueten, Wiege-Balken,
// Spielerfarbe, Verbindungszustand), Helligkeitsdeckel und Gamma.
// Fehlt zum Scharfschalten (drei Handgriffe, siehe README, Abschnitt
// "LED-Ring nachruesten"):
//   1. Ring anloeten, Datenleitung an Pins::LED_RING_DATA (Board-Profil).
//   2. `MASSARBEIT_HAS_LED_RING` im Board-Profil auf 1 und
//      `MASSARBEIT_LED_RING_COUNT` auf die tatsaechliche LED-Zahl setzen.
//   3. In platformio.ini die auskommentierte NeoPixel-Abhaengigkeit
//      aktivieren.
//
// ---------------------------------------------------------------------------
// Warum das Modul trotzdem immer mitkompiliert wird
// ---------------------------------------------------------------------------
// `ENABLED` ist ein `static constexpr bool` aus dem Board-Profil, kein
// #if um die halbe Datei. Jede oeffentliche Methode steigt als Erstes ueber
// `if (!ENABLED) return;` aus - der Compiler wirft den gesamten Rumpf danach
// als toten Code weg, das ausgeschaltete Binary waechst also praktisch nicht.
// Gleichzeitig wird die Lichtlogik bei JEDEM Build durchkompiliert und kann
// nicht unbemerkt verrotten, waehrend sie in der Schublade liegt. Nur der
// Zugriff auf die NeoPixel-Bibliothek selbst haengt an einem echten #if -
// die Abhaengigkeit existiert im ausgeschalteten Build ja gar nicht.
//
// ---------------------------------------------------------------------------
// Verhaeltnis zu TftDisplay/LedStatusUi
// ---------------------------------------------------------------------------
// Der Ring ist KEIN drittes DeviceUi-Backend, sondern laeuft parallel zur
// vorhandenen Anzeige mit (die offene Frage aus der Roadmap ist damit zu
// Gunsten der Erweiterung entschieden: auf der Vision ergaenzt der Ring das
// TFT, auf der Basis die einfarbige Status-LED - beide Geraete behalten
// ihre bisherige Anzeige unveraendert). TftDisplay und LedStatusUi kennen
// ihn nur ueber attachLedRing() und reichen Cue/Spielerwechsel weiter; das
// BLE-Protokoll aendert sich dadurch kein Byte.

#include <Arduino.h>

#include "BoardConfig.h"
#include "Config.h"
#include "DeviceUiTypes.h"

class LedRing {
public:
    // true, sobald das Board-Profil den Ring freigibt - alles andere in
    // dieser Klasse haengt daran und faellt sonst beim Kompilieren weg.
    static constexpr bool ENABLED = (MASSARBEIT_HAS_LED_RING != 0);
    static constexpr uint16_t COUNT = MASSARBEIT_LED_RING_COUNT;

    // Einmalig in setup(). Faerbt den Ring nicht ein, sondern loescht ihn nur -
    // die Bootanimation gehoert der jeweiligen Hauptanzeige (Sprite bzw.
    // atmende Status-LED), der Ring steigt erst mit der ersten update()
    // aus loop() ein.
    void begin();

    // In loop() aufrufen, gleiche Signatur wie DeviceUi::update(). Rechnet
    // intern hoechstens alle LED_RING_FRAME_INTERVAL_MS ein neues Bild.
    void update(bool hx711Connected, bool bleConnected);

    // Aktuelles Gewicht fuer den Wiege-Balken (siehe renderWeighing()).
    // Ohne diesen Aufruf zeigt der Ring einfach kein Gewicht an, alles
    // andere funktioniert unveraendert.
    void setWeight(float grams);

    // Von TftDisplay/LedStatusUi weitergereicht (attachLedRing()) - der Ring
    // sieht damit genau dieselben Zustaende wie die Hauptanzeige.
    void setRemoteCue(RemoteCue cue, GameKind game = GameKind::None);
    void setActivePlayer(GameKind game, uint16_t color565);
    void clearActivePlayer();

    // --- Startampel (Formel 1) --------------------------------------------
    // Fuenf rote Lampen gehen nacheinander an, bleiben `holdMs` stehen und
    // erloeschen dann gemeinsam ("lights out") - genau die Ampel, die die
    // App fuer das Zeitspiel "Formel 1" auf dem Bildschirm zeigt, nur rundum
    // sichtbar. Wer den Gruen-Zeitpunkt bestimmt, ist bewusst offen gelassen:
    //   - App entscheidet:  startRaceLights(0) und spaeter raceLightsGreen()
    //   - Waage entscheidet: startRaceLights(<ausgeloste Haltezeit>)
    // Beides ist hier moeglich, ohne dass sich der Rest aendert (siehe
    // ROADMAP.md, "Formel 1 auf dem Geraet nachziehen" - dort haengt noch
    // die Protokollhaelfte, die diese Methoden von der App aus ausloest).
    void startRaceLights(uint32_t holdMs = 0);
    void raceLightsGreen();
    // Fehlstart/Abbruch: kurzes rotes Warnblinken, danach zurueck in den
    // normalen Zustand.
    void abortRaceLights();

    // Vor dem Deep Sleep: Ring dunkel schalten. Ohne das leuchtet ein
    // WS2812B mit dem zuletzt gesetzten Bild einfach weiter - der Chip
    // schlaeft, die LEDs halten ihr Latch.
    void prepareForSleep();

private:
    struct Rgb {
        uint8_t r, g, b;
    };

    // Ampel-Ablauf. Idle = keine Ampel aktiv, der Ring zeigt seinen
    // normalen Zustand.
    enum class RacePhase : uint8_t {
        Idle,
        LampsOn,   // Lampen gehen nacheinander an
        Hold,      // alle fuenf rot, Wartezeit bis Gruen
        Go,        // "lights out" + gruener Umlauf
        FalseStart // Abbruch/Fehlstart
    };

    void renderFrame(uint32_t now, bool hx711Connected, bool bleConnected);

    // Einzelne Bilder. Jedes schreibt vollstaendig in buffer_, keins liest
    // den vorherigen Inhalt - so kann die Reihenfolge in renderFrame() ohne
    // Nebenwirkungen umsortiert werden.
    void renderRaceLights(uint32_t now);
    void renderHx711Error(uint32_t now);
    void renderCue(uint32_t now, RemoteCue cue);
    void renderAway(uint32_t now, GameKind game);
    bool renderWeighing(uint32_t now); // false, wenn gerade nichts drauf steht
    void renderActivePlayer(uint32_t now);
    void renderWaitingForApp(uint32_t now);
    void renderIdle(uint32_t now);

    // Bausteine, aus denen die Bilder zusammengesetzt sind.
    void clear();
    void fillAll(Rgb color, float scale = 1.0f);
    void setPixel(int16_t index, Rgb color, float scale = 1.0f);
    // Weicher Punkt an einer gebrochenen Position (0..COUNT), verteilt seine
    // Helligkeit auf die zwei Nachbar-LEDs - ohne das wirkt jede Bewegung
    // auf einem Ring mit 12-24 LEDs ruckartig.
    void setPixelBlended(float position, Rgb color, float scale = 1.0f);
    // Fuellt `fraction` (0..1) des Rings ab Position 0 im Uhrzeigersinn,
    // die angebrochene LED anteilig gedimmt - Grundlage des Wiege-Balkens
    // und der Turm-Away-Animation.
    void fillArc(float fraction, Rgb color, float scale = 1.0f);

    void show();

    // 0..1 -> weicher Auf-/Abblendverlauf ohne Knick an den Umkehrpunkten
    // (dasselbe Kosinus-Fenster wie LedStatusUi::applySignal()).
    static float breathe(uint32_t elapsedMs, uint16_t periodMs);
    static Rgb scaled(Rgb color, float factor);
    static Rgb fromColor565(uint16_t color565);
    static Rgb gameColor(GameKind game);

    bool ready_ = false;
    Rgb buffer_[COUNT > 0 ? COUNT : 1] = {};
    uint32_t lastFrameMs_ = 0;
    const char* lastStateName_ = nullptr;
    uint32_t stateStartMs_ = 0;

    RemoteCue remoteCue_ = RemoteCue::None;
    GameKind remoteCueGame_ = GameKind::None;
    uint32_t remoteCueSetMs_ = 0;

    bool hasActivePlayer_ = false;
    GameKind activeGame_ = GameKind::None;
    Rgb activePlayerColor_ = {255, 255, 255};

    float weightG_ = 0.0f;

    RacePhase racePhase_ = RacePhase::Idle;
    uint32_t racePhaseStartMs_ = 0;
    uint32_t raceHoldMs_ = 0;
};
