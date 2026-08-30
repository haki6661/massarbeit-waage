#include "Scale.h"

Scale::Scale(uint8_t dataPin, uint8_t clockPin, float calibrationFactor)
    : dataPin(dataPin), clockPin(clockPin), calibrationFactor(calibrationFactor),
      currentWeight(0.0f) {}

bool Scale::begin() {
    Serial.println("[Scale] Starte HX711-Initialisierung...");

    preferences.begin("scale", false);
    calibrationFactor = preferences.getFloat("calib", calibrationFactor);
    preferences.end();
    Serial.printf("[Scale] Kalibrierfaktor geladen: %.6f\n", calibrationFactor);

    hx711.begin(dataPin, clockPin);
    hx711.set_scale(calibrationFactor);

    Serial.println("[Scale] Teste HX711-Verbindung...");
    unsigned long startTime = millis();
    bool testPassed = false;

    while (millis() - startTime < 3000) {
        if (hx711.is_ready()) {
            long testReading = hx711.read();
            if (testReading != 0) { // HX711 liefert 0, wenn nicht angeschlossen
                testPassed = true;
                Serial.printf("[Scale] Testmesswert: %ld\n", testReading);
                break;
            }
        }
        delay(100);
    }

    if (!testPassed) {
        Serial.println("[Scale] FEHLER: HX711 antwortet nicht!");
        Serial.println("[Scale] Pruefen: VCC, GND, DT->GPIO" + String(dataPin) +
                        ", SCK->GPIO" + String(clockPin) + ", Waegezellen-Anschluss.");
        isConnected = false;
        return false;
    }

    isConnected = true;
    Serial.println("[Scale] HX711 verbunden. Tariere...");
    hx711.tare();
    lastAutoZeroMs = millis();
    Serial.println("[Scale] Bereit.");
    return true;
}

void Scale::tare(uint8_t times) {
    if (!isConnected) {
        Serial.println("[Scale] Tare uebersprungen: HX711 nicht verbunden.");
        return;
    }

    Serial.println("[Scale] Tariere...");
    hx711.tare(times);

    currentFilterState = STABLE;
    lastActivity = 0;
    currentWeight = 0.0f;
    samplesInitialized = false;
    lastAutoZeroMs = millis(); // frische Tara zaehlt schon als "gerade nachgezogen"
    Serial.println("[Scale] Tara fertig.");
}

void Scale::set_scale(float factor) {
    if (calibrationFactor != factor) {
        calibrationFactor = factor;
        hx711.set_scale(calibrationFactor);
        saveCalibration();
    }
}

void Scale::saveCalibration() {
    preferences.begin("scale", false);
    preferences.putFloat("calib", calibrationFactor);
    preferences.end();
    Serial.printf("[Scale] Kalibrierfaktor gespeichert: %.6f\n", calibrationFactor);
}

float Scale::getWeight() {
    if (!isConnected) {
        return 0.0f;
    }

    static unsigned long lastReadTime = 0;
    unsigned long now = millis();

    // Max. 50Hz - schnell genug, ohne den HX711 (max. ~80Hz intern) zu ueberfordern.
    if (now - lastReadTime < 20) {
        return currentWeight;
    }
    lastReadTime = now;

    if (!hx711.is_ready()) {
        return currentWeight;
    }

    float rawReading = hx711.get_units(1);
    if (isnan(rawReading)) {
        return currentWeight;
    }
    lastRawReading = rawReading;

    if (!samplesInitialized) {
        initializeSamples(rawReading);
        currentWeight = rawReading;
        currentFilterState = STABLE;
        return currentWeight;
    }

    readings[readingIndex] = rawReading;
    readingIndex = (readingIndex + 1) % MAX_SAMPLES;

    float change = fabsf(rawReading - currentWeight);

    switch (currentFilterState) {
        case STABLE:
            if (change > ACTIVITY_THRESHOLD) {
                currentFilterState = BREWING;
                lastActivity = now;
            }
            break;
        case BREWING:
            if (change > ACTIVITY_THRESHOLD) {
                lastActivity = now;
            } else if (now - lastActivity > STABILITY_TIMEOUT) {
                currentFilterState = TRANSITIONING;
            }
            break;
        case TRANSITIONING:
            if (change > ACTIVITY_THRESHOLD) {
                currentFilterState = BREWING;
                lastActivity = now;
            } else if (now - lastActivity > STABILITY_TIMEOUT * 2) {
                currentFilterState = STABLE;
            }
            break;
    }

    float filtered = (currentFilterState == BREWING)
                          ? medianFilter(MEDIAN_SAMPLES)
                          : averageFilter(AVERAGE_SAMPLES);

    // Grosse Spruenge (>5g) sofort uebernehmen statt wegzufiltern - z.B. wenn
    // jemand schlagartig ein Glas draufstellt.
    if (change > 5.0f) {
        filtered = rawReading;
        initializeSamples(rawReading);
        if (currentFilterState == STABLE) {
            currentFilterState = BREWING;
            lastActivity = now;
        }
    }

    currentWeight = filtered;

    // Auto-Zero-Nachfuehrung (siehe Scale.h) - nur wenn wirklich ruhig UND
    // nahe Null, sonst wuerde ein leichtes, absichtlich aufgestelltes Objekt
    // faelschlich weggetart. hx711.tare() statt Scale::tare(): wenige Samples,
    // kein Reset des Smart-Filter-Zustands/Log-Rauschens noetig - reine
    // Drift-Korrektur im Hintergrund.
    if (currentFilterState == STABLE && fabsf(currentWeight) < AUTO_ZERO_BAND_G &&
        now - lastAutoZeroMs > AUTO_ZERO_INTERVAL_MS) {
        hx711.tare(5);
        currentWeight = 0.0f;
        initializeSamples(0.0f);
        lastAutoZeroMs = now;
    }

    return currentWeight;
}

float Scale::getCurrentWeight() {
    return currentWeight;
}

long Scale::getRawValue() {
    if (!isConnected) {
        return 0;
    }
    // Mittel aus 10 Messungen statt nur einer einzelnen - der Rohwert
    // schwankt im Ruhezustand um ca. +/-100-150 Counts (normales HX711-
    // Rauschen), ein Einzelwert kann bei der Kalibrierroutine leicht ein
    // Ausreisser sein und einen deutlich falschen (z.B. viel zu kleinen
    // oder falsch vorzeichenbehafteten) Kalibrierfaktor erzeugen.
    return hx711.get_value(10);
}

void Scale::initializeSamples(float initialValue) {
    for (int i = 0; i < MAX_SAMPLES; i++) {
        readings[i] = initialValue;
    }
    samplesInitialized = true;
}

float Scale::medianFilter(int samples) {
    if (samples > MAX_SAMPLES) samples = MAX_SAMPLES;

    float temp[MAX_SAMPLES];
    for (int i = 0; i < samples; i++) {
        int idx = (readingIndex - 1 - i + MAX_SAMPLES) % MAX_SAMPLES;
        temp[i] = readings[idx];
    }

    for (int i = 0; i < samples - 1; i++) {
        for (int j = 0; j < samples - i - 1; j++) {
            if (temp[j] > temp[j + 1]) {
                float swap = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = swap;
            }
        }
    }
    return temp[samples / 2];
}

float Scale::averageFilter(int samples) {
    if (samples > MAX_SAMPLES) samples = MAX_SAMPLES;

    float sum = 0;
    for (int i = 0; i < samples; i++) {
        int idx = (readingIndex - 1 - i + MAX_SAMPLES) % MAX_SAMPLES;
        sum += readings[idx];
    }
    return sum / samples;
}
