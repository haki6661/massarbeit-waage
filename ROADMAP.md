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

## 3. Physische Güte-Anzeige (miss/close/perfect) für Golf wiederherstellen

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

## 4. App-Sync: Geräte-Spielauswahl schaltet die App mit um

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
- Gilt nur für die große Waage - die Light-Variante (siehe unten) hat kein
  Display und damit keine Geräte-Auswahl.

**Status:** Nicht begonnen.

## 5. Zweite Gerätevariante: "Maßarbeit Waage Light" (ESP32-C3, ohne Display)

**Idee:** Eine abgespeckte, deutlich billigere Waage auf einem kleinen
ESP32-C3-Board **ohne TFT**. Sie wiegt und funkt, sonst nichts - das
gesamte Spielgeschehen läuft ohnehin auf dem Handy. Die App erkennt beim
Verbinden selbst, welches Modell dranhängt, und verhält sich entsprechend.

**Warum:** Das TFT ist am aktuellen Gerät der größte Posten bei Preis,
Stromverbrauch und Bootzeit. Für Runden, in denen sowieso alle aufs Handy
schauen, reicht ein Board für ~3 EUR. Nebeneffekt: mehrere Light-Waagen
gleichzeitig werden bezahlbar (mehrere Tische/Gruppen).

### Grundsatzentscheidung: EINE Software, nicht zwei

**Entschieden:** Eine gemeinsame Firmware-Codebasis mit zwei Build-Targets
und **eine** App mit Laufzeit-Erkennung. Ausdrücklich KEIN zweites
Firmware-Projekt.

Der Punkt, der die Frage entscheidet: ESP32-S3 und ESP32-C3 sind
verschiedene Prozessor-Architekturen (Xtensa LX7 vs. RISC-V). Eine einzelne
`.bin` kann physisch nicht auf beiden starten. "Firmware erkennt selbst,
worauf sie läuft" gibt es auf Binary-Ebene also gar nicht, egal wie der Code
geschrieben ist. Die echte Wahl ist: *eine Codebasis, aus der zwei Binaries
fallen* vs. *zwei getrennt gepflegte Projekte*.

Für eine Codebasis spricht der Anteil des geteilten Codes:

| Geteilt (unverändert für beide) | Variantenspezifisch |
|---|---|
| `Scale` (HX711, Smart-Filter, Auto-Zero, Schock-Korrektur) | Pinbelegung |
| komplettes BLE-Protokoll inkl. aller Kommandos | Anzeige: TFT vs. Status-LED |
| `OtaUpdater` (Firmware-Update per BLE) | Akkumessung (ja/nein) |
| `CalibrationRoutine` + NVS-Speicherung | Tastenanzahl (2 vs. 1) |
| Deep Sleep / Auto-Sleep-Logik | Deep-Sleep-Aufweckpin (ext0 vs. GPIO-Wakeup) |

Das sind grob 90% geteilter Code gegen eine Handvoll Pins und die Anzeige.
Zwei Repos hieße: jeder Filter-Fix, jede neue BLE-Kommandonummer, jeder
OTA-Fix doppelt - und genau daraus entstehen Firmware-Stände, die nicht mehr
zum selben App-Protokoll passen. Der Preis der einen Codebasis ist ein
Board-Profil-Header, eine `DeviceUi`-Abstraktion und ein paar `#if` in
`main.cpp`. Deutlich billiger.

Auf **App-Seite** geht Laufzeit-Erkennung dagegen wirklich (eine App, ein
Deployment): die Waage meldet beim Verbinden Modell + Fähigkeiten, die UI
passt sich an. Die Spiel-Kommandos `0x10`-`0x15` bleiben für beide Varianten
identisch - die Light rendert sie eben als LED-Farbe statt als
Vollbild-Animation. Die App muss gar nicht wissen, *wie* das Gerät sie
darstellt.

### Hardware Light

Zielboard: beliebiges kleines ESP32-C3 mit nativem USB, in der Praxis
"ESP32-C3 SuperMini" (~3 EUR, 22x18mm). PlatformIO-Boardprofil dafür:
`esp32-c3-devkitm-1` (gleicher Chip, 4MB Flash, Standard-Partitionstabelle
mit zwei OTA-Slots - nötig fürs Firmware-Update per BLE).

