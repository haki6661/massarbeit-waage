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

**Idee:** Der Boot-Ablauf ist aktuell: `display.showMessage("Massarbeit
Waage", "Starte...")` direkt nach `display.begin()`, dann HX711-Init,
dann `playBootAnimation()` (~2,5s feste Balkenwaage-Sequenz + Boot-Checks).
Der "Starte..."-Text soll weg — stattdessen soll die Balkenwaage-Animation
selbst schon laufen, während im Hintergrund initialisiert wird (Endlos-
Loop, keine feste Dauer), und erst wenn wirklich alles bereit ist,
überblendet sie in den normalen Gewichtsscreen. Zusätzlich der gesamte
Boot-Vorgang generell beschleunigen.

**Warum:** "Starte..." als reiner Text ist unschön/unfertig wirkend,
außerdem dauert der Boot aktuell spürbar (feste 2,5s Animation kommen on
top zur eigentlichen Initialisierung).

**Offene Fragen, noch nicht entschieden:**

- `playBootAnimation()` ist aktuell blockierend mit fester Dauer
  (`swingDurationMs`), läuft NACH der Initialisierung. Für eine echte
  Hintergrund-Animation müsste die Balkenwaage-Schleife parallel zur
  Initialisierung laufen (z.B. HX711-Init selbst non-blocking machen, oder
  die Animation in kleinen Schritten zwischen den Init-Aufrufen weiter-
  drehen) - im Zweifel mehr Umbau als nur der Text-Wegfall.
- Wo genau lässt sich Zeit sparen? `Serial`-Warten auf USB (`while (!Serial
  && millis() - waitStart < 3000)` in `main.cpp`) ist der größte bekannte
  Posten - lohnt sich, das beim Batteriebetrieb (kein Serial Monitor dran)
  zu überspringen statt die vollen 3s zu warten.
- Sollen die Boot-Checks (HX711/Akku/BLE, aktuell Phase 2 der Animation)
  trotzdem noch kurz sichtbar sein (z.B. nur bei einem Fehler), oder
  komplett weg zugunsten von reiner Geschwindigkeit?

**Status:** Nicht begonnen.
