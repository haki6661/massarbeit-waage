# Maßarbeit Waage - Firmware

Firmware fuer eine **eigene Praezisionswaage** fuer das Maßarbeit-Trinkspiel,
basierend auf [WeighMyBru2](https://github.com/031devstudios/weighmybru2). Ein
Board macht alles: Waegezelle auslesen, BLE-Service fuer die Handy-Web-App
(siehe Schwesterprojekt "Maßarbeit App"), Anzeige.

**Zwei Geraetevarianten, eine Codebasis:**

| | grosse Waage | Waage Light |
|---|---|---|
| Board | LilyGO T-Display S3 (ESP32-S3) | LilyGO T7 v1.3 Mini32 / "mini D1 PLUS" (ESP32) |
| Anzeige | 1,9" ST7789-TFT 170x320 | eine Status-LED |
| Taster | zwei | einer (extern) |
| Akku | ja | ja (JST-1.25mm + TP4054 an Bord) |
| Build-Target | `t-display-s3` | `massarbeit-light-t7` |
| BLE-Name | `Massarbeit-Waage` | `Massarbeit-Light` |

Der geteilte Anteil ist gross: Waegezellen-Auswertung, das komplette
BLE-Protokoll, Firmware-Update per BLE, Kalibrierung und Deep Sleep sind
identisch - variantenspezifisch sind nur Pins, Anzeige und Tastenanzahl. Weil
ESP32-S3 und ESP32 verschiedene Chip-Targets sind, koennen die beiden sich
aber keine `.bin` teilen: es gibt zwei Build-Targets aus einer Codebasis
(siehe `include/BoardConfig.h`). Die App dagegen erkennt beim Verbinden
selbst, welches Modell dranhaengt (Geraete-Info-Characteristic, siehe
Abschnitt "BLE").

Aktueller Stand: HX711-Handling + Kalibrierroutine + BLE-Gewichts-/Akkuservice
(inkl. Fernsteuerung der Anzeige durch die App, siehe Abschnitt "BLE"
weiter unten) + TFT-Anzeige mit Spieler-Badge und spielspezifischen
Away-Animationen + Pixel-Art-Sprite-Bootanimation (siehe `data/`) +
Status-LED-Anzeige derselben Zustände auf der Light + Taster-Bedienung +
Deep-Sleep-Stromsparmodus + Firmware-Update direkt aus der App per BLE (kein
WLAN am Partyort nötig, siehe Abschnitt "Firmware-Update per BLE" weiter
unten) + Entwicklungs-OTA per WLAN (nur grosse Waage). Die Away-/Ergebnis-
Animationen (nicht der Boot) sind weiterhin rein prozedural aus
Linien/Kreisen/Formen, kein Sprite noetig.

**UI-Relaunch (Firmware 1.5.0):** Die TFT-Anzeige tragt jetzt dieselbe
Farbsprache wie die neu gestaltete Handy-App (dunkler Grund, ein Akzent
"Sonnenuntergang", success/danger/warning-Statusfarben - siehe
`TftDisplay::begin()`), je Spiel eine eigene Kennfarbe (statt vorher immer
Olivgruen) sowie eine kraeftige, geometrische Bold-Schrift
(`FreeSansBold10pt7b`, `include/`) fuer Spielname/Cue-Titel statt des
technischen 5x7-Pixel-Fonts. Ausserdem: die Puls-/Away-Animationen
(BEREIT/VOLLTREFFER/Away-Motive) zeichnen unveraenderliche Bildteile nur noch
einmal und radieren pro Frame nur noch die tatsaechlich bewegte Flaeche
("Dirty Rect") statt bei jedem ~150ms-Redraw den kompletten Bildschirm zu
leeren - behebt das sichtbare Schwarz-Aufblitzen waehrend laufender
Animationen.

## Hardware

Beide Varianten teilen sich HX711 + 3kg-Single-Point-Waegezelle und einen
1S-LiPo am jeweils eingebauten Laderegler. Die Pinbelegungen stehen als
Board-Profile in `include/boards/`.

### Grosse Waage: LilyGO T-Display S3

| Zweck | GPIO |
|---|---|
| HX711 DOUT / SCK | 1 / 2 |
| Taste 1 (BOOT) / Taste 2 | 0 / 14 |
| Display (8-Bit-Parallelbus, siehe unten) | 5,6,7,8,9,38–48 |
| Peripherie-Power-Enable | 15 |
| Batteriespannung (ADC) | 4 |

Quelle: [Xinyuan-LilyGO/T-Display-S3](https://github.com/Xinyuan-LilyGO/T-Display-S3)
README-Pinout-Tabelle. **Wichtig:** GPIO5/6 sind bei WeighMyBru2s Original-
Boards der HX711, hier aber LCD RST/CS - deshalb HX711 auf GPIO1/2 verlegt.
Das ST7789-Display haengt an einem **8-Bit-Parallelbus** (Intel-8080-Timing),
nicht an SPI - daher `Arduino_GFX` (`Arduino_ESP32PAR8Q`-Bus) statt
`TFT_eSPI`/SPI-Displaytreiber.

### Waage Light: LilyGO T7 v1.3 Mini32 ("mini D1 PLUS")

ESP32-WROOM-32, 4MB Flash, CH340C, TP4054-Laderegler mit JST-1.25mm-Buchse.
PlatformIO-Boardprofil `ttgo-t7-v13-mini32`.

| Zweck | GPIO | Anmerkung |
|---|---|---|
| HX711 DOUT / SCK | 25 / 26 | auf der Stiftleiste, kein Strapping-Pin |
| Taster (extern, gegen GND) | 33 | interner Pullup, kein Widerstand noetig |
| Status-LED | 22 | Onboard-LED (`LED_BUILTIN` der Arduino-Variante) |
| Batteriespannung (ADC) | 35 | ADC1_CH7, Onboard-Spannungsteiler (Faktor 2) |

Zwei Randbedingungen bestimmen die Pinwahl - **nicht** umsortieren, ohne sie
zu pruefen:

- **Der Onboard-BOOT-Taster (GPIO0) taugt nicht als Bedientaster.** GPIO0 ist
  der Strapping-Pin fuer den Flash-Download-Modus; da genau dieser Taster aus
  dem Deep Sleep aufweckt, waere er beim folgenden Boot zwangslaeufig noch
  gedrueckt - der Chip wuerde in den Bootloader statt in die Firmware starten
  (dasselbe Problem wie bei Taste 1 der grossen Waage, siehe `Buttons.h`).
- **Nur RTC-faehige GPIOs koennen aus dem Deep Sleep aufwecken.** GPIO33 ist
  RTC-faehig und kein Strapping-Pin; GPIO2/12/15 waeren zwar auch RTC-faehig,
  sind aber ihrerseits Strapping-Pins.

Zwei Annahmen ueber das Board sind aus den offiziellen Quellen nicht restlos
belegt und beim ersten Aufbau kurz gegenzupruefen - beide sind einzeilig
korrigierbar und keine Sackgasse:

- **LED-Polaritaet.** `MASSARBEIT_STATUS_LED_ACTIVE_LOW` steht auf `0` (HIGH
  schaltet ein). Leuchtet die LED genau invers zu dem, was das Serial-Log
  sagt: auf `1` aendern.
- **Batterie-Spannungsteiler.** Sollte die eigene Board-Revision keinen
  bestueckt haben, meldet `Battery::readPercent()` von selbst "unbekannt"
  (Plausibilitaetsuntergrenze), und die App blendet die Akkuanzeige aus,
  statt eine Zahl zu raten.

## Was aus WeighMyBru2 uebernommen wurde

| Modul | Status |
|---|---|
| `Scale.h/.cpp` (HX711, Smart-Filter, NVS-Kalibrierung) | 1:1 portiert, nur FlowRate-Kopplung entfernt |
| BLE-Service (NimBLE, eigene Service-UUID) | Nur das simple "Bean Conqueror"-Float-Format uebernommen (siehe `include/Config.h`), GaggiMate-Protokoll bewusst weggelassen |
| Kalibrier-Workflow (`Faktor = Rohwert / bekanntes Gewicht`) | Logik uebernommen, ueber Serial+Taster statt Web-Formular |
| Display, Touch-Sensoren, FlowRate, WebServer/WiFiManager, ESP-NOW-Relais | Nicht uebernommen (siehe Architektur-Ueberblick weiter oben im Chat) - fuer Maßarbeit nicht relevant |

## Projektstruktur

```
platformio.ini            <- zwei Build-Targets (+ Dev-OTA-Target)
include/
  BoardConfig.h        <- Dispatcher: waehlt das Board-Profil zur Variante
  boards/
    t_display_s3.h       <- Pins + Faehigkeiten der grossen Waage
    light_t7_mini32.h    <- Pins + Faehigkeiten der Light
  Config.h              <- BLE-UUIDs, Kalibrier-Default, Dev-WLAN/OTA-Zugangsdaten
src/
  main.cpp              <- verdrahtet alle Module
  Scale.h/.cpp           <- HX711 (aus WeighMyBru2 portiert)
  BleWeightService.h/.cpp <- NimBLE-Service: Gewicht + Akkustand (Notify) + Tare/Anzeige-Kommandos (Write) + Geraete-Info
  Buttons.h/.cpp          <- physische Taster statt Touch-Pads (OneButton-Lib)
  DeviceUi.h              <- waehlt TftDisplay oder LedStatusUi je Variante
  DeviceUiTypes.h         <- RemoteCue/GameKind/LocalScreen (beide Varianten)
  TftDisplay.h/.cpp       <- nur grosse Waage: Arduino_GFX-Ausgabe (Status,
                            Spieler-Badge, spielspezifische Away-Animationen,
                            Sprite-Bootanimation aus SPIFFS)
  LedStatusUi.h/.cpp      <- nur Light: dieselben Zustaende als LED-Muster
  Battery.h/.cpp          <- Akkuspannung (kalibrierter ADC, aus LilyGOs Beispiel) + Prozent-Schaetzung
  CalibrationRoutine.h/.cpp <- interaktive Kalibrierung ueber Serial+Taster
  OtaUpdater.h/.cpp        <- Firmware-Update per BLE (Chunks -> Update.h)
  DevOta.h/.cpp           <- WLAN + ArduinoOTA, nur grosse Waage/Entwicklung
scripts/
  release.py             <- baut beide Varianten und schreibt das Manifest
data/
  f000.raw … f049.raw     <- Boot-Sprite-Frames (SPIFFS, siehe data/README.md)
  pal.raw                 <- gemeinsame 256-Farben-Palette dafuer
firmware/
  manifest.json          <- Version + Groesse + MD5 je Variante
  t-display-s3.bin        <- Release-Binary grosse Waage (App laedt es per BLE)
  light-t7.bin            <- Release-Binary Light
```

## Bedienung

### Grosse Waage (zwei Taster)

- **Taste 1** kurz: Geräte-Spielauswahl, nächstes Spiel
- **Taste 1** lang (~2s): sofort Deep Sleep (Display aus, ~wenige µA statt 60-150+ mA aktiv)
- **Taste 2** kurz: Auswahl bestätigen
- **Taste 2** lang (~1,5s): Kalibrierroutine starten (siehe unten)
- **Taste 2 drücken, während die Waage schläft**: aufwecken (kompletter Neustart der Firmware - kein spezieller Resume-Pfad, das ist bei ESP32-Deep-Sleep so vorgesehen)
- **Taste 2 beim Booten halten**: WLAN + ArduinoOTA aktivieren (nur Entwicklung, siehe unten)

Aufwecken geht bewusst nur über Taste 2 (GPIO14), nicht über Taste 1: GPIO0
(Taste 1) ist der BOOT-Strapping-Pin, wäre er beim Aufwach-Bootvorgang noch
gedrückt, könnte der Chip in den Flash-Download-Modus statt in die Firmware
starten.

### Waage Light (ein Taster)

- **kurz**: Tara (auf der grossen Waage übernimmt das die Spielauswahl, hier ist der Klick frei)
- **lang (~2s)**: sofort Deep Sleep
- **Doppelklick**: Kalibrierroutine starten
- **drücken, während sie schläft**: aufwecken

Eine Geräte-Spielauswahl gibt es auf der Light nicht - ohne Display ist
nichts auszuwählen. Dev-OTA per WLAN entfällt ebenfalls (die Aktivierung
hing am zweiten Taster); Firmware-Updates laufen per BLE aus der App.

### Beide

Automatischer Deep Sleep nach 10 Minuten ohne Gewichtsänderung und ohne
Tastendruck (`AUTO_SLEEP_TIMEOUT_MS` in `include/Config.h`) - außer während
Dev-OTA aktiv ist.

### Was die Status-LED der Light sagt

Die LED ist einfarbig, also trägt allein das Zeitmuster die Information
(siehe `LedStatusUi.cpp`). Ausführlicher steht alles zusätzlich im
Serial-Log, das auf der Light den Bildschirm ersetzt.

| Zustand | Signal |
|---|---|
| Boot | schnelles Auf-/Abblenden |
| HX711 antwortet nicht | 3 schnelle Blitze, dann Pause |
| wartet auf die App | ein kurzer Blitz alle 2s |
| verbunden, Leerlauf | Doppel-"Herzschlag" alle 3s |
| Spieler am Zug | schwaches Dauerlicht |
| "Bereit, jetzt trinken" (`0x11`) | schnelles, gleichmäßiges Blinken |
| Glas weg / Abschlag (`0x13`) | langsames Atmen |
| Volltreffer (`0x12`) | 2s Dauerlicht |
| nah dran (`0x12`) | zwei lange Blitze |
| daneben (`0x12`) | ein langer, gedimmter Blitz |

## Kalibrieren (Schritt 4 - mit der 3kg-Zelle neu ermitteln)

1. Serial Monitor oeffnen (115200 Baud).
2. Kalibrierung starten: grosse Waage Taste 2 ca. 1,5s halten, Light
   Doppelklick auf den Taster - bis "Kalibrierung gestartet" erscheint.
3. Waage leeren, Enter im Serial Monitor druecken.
4. Bekanntes Referenzgewicht auflegen, dessen Gramm-Zahl eingeben (z.B. `500`), Enter druecken.
5. Neuer Kalibrierfaktor wird berechnet, geloggt, auf der Geraeteanzeige
   gezeigt (auf der Light nur ueber Serial) und automatisch im NVS gespeichert (`Preferences`, Namespace `scale`, Key
   `calib`) - `DEFAULT_CALIBRATION_FACTOR` in `include/Config.h` wird danach
   nicht mehr verwendet.

## BLE

Service-/Characteristic-UUIDs stehen in `include/Config.h` (Herkunft dort
dokumentiert). Eine Notify-Characteristic liefert das Gewicht als simplen
4-Byte-Little-Endian-Float in Gramm (~20x/s), eine zweite liefert den
Akkustand als 1 Byte (0-100 = Prozent, `0xFF` = kein verlässlicher Wert,
z.B. USB gesteckt - siehe `Battery::readPercent()`), alle 5s (Akkustand
ändert sich langsam, spart Funkverkehr). Eine Read-Characteristic liefert die
Firmware-Version als UTF-8-String (`FIRMWARE_VERSION`) - damit erkennt die
App, ob ein Update verfügbar ist (siehe "Firmware-Update per BLE" weiter
unten). Eine weitere Read-Characteristic (`6E40000A-…`) liefert Modell und
Fähigkeiten als UTF-8-JSON:

```json
{"model":"light-t7","name":"Massarbeit Waage Light","fw":"1.7.0",
 "variant":"light-t7",
 "caps":{"display":false,"battery":true,"buttons":1,"led":true,"ota":true}}
```

Daran erkennt die App beim Verbinden, welches Modell dranhängt, und stellt
sich darauf ein (Modellname statt pauschal "Waage verbunden", passende
Firmware-Variante beim Update, keine Texte, die ein Display voraussetzen).
JSON statt einer Byte-Bitmaske ist Absicht: es lässt sich um ein Feld
erweitern, ohne dass App und Firmware gleichzeitig aktualisiert werden
müssen. Fehlt die Characteristic (ältere Firmware), nimmt die App "große
Waage mit Display" an - alte Geräte funktionieren unverändert weiter.

Eine Write-Characteristic nimmt Kommandos entgegen:

| Byte(s) | Bedeutung |
|---|---|
| `0x01` | Tare |
| `0x10` | Display zurück auf normale Gewichtsanzeige |
| `0x11` | Display zeigt "Bereit, jetzt trinken" (Turn-Readiness-Ritual der App) |
| `0x12 <Güte>` | Display zeigt Ergebnis-Rückmeldung: `0`=daneben, `1`=nah dran, `2`=Volltreffer |
| `0x13 <gameId>` | Display zeigt spielspezifische "Glas ist weg"-Animation (Golf/Dart/Blackjack/Tower — Glas komplett von der Waage gehoben, App-seitige `useSipDetector`-"away"-Phase) |
| `0x14 <gameId><r><g><b><nameLen><name>` | Zug gestartet: Namens-/Farb-Badge + Spiel-Icon oben auf dem Gewichtsscreen |
| `0x15` | Zug beendet - Badge weg |

`gameId`: `0`=keins, `1`=Golf, `2`=Dart, `3`=Blackjack, `4`=Tower, `5`=Scale
(siehe `GameKind` in `DeviceUiTypes.h`). `0x10`-`0x15` lösen keine eigene
Gewichtslogik aus - die Waage kennt kein Spielkonzept, sie zeigt nur, was
die App ihr sagt (siehe `RemoteCue`/`DeviceUi::setActivePlayer()`). Die
Kommandos sind für beide Varianten identisch - die Light rendert sie eben
als LED-Muster statt als Vollbild-Animation, die App muss davon nichts
wissen. Die
Away-/Ergebnis-Animationen sind rein prozedural (Linien/Kreise/Formen),
nur die Bootanimation nutzt echte Pixel-Art-Sprites (siehe `data/`).

## Bauen & Flashen

```
pio run -e t-display-s3 -t upload          # grosse Waage, ueber USB-C
```

```
pio run -e massarbeit-light-t7 -t upload   # Waage Light, ueber USB
```

```
pio device monitor
```

Welche Variante gebaut wird, entscheidet allein das Environment: es setzt
`-DMASSARBEIT_VARIANT_PRO` bzw. `-DMASSARBEIT_VARIANT_LIGHT`, woraufhin
`include/BoardConfig.h` das passende Board-Profil einbindet. Fehlt das Flag,
bricht der Build mit einer `#error`-Meldung ab, statt still etwas Falsches
zu bauen.

Nur grosse Waage: die Boot-Sprite-Frames (`data/`) liegen auf einer eigenen
SPIFFS-Partition (siehe `platformio.ini`, `board_build.filesystem = spiffs`)
und werden vom normalen Firmware-Flash NICHT mit übertragen - nur nötig, wenn
sich an `data/` etwas ändert (nicht bei jedem Code-Flash). Die Light hat
weder Sprites noch SPIFFS:

```
pio run -e t-display-s3 -t uploadfs --upload-port <PORT>
```

## Firmware-Update per BLE (aus der App)

Die "Maßarbeit App" hat unter Einstellungen einen "Firmware"-Bereich: sie
lädt `firmware/manifest.json` + die zum verbundenen Modell passende `.bin`
aus diesem Repo per `raw.githubusercontent.com` herunter (kein Backend, kein
Token nötig - das Repo ist public) und überträgt sie in Chunks
(`OtaUpdater.h`) direkt per Bluetooth an die Waage. Welche `.bin` die
richtige ist, sagt die Geräte-Info-Characteristic (`variant`, siehe
Abschnitt "BLE") - eine S3-Firmware landet also nie auf einer Light. Kein WLAN am Partyort nötig - nur das Handy braucht
Internet zum Herunterladen.

Sicherheitsmechanismus: die neue Firmware landet in der inaktiven
OTA-Partition (`app0`/`app1`, siehe Board-Partitionstabelle). Erst wenn
Größe UND MD5 exakt passen, schaltet `Update.end(true)` die Waage beim
nächsten Neustart auf die neue Partition um. Bricht die Übertragung ab
(Verbindungsverlust, falsche Prüfsumme, Timeout), bleibt unverändert die
bisherige Firmware aktiv - die Waage kann durch ein fehlgeschlagenes
Update nicht "gebrickt" werden.

**Wichtig:** Ein App-BLE-Update überträgt nur die App-Partition, nie die
SPIFFS-Partition (`data/`, siehe unten). Ein Gerät, das per BLE von
einer Firmware ohne Sprite-Bootanimation aktualisiert wird, bootet danach
zwar sofort mit dem neuen Code, zeigt aber (bis einmal per USB + `pio run
-t uploadfs` die Frames übertragen wurden) keine Bootanimation - fällt
sauber auf einen leeren Screen für die `MIN_VISIBLE_MS`-Mindestdauer zurück,
kein Hänger.

### Release bauen

1. `FIRMWARE_VERSION` in `include/Config.h` hochzählen (bewusst von Hand -
   das Skript zählt nicht selbst hoch).
2. Release bauen:

```bash
python scripts/release.py
```

Das baut beide Varianten, kopiert die Binaries nach `firmware/`, rechnet
Größe + MD5 aus und schreibt beides in `firmware/manifest.json`. Nur eine
Variante geht auch: `python scripts/release.py light-t7`; der Eintrag der
anderen bleibt dabei erhalten.

3. Committen + auf `main` pushen - die App erkennt das neue Manifest beim
   nächsten Öffnen der Einstellungen.

Der frühere 5-Schritte-Handbetrieb (bauen, `md5sum`, kopieren, Manifest von
Hand pflegen) war schon mit einer Variante fehleranfällig und wird mit zweien
unzumutbar: ein vergessener MD5 fällt erst am Gerät auf, wenn das Update
heruntergeladen, geprüft und verworfen wurde.

**Manifest-Format:** `variants` ordnet jeder Modell-Kennung (dem `variant`
aus der Geräte-Info) ihre Datei zu. Die alten Top-Level-Felder
(`version`/`file`/`size`/`md5`) bleiben zusätzlich als Spiegel der
S3-Variante bestehen, damit ein alter, im Browser gecachter App-Stand nicht
kaputtgeht.

## Entwicklungs-OTA

Nur fuer die Entwicklungsphase, wenn direkt am Code gearbeitet wird (fuer
fertige Releases an die Party-Waage siehe "Firmware-Update per BLE" oben -
siehe `src/DevOta.h` fuer den Hintergrund). **Nur grosse Waage:** der Light
fehlt der zweite Taster fuer die Aktivierung, dort sind alle DevOta-Methoden
leere Attrappen und der WLAN-Code faellt komplett aus dem Binary.

1. In `include/Config.h` `DEV_WIFI_SSID`/`DEV_WIFI_PASSWORD` eintragen (nicht committen, falls das Repo geteilt wird).
2. Board per USB flashen, dabei **Taste 2 gedrueckt halten**, bis "Aktiviere WLAN + ArduinoOTA" im Serial-Log erscheint.
3. IP-Adresse aus dem Serial-Log ablesen.
4. Danach kabellos updaten:
   ```
   pio run -e t-display-s3-ota -t upload --upload-port <IP>
   ```

Ohne gehaltene Taste 2 beim Boot bleibt WLAN komplett aus (Akkuschonung im
normalen Party-Betrieb).
