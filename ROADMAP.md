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
Pixel-Art-Sprite-Animation (`data/`, siehe deren README) - kein
Text-Screen mehr davor. Läuft parallel zur echten Initialisierung
(`runNextBootStep()` in `main.cpp` erledigt battery/buttons/scale/BLE
einen Schritt pro gezeigtem Frame) statt hinterher eine feste Dauer
draufzuschlagen, plus die Serial-USB-Warteschleife von 3s auf 300ms
verkürzt. Normaler Boot jetzt unter 2s statt vorher ~4-5s. Die Boot-Checks
(HX711/Akku/BLE) sind als eigener Text-Screen komplett weg - nur bei
tatsächlichem HX711-Fehler erscheint danach weiterhin eine Fehlermeldung.

## 3. Neuer Spielmodus: nur Zeit stoppen ("Ex"-Modus, Glas muss leer sein)

**Idee:** Ein einfacher Modus ohne Punkte-/Zielsystem wie bei den
bestehenden Spielen (Golf/Dart/Blackjack/Tower) - es wird nur die Zeit
gestoppt, die jemand zum vollständigen Leertrinken ("exen") eines Glases
braucht. Die Zeit läuft erst ab, bis das Glas wieder auf ~Taragewicht
zurück ist (komplett leer), nicht schon bei geringerer Restmenge.

**Warum:** Schnelles, unkompliziertes Partyspiel ohne Zielgenauigkeit -
reine Geschwindigkeitsmessung statt "wie nah am Zielgewicht" wie bei den
anderen Spielen.

**Später denkbar:** Trink-Statistiken je Spieler - nicht nur die
Gesamtzeit, sondern z.B. der g/s-Verlauf während des Exens (wie schnell
zu Beginn vs. gegen Ende), für einen Vergleich/eine Bestenliste "wer
trinkt am schnellsten".

**Offene Fragen, noch nicht entschieden:**

- Wann startet die Zeitmessung - sobald das Glas von der Waage gehoben
  wird (analog zum "Away"-Zustand der anderen Spiele), oder über einen
  eigenen Start-Trigger?
- Wie wird "leer" erkannt - exakte Rückkehr aufs Taragewicht (0g) beim
  Wiederabstellen, oder ein Toleranzband (z.B. <5g Rest zählt als leer)?
- Eigenes `GameKind` (mit Icon/Away-Animation) oder Sonderfall im
  bestehenden Katalog?
- Wo/wie wird die g/s-Kurve gespeichert - nur fürs Live-Feedback in der
  App während des Exens, oder dauerhaft je Spieler/Session für eine
  spätere Statistik-Ansicht?

**Status:** Nicht begonnen.

## 4. Neuer Spielmodus: Eingießen statt Austrinken (Zielwert einschenken)

**Idee:** Das Gegenteil der bisherigen Spiele (Golf/Dart/Blackjack/Tower
zielen alle auf einen bestimmten Trink-/Abtrag-Wert ab, während das Glas
von der Waage genommen wird). Hier bleibt das Glas die ganze Zeit auf der
Waage stehen, und es muss ein vorgegebener Zielwert eingeschenkt/
eingegossen werden - Ziel ist also ein bestimmtes Gewicht *hinzuzufügen*
statt abzunehmen, möglichst genau getroffen.

**Warum:** Andere Fähigkeit als die bisherigen Spiele (Eingießen mit
Gefühl statt Trinken mit Gefühl), lässt sich z.B. auch nüchtern/ohne
tatsächlichen Alkoholkonsum als reines Geschicklichkeitsspiel nutzen.

**Offene Fragen, noch nicht entschieden:**

- Zielwert fix (z.B. "genau 40ml/g") oder pro Runde zufällig/von der App
  vorgegeben, wie bei den bestehenden Spielen?
- Da das Glas die ganze Zeit auf der Waage bleibt, gibt es keinen
  "Away"-Zustand wie bei den anderen Spielen - wie sieht der Ablauf/die
  Displayanzeige während des Eingießens stattdessen aus (z.B. Live-Balken
  Richtung Zielwert)?
- Wann gilt der Versuch als abgeschlossen - eigener Bestätigungs-Trigger
  (Taste/App), oder automatisch, sobald der Wert eine Zeit lang stabil
  bleibt (ähnlich der Stabilitätserkennung beim Eichen, siehe
  `calibrationStore.ts` im App-Repo)?
- Eigenes `GameKind` oder Sonderfall im bestehenden Katalog?

**Status:** Nicht begonnen.
