#include "Battery.h"
#include "BoardConfig.h"
#include <esp_adc_cal.h>
#include <math.h>

namespace {
    esp_adc_cal_characteristics_t adcChars;
}

void Battery::begin() {
#if MASSARBEIT_HAS_BATTERY
    // Nutzt die werksseitige ADC-Kalibrierkurve des Chips statt einer
    // linearen Naeherung (Vref-Default 1100mV, wie im offiziellen Beispiel).
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adcChars);
#endif
}

float Battery::readVoltage() {
#if !MASSARBEIT_HAS_BATTERY
    return 0.0f; // Board ohne Akkumessung - siehe Board-Profil
#else
    uint32_t raw = analogRead(Pins::BATTERY_ADC);
    // Spannungsteiler vor dem ADC (beide Boards: Faktor 2), siehe
    // MASSARBEIT_BATTERY_DIVIDER im Board-Profil.
    uint32_t millivolts = esp_adc_cal_raw_to_voltage(raw, &adcChars) * MASSARBEIT_BATTERY_DIVIDER;

    if (millivolts > 4300) {
        // Kein Akku angeschlossen / USB gesteckt -> ADC sieht nur die
        // Ladespannung des Ladereglers, kein verlaesslicher Zellwert.
        return 0.0f;
    }
    if (millivolts < 2500) {
        // Unterhalb der Entladeschlussspannung einer 1S-Zelle (~3.0V) mit
        // Sicherheitsabstand: entweder haengt kein Akku dran, oder das Board
        // hat den erwarteten Spannungsteiler gar nicht bestueckt und der
        // ADC-Pin haengt in der Luft. Beides ist "kein verlaesslicher Wert" -
        // lieber gar nichts melden (-> 0xFF, App blendet die Anzeige aus) als
        // eine erfundene Zahl. Ein wirklich so leerer Akku haette den ESP32
        // ohnehin laengst abgeschaltet.
        return 0.0f;
    }
    return millivolts / 1000.0f;
#endif
}

int8_t Battery::readPercent() {
    float voltage = readVoltage();
    if (voltage <= 0.1f) return -1;

    // Stueckweise-lineare Naeherung der 1S-LiPo-Entladekurve: die Spannung
    // faellt am Anfang und Ende der Entladung deutlich staerker als in der
    // Mitte, eine reine lineare 3.0-4.2V-Rechnung waere ueber weite Strecken
    // sichtbar falsch (z.B. "50%" schon bei noch fast vollem Akku).
    struct Point { float volts; uint8_t percent; };
    static const Point CURVE[] = {
        {3.45f, 0}, {3.68f, 10}, {3.74f, 20}, {3.77f, 30}, {3.79f, 40},
        {3.82f, 50}, {3.87f, 60}, {3.92f, 70}, {3.98f, 80}, {4.06f, 90}, {4.20f, 100},
    };
    constexpr size_t n = sizeof(CURVE) / sizeof(CURVE[0]);

    if (voltage <= CURVE[0].volts) return 0;
    if (voltage >= CURVE[n - 1].volts) return 100;

    for (size_t i = 1; i < n; i++) {
        if (voltage <= CURVE[i].volts) {
            float span = CURVE[i].volts - CURVE[i - 1].volts;
            float t = (voltage - CURVE[i - 1].volts) / span;
            return (int8_t)roundf(CURVE[i - 1].percent + t * (CURVE[i].percent - CURVE[i - 1].percent));
        }
    }
    return 100; // unerreichbar, s.o. Bounds-Checks
}
