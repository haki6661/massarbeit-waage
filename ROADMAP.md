# Roadmap

Lose Sammlung geplanter, noch nicht umgesetzter Änderungen — im Gegensatz
zu den `TODO`-Kommentaren im Code (die sind lokal/klein), geht's hier um
größere, noch nicht im Detail durchdachte Vorhaben.

Umgesetzte Punkte werden hier **gelöscht**, nicht als "erledigt" stehen
gelassen — was live ist, steht im Code und in der Git-Historie, und eine
Roadmap voller Häkchen verdeckt nur, was noch offen ist.

Die Nummerierung gruppiert thematisch (erst neue Spielmodi, dann Ausbau des
Bestehenden, dann Hardware) und sagt **nichts über die Priorität**. Sie ist
auch keine stabile ID: beim Aufräumen wird neu durchnummeriert. Code-
Kommentare verweisen deshalb über den Titel eines Punktes hierher, nicht
über seine Nummer.

## 1. Neuer Spielmodus: nur Zeit stoppen ("Ex"-Modus, Glas muss leer sein)

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

## 2. Neuer Spielmodus: Eingießen statt Austrinken (Zielwert einschenken)

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

## 3. Olympiade: alle Spiele am Stück, Platzierungen über den ganzen Abend

**Idee:** Ein Turniermodus über den gesamten Katalog. Alle Spiele werden
nacheinander durchgespielt, jedes ist eine "Disziplin". Die Platzierung
jedes Spielers wird pro Disziplin festgehalten und über alle Disziplinen
hinweg mitgeführt. Am Ende steht die Siegerehrung: ein Treppchen mit den
ersten drei Plätzen und einer Animation dazu.

**Warum:** Bisher steht jede Runde für sich - man spielt Golf, dann
vielleicht Dart, und nichts verbindet die beiden. Die Olympiade gibt dem
Abend einen Bogen mit Anfang und Ende und einen Grund, auch das Spiel
mitzuspielen, in dem man schlecht ist. "Boxen" hat diesen Gedanken schon
innerhalb eines Spiels (laufendes Konto statt Einzelrunden) - die Olympiade
zieht ihn eine Ebene höher, über die Spiele hinweg.

**Was dafür fehlt - der eigentliche Knackpunkt:** `GamePlugin` (siehe
`src/lib/types.ts` im App-Repo) kann heute nur *einen Sieger* melden:
`checkWinner()` liefert ein `WinnerResult` mit genau einer `playerId`/
`teamId`. Eine Olympiade braucht aber die **vollständige Reihenfolge aller
Spieler** einer Disziplin, nicht nur Platz 1. Das Interface braucht also
etwas wie ein `getRanking?(state, players, teams)`, das die Spieler-IDs
sortiert zurückgibt - und jedes Spiel muss das beantworten können:

- **Golf/Dart/Blackjack/Wackelturm:** haben eine Punktzahl/Distanz je
  Spieler, daraus lässt sich sortieren.
- **Boxen:** scheidet Spieler nacheinander aus - die Rangfolge ist die
  umgekehrte K.O.-Reihenfolge, dafür muss die Ausscheide-Reihenfolge
  überhaupt erst mitgeschrieben werden.
- **Scale:** hat bewusst gar kein Scoring (siehe `shortDescription`) und
  gehört nicht in eine Olympiade - genauso wenig wie Spiele mit
  `comingSoon: true`.

**Offene Fragen, noch nicht entschieden:**

- Wie werden Platzierungen in eine Gesamtwertung verrechnet? Drei
  Kandidaten: olympisch (nur Medaillen zählen, Gold vor Silber vor Bronze),
  Formel-1-artig (Punkte je Platz, z.B. 10/8/6/5/…) oder golfartig (Summe
  der Platzziffern, niedrigste gewinnt). Die drei fühlen sich sehr
  unterschiedlich an: bei Variante 1 ist ein vierter Platz genauso wertlos
  wie der letzte, bei Variante 3 zählt jede Disziplin gleich viel.
- Wo lebt der Turnierstand? `useGameRuntimeStore` hält heute genau ein
  laufendes Spiel; der Olympiade-Stand muss den Wechsel zwischen
  `GameScreen`, Katalog und Lobby überleben. Eigener Store oder eine
  Erweiterung des bestehenden?
