#pragma once

// Anzeige-Zustaende, die BEIDE Geraetevarianten kennen - die grosse Waage
// zeigt sie als Vollbild auf dem TFT (TftDisplay), die Light als Blink-/
// Helligkeitsmuster auf ihrer Status-LED (LedStatusUi). Bewusst in einer
// eigenen Datei ohne Arduino_GFX-Abhaengigkeit, damit der Light-Build (der
// die GFX-Library gar nicht dabei hat) sie ebenfalls einbinden kann.
//
// Welche Implementierung dahintersteht, waehlt DeviceUi.h aus.

#include <Arduino.h>

// Lokaler Anzeige-Zustand, wenn WEDER ein RemoteCue laeuft NOCH ein Spieler
// am Zug ist - die Waage ist "zwischen Spielen". Auf der grossen Waage ist
// das die Geraete-Spielauswahl: Taste 1 kurz schaltet durch die Spieleliste,
// Taste 2 kurz bestaetigt. Die Light hat mangels Display keine Auswahl.
enum class LocalScreen {
    GamePicker,     // "<Icon> <Name>", Taste 1/2-Hinweis
    GameConfirmed,  // "<Name> ausgewaehlt - jetzt in der App oeffnen"
};

// Von der App per BLE ferngesteuerte Anzeige-Hinweise (siehe Config.h fuer
// das Kommando-Protokoll) - ueberlagern LocalScreen voruebergehend, z.B.
// waehrend des Turn-Readiness-Rituals eines Spiels.
enum class RemoteCue {
    None,
    Ready,          // "Bereit, jetzt trinken"
    Away,           // Glas komplett von der Waage gehoben ("Abschlag") - laeuft
                    // als Endlosschleife weiter, bis Ergebnis/Idle kommt
    ResultMiss,     // Schluck lag daneben
    ResultClose,    // Schluck lag nah am Ziel
    ResultPerfect,  // Schluck hat exakt getroffen
};

// Welches Spiel gerade laeuft - bestimmt auf dem TFT, welche Away-Animation
// gezeigt wird (Ball/Pfeil/Karte/Block, siehe renderAway*() in
// TftDisplay.cpp) und das kleine Icon im Spieler-Badge. Werte muessen exakt
// zum <gameId>-Byte im BLE-Protokoll passen, siehe Config.h.
enum class GameKind : uint8_t {
    None = 0,
    Golf = 1,
    Dart = 2,
    Blackjack = 3,
    Tower = 4,
    Scale = 5,
    Boxen = 6,
};