| Zweck | GPIO | Anmerkung |
|---|---|---|
| HX711 DOUT / SCK | 6 / 7 | frei, kein Strapping, keine USB-/UART-Funktion |
| Taster (gegen GND) | 5 | interner Pullup, kein Widerstand nötig |
| Status-LED | 8 | auf dem SuperMini onboard, gegen 3V3 verschaltet -> LOW = an |
| Batterie-ADC (optional) | 1 | nur mit nachgerüstetem Spannungsteiler, ADC1 |

Drei Randbedingungen des C3, die die Pinwahl bestimmen - **nicht**
umsortieren, ohne sie zu prüfen:

- **GPIO2/8/9 sind Strapping-Pins.** Taugen nicht als Taster: ein beim
  Einschalten gedrückter Taster könnte den Chip in den Flash-Download-Modus
  booten lassen (gleiches Problem wie GPIO0 am S3, siehe `Buttons.h`).
- **Nur GPIO0-5 sind RTC-fähig**, und nur die können aus dem Deep Sleep
  aufwecken -> der Taster MUSS auf GPIO0-5 liegen.
- **GPIO18/19 = nativer USB (D-/D+), GPIO20/21 = UART0.** Beide freilassen,
  sonst gehen Flashen bzw. Serial-Monitor kaputt.

Kein Akku-Spannungsteiler auf einem nackten C3-Board -> Akkustand ist nicht
messbar, `Battery::readPercent()` liefert `-1` -> `0xFF` über BLE -> die App
blendet die Anzeige aus (statt eine Zahl zu raten). Wer 2x 100k von BAT+
nach GND lötet (Mitte an GPIO1), setzt das Fähigkeits-Makro auf 1, der Rest
läuft unverändert.

### Umbau Firmware (Repo `massarbeit-waage`)

1. **`platformio.ini`:** gemeinsames `[env]` (platform, framework,
   HX711/NimBLE/OneButton, Monitor), darüber zwei Targets:
   `t-display-s3` (+ `-ota`) und `massarbeit-light-c3`. Unterschiede:
   `board`, `-DMASSARBEIT_VARIANT_PRO` bzw. `-DMASSARBEIT_VARIANT_LIGHT`,
   `Arduino_GFX` + `board_build.filesystem = spiffs` nur beim S3, und
   `build_src_filter`, damit `TftDisplay.cpp` bzw. `LedStatusUi.cpp` je
   Target aus dem Build fliegen (PlatformIO kompiliert sonst immer alles in
   `src/`).
2. **`include/BoardConfig.h` wird zum Dispatcher**, die eigentlichen Pins
   ziehen um nach `include/boards/t_display_s3.h` und
   `include/boards/light_c3.h`. Jede Datei bindet weiter nur
   `BoardConfig.h` ein und muss nichts über Varianten wissen. Ein
   Board-Profil definiert außer den Pins einen Fähigkeits-Steckbrief:
   `MASSARBEIT_MODEL_ID` / `_MODEL_NAME` / `_HAS_TFT` / `_HAS_STATUS_LED` /
   `_STATUS_LED_RGB` / `_HAS_BATTERY` / `_BUTTON_COUNT`. Fehlt die
   Variantenauswahl, `#error` statt stiller Fehlbau.
3. **`DeviceUi` statt direktem `TftDisplay`:** die Enums (`RemoteCue`,
   `GameKind`, `LocalScreen`) wandern in ein eigenes `DeviceUiTypes.h`,
   ein `DeviceUi.h` wählt per `using DeviceUi = TftDisplay;` bzw.
   `= LedStatusUi;` aus. Bewusst Compile-Zeit-Typedef statt virtueller
   Basisklasse: kein vtable-Overhead, und `TftDisplay` muss dafür nicht
   angefasst werden. `BleWeightService` und `CalibrationRoutine` nehmen
   dann `DeviceUi&` statt `TftDisplay&`.
   Gemeinsame Methodenoberfläche: `begin()`, `update(hx711Ok, bleOk)`,
   `showMessage()`, `runBootSequence(stepInit)`, `setRemoteCue()`,
   `setActivePlayer()`, `clearActivePlayer()`, `pickerNext()`,
   `pickerConfirm()`, `color565FromRgb()`.
   `TftDisplay::playBootSprite()` bleibt unter diesem Namen bestehen (die
   ganze Sprite-Doku verweist darauf) und wird nur von einem dünnen
   `runBootSequence()` aufgerufen - die Light hat keine Sprites, aber
   dieselbe Aufrufstelle in `main.cpp`.
