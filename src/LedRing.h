#pragma once

// WS2812B-Lichtleiste bzw. -ring (5V, adressierbar).
// Siehe ROADMAP.md, "WS2812B-RGB-LED-Ring im Deckel bestuecken und
// scharfschalten".
//
// ---------------------------------------------------------------------------
// Wo das gerade laeuft
// ---------------------------------------------------------------------------
// Basis  (t_oi_plus.h):    AKTIV - gerade 8er-Leiste, direkt an 5V/GND.
// Vision (t_display_s3.h): noch AUS - dort ist ein Ring im Deckel geplant.
//
// Freigegeben wird das je Board-Profil ueber MASSARBEIT_HAS_LED_RING; dazu
// gehoeren MASSARBEIT_LED_RING_COUNT (LED-Zahl), _IS_STRIP (Geometrie) und
// _HAS_POWER_SWITCH (Schalt-MOSFET in der 5V-Zuleitung, siehe README).
//
// ---------------------------------------------------------------------------
// Warum das Modul auch ausgeschaltet mitkompiliert wird
// ---------------------------------------------------------------------------
// `ENABLED` ist ein `static constexpr bool` aus dem Board-Profil, kein
// #if um die halbe Datei. Jede oeffentliche Methode steigt als Erstes ueber
// `if (!ENABLED) return;` aus - der Compiler wirft den gesamten Rumpf danach
// als toten Code weg, das ausgeschaltete Binary waechst also praktisch nicht.
// Gleichzeitig wird die Lichtlogik bei JEDEM Build durchkompiliert und kann
// in der Variante, die sie gerade nicht nutzt, nicht unbemerkt verrotten.
// Nur der Zugriff auf die NeoPixel-Bibliothek selbst haengt an einem echten
// #if - im ausgeschalteten Build soll sie gar nicht erst eingebunden werden.
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

    // Gerade Leiste statt geschlossenem Ring (Board-Profil). Aendert das
    // Verhalten jeder Bewegung: auf dem Ring laeuft ein Punkt im Kreis, auf
    // der Leiste pendelt er hin und her. Ein Punkt, der am Ende der Leiste
    // verschwindet und vorne wieder auftaucht, sieht nach Fehler aus, nicht
    // nach Animation.
    static constexpr bool IS_STRIP = (MASSARBEIT_LED_RING_IS_STRIP != 0);

    // Schweiflaenge bewegter Muster, an die LED-Zahl gekoppelt: auf einer
    // 8er-Leiste wuerde ein fester 5-Pixel-Schweif praktisch alles ausleuchten
    // und die Bewegung unsichtbar machen.
    static constexpr uint8_t TAIL = (COUNT >= 16) ? 4 : (COUNT >= 10 ? 3 : 2);

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
    // Rote Lampen gehen nacheinander an, bleiben `holdMs` stehen und
    // schalten dann gemeinsam auf Gruen - genau die Ampel, die die App fuer
    // das Zeitspiel "Formel 1" auf dem Bildschirm zeigt (Formel1Lights.tsx),
    // im selben Takt und mit derselben Lampenzahl, nur an der Waage selbst.
    // Vier Lampen auf acht LEDs: jede Lampe ist genau zwei LEDs breit.
    //
    // `lampIntervalMs`/`lampCount` kommen aus der App (LIGHT_INTERVAL_MS/
    // LIGHT_COUNT), 0 = Vorgabe aus Config.h. So gibt es fuer den Takt nur
    // eine Wahrheit - laeuft er auseinander, geht die Waage zu einem anderen
    // Zeitpunkt auf Gruen als der Bildschirm.
    //
    // Ausgeloest wird das per BLE (0x30/0x31/0x32, siehe Config.h) aus der
    // App. Wer den Gruen-Zeitpunkt bestimmt, bleibt dabei offen und wird vom
    // Kommando entschieden:
    //   - Waage entscheidet: startRaceLights(<ausgeloste Haltezeit>) - sie
    //     zaehlt selbst herunter und geht ohne weiteres Kommando auf Gruen.
    //   - App entscheidet:   startRaceLights(0), spaeter raceLightsGreen().
    // Heute schickt die App die ausgeloste Haltezeit mit (erster Fall).
    // Auf dem TFT der Vision fehlt die Ampel noch (siehe ROADMAP.md,
    // "Formel 1 auf der Vision (TFT) nachziehen").
    void startRaceLights(uint32_t holdMs = 0, uint16_t lampIntervalMs = 0, uint8_t lampCount = 0);
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
        Red,       // Lampen gehen nacheinander an, danach alle rot bis Gruen
        Go,        // alle Lampen gruen, bis das Glas abgehoben ist
        FalseStart // Abbruch/Fehlstart
    };

    void renderFrame(uint32_t now, bool hx711Connected, bool bleConnected);

    // Einzelne Bilder. Jedes schreibt vollstaendig in buffer_, keins liest
    // den vorherigen Inhalt - so kann die Reihenfolge in renderFrame() ohne
    // Nebenwirkungen umsortiert werden.
    void renderRaceLights(uint32_t now);
    void drawRaceLamp(uint8_t lamp, Rgb color, float scale = 1.0f);
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
    // die angebrochene LED anteilig gedimmt - Grundlage des Wiege-Balkens.
    void fillArc(float fraction, Rgb color, float scale = 1.0f);
    // Ein `length` LEDs breiter Block ab der gebrochenen Position `start`,
    // weich auf die Nachbarn verteilt (setPixelBlended() je LED) - fuer
    // Dinge, die als Ganzes wandern: Karte, Turmblock, Handschuh.
    void drawBlock(float start, uint16_t length, Rgb color, float scale = 1.0f);

    void show();

    // 0..1 -> weicher Auf-/Abblendverlauf ohne Knick an den Umkehrpunkten
    // (dasselbe Kosinus-Fenster wie LedStatusUi::applySignal()).
    static float breathe(uint32_t elapsedMs, uint16_t periodMs);

    // Laufposition eines bewegten Punktes, ein kompletter Durchlauf je
    // `periodMs`. Ring: laeuft rundum weiter. Leiste: pendelt in derselben
    // Zeit einmal hin und zurueck. Bewusst zeit- statt pixelbasiert, damit
    // dieselbe Animation auf 8 und auf 24 LEDs gleich schnell wirkt.
    float travel(uint32_t elapsedMs, uint32_t periodMs) const;
    static Rgb scaled(Rgb color, float factor);
    static Rgb fromColor565(uint16_t color565);
    static Rgb gameColor(GameKind game);
    // Farbrad: 0..1 -> voll gesaettigte Farbe (rot -> gelb -> gruen ->
    // cyan -> blau -> magenta -> rot), fuer den Regenbogen im Leerlauf.
    static Rgb hueToRgb(float hue);

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
    uint16_t raceLampIntervalMs_ = LED_RING_RACE_LAMP_INTERVAL_MS;
    uint8_t raceLampCount_ = LED_RING_RACE_LAMP_COUNT;
};
