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

    // doReset=false: die Default-Implementierung der Library ruft sonst intern
    // reset() -> read() auf, was blockierend auf eine LOW-Flanke an DOUT
    // wartet. Ohne angeschlossenen HX711 (DOUT haengt per Pullup dauerhaft
    // HIGH) haengt sich das Geraet damit VOR dem eigentlichen, zeitlich
    // begrenzten Verbindungstest weiter unten fuer immer auf - kein Crash,
    // kein Watchdog-Reset (yield() in der Library-Schleife fuettert ihn
    // brav weiter), einfach stiller Stillstand. Reset/Tara macht
    // Scale::begin() ohnehin selbst (siehe unten), sobald der Test erfolgreich war.
    hx711.begin(dataPin, clockPin, false, false);
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
        // Nicht aufgeben: getWeight() sucht ihn alle RECONNECT_INTERVAL_MS
        // neu - wer das Kabel nachtraeglich einsteckt, muss nicht neu starten.
        lastReconnectAttemptMs = millis();
        return false;
    }

    isConnected = true;
    Serial.println("[Scale] HX711 verbunden. Tariere...");
    hx711.tare();
    // Stempel bewusst NACH der Tara: die dauert bei 10 SPS rund zwei Sekunden
    // und liegt damit ueber HX711_LOST_TIMEOUT_MS (siehe noteHx711Responded()).
    noteHx711Responded();
    lastAutoZeroMs = millis();
    Serial.println("[Scale] Bereit.");
    return true;
}

void Scale::powerDown() {
    // Bewusst direkt ueber den Takt-Pin statt ueber die Library: das ist
    // exakt der im HX711-Datenblatt beschriebene Weg (PD_SCK laenger als
    // 60µs HIGH halten -> Chip geht in den Power-Down) und haengt an keiner
    // Library-Version. Der Pin ist seit hx711.begin() ohnehin OUTPUT.
    //
    // Erst LOW, dann HIGH: faengt den Fall ab, dass der Takt gerade mitten in
    // einer Uebertragung schon HIGH steht - der Chip zaehlt die HIGH-Dauer ab
    // der Flanke, ein bereits stehendes HIGH wuerde also nicht sicher als
    // Power-Down-Kommando gelten.
    digitalWrite(clockPin, LOW);
    delayMicroseconds(2);
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(80); // Datenblatt: >60µs
    Serial.println("[Scale] HX711 in den Standby geschickt.");
}

