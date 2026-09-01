# Maßarbeit Waage - Firmware

Firmware fuer eine **eigene Praezisionswaage** auf dem **LilyGO T-Display S3**
(ESP32-S3, eingebautes 1,9" ST7789-TFT 170x320, zwei Taster, USB-C), basierend
auf [WeighMyBru2](https://github.com/031devstudios/weighmybru2), fuer das
Maßarbeit-Trinkspiel. Ein Board macht alles: Waegezelle auslesen, BLE-Service
fuer die Handy-Web-App (siehe Schwesterprojekt "Maßarbeit App"), TFT-Anzeige.

Aktueller Stand: HX711-Handling + Kalibrierroutine + BLE-Gewichts-/Akkuservice
(inkl. Fernsteuerung des Displays durch die App, siehe Abschnitt "BLE"
weiter unten) + TFT-Anzeige mit Spieler-Badge und spielspezifischen
Away-Animationen + Pixel-Art-Sprite-Bootanimation (siehe `data/`) +
Taster-Bedienung + Deep-Sleep-Stromsparmodus + Firmware-Update direkt aus der
App per BLE (kein WLAN am Partyort nötig, siehe Abschnitt "Firmware-Update
per BLE" weiter unten) + Entwicklungs-OTA per WLAN. Die Away-/Ergebnis-
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

- LilyGO T-Display S3
- HX711 + 3kg-Single-Point-Waegezelle
- 3000mAh-LiPo am eingebauten JST-1.25mm-Laderegler

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

## Was aus WeighMyBru2 uebernommen wurde

| Modul | Status |
|---|---|
| `Scale.h/.cpp` (HX711, Smart-Filter, NVS-Kalibrierung) | 1:1 portiert, nur FlowRate-Kopplung entfernt |
| BLE-Service (NimBLE, eigene Service-UUID) | Nur das simple "Bean Conqueror"-Float-Format uebernommen (siehe `include/Config.h`), GaggiMate-Protokoll bewusst weggelassen |
| Kalibrier-Workflow (`Faktor = Rohwert / bekanntes Gewicht`) | Logik uebernommen, ueber Serial+Taster statt Web-Formular |
| Display, Touch-Sensoren, FlowRate, WebServer/WiFiManager, ESP-NOW-Relais | Nicht uebernommen (siehe Architektur-Ueberblick weiter oben im Chat) - fuer Maßarbeit nicht relevant |

## Projektstruktur

```
platformio.ini
include/
  BoardConfig.h        <- T-Display-S3-Pinbelegung
  Config.h              <- BLE-UUIDs, Kalibrier-Default, Dev-WLAN/OTA-Zugangsdaten
src/
  main.cpp              <- verdrahtet alle Module
  Scale.h/.cpp           <- HX711 (aus WeighMyBru2 portiert)
  BleWeightService.h/.cpp <- NimBLE-Service: Gewicht + Akkustand (Notify) + Tare/Display-Kommandos (Write)
  Buttons.h/.cpp          <- physische Taster statt Touch-Pads (OneButton-Lib)
  TftDisplay.h/.cpp       <- Arduino_GFX-Ausgabe: Gewicht/Status, Spieler-
                            Badge, spielspezifische Away-Animationen,
                            Sprite-Bootanimation (SPIFFS)
  Battery.h/.cpp          <- Akkuspannung (kalibrierter ADC, aus LilyGOs Beispiel) + Prozent-Schaetzung
  CalibrationRoutine.h/.cpp <- interaktive Kalibrierung ueber Serial+Taster
  OtaUpdater.h/.cpp        <- Firmware-Update per BLE (Chunks -> Update.h)
  DevOta.h/.cpp           <- WLAN + ArduinoOTA, nur fuer die Entwicklungsphase
data/
  f000.raw … f049.raw     <- Boot-Sprite-Frames (SPIFFS, siehe data/README.md)
  pal.raw                 <- gemeinsame 256-Farben-Palette dafuer
firmware/
  manifest.json          <- Version + Groesse + MD5 der aktuellen Release-.bin
  t-display-s3.bin        <- Release-Binary, von der App per BLE geladen
```

## Bedienung

- **Taste 1** kurz: Tara
- **Taste 1** lang (~2s): sofort Deep Sleep (Display aus, ~wenige µA statt 60-150+ mA aktiv)
- **Taste 2** kurz: zwischen Gewichts- und Status-Anzeige wechseln
- **Taste 2** lang (~1,5s): Kalibrierroutine starten (siehe unten)
- **Taste 2 drücken, während die Waage schläft**: aufwecken (kompletter Neustart der Firmware - kein spezieller Resume-Pfad, das ist bei ESP32-Deep-Sleep so vorgesehen)
- **Taste 2 beim Booten halten**: WLAN + ArduinoOTA aktivieren (nur Entwicklung, siehe unten)

Zusätzlich automatischer Deep Sleep nach 10 Minuten ohne Gewichtsänderung
und ohne Tastendruck (`AUTO_SLEEP_TIMEOUT_MS` in `include/Config.h`) - außer
während Dev-OTA aktiv ist. Aufwecken geht bewusst nur über Taste 2
(GPIO14), nicht über Taste 1: GPIO0 (Taste 1) ist der BOOT-Strapping-Pin,
wäre er beim Aufwach-Bootvorgang noch gedrückt, könnte der Chip in den
Flash-Download-Modus statt in die Firmware starten.

## Kalibrieren (Schritt 4 - mit der 3kg-Zelle neu ermitteln)

1. Serial Monitor oeffnen (115200 Baud).
2. Taste 2 ca. 1,5s gedrueckt halten, bis "Kalibrierung gestartet" erscheint.
3. Waage leeren, Enter im Serial Monitor druecken.
4. Bekanntes Referenzgewicht auflegen, dessen Gramm-Zahl eingeben (z.B. `500`), Enter druecken.
5. Neuer Kalibrierfaktor wird berechnet, geloggt, aufs TFT geschrieben und
   automatisch im NVS gespeichert (`Preferences`, Namespace `scale`, Key
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
unten). Eine Write-Characteristic nimmt Kommandos entgegen:

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
(siehe `GameKind` in `TftDisplay.h`). `0x10`-`0x15` lösen keine eigene
Gewichtslogik aus - die Waage kennt kein Spielkonzept, sie zeigt nur, was
die App ihr sagt (siehe `TftDisplay::RemoteCue`/`setActivePlayer()`). Die
Away-/Ergebnis-Animationen sind rein prozedural (Linien/Kreise/Formen),
nur die Bootanimation nutzt echte Pixel-Art-Sprites (siehe `data/`).

## Bauen & Flashen

```
pio run -e t-display-s3 -t upload      # ueber USB-C
pio device monitor
```

Die Boot-Sprite-Frames (`data/`) liegen auf einer eigenen SPIFFS-
Partition (siehe `platformio.ini`, `board_build.filesystem = spiffs`) und
werden vom normalen Firmware-Flash NICHT mit übertragen - nur nötig, wenn
sich an `data/` etwas ändert (nicht bei jedem Code-Flash):

```
pio run -e t-display-s3 -t uploadfs --upload-port <PORT>
```

## Firmware-Update per BLE (aus der App)

Die "Maßarbeit App" hat unter Einstellungen einen "Firmware"-Bereich: sie
lädt `firmware/manifest.json` + die passende `.bin` aus diesem Repo per
`raw.githubusercontent.com` herunter (kein Backend, kein Token nötig - das
Repo ist public) und überträgt sie in Chunks (`OtaUpdater.h`) direkt per
Bluetooth an die Waage. Kein WLAN am Partyort nötig - nur das Handy braucht
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

1. `pio run -e t-display-s3` - baut `.pio/build/t-display-s3/firmware.bin`.
2. MD5 + Größe ermitteln, z.B. `md5sum .pio/build/t-display-s3/firmware.bin`.
3. Binary nach `firmware/t-display-s3.bin` kopieren.
4. `firmware/manifest.json` aktualisieren (`version` = neue `FIRMWARE_VERSION`
   aus `include/Config.h`, `size`, `md5`).
5. Committen + auf `main` pushen - die App erkennt das neue Manifest beim
   nächsten Öffnen der Einstellungen.

Kein CI/Build-Skript dafür (noch) - bewusst manuell, bis sich ein wirklicher
Bedarf für automatisierte Releases zeigt.

## Entwicklungs-OTA

Nur fuer die Entwicklungsphase, wenn direkt am Code gearbeitet wird (fuer
fertige Releases an die Party-Waage siehe "Firmware-Update per BLE" oben -
siehe `src/DevOta.h` fuer den Hintergrund).

1. In `include/Config.h` `DEV_WIFI_SSID`/`DEV_WIFI_PASSWORD` eintragen (nicht committen, falls das Repo geteilt wird).
2. Board per USB flashen, dabei **Taste 2 gedrueckt halten**, bis "Aktiviere WLAN + ArduinoOTA" im Serial-Log erscheint.
3. IP-Adresse aus dem Serial-Log ablesen.
4. Danach kabellos updaten:
   ```
   pio run -e t-display-s3-ota -t upload --upload-port <IP>
   ```

Ohne gehaltene Taste 2 beim Boot bleibt WLAN komplett aus (Akkuschonung im
normalen Party-Betrieb).