- Reihenfolge der Disziplinen: feste Reihenfolge, ausgelost, oder wählt der
  jeweils Führende/Letzte das nächste Spiel?
- Alle Spiele Pflicht oder Auswahl? Sechs Disziplinen mit mehreren Spielern
  sind ein sehr langer Abend - vermutlich braucht es eine "kurze Olympiade"
  mit auswählbarer Teilmenge.
- Gleichstand: auf dem Treppchen und in der Gesamtwertung. Stichkampf,
  geteilter Platz, oder entscheidet eine bestimmte Disziplin?
- Teams: `GamePlugin` kennt `supportsTeams`, aber nicht jedes Spiel
  unterstützt sie. Läuft die Olympiade grundsätzlich als Einzelwertung,
  oder sollen Teams mitgeführt werden - und was passiert dann in einer
  Disziplin ohne Team-Unterstützung?
- Abbruch mittendrin: was passiert mit einer Olympiade, die nach drei von
  sechs Disziplinen endet (die Party ist ja irgendwann vorbei)? Wertung des
  Zwischenstands anbieten oder verwerfen?
- Zeigt die Waage bei der Siegerehrung mit? Dafür bräuchte es einen neuen
  `RemoteCue` und eine Treppchen-Animation im Display-Code - und die
  Basisvariante hat kein Display, könnte den Moment höchstens über ihre
  Status-LED andeuten.

**Zur Siegerehrung selbst:** Treppchen mit Platz 1/2/3, die Spieler in
ihren Lobby-Farben. Stilistisch an den bestehenden Reveal-Ritualen
orientieren (`SipRevealOverlay`, die spielspezifischen Reveal-Komponenten) -
also Spannungsaufbau vor der Auflösung, statt das Ergebnis einfach
hinzustellen: erst Bronze, dann Silber, dann Gold.

**Status:** Nicht begonnen.

## 4. Physische Güte-Anzeige (miss/close/perfect) für Golf wiederherstellen

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

## 5. App-Sync: Geräte-Spielauswahl schaltet die App mit um

**Idee:** Die Geräte-Spielauswahl (Taste 1/2 auf der Waage, seit Firmware
1.4.0) ist bis heute reine Anzeige: sie zeigt "X ausgewählt - jetzt in der
App öffnen", das eigentliche Starten passiert weiter von Hand am Handy. Die
Auswahl soll stattdessen in der App ankommen und dort direkt das passende
Spiel öffnen.

**Warum:** Der halbe Weg ist aktuell unbefriedigend - man wählt an der Waage
etwas aus und muss es am Handy nochmal auswählen. Entweder die Auswahl am
Gerät kann etwas, oder sie kann weg.

**Was dafür fehlt:** eine BLE-Notify-Richtung Gerät -> App (bisher fließen
Kommandos nur App -> Gerät, siehe `BLE_COMMAND_CHAR_UUID` in
`include/Config.h`) plus ein App-seitiger Listener, der darauf hin die
Navigation umschaltet. Deshalb bei der Einführung der Spielauswahl bewusst
zurückgestellt und nicht mitgemacht.

**Offene Fragen, noch nicht entschieden:**

- Soll die App bei einer Geräte-Auswahl wirklich hart umschalten, oder nur
  einen Vorschlag einblenden ("Waage schlägt Golf vor - öffnen?")? Hart
  umschalten kann mitten in einer laufenden Runde stören.
- Zweite, kleinere Hälfte desselben Themas: die Spieleliste `PICKER_GAMES`
  (`TftDisplay.cpp`) wird von Hand synchron zu `GAME_REGISTRY`
  (`gameRegistry.ts`, App-Repo) gepflegt - ein neues Spiel muss also an zwei
  Stellen eingetragen werden. Mit einer Gerät->App-Richtung im Protokoll
  wäre auch der umgekehrte Weg denkbar (App schickt die Spieleliste beim
  Verbinden ans Gerät), dann fiele die doppelte Pflege weg.
- Gilt nur für die Vision - die Basisvariante hat kein Display und
  damit keine Geräte-Auswahl.

