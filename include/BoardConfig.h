#pragma once

// Dispatcher: waehlt anhand des Build-Targets (siehe platformio.ini) das
// passende Board-Profil aus include/boards/ aus. Jede andere Datei bindet
// weiterhin nur "BoardConfig.h" ein und muss nichts ueber Varianten wissen.
//
// Die beiden Geraetevarianten heissen im Code durchgehend "Vision" und
// "Basis" - das sind die Produktnamen "Massarbeit Vision" (mit TFT) und
// "Massarbeit" (ohne Display, nur Status-LED). Die Modellkennungen
// (MASSARBEIT_MODEL_ID) bleiben dagegen an der HARDWARE haengen
// ("t-display-s3", "t-oi-plus"): sie sind der Schluessel, unter dem die App
// im Firmware-Manifest das passende Release sucht, und muessen deshalb
// stabil bleiben, auch wenn sich ein Produktname wieder aendert.
//
// Ein Board-Profil liefert zwei Dinge:
//   1. namespace Pins  - die konkrete GPIO-Belegung
//   2. MASSARBEIT_*    - den Faehigkeits-Steckbrief (Display? Akku? wie viele
//                        Taster? Status-LED?), aus dem sich der restliche
//                        Code per #if zusammensetzt UND der per BLE an die
//                        App gemeldet wird (siehe BLE_DEVICE_INFO_CHAR_UUID
//                        in Config.h).
//
// Warum zwei Binaries statt einer selbsterkennenden Firmware: der S3 ist ein
// Xtensa-LX7-Kern, der C3 der Basis ein RISC-V - schon die Befehlssaetze sind
// verschieden, eine einzelne .bin kann physisch nicht auf beiden starten.
// "Firmware erkennt selbst, worauf sie laeuft" gibt es auf Binary-Ebene also
// gar nicht. Die echte Wahl ist "eine Codebasis, aus der zwei Binaries
// fallen" gegen "zwei getrennte Projekte", und die faellt klar zugunsten
// einer Codebasis aus: Scale/BLE-Protokoll/OTA/Kalibrierung/Deep-Sleep sind
// fuer beide identisch, variantenspezifisch sind nur Pins, Anzeige und
// Tastenanzahl.

#if defined(MASSARBEIT_VARIANT_VISION)
    #include "boards/t_display_s3.h"
#elif defined(MASSARBEIT_VARIANT_BASE)
    #include "boards/t_oi_plus.h"
#else
    #error "Keine Geraetevariante gewaehlt - -DMASSARBEIT_VARIANT_VISION oder -DMASSARBEIT_VARIANT_BASE fehlt (siehe platformio.ini)."
#endif