4. **Neu: `src/LedStatusUi.h/.cpp`** - dieselbe Oberfläche, aber eine
   einzelne Status-LED + Serial-Ausgabe statt Bildschirm. Zustand ->
   Signal (nicht-blockierend über `millis()`, Vorschlag):
   Boot = pulsierend / kein BLE = langsames Orange-Blinken / verbunden im
   Leerlauf = grüner Herzschlag alle 3s / HX711-Fehler = schnelles Rot /
   `Ready` = grünes Pulsieren / `Away` = Spielfarbe im Lauflicht /
   `ResultPerfect|Close|Miss` = 2s Grün/Orange/Rot / Spieler am Zug =
   Spielerfarbe gedimmt.
   RGB-LED (WS2812) über das im Arduino-Core eingebaute `neopixelWrite()` -
   **keine zusätzliche Library nötig**. Bei einfarbiger LED
   (`MASSARBEIT_STATUS_LED_RGB 0`, SuperMini) wird jede Farbe stattdessen
   auf ein eigenes Blinkmuster abgebildet, damit die Zustände trotzdem
   unterscheidbar bleiben.
5. **Bedienung mit nur einem Taster** (`Buttons.h` bekommt
   `#if MASSARBEIT_BUTTON_COUNT >= 2`): kurz = Tara, lang (2s) = Deep Sleep,
   Doppelklick = Kalibrierroutine. Die Geräte-Spielauswahl entfällt
   ersatzlos - ohne Display gibt es nichts auszuwählen (siehe auch
   "App-Sync: Geräte-Spielauswahl schaltet die App mit um" weiter oben).
6. **Deep Sleep:** der C3 kennt **kein `ext0`**. Statt
   `esp_sleep_enable_ext0_wakeup()` dort
   `esp_deep_sleep_enable_gpio_wakeup(BIT(pin), ESP_GPIO_WAKEUP_GPIO_LOW)`,
   und die Aufwach-Ursache ist `ESP_SLEEP_WAKEUP_GPIO` statt
   `ESP_SLEEP_WAKEUP_EXT0` (in `main.cpp` beide prüfen, dann braucht die
   Stelle kein `#if`). `POWER_ON` (Peripherie abschalten) entfällt auf der
   Light komplett.
7. **BLE: neue Geräte-Info-Characteristic** `6E40000A-...`, read-only,
   UTF-8-JSON - der Kern der App-seitigen Erkennung:
   ```json
   {"model":"light-c3","name":"Massarbeit Waage Light","fw":"1.6.0",
    "variant":"light-c3",
    "caps":{"display":false,"battery":false,"buttons":1,"led":true,"ota":true}}
   ```
   JSON statt Byte-Bitmaske bewusst: lässt sich um ein Feld erweitern, ohne
   dass App und Firmware gleichzeitig aktualisiert werden müssen. Auf der
   Firmware reicht ein `snprintf`, kein Parser. Fehlt die Characteristic
   (ältere Firmware), nimmt die App "große Waage mit Display" an - alte
   Geräte funktionieren unverändert weiter.
   Zusätzlich `BLE_DEVICE_NAME` je Variante (`Massarbeit-Waage` vs.
   `Massarbeit-Light`), damit die zwei im Bluetooth-Dialog auseinanderzu-
   halten sind. Am Kommandoprotokoll `0x01`/`0x10`-`0x15` ändert sich nichts.
8. **Kein SPIFFS auf der Light** (keine Boot-Sprites, kein `uploadfs`).

### Umbau App (Repo `massarbeit-app`)

1. **`WeightSource`-Interface** bekommt `getDeviceInfo(): DeviceInfo | null`
   (synchron, beim Verbinden einmal gelesen und gecacht - die UI liest es
   reaktiv über den bestehenden `onConnectionChange`-Listener, kein zweiter
   Benachrichtigungsweg nötig). `DeviceInfo`: `modelId`, `name`,
   `firmwareVersion`, `hasDisplay`, `hasBattery`, `buttonCount`,
   `firmwareVariant`.
