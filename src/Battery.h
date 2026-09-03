#pragma once

#include <Arduino.h>

// Batteriespannung ueber den werksseitig kalibrierten ADC-Pfad des ESP32
// lesen. Formel 1:1 aus LilyGOs offiziellem Beispiel uebernommen
// (Xinyuan-LilyGO/T-Display-S3, examples/GetBatteryVoltage) - nicht die
// naive analogRead()*3.3/4095-Rechnung, die deutlich ungenauer waere.
//
// Gilt unveraendert fuer beide Geraetevarianten: ADC-Pin und Teilerfaktor
// kommen aus dem Board-Profil (Pins::BATTERY_ADC,
// MASSARBEIT_BATTERY_DIVIDER), der Rest ist identisch.
class Battery {
public:
    void begin();

    // Spannung in Volt, oder 0.0f wenn USB-C gesteckt ist (dann liegt am
    // ADC nur die TP4056-Ladespannung an, keine verlaessliche Zellspannung -
    // siehe offizielles Beispiel/README).
    float readVoltage();

    // Grobe Prozent-Schaetzung aus der Zellspannung (stueckweise-lineare
    // Naeherung der typischen 1S-LiPo-Entladekurve, siehe Battery.cpp) -
    // fuer ein Party-Gadget voellig ausreichend, aber keine Praezisions-
    // messung (dafuer muesste der Ladestrom bekannt sein, nicht nur die
    // Ruhespannung). -1, wenn kein verlaesslicher Wert vorliegt (USB
    // gesteckt, siehe readVoltage()) - wird 1:1 als 0xFF ueber BLE gesendet,
    // siehe BleWeightService.
    int8_t readPercent();

private:
    // Gleitender Mittelwert ueber aufeinanderfolgende readVoltage()-Aufrufe
    // (zusaetzlich zum Oversampling INNERHALB eines Aufrufs, siehe .cpp) -
    // daempft Rest-Rauschen, das sonst am Rand einer Kurven-Stufe in
    // Battery::readPercent() sichtbar hin- und herspringt (z.B. 79%/81%
    // abwechselnd trotz stabilem Akku). Wird zurueckgesetzt, sobald ein
    // ungueltiger Messwert auftritt (siehe .cpp), damit ein spaeter wieder
    // gueltiger Wert nicht erst langsam "eingeschwungen" werden muss.
    float filteredVoltage_ = 0.0f;
    bool hasFilteredVoltage_ = false;
};
