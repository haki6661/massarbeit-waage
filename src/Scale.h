#pragma once

// Portiert aus WeighMyBru2 (https://github.com/031devstudios/weighmybru2,
// include/Scale.h + src/Scale.cpp) - HX711-Handling, Smart-Filter und
// NVS-Kalibrierung sind board-unabhaengig und wurden fast 1:1 uebernommen.
// Entfernt wurde nur die FlowRate-Kopplung (Espresso-Flussraten-Berechnung
// pausieren waehrend des Tarierens) - fuer Maßarbeit nicht relevant.

#include <HX711.h>
#include <Preferences.h>

class Scale {
public:
    Scale(uint8_t dataPin, uint8_t clockPin, float calibrationFactor);

    bool begin(); // false, wenn der HX711 nicht antwortet
    void tare(uint8_t times = 20);
    void set_scale(float factor); // setzt + speichert den Kalibrierfaktor (NVS)

    float getWeight();        // gefiltertes Gewicht, max. 1x/20ms neu berechnet
    float getCurrentWeight(); // letzter gefilterter Wert, ohne neue Messung
    long  getRawValue();      // unkalibrierter Rohwert (Mittel aus 10 Messungen) - fuer die Kalibrierroutine

    // Letzter unkalibrierter, ungefilterter Messwert aus getWeight() (in
    // "Gramm" bei aktuellem Kalibrierfaktor) - fuer die Debug-Anzeige, um
    // ohne extra HX711-Zugriff zu sehen, dass sich ueberhaupt was tut.
    float getLastRawReading() const { return lastRawReading; }

    float getCalibrationFactor() const { return calibrationFactor; }
    bool  isHX711Connected() const { return isConnected; }

private:
    HX711 hx711;
    Preferences preferences;
    uint8_t dataPin;
    uint8_t clockPin;
    float calibrationFactor;
    float currentWeight;
    float lastRawReading = 0.0f;
    bool isConnected = false;

    void saveCalibration();

    // Smart-Filter: waehrend "Brewing"-artiger Aktivitaet (schnelle Aenderung)
    // Median, sonst gleitender Durchschnitt - reduziert Rauschen ohne bei
    // schnellen Gewichtsaenderungen hinterherzuhinken. Fuer Maßarbeit reicht
    // das genauso gut wie fuers Espresso-Brauen.
    static const int MAX_SAMPLES = 10;
    float readings[MAX_SAMPLES] = {};
    int readingIndex = 0;
    bool samplesInitialized = false;

    enum FilterState { STABLE, BREWING, TRANSITIONING };
    FilterState currentFilterState = STABLE;
    unsigned long lastActivity = 0;

    // War 0.2g - lag deutlich UNTER dem normalen Einzelsample-Rauschen von
    // HX711+Waegezelle (per Live-Test ca. +/-1-3g, siehe Commit-Historie).
    // Ergebnis: der Filter blieb dauerhaft im BREWING-Zustand (ganz normales
    // Rauschen wurde staendig als "Aktivitaet" gewertet) und erreichte nie
    // STABLE - wodurch auch die Auto-Zero-Nachfuehrung (an STABLE gekoppelt,
    // s.u.) faktisch nie griff und sich Drift ueber mehrere Minuten
    // unbemerkt aufaddieren konnte. Jetzt bewusst ueber dem beobachteten
    // Rauschband, aber weit unter jedem echten Glas/Objekt (>>5g, siehe
    // "grosse Spruenge sofort uebernehmen" unten).
    static constexpr float ACTIVITY_THRESHOLD = 4.0f;      // g Aenderung, die als "Aktivitaet" zaehlt
    static constexpr unsigned long STABILITY_TIMEOUT = 2000; // ms Ruhe, bis wieder als stabil gilt
    static const int MEDIAN_SAMPLES = 3;
    static const int AVERAGE_SAMPLES = 2;

    // Auto-Zero-Nachfuehrung: HX711/Waegezelle driften nach dem Einschalten
    // spuerbar (thermisches Einschwingen ueber mehrere Minuten, typisch fuer
    // guenstige Zellen) - ohne Gegenmassnahme wandert die Anzeige bei leerer
    // Waage langsam von 0g weg, auch ganz ohne Aktivitaet. Loesung wie bei
    // echten Kuechen-/Briefwaagen ueblich: liegt der gefilterte Wert laenger
    // als AUTO_ZERO_INTERVAL_MS ruhig innerhalb eines kleinen Bands um Null,
    // automatisch nachtarieren (siehe getWeight()) - faengt die Drift
    // periodisch ab, ohne ein absichtlich aufgestelltes Glas zu beeinflussen
    // (die sind alle deutlich schwerer als das Band).
    // Band bewusst grosszuegig (aber weit unter jedem echten Glas/Becher,
    // siehe MockWeightSource im App-Repo: 220-400g) UND Intervall bewusst
    // kurz - sonst kann die Drift aus dem Band herauslaufen, bevor
    // ueberhaupt einmal nachgetart wurde, und faengt sich danach nie wieder
    // ein (die Pruefung unten greift nur INNERHALB des Bands).
    static constexpr float AUTO_ZERO_BAND_G = 10.0f;
    static constexpr unsigned long AUTO_ZERO_INTERVAL_MS = 8000;
    unsigned long lastAutoZeroMs = 0;

    void initializeSamples(float initialValue);
    float medianFilter(int samples);
    float averageFilter(int samples);
};
