# Roadmap

Lose Sammlung geplanter, noch nicht umgesetzter Änderungen — im Gegensatz
zu den `TODO`-Kommentaren im Code (die sind lokal/klein), geht's hier um
größere, noch nicht im Detail durchdachte Vorhaben.

Hier steht ausschließlich, was an der **Waage** zu tun ist: Firmware,
Display, Gehäuse, Elektronik, BLE-Protokoll. Was in der App zu tun ist —
neue Spielmodi, Spielregeln, Turnierwertung, Mehrgeräte-Verwaltung — steht
im Abschnitt "Roadmap" von `PRODUCT.md` im Schwesterprojekt "Maßarbeit App".
Faustregel: wo müsste man tippen, um den Punkt zu erledigen? Eine `.cpp`,
ein Pin, ein Kommando-Byte → hierher. Ein Spiel, ein Store, ein Screen →
dorthin. Punkte, für die beide Seiten etwas tun müssen, stehen dort, wo der
eigentliche Knackpunkt liegt, und nennen die andere Hälfte ausdrücklich.

Umgesetzte Punkte werden hier **gelöscht**, nicht als "erledigt" stehen
gelassen — was live ist, steht im Code und in der Git-Historie, und eine
Roadmap voller Häkchen verdeckt nur, was noch offen ist.

Die Nummerierung gruppiert thematisch (erst Firmware/Display, dann
Hardware) und sagt **nichts über die Priorität**. Sie ist auch keine stabile
ID: beim Aufräumen wird neu durchnummeriert. Code-Kommentare verweisen
deshalb über den Titel eines Punktes hierher, nicht über seine Nummer.

## 1. Formel 1 auf dem Gerät nachziehen

**Idee:** Die App hat mit "Formel 1" ein Zeitspiel bekommen: Glas aufstellen,
fünf rote Startlampen, nach einer ausgelosten Haltezeit Grün, dann abheben,
die Mindestmenge der gewählten Strecke (100/200/300 g) wegtrinken und das
Glas abstellen — gemessen wird die Zeit von Grün bis zum Abstellen, dazu
Reaktionszeit und Durchfluss in ml/s. Die Waage weiß davon bisher nichts.

**Warum:** Bei diesem Spiel schaut man in genau dem entscheidenden Moment
NICHT aufs Handy — man greift zum Glas. Der Start gehört deshalb auf das
Gerät, das direkt vor einem steht, nicht nur auf den Bildschirm daneben.

**Was dafür fehlt:**

- Eintrag in `PICKER_GAMES` (`TftDisplay.cpp`) — die Liste wird von Hand
  synchron zu `GAME_REGISTRY` (App-Repo) gepflegt, ein neues Spiel muss also
  auch hier eingetragen werden. Siehe Punkt 2, der genau diese doppelte
  Pflege abschaffen würde.
- Eine Ampel-Darstellung als `RemoteCue`: fünf rote Lampen, die nacheinander
  angehen, dann ausgehen. Das bestehende Cue-Vokabular (Bereit / Away /
  Ergebnis-Güte) trifft das nicht — "bereit" heißt bei Formel 1 gerade
  NICHT "du darfst trinken".
- Offene Frage: Wer bestimmt den Grün-Zeitpunkt? Heute lost ihn die App aus
  und leitet ihn rechnerisch aus dem Aufstell-Zeitstempel ab
  (`greenAtMs` in `formel1State.ts`). Schickt die App der Waage nur "starte
  jetzt eine Ampel mit X ms Haltezeit", oder soll die Waage selbst auslosen?
  Zweiteres wäre für die Anzeige flüssiger, würde aber die
  Fehlstart-Bewertung auf zwei Uhren verteilen, die über BLE nur ungenau
  synchron sind.
- Die Basisvariante ohne Display kann eine Ampel höchstens über ihre
  Status-LED andeuten (rot → aus). Ob das reicht oder ob dieses Spiel dort
  einfach am Handy bleibt, ist nicht entschieden.
