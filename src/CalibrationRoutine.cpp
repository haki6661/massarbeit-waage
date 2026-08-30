#include "CalibrationRoutine.h"

CalibrationRoutine::CalibrationRoutine(Scale& scale, TftDisplay& display)
    : scale_(scale), display_(display) {}

String CalibrationRoutine::readSerialLine() {
    String line;
    uint32_t lastPromptMs = millis();

    while (true) {
        while (Serial.available()) {
            char c = static_cast<char>(Serial.read());
            if (c == '\n' || c == '\r') {
                if (line.length() > 0) {
                    return line;
                }
                // fuehrende Leerzeilen ignorieren
            } else {
                line += c;
            }
        }

        // Alle 15s dezent erinnern, damit man nicht raetselt, ob das Geraet
        // haengt.
        if (millis() - lastPromptMs > 15000) {
            Serial.println("[Kalibrierung] Warte auf Eingabe im Serial Monitor...");
            lastPromptMs = millis();
        }
        delay(10);
    }
}

void CalibrationRoutine::run() {
    Serial.println();
    Serial.println("==================================================");
    Serial.println(" Kalibrierung gestartet");
    Serial.println("==================================================");

    display_.showMessage("Kalibrierung", "Waage leeren,\ndann Enter im\nSerial Monitor.");
    Serial.println("[Kalibrierung] Waage leeren und Enter druecken...");
    readSerialLine();

    scale_.tare();
    Serial.println("[Kalibrierung] Tariert.");

    display_.showMessage("Kalibrierung", "Bekanntes Gewicht\nauflegen. Gramm\neingeben + Enter.");
    Serial.println("[Kalibrierung] Bekanntes Gewicht auflegen, Gramm eingeben (z.B. 500) und Enter druecken.");
    String input = readSerialLine();
    float knownWeightGrams = input.toFloat();

    if (knownWeightGrams <= 0.0f) {
        Serial.printf("[Kalibrierung] FEHLER: Ungueltiges Gewicht '%s'. Abgebrochen.\n", input.c_str());
        display_.showMessage("Kalibrierung", "Fehler:\nungueltiges Gewicht.\nAbgebrochen.");
        delay(2000);
        return;
    }

    long raw = scale_.getRawValue();
    if (raw == 0) {
        Serial.println("[Kalibrierung] FEHLER: Rohwert ist 0 - HX711 verbunden?");
        display_.showMessage("Kalibrierung", "Fehler: Rohwert 0.\nHX711 pruefen.");
        delay(2000);
        return;
    }

    float newFactor = static_cast<float>(raw) / knownWeightGrams;
    scale_.set_scale(newFactor); // speichert automatisch ins NVS

    Serial.printf("[Kalibrierung] Fertig. Rohwert=%ld, Referenz=%.2fg, neuer Faktor=%.6f\n",
                  raw, knownWeightGrams, newFactor);

    char body[64];
    snprintf(body, sizeof(body), "Fertig!\nFaktor: %.4f", newFactor);
    display_.showMessage("Kalibrierung", body);
    delay(3000);
}