**Status:** Nicht begonnen.

## 6. Basisvariante: hörbare Rückmeldung und Kalibrierung ohne Display

**Idee:** Zwei offene Enden aus dem Bau der Basis (siehe `LedStatusUi.cpp`
und `CalibrationRoutine.h`), die bewusst erst einmal minimal gelöst wurden.

**Warum:** Die Status-LED trägt die Zustände zwar zuverlässig, aber sie
setzt voraus, dass jemand hinschaut - auf einer Party konkurriert sie mit
Licht, Bewegung und Handydisplays. Und die Kalibrierung hängt weiterhin am
Serial Monitor, also am USB-Kabel.

**Offene Fragen, noch nicht entschieden:**

- Soll die Basis Ereignisse zusätzlich über einen Piezo/Summer quittieren
  (hörbar statt sichtbar)? Ein kurzer Ton beim Volltreffer/Fehlschlag käme
  auch dann an, wenn gerade niemand auf die Waage schaut. Kostet einen
  weiteren freien GPIO und ein Bauteil.
- Kalibrierung ohne Display: bleibt es bei der Serial-Routine (zum
  Einrichten hängt man ohnehin am USB), oder wandert sie ganz in die App?
  Die 3-Punkt-Eichung dort (`calibrationStore.ts` im App-Repo) deckt den
  Alltagsfall schon ab - die Firmware-Grundkalibrierung braucht man aber
  weiterhin einmal je Gerät.
- Reicht der Umfang der LED-Muster, oder fehlt ein Zustand? Die Zuordnung
  steht in der Tabelle im README ("Was die Status-LED der Basis sagt").

**Status:** Nicht begonnen. Die Basis selbst läuft (Firmware 1.7.1), das
hier ist Feinschliff.

## 7. Mehrere Waagen gleichzeitig an einer App-Instanz

**Idee:** Mehr als eine Waage an dieselbe laufende App hängen - für mehrere
Tische/Gruppen auf derselben Party.

**Warum:** Erst durch die billige Basis ist "mehrere Waagen" überhaupt eine
realistische Anschaffung. Solange eine Waage der Preis eines T-Display S3
war, stellte sich die Frage praktisch nicht.

**Was dafür fehlt - der eigentliche Knackpunkt:** `sharedWeightSource`
(App-Repo) ist bewusst ein Singleton: genau eine `BleWeightSource`, genau
eine `CorrectedWeightSource` darüber, und jeder Hook/jedes Spiel greift
direkt darauf zu. Mehrere Geräte hieße, aus dieser globalen Konstante eine
Sammlung zu machen - und an jeder Verbrauchsstelle zu entscheiden, welche
Waage gemeint ist.

**Offene Fragen, noch nicht entschieden:**

- Wozu genau? Zwei unabhängige Runden parallel (jede Waage ihr eigenes
  Spiel), oder eine Runde, in der mehrere Spieler gleichzeitig an
  verschiedenen Waagen trinken?
- Wie wird eine Waage einem Spieler/Tisch zugeordnet - von Hand in der
  Lobby, oder automatisch über den BLE-Namen/die Modellkennung (siehe
  `getDeviceInfo()`)?
- Web Bluetooth erlaubt mehrere gleichzeitige GATT-Verbindungen, verlangt
  aber je Gerät einen eigenen `requestDevice()`-Dialog aus einer
  Nutzer-Geste. Wie sieht der Verbindungsablauf für drei Waagen aus, ohne
  dass er sich wie eine Behörde anfühlt?
- Reicht der Durchsatz? Jede Waage schickt 20 Gewichts-Notifies pro Sekunde.

**Status:** Nicht begonnen. Deutlich größere Ausbaustufe als es zunächst
klingt - hier notiert, weil die Basis sie überhaupt erst realistisch macht.

## 8. Neue Gehäuse entwerfen und drucken (Vision und Basis)