- **Auf dem LED-Ring ist die Ampel dagegen schon fertig** (`LedRing::
  startRaceLights()` / `raceLightsGreen()` / `abortRaceLights()`, siehe
  Punkt 6): fünf rote Lampen, Haltezeit, gemeinsames Ausgehen. Beide
  Varianten der offenen Frage oben sind dort bereits vorgesehen — Haltezeit
  mitgeben (Waage lost aus) oder Grün von außen setzen (App lost aus). Was
  fehlt, ist genau die BLE-Hälfte, die diese Methoden auslöst, plus die
  TFT-Darstellung. Der Ring ist allerdings noch in keiner Variante
  bestückt/aktiviert, taugt also nicht als alleiniger Weg.

**Status:** Nicht begonnen. Das Spiel selbst läuft in der App bereits
vollständig, auch ohne diese Ergänzung.

## 2. App-Sync: Geräte-Spielauswahl schaltet die App mit um

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

## 3. Sprite-Frames für die Zug-Animationen aufs TFT bringen

**Idee:** Die vier Momente eines Zuges (Bereit / Abschlag / Warten, während
das Glas weg ist / Ergebnis) werden auf dem Display heute aus Grundformen
zusammengesetzt (`TftDisplay::RemoteCue::Away` zeichnet Kreis und Linie).
Sie sollen richtige Sprite-Frames bekommen, passend zu den Animationen, die
die App für denselben Moment zeigt.

**Warum:** Beide Seiten zeigen denselben Moment; solange die eine flache
SVG-Animationen hat und die andere zwei Grundformen, wirkt das Gerät wie ein
Platzhalter neben der App.

**Was dafür fehlt — der Weg ist bewusst ein anderer als in der App:** Kein
Laufzeit-Decoding von PNG/Spritesheet (kein Dateisystem, unnötiger Flash-/
RAM-Verbrauch). Stattdessen einzelne Sprite-Frames zur Build-Zeit in rohe
RGB565-Bitmap-Arrays konvertieren (Bildkonverter-Tool, z.B.
LVGL-Image-Converter oder image2cpp) und als `PROGMEM`-Header fest einbacken,
Anzeige über `gfx_->draw16bitRGBBitmap(...)` (Arduino_GFX). Gleiches Konzept
wie app-seitig (wiederverwendbare Einzelelemente, mehrere Frames =
Animation), nur pro Plattform anders verpackt.

**Offene Fragen, noch nicht entschieden:**

- Woher kommen die Frames? Die App hat sich im UI-Relaunch gegen Pixel-Art
  und für flache SVG/CSS-Animationen entschieden — die recherchierten
  Pixel-Asset-Packs (siehe "Asset-Pipeline für Spiel-Animationen" in
  `PRODUCT.md`, App-Repo) passen damit nicht mehr automatisch zu beiden
  Seiten. Entweder die flachen Formen der App werden zu Frames gerastert,
  oder das Gerät bekommt bewusst eine eigene Bildsprache.
- Wie viel Flash darf das kosten? Ein Frame in 64×64 RGB565 sind schon 8 KB;
  eine Animation mit acht Frames für vier Momente wäre ein Vielfaches davon.
- Gilt nur für die Vision — die Basisvariante hat kein Display.

**Status:** Nicht begonnen.

## 4. Basisvariante: hörbare Rückmeldung und Kalibrierung ohne Display

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

## 5. Neue Gehäuse entwerfen und drucken (Vision und Basis)

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

## 6. WS2812B-RGB-LED-Ring im Deckel bestücken und scharfschalten

**Idee:** Einen Ring aus adressierbaren WS2812B-LEDs in den Deckel einbauen
und darüber dieselben Zustände zeigen, die schon über `RemoteCue`/
`DeviceUi::setActivePlayer()` laufen — nur als Lichteffekt rundum sichtbar
statt auf einem kleinen Display oder einer einzelnen Status-LED.

