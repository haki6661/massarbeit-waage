# Roadmap

Lose Sammlung geplanter, noch nicht umgesetzter Änderungen — im Gegensatz
zu den `TODO`-Kommentaren im Code (die sind lokal/klein), geht's hier um
größere, noch nicht im Detail durchdachte Vorhaben.

## 1. Taste 1 kurz: Tara raus, stattdessen Spiel-Schnellauswahl

**Idee:** Taste 1 (kurz) tariert aktuell direkt. Stattdessen soll Taste 1
(kurz) durch eine Liste von Spielen durchschalten (ohne Spielernamen, nur
das Spiel selbst — reine Geräte-Vorauswahl), Taste 2 (kurz) bestätigt die
Auswahl. Tara fällt als direkte Taste-1-Funktion weg.

**Warum:** Die Waage soll ein Spiel starten können, ohne dass zwingend
jemand zuerst am Handy den Katalog öffnen muss.

**Offene Fragen, noch nicht entschieden:**

- Woher kennt die Firmware die Spieleliste? Fest im Code hinterlegen
  (Name + `GameKind`, siehe `TftDisplay.h`) und mit `src/lib/gameRegistry.ts`
  im App-Repo synchron halten, oder von der App per BLE übertragen lassen
  (aufwendiger, aber immer aktuell)?
- Was passiert mit Tara, wenn es nicht mehr auf Taste 1 kurz liegt? Ganz
  weg (App übernimmt Tara komplett über den bestehenden BLE-Befehl `0x01`),
  oder auf eine andere Taste/Geste verlegen (z.B. Taste 1 lang, das aktuell
  Deep Sleep ist — dann bräuchte Deep Sleep eine neue Taste)?
- Wie wirkt sich die Auswahl aus, wenn die App bereits verbunden ist und
  über sie ein Spiel läuft — gewinnt die App-Auswahl, oder synchronisieren
  sich beide (z.B. Geräte-Auswahl sendet einen neuen BLE-Befehl, der die
  App auffordert, direkt in dieses Spiel zu wechseln)?
- UI auf dem Display: eigener Screen mit Spielname + kleinem Icon
  (`renderGameIcon()` existiert bereits für die Spieler-Badges, ließe sich
  wiederverwenden), Fortschritt/Auswahl-Indikator beim Durchschalten.

**Status:** Nicht begonnen.

## 2. Boot beschleunigen, kein "Starte..."-Text mehr

**Status:** ✅ Erledigt. `TftDisplay::playBootSprite()` zeigt sofort die
Pixel-Art-Sprite-Animation (`data/boot/`, siehe deren README) - kein
Text-Screen mehr davor. Läuft parallel zur echten Initialisierung
(`runNextBootStep()` in `main.cpp` erledigt battery/buttons/scale/BLE
einen Schritt pro gezeigtem Frame) statt hinterher eine feste Dauer
draufzuschlagen, plus die Serial-USB-Warteschleife von 3s auf 300ms
verkürzt. Normaler Boot jetzt unter 2s statt vorher ~4-5s. Die Boot-Checks
(HX711/Akku/BLE) sind als eigener Text-Screen komplett weg - nur bei
tatsächlichem HX711-Fehler erscheint danach weiterhin eine Fehlermeldung.
