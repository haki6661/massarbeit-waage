# Roadmap

Lose Sammlung geplanter, noch nicht umgesetzter Änderungen — im Gegensatz
zu den `TODO`-Kommentaren im Code (die sind lokal/klein), geht's hier um
größere, noch nicht im Detail durchdachte Vorhaben.

## 1. Taste 1 kurz: Tara raus, stattdessen Spiel-Schnellauswahl

**Status:** ✅ Groesstenteils erledigt (Firmware 1.4.0). Taste 1 kurz schaltet
durch die Spieleliste (`PICKER_GAMES` in `TftDisplay.cpp`), Taste 2 kurz
bestaetigt ("X ausgewaehlt - jetzt in der App oeffnen"). Tara ist als eigene
Tastenfunktion ersatzlos weg - laeuft nur noch automatisch (Auto-Zero-
Nachfuehrung) oder ueber die App (BLE-Kommando 0x01). Deep Sleep unveraendert
auf Taste 1 lang. Im Zuge dessen gleich die gesamte Live-Gewichtsanzeige
(bisheriger `DisplayMode::Weight`/`Status`) durch Spielzustands-Anzeigen
ersetzt (siehe unten, "generell Display").

**Bewusst zurueckgestellt:**
- Spieleliste ist fest im Firmware-Code hinterlegt, manuell synchron zu
  `GAME_REGISTRY` in `gameRegistry.ts` (App-Repo) gepflegt - kein
  automatischer/BLE-basierter Sync. Neues Spiel im App-Repo = `PICKER_GAMES`
  von Hand nachziehen.
- Kein automatisches Umschalten der App bei einer Geraete-Auswahl (App-Sync)
  - die Bestaetigung/der eigentliche Spielstart passiert weiter von Hand in
  der App. Waere ein groesserer Zusatzschritt (neue BLE-Notify-Richtung
  Geraet->App + App-seitiger Listener) und ist eine eigene, noch nicht
  angegangene Ausbaustufe.

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

## 5. Neuer Spielmodus: "Boxen" (Survival mit Lebensanzeige, Abweichung als Schaden)

