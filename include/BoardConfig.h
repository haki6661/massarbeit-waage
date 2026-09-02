#pragma once

// Dispatcher: waehlt anhand des Build-Targets (siehe platformio.ini) das
// passende Board-Profil aus include/boards/ aus. Jede andere Datei bindet
// weiterhin nur "BoardConfig.h" ein und muss nichts ueber Varianten wissen.
//
// Ein Board-Profil liefert zwei Dinge:
//   1. namespace Pins  - die konkrete GPIO-Belegung
//   2. MASSARBEIT_*    - den Faehigkeits-Steckbrief (Display? Akku? wie viele
//                        Taster? Status-LED?), aus dem sich der restliche
//                        Code per #if zusammensetzt UND der per BLE an die
//                        App gemeldet wird (siehe BLE_DEVICE_INFO_CHAR_UUID
//                        in Config.h).
//
// Warum zwei Binaries statt einer selbsterkennenden Firmware: ESP32-S3
// (Xtensa LX7) und der ESP32 der Light (Xtensa LX6) sind zwar beide Xtensa,
// aber verschiedene Chip-Targets mit verschiedenen SDK-Konfigurationen - eine
// einzelne .bin startet nicht auf beiden. Die echte Wahl ist deshalb "eine
// Codebasis, aus der zwei Binaries fallen" gegen "zwei getrennte Projekte",
// und die faellt klar zugunsten einer Codebasis aus: Scale/BLE-Protokoll/
// OTA/Kalibrierung/Deep-Sleep sind fuer beide identisch, variantenspezifisch
// sind nur Pins, Anzeige und Tastenanzahl.

#if defined(MASSARBEIT_VARIANT_PRO)
    #include "boards/t_display_s3.h"
#elif defined(MASSARBEIT_VARIANT_LIGHT)
    #include "boards/light_t7_mini32.h"
#else
    #error "Keine Geraetevariante gewaehlt - -DMASSARBEIT_VARIANT_PRO oder -DMASSARBEIT_VARIANT_LIGHT fehlt (siehe platformio.ini)."
#endif
