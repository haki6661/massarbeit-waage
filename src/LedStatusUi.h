#pragma once

// Geraete-Anzeige der "Waage Light": dieselbe Methodenoberflaeche wie
// TftDisplay (siehe DeviceUi.h), aber statt eines Bildschirms gibt es genau
// eine einfarbige Onboard-LED plus Serial-Ausgabe.
//
// Die LED kann keine Farben, also traegt das ZEITMUSTER die Information -
// jeder Zustand bekommt ein eigenes, aus zwei Metern Entfernung
// unterscheidbares Signal (siehe Tabelle in LedStatusUi.cpp). Helligkeits-
// verlaeufe ("Atmen") laufen ueber LEDC-PWM, nicht ueber digitalWrite.
//
// Alles ist nicht-blockierend ueber millis() gebaut und wird von update()
// aus der Hauptschleife getaktet - die einzige Ausnahme ist
// runBootSequence(), das (wie sein TFT-Pendant playBootSprite()) bewusst
// blockierend in setup() laeuft und dabei die Initialisierungsschritte
// abarbeitet.

#include <Arduino.h>

#include "DeviceUiTypes.h"

class LedStatusUi {
public:
    void begin();

    // In loop() aufrufen. Bestimmt aus dem aktuellen Zustand das Signal und
    // setzt die LED-Helligkeit fuer diesen Moment. Prioritaet (hoechste
    // zuerst): HX711-Fehler > RemoteCue > Spieler am Zug > BLE-Verbindung.
    void update(bool hx711Connected, bool bleConnected);

    // Auf dem TFT ein Vollbild-Text, hier eine Serial-Zeile - die Light hat
    // keinen Bildschirm, auf dem eine Kalibrier-Aufforderung stehen koennte.
    // Beim Kalibrieren haengt ohnehin ein USB-Kabel dran (siehe
    // CalibrationRoutine.h), die Meldung kommt also da an, wo sie gebraucht
    // wird.
    void showMessage(const String& title, const String& body = "");

    // Startsequenz: laesst die LED schnell atmen und ruft dabei zwischen den
    // Schritten `stepInit()` auf, bis das false zurueckgibt (gleiche
    // Vertragslage wie TftDisplay::playBootSprite()). Blockierend, einmalig
    // in setup().
    void runBootSequence(bool (*stepInit)());

    void setRemoteCue(RemoteCue cue, GameKind game = GameKind::None);

    void setActivePlayer(GameKind game, uint16_t color565, const String& name);
    void clearActivePlayer();

    // Ohne Display gibt es nichts auszuwaehlen - die Geraete-Spielauswahl
    // entfaellt auf der Light ersatzlos. Bewusst als leere Methoden erhalten,
    // damit die Oberflaeche beider Varianten identisch bleibt (main.cpp ruft
    // sie auf der Light gar nicht erst auf, siehe MASSARBEIT_BUTTON_COUNT).
    void pickerNext() {}
    void pickerConfirm() {}

    // LED aus, bevor der Chip in den Deep Sleep geht - sonst kann sie je nach
    // Beschaltung im Schlaf weiterglimmen (und zieht Strom, den der Deep
    // Sleep gerade sparen soll).
    void prepareForSleep();

    // Gleiche Signatur wie beim TFT (BleWeightService ruft das fuer das
    // 0x14-Kommando auf), gleiche RGB565-Packung - hier nur zum Merken der
    // Spielerfarbe, die einfarbige LED kann sie nicht zeigen.
    uint16_t color565FromRgb(uint8_t r, uint8_t g, uint8_t b) const {
        return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }

private:
    // Grundformen, aus denen alle Signale zusammengesetzt sind.
    enum class Pattern : uint8_t {
        Off,
        Solid,    // konstante Helligkeit
        Blink,    // `pulses` Blitze je Zyklus, danach `gapMs` Pause
        Breathe,  // weicher Auf-/Abblendzyklus
    };

    struct Signal {
        Pattern pattern;
        uint8_t level;    // Helligkeit im "an"-Zustand (Solid/Blink/Breathe-Maximum)
        uint8_t pulses;   // Blink: Blitze je Zyklus
        uint16_t onMs;    // Blink: Dauer eines Blitzes
        uint16_t offMs;   // Blink: Pause zwischen zwei Blitzen desselben Zyklus
        uint16_t gapMs;   // Blink: Pause am Zyklusende
        uint16_t periodMs;// Breathe: Dauer eines kompletten Atemzugs
        const char* name; // fuer die Serial-Ausgabe beim Zustandswechsel
    };

    Signal currentSignal(bool hx711Connected, bool bleConnected);
    void applySignal(const Signal& signal, uint32_t now);
    void setLevel(uint8_t level);

    bool ready_ = false;
    // Bewusst int16_t mit -1 als "noch nie gesetzt": 0xFF waere als uint8_t
    // nicht von der gueltigen Vollhelligkeit 255 zu unterscheiden, ein
    // allererstes setLevel(255) wuerde stillschweigend verschluckt.
    int16_t lastLevel_ = -1;
    const char* lastSignalName_ = nullptr;
    uint32_t signalStartMs_ = 0; // Beginn des aktuellen Signals (Bezugspunkt der Muster)

    RemoteCue remoteCue_ = RemoteCue::None;
    GameKind remoteCueGame_ = GameKind::None;
    uint32_t remoteCueSetMs_ = 0;

    bool hasActivePlayer_ = false;
    GameKind activeGame_ = GameKind::None;
    uint16_t activePlayerColor565_ = 0;
    String activePlayerName_;
};