**Idee:** Angelehnt an das ungeplugged Trinkspiel "Grammtrinken" (siehe
Recherche im Chat, z.B.
[mybeerpong.com/blogs/trinkspiele/grammtrinken](https://mybeerpong.com/blogs/trinkspiele/grammtrinken)):
reihum bestimmt ein Spieler per eigenem, zufälligem Schluck die
Grammzahl, die die anderen treffen müssen. Anders als bei Golf/Dart -
deren Ergebnis pro Runde für sich steht - wird hier die Abweichung vom
Zielwert Runde für Runde **aufsummiert** ("Schaden"). Ab einem
Schadens-Schwellwert ist ein Spieler "K.O." und scheidet aus - daher der
Name "Boxen".

**Warum:** Deckt einen Mechanismus ab, den es im bestehenden Katalog noch
nicht gibt (laufendes Punktekonto/Ausscheiden statt unabhängiger
Einzelrunden) - mehr Spannung über eine ganze Session statt nur je Zug.

**Lebensanzeige in Retro-Pixel-Optik:** Statt reinem Zahlenwert soll der
verbleibende "Lebensbalken" auf dem Waagen-Display als Pixel-Art-Element
dargestellt werden, sinkend mit jedem Treffer - passend zum bereits
geplanten Ausbau der Pixel-Art-Animationen für weitere Game-States (siehe
Chat-Diskussion zu Boot-Sprite/SD-Karten-Speicherlösung, noch nicht als
eigener Roadmap-Punkt festgehalten). Setzt also auf denselben
Sprite-Player auf, der ohnehin für die anderen Zustände (Ready/Away/
Result) geplant ist.

**Offene Fragen, noch nicht entschieden:**

- Schadens-Schwellwert für K.O. (Vorlage "Grammtrinken" nutzt 100g
  Gesamtabweichung) - fix oder einstellbar?
- Wie viele "Leben"/wie grob die Lebensanzeige gestuft (z.B. 5-10 Segmente
  in Pixel-Optik) im Verhältnis zum Schadens-Schwellwert?
- Eigenes `GameKind` (mit eigenem Icon/Away-Animation) - Boxhandschuh als
  Motiv?

**Entschieden:**
- Session endet mit dem letzten Überlebenden (Battle-Royale-Stil) - kein
  Fixrunden-/Ranglistenmodus.
- Zielwert je Runde wird reihum von einem Spieler per eigenem,
  zufälligem Schluck vorgegeben (wie beim Vorbild "Grammtrinken") - nicht
  von der App vorgegeben/gewürfelt wie bei den bestehenden Spielen. Der
  Vorgeber selbst nimmt in dieser Runde nicht am Matchen teil, die
  übrigen Spieler müssen seinen Wert treffen.

**Status:** Nicht begonnen.

## 7. Physische Güte-Anzeige (miss/close/perfect) für Golf wiederherstellen

**Idee:** Golf zeigt seinen Reveal jetzt über ein eigenes "Grün" in der App
(Ball + Fahne statt Tacho, siehe massarbeit-app PR #20) statt über den
geteilten Tacho/`LiveDialConfig`. Nebeneffekt: `getLiveDialConfig()` liefert
für Golf jetzt immer `null`, wodurch `classifySipQuality()` nie mehr
aufgerufen wird und die Waage nach einem Golf-Schluck keine
Güte-Rückmeldung (`showResultCue` / BLE-Kommando `0x12`, miss/close/perfect
auf dem Display) mehr sendet.

**Warum:** Konsistenz mit den anderen Spielen (Dart/Blackjack/Wackelturm
zeigen die Güte auf dem physischen Display weiterhin), und die kurze
Rückmeldung direkt auf der Waage war ein nettes Sofort-Feedback, bevor man
überhaupt aufs Handy schaut.

**Offene Fragen, noch nicht entschieden:**

- Eigener, schlankerer Weg für Spiele, Güte zu klassifizieren, ohne sich
  für den (jetzt golfseitig ungenutzten) Tacho/`LiveDialConfig` zu
  entscheiden - z.B. ein optionales `getSipQuality?(state, event)`-Feld auf
  `GamePlugin`, das GameScreen direkt für `showResultCue` nutzt?
- Oder `getLiveDialConfig()` für Golf doch mit echten Werten (`target` =
  `remainingTargetM` VOR dem Schluck, `tolerance` = `SINK_TOLERANCE_M`)
  füllen, aber `GameScreen` per Flag (z.B. `hasCustomReveal`) davon
  abhalten, den Tacho selbst zu rendern - würde `classifySipQuality()`
  unverändert weiter funktionieren lassen.

**Status:** Nicht begonnen.

## 8. Golf-"Grün" mit Pixel-Art-Texturen statt flachem Farbverlauf

**Idee:** Das neue Golf-Grün in der App (siehe massarbeit-app PR #20-#25 -
Ball+Fahne statt Tacho, eigene Pixelart-Fahnen-Sprite-Animation) nutzt aktuell
nur einen flachen Farbverlauf als Untergrund. Passend zur bereits gewählten
Pixel-Art-Fahne soll die ganze Grün-Fläche selbst auch strukturiertes
Pixel-Art-Terrain bekommen (z.B. Gras-/Fairway-Textur, evtl. mit leichtem
Rasenmuster-Streifen wie bei echten Golfplätzen).

**Warum:** Konsistenter visueller Stil - aktuell wirkt die Fahne (Pixel-Art)
und der Untergrund (glatter CSS-Gradient) stilistisch nicht zusammengehörig.

**Offene Fragen, noch nicht entschieden:**

- Als Tile-Textur (kleines wiederholtes Pixel-Art-Muster, per CSS
  `background-image`/`background-repeat`) oder als volles, einmalig
  gezeichnetes Hintergrundbild pro Zeile?
- Selbst gezeichnet (wie die Fahne) oder ebenfalls vom Nutzer geliefert?
- Bewegt sich die Textur mit (Parallax/Scroll-Effekt bei Ball-/Fahnen-
  Bewegung) oder bleibt sie starr?

**Status:** Nicht begonnen.

## 9. Neues Gehäuse für die Waage entwerfen und drucken

**Idee:** Ein anderes/neues 3D-druckbares Gehäuse für die Waage, statt des
aktuellen Stands in `cad/` (`massarbeit_waage_case.scad` +
`massarbeit_waage_base.stl`/`massarbeit_waage_platform.stl`).

**Referenz/Inspiration:** [Smart DIY Kitchen Scale for Precision Cooking (instructables.com)](https://www.instructables.com/Smart-DIY-Kitchen-Scale-for-Precision-Cooking/)

**Offene Fragen, noch nicht entschieden:**

- Was genau soll sich ändern - Optik/Formsprache, Material-/Druckbarkeit
  (Stützstruktur, Druckzeit), Passgenauigkeit fürs T-Display-S3-Board,
  oder auch neue Anforderungen (z.B. Aussparung fürs in der Chat-Recherche
  diskutierte SD-Kartenmodul fürs Speicherproblem)?
- Komplett neuer Entwurf oder Weiterentwicklung des bestehenden
  `massarbeit_waage_case.scad`?
- Bleibt es bei OpenSCAD (parametrisch, gut versionierbar als Textdatei)
  als Werkzeug?

**Status:** Nicht begonnen.
