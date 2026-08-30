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

    static constexpr float ACTIVITY_THRESHOLD = 0.2f;      // g Aenderung, die als "Aktivitaet" zaehlt
    static constexpr unsigned long STABILITY_TIMEOUT = 2000; // ms Ruhe, bis wieder als stabil gilt
    static const int MEDIAN_SAMPLES = 3;
    static const int AVERAGE_SAMPLES = 2;

    void initializeSamples(float initialValue);
    float medianFilter(int samples);
    float averageFilter(int samples);
};