**Idee:** Ein anderes/neues 3D-druckbares Gehäuse für die Vision, statt
des aktuellen Stands in `cad/` (`massarbeit_waage_case.scad` +
`massarbeit_waage_base.stl`/`massarbeit_waage_platform.stl`) - und ein
Gehäuse für die Basis, für die es bisher überhaupt keins gibt.

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
- Die Basis braucht ein deutlich kleineres Gehäuse ohne Display-Ausschnitt
  (LilyGO T-OI Plus im D1-mini-Format statt T-Display S3, dafür ein externer
  Taster, eine sichtbare Status-LED und ein Fach für die 16340-Zelle). Eigener Entwurf oder parametrische
  Variante desselben `.scad` - also ein Parameter "Modell" statt zweier
  Dateien, die auseinanderdriften?

**Status:** Nicht begonnen.

## 9. WS2812B-RGB-LED-Ring im Deckel für visuelle Cues

**Idee:** Einen Ring aus adressierbaren WS2812B-LEDs in den Deckel einbauen
und darüber dieselben Zustände zeigen, die heute schon über `RemoteCue`/
`DeviceUi::setActivePlayer()` laufen (Bereit, Away, Ergebnis-Güte,
Spielerfarbe) - nur eben als Lichteffekt rundum sichtbar statt auf einem
kleinen Display oder einer einzelnen Status-LED.

**Warum:** Sowohl das TFT der Vision als auch die einfarbige Status-LED der
Basis sind nur aus einem engen Blickwinkel gut lesbar - auf einer Party
mit mehreren Leuten um den Tisch verliert das schnell. Ein Ring ist von
allen Seiten sichtbar und deutlich festlicher. Passt auch protokollseitig
gut: der Spielerfarbe-Badge (`COMMAND_PLAYER_TURN`, `0x14 <gameId><r><g><b>…`,
siehe `include/Config.h`) transportiert schon eine RGB-Farbe - ein Ring
könnte die 1:1 übernehmen, ohne dass sich am BLE-Protokoll etwas ändern
muss.

**Was dafür fehlt:**

- Eine WS2812B-Library (FastLED oder Adafruit_NeoPixel) als neue
  Abhängigkeit.
- Ein freier GPIO für die Datenleitung auf beiden Boards - auf der Basis
  ist GPIO4 der naheliegende Kandidat (im Board-Profil `t_oi_plus.h` schon
  als "für spätere Analog-Erweiterungen frei" vermerkt), auf der Vision ist
  noch nicht durchgeprüft, welcher der vom Display/HX711/Tastern
  unbenutzten Pins frei ist (siehe `t_display_s3.h`).
- Ein drittes `DeviceUi`-Rendering-Backend (neben `TftDisplay` und
  `LedStatusUi`) bzw. eine Erweiterung, die parallel zur bestehenden
  Anzeige mitläuft - noch nicht entschieden, welches von beidem.

**Offene Fragen, noch nicht entschieden:**

- Beide Varianten, oder erstmal nur eine? Bei der Basis würde der Ring die
  einzige echte Farbanzeige (ersetzt/ergänzt die einfarbige Status-LED),
  bei der Vision käme er zusätzlich zum TFT.
- Stromversorgung: WS2812B ziehen bis zu ~60mA je LED bei Vollweiß - bei
  einem Ring mit mehreren LEDs gegen den kleinen Akku (16340 bzw. den
  Vision-Akku) nicht vernachlässigbar. Braucht eine feste
  Helligkeits-/Farbobergrenze im Code statt unein­geschränktem
  `setBrightness()`, sonst leert sich der Akku im Party-Betrieb spürbar
  schneller.
- Pegelproblem: ESP32-GPIOs liefern 3.3V, WS2812B-Datenleitung ist für
  5V-Logik spezifiziert. Kurze Leitungen laufen oft trotzdem stabil, das
  muss aber am Aufbau gegengeprüft werden (ggf. Level-Shifter oder
  Vorwiderstand + Puffer-Kondensator), bevor der Ring fest verbaut wird.
- Ring-Durchmesser/LED-Anzahl hängen am Deckel-Formfaktor - gehört
  eigentlich mit Punkt 8 (neue Gehäuse) zusammen gedacht, nicht isoliert.
- Bildet der Ring nur die bestehenden `RemoteCue`-Zustände 1:1 ab, oder
  entstehen eigene Lichtmuster (z.B. ein Lauflicht während des Wiegens),
  die es bei TFT/Status-LED so nicht gibt?

**Status:** Nicht begonnen.