**Warum:** Sowohl das TFT der Vision als auch die einfarbige Status-LED der
Basis sind nur aus einem engen Blickwinkel gut lesbar — auf einer Party mit
mehreren Leuten um den Tisch verliert das schnell. Ein Ring ist von allen
Seiten sichtbar und deutlich festlicher.

**Was schon da ist (Firmware):** `src/LedRing.h/.cpp` enthält die
komplette Lichtlogik — Prioritäten, alle Muster (Formel-1-Startampel, Away je
Spiel, Ergebnis-Güten, Wiege-Balken, Spielerfarbe, Verbindungszustand),
Helligkeitsdeckel und Gamma. Der Ring läuft **parallel** zur bestehenden
Anzeige mit (die frühere offene Frage "drittes Backend oder Erweiterung?" ist
damit zugunsten der Erweiterung entschieden); `TftDisplay`/`LedStatusUi`
reichen ihm über `attachLedRing()` dieselben Cue-/Spielerwechsel weiter, am
BLE-Protokoll ändert sich kein Byte. In beiden Board-Profilen steht
`MASSARBEIT_HAS_LED_RING` auf 0, der Code fällt also als toter Code weg,
wird aber bei jedem Build mitkompiliert.

**Was noch fehlt — Hardware und drei Handgriffe** (Schritt für Schritt im
README, "LED-Ring nachrüsten"):

- Ring beschaffen und in den Deckel bauen; Durchmesser/LED-Anzahl hängen am
  Gehäuse und gehören deshalb mit Punkt 5 zusammen gedacht.
- Aufbau gegenprüfen: Pegel 3,3V → 5V (ggf. Level-Shifter), Vorwiderstand
  300-500Ω, Puffer-Kondensator ~1000µF, 5V-Versorgung getrennt vom Akku.
  Erst danach `MASSARBEIT_HAS_LED_RING` auf 1 setzen, `…_COUNT` korrigieren
  und die NeoPixel-Abhängigkeit in `platformio.ini` einkommentieren.
- Die gewählten Datenpins am realen Board verifizieren (Vision GPIO13, Basis
  GPIO4) — auf der Vision aus der offiziellen Pin-Tabelle abgeleitet, aber
  noch nicht mit dem Multimeter bestätigt.
- Praxistest der Helligkeit: `LED_RING_MAX_BRIGHTNESS` (Vorgabe 40/255) ist
  am Schreibtisch gewählt, nicht im Partylicht gemessen. Dazu gehört die
  Frage, wie viel Akkulaufzeit der Leerlauf-Ring tatsächlich kostet.

**Offene Fragen, noch nicht entschieden:**

- Beide Varianten bestücken, oder erstmal nur eine? Bei der Basis wäre der
  Ring die einzige echte Farbanzeige, bei der Vision käme er zum TFT hinzu.
  Die Firmware unterstützt beides bereits unverändert.
- Braucht die Basis dann noch ihre einfarbige Status-LED, oder wird sie
  redundant? (Aktuell laufen beide parallel, was sich nicht widerspricht,
  weil die Zustands-Rangfolge in beiden dieselbe ist.)

**Status:** Firmware-Seite steht und ist ausgeschaltet eingebaut; offen ist
die Hardware.

## 7. Siegerehrung der Olympiade auf dem Gerät

**Idee:** Die App plant einen Turniermodus über den ganzen Abend (siehe
"Olympiade" in `PRODUCT.md`, App-Repo), der mit einem Treppchen für Platz
1/2/3 endet. Die Waage könnte diesen Moment mittragen.

**Was dafür fehlt:** ein neuer `RemoteCue` für die Siegerehrung plus eine
Treppchen-Animation im Display-Code. Die Basisvariante hat kein Display und
könnte den Moment höchstens über ihre Status-LED andeuten.

**Status:** Nicht begonnen — und sinnvoll erst, wenn die Olympiade in der
App überhaupt existiert.
