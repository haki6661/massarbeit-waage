#include "CalibrationRoutine.h"

CalibrationRoutine::CalibrationRoutine(Scale& scale, DeviceUi& ui)
    : scale_(scale), ui_(ui) {}

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

void CalibrationRoutine::waitForAnyInput() {
    uint32_t lastPromptMs = millis();
    while (!Serial.available()) {
        if (millis() - lastPromptMs > 15000) {
            Serial.println("[Kalibrierung] Warte auf Eingabe im Serial Monitor (einfach Enter druecken reicht)...");
            lastPromptMs = millis();
        }
        delay(10);
    }
    // Puffer leeren, damit keine Reste (z.B. ein einzelnes \n) die naechste
    // readSerialLine()-Zeile verfaelschen.
    delay(20); // kurz warten, falls noch mehr Bytes unterwegs sind (z.B. \r\n als zwei Events)
    while (Serial.available()) Serial.read();
}

void CalibrationRoutine::run() {
    Serial.println();
    Serial.println("==================================================");
    Serial.println(" Kalibrierung gestartet");
    Serial.println("==================================================");

    ui_.showMessage("Kalibrierung", "Waage leeren,\ndann Enter im\nSerial Monitor.");
    Serial.println("[Kalibrierung] Waage leeren und Enter druecken...");
    waitForAnyInput();

    scale_.tare();
    Serial.println("[Kalibrierung] Tariert.");

    ui_.showMessage("Kalibrierung", "Bekanntes Gewicht\nauflegen. Gramm\neingeben + Enter.");
    Serial.println("[Kalibrierung] Bekanntes Gewicht auflegen, Gramm eingeben (z.B. 500) und Enter druecken.");
    String input = readSerialLine();
    float knownWeightGrams = input.toFloat();

    if (knownWeightGrams <= 0.0f) {
        Serial.printf("[Kalibrierung] FEHLER: Ungueltiges Gewicht '%s'. Abgebrochen.\n", input.c_str());
        ui_.showMessage("Kalibrierung", "Fehler:\nungueltiges Gewicht.\nAbgebrochen.");
        delay(2000);
        return;
    }

    long raw = scale_.getRawValue();
    if (raw == 0) {
        Serial.println("[Kalibrierung] FEHLER: Rohwert ist 0 - HX711 verbunden?");
        ui_.showMessage("Kalibrierung", "Fehler: Rohwert 0.\nHX711 pruefen.");
        delay(2000);
        return;
    }

    float newFactor = static_cast<float>(raw) / knownWeightGrams;

    // Plausibilitaetspruefung: ein negativer oder betragsmaessig winziger
    // Faktor bedeutet praktisch immer, dass beim Ablesen kein (oder das
    // falsche) Gewicht auf der Waage stand - z.B. wenn der Referenz-
    // gegenstand noch nicht ganz aufgesetzt/eingeschwungen war. So ein
    // Faktor wuerde die Waage duetlich schlechter machen als der bisherige
    // (z.B. Gewichte mit falschem Vorzeichen anzeigen) - lieber abbrechen
    // und zum erneuten Versuch auffordern, statt ihn zu speichern.
    if (newFactor <= 1.0f) {
        Serial.printf("[Kalibrierung] FEHLER: Unplausibler Faktor %.6f (Rohwert=%ld) - "
                      "stand das Referenzgewicht wirklich schon stabil auf der Waage? "
                      "Abgebrochen, alter Faktor bleibt erhalten.\n", newFactor, raw);
        ui_.showMessage("Kalibrierung", "Fehler: Wert\nunplausibel.\nBitte erneut\nversuchen.");
        delay(2500);
        return;
    }

    scale_.set_scale(newFactor); // speichert automatisch ins NVS

    Serial.printf("[Kalibrierung] Fertig. Rohwert=%ld, Referenz=%.2fg, neuer Faktor=%.6f\n",
                  raw, knownWeightGrams, newFactor);

    char body[64];
    snprintf(body, sizeof(body), "Fertig!\nFaktor: %.4f", newFactor);
    ui_.showMessage("Kalibrierung", body);
    delay(3000);
}