void Scale::tare(uint8_t times) {
    if (!isConnected) {
        Serial.println("[Scale] Tare uebersprungen: HX711 nicht verbunden.");
        return;
    }

    Serial.println("[Scale] Tariere...");
    hx711.tare(times);
    noteHx711Responded();

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

void Scale::markHx711Lost(const char* reason) {
    isConnected = false;
    loadCellMissing = false;
    saturatedSinceMs = 0;
    zeroReadings = 0;
    currentWeight = 0.0f;
    samplesInitialized = false;
    lastReconnectAttemptMs = millis();
    Serial.printf("[Scale] FEHLER: HX711 verloren (%s) - Verkabelung pruefen.\n", reason);
}

void Scale::tryReconnect(unsigned long now) {
    if (now - lastReconnectAttemptMs < RECONNECT_INTERVAL_MS) return;
    lastReconnectAttemptMs = now;
    // Nur nach is_ready(): read() der Library wartet sonst blockierend auf
    // DOUT = LOW - ohne HX711 fuer immer (siehe Kommentar in begin()).
    if (!hx711.is_ready()) return;
    if (static_cast<long>(hx711.read()) == 0) return;

    Serial.println("[Scale] HX711 wieder da - tariere neu.");
    isConnected = true;
    lastReadyMs = now;
    zeroReadings = 0;
    tare(10);
}

float Scale::getWeight() {
    unsigned long now = millis();
    if (!isConnected) {
        tryReconnect(now);
        return 0.0f;
    }

    static unsigned long lastReadTime = 0;

    // Max. 50Hz - schnell genug, ohne den HX711 (max. ~80Hz intern) zu ueberfordern.
    if (now - lastReadTime < 20) {
        return currentWeight;
    }
    lastReadTime = now;

    if (!hx711.is_ready()) {
        // Kommt gar nichts mehr, ist der HX711 weg (Stecker, Versorgung) -
        // bisher blieb in dem Fall einfach das letzte Gewicht stehen.
        if (now - lastReadyMs > HX711_LOST_TIMEOUT_MS) markHx711Lost("keine Messwerte mehr");
        return currentWeight;
    }
    lastReadyMs = now;

    // Rohwert selbst lesen statt get_units(1): nur so laesst sich vor dem
    // Umrechnen pruefen, ob er ueberhaupt plausibel ist. Die Umrechnung
    // unten ist exakt die der Library (get_units = (roh - Offset) / Faktor).
    long raw = static_cast<long>(hx711.read());

    if (raw == 0) {
        if (++zeroReadings >= ZERO_READINGS_LOST) markHx711Lost("nur noch Nullen");
        return currentWeight;
    }
    zeroReadings = 0;

    if (raw >= SATURATION_RAW || raw <= -SATURATION_RAW) {
        if (saturatedSinceMs == 0) saturatedSinceMs = now;
        if (!loadCellMissing && now - saturatedSinceMs >= LOAD_CELL_MISSING_AFTER_MS) {
            loadCellMissing = true;
            currentWeight = 0.0f;
            samplesInitialized = false;
            Serial.println("[Scale] FEHLER: Waegezelle liefert nur Anschlagwerte - nicht angeschlossen?");
        }
        // Ein Anschlagwert ist nie ein echtes Gewicht - gar nicht erst in den
        // Filter, sonst gehen fuer die Dauer bis zur Erkennung absurde
        // Kilogramm-Werte an die App.
        return currentWeight;
    }
    saturatedSinceMs = 0;

    if (loadCellMissing) {
        loadCellMissing = false;
        Serial.println("[Scale] Waegezelle wieder da - tariere neu.");
        // Der bisherige Nullpunkt kann aus der Zeit ohne Zelle stammen (z.B.
        // Tara beim Booten auf Anschlagwerten) und waere dann weit daneben.
        tare(10);
        return currentWeight;
    }

    float rawReading = (raw - hx711.get_offset()) / hx711.get_scale();
    if (isnan(rawReading) || isinf(rawReading)) {
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
    bool nearZero = fabsf(currentWeight) < AUTO_ZERO_BAND_G;
    if (currentFilterState == STABLE && nearZero &&
        now - lastAutoZeroMs > AUTO_ZERO_INTERVAL_MS) {
        hx711.tare(5);
        noteHx711Responded();
        currentWeight = 0.0f;
        initializeSamples(0.0f);
        lastAutoZeroMs = now;
    }

    // Schock-Nullpunkt-Korrektur (siehe Scale.h) - greift unabhaengig vom
    // Intervall oben, sobald sich ein eindeutig unmoeglicher NEGATIVER Wert
    // eingependelt hat. Eigener, kurzer Ruhe-Check statt auf den vollen
    // (langsamen) Smart-Filter-STABLE-Zustand zu warten - lastActivity wird
    // von der Zustandsmaschine oben ohnehin schon bei jeder >4g-Aenderung
    // aktualisiert, reicht direkt als Absicherung gegen das mechanische
    // Nachschwingen.
    if (!nearZero && currentWeight < NEGATIVE_DRIFT_THRESHOLD_G && now - lastActivity > NEGATIVE_DRIFT_SETTLE_MS) {
        hx711.tare(5);
        noteHx711Responded();
        currentWeight = 0.0f;
        initializeSamples(0.0f);
        lastAutoZeroMs = now;
        Serial.println("[Scale] Negativer Nullpunkt-Versatz erkannt, sofort korrigiert.");
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
    long raw = hx711.get_value(10);
    // Zehn Messungen sind bei 10 SPS knapp eine Sekunde, und die App fragt
    // waehrend der Eichung mehrmals pro Sekunde an - ohne diesen Stempel
    // verhungert getWeight() dauerhaft (siehe noteHx711Responded()).
    noteHx711Responded();
    return raw;
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