2. **`BleWeightSource`** liest die Geräte-Info-Characteristic in
   `connectGatt()` im eigenen `try/catch` - exakt das Muster, mit dem
   Akkustand und OTA-Characteristics schon optional behandelt werden.
   Fallback bei Fehlen: `t-display-s3`, `hasDisplay: true`.
   `CorrectedWeightSource` reicht durch, `MockWeightSource` liefert einen
   Fake - dort per Query-Parameter (`?device=light`) umschaltbar, damit sich
   die Light-UI ohne Hardware testen lässt.
3. **`lib/firmwareUpdate.ts`:** Manifest-Schema v2 mit
   `variants: { "t-display-s3": {...}, "light-c3": {...} }`, Auswahl über
   `deviceInfo.firmwareVariant`. Die alten Top-Level-Felder
   (`version`/`file`/`size`/`md5`) bleiben zusätzlich als Spiegel der
   S3-Variante bestehen, damit ein alter, im Browser gecachter App-Stand
   nicht kaputtgeht. Gibt es für ein Modell noch kein Release, sauber
   "für dieses Modell liegt noch kein Update bereit" anzeigen statt Fehler.
4. **UI-Anpassungen:** `ConnectScaleButton` zeigt den Modellnamen statt
   pauschal "Waage verbunden"; `BatteryIndicator` bleibt bei
   `hasBattery: false` weg (tut er faktisch schon, weil `0xFF` -> `null`);
   `SettingsScreen` zeigt Modell + Firmware-Variante; Texte, die ein
   Display voraussetzen, für die Light umformulieren.

### Release-Prozess

Ein Manifest, ein Update-Weg, zwei Binaries. Der bisherige 5-Schritte-
Handbetrieb (bauen, `md5sum`, kopieren, `manifest.json` von Hand pflegen,
committen) wird mit zwei Varianten fehleranfällig - deshalb ein kleines
`scripts/release.py`: baut ein Environment, kopiert die `.bin` nach
`firmware/`, rechnet Größe + MD5 aus und schreibt den Varianten-Eintrag ins
Manifest. Python ist ohnehin da, PlatformIO läuft darauf.

**Offene Fragen, noch nicht entschieden:**

- Welches C3-Board konkret gekauft wird (SuperMini mit einfarbiger LED vs.
  ein Board mit WS2812) - davon hängt ab, ob die LED Farben zeigen kann oder
  nur Blinkmuster.
- Akku ja/nein: mit LiPo + Spannungsteiler + Laderegler ist der Preisvorteil
  gegenüber der großen Waage schon halb weg. Vielleicht bewusst reiner
  USB-Powerbank-Betrieb, dann fällt Akkumessung *und* Deep-Sleep-Nutzen
  weitgehend weg.
- Reicht der Status-LED-Umfang, oder soll die Light Ereignisse zusätzlich
  über einen Piezo/Summer quittieren (hörbar statt sichtbar - auf einer
  Party evtl. praktischer)?
- Kalibrierung ohne Display: bleibt es bei der Serial-Routine (man hängt
  zum Einrichten ohnehin am USB), oder wandert sie ganz in die App? Die
  3-Punkt-Eichung dort deckt den Alltagsfall schon ab.
- Eigenes Gehäuse für die Light (viel kleiner, ohne Display-Ausschnitt) -
  eigener Entwurf oder parametrische Variante des bestehenden
  `massarbeit_waage_case.scad`? Verzahnt sich mit "Neues Gehäuse für die
  Waage entwerfen und drucken" weiter unten.
- Sollen mehrere Waagen gleichzeitig an einer App-Instanz hängen können?
  Aktuell ist `sharedWeightSource` bewusst ein Singleton. Wäre eine eigene,
  deutlich größere Ausbaustufe - hier nur als Folgefrage notiert, weil
  billige Light-Geräte sie überhaupt erst realistisch machen.

**Status:** Nicht begonnen. Die Architekturentscheidung oben (eine
Codebasis + zwei Build-Targets, eine App mit Laufzeit-Erkennung) ist
getroffen, die Umsetzung steht noch komplett aus.

## 6. Neues Gehäuse für die Waage entwerfen und drucken

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
