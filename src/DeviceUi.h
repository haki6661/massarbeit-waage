#pragma once

// Waehlt die Geraete-Anzeige passend zur gebauten Variante aus: TFT-Vollbild
// (grosse Waage) oder Status-LED (Light). Alles, was "irgendwas anzeigen"
// will - main.cpp, BleWeightService, CalibrationRoutine - nimmt ab hier
// `DeviceUi&` statt `TftDisplay&` und muss die Variante nicht kennen.
//
// Bewusst ein Compile-Zeit-Typedef statt einer virtuellen Basisklasse: es
// laeuft ohnehin nie mehr als eine Implementierung in einem Binary, ein
// vtable-Umweg bei jedem update()-Aufruf waere reiner Overhead - und
// TftDisplay muss dafuer nicht angefasst werden.
//
// Gemeinsame Methodenoberflaeche beider Klassen:
//   begin(), update(hx711Ok, bleOk), showMessage(title, body),
//   runBootSequence(stepInit), setRemoteCue(cue, game),
//   setActivePlayer(game, color565, name), clearActivePlayer(),
//   pickerNext(), pickerConfirm(), prepareForSleep(), color565FromRgb(r, g, b)

#include "BoardConfig.h"
#include "DeviceUiTypes.h"

#if MASSARBEIT_HAS_TFT
    #include "TftDisplay.h"
    using DeviceUi = TftDisplay;
#else
    #include "LedStatusUi.h"
    using DeviceUi = LedStatusUi;
#endif
