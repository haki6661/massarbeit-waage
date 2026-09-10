#pragma once

// Fuer MASSARBEIT_BLE_NAME/-MODEL_* (BLE-Geraetename und Geraete-Info sind
// variantenabhaengig, siehe include/boards/).
#include "BoardConfig.h"

// ============================================================================
// Firmware-Version
// ----------------------------------------------------------------------------
// Bei jedem Release-Build hochzaehlen (SemVer) - wird per BLE_VERSION_CHAR_UUID
// ausgelesen, damit die App in den Einstellungen erkennt, ob eine neuere
// Firmware im GitHub-Repo verfuegbar ist (siehe BLE_OTA_*-Abschnitt unten
// und SettingsScreen im App-Repo).
// ============================================================================
#define FIRMWARE_VERSION "1.12.2"

// ============================================================================
// BLE-Konfiguration
// ----------------------------------------------------------------------------
// Service-/Characteristic-UUIDs bewusst von WeighMyBru2 uebernommen (dessen
// "Bean Conqueror"-Format: simples 4-Byte-Little-Endian-Float, read+notify).
// Das GaggiMate-Protokoll (20-Byte-Pakete, Heartbeats, Maschinen-Kommandos)
// wurde NICHT uebernommen - fuer Maßarbeit unnoetiger Ballast.
// Beibehalten der UUIDs kostet nichts und haelt die Tuer offen, falls eine
// bestehende App (z.B. Bean Conqueror) das Geraet zufaellig schon lesen kann.
// ============================================================================

// Je Variante ein eigener Name (siehe Board-Profil), damit die Vision
// und die Basis im Bluetooth-Dialog des Handys auseinanderzuhalten sind.
#define BLE_DEVICE_NAME MASSARBEIT_BLE_NAME

#define BLE_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
// Gewicht: read+notify, <float32 LE Gramm><uint8 SensorStatus> - das
// Statusbyte (seit 1.12.0) meldet 0 = ok, 1 = HX711 antwortet nicht,
// 2 = Waegezelle fehlt (siehe SensorStatus in Scale.h). Die App zeigt damit
// oben einen Warnhinweis; aeltere Apps lesen nur die ersten vier Bytes.
#define BLE_WEIGHT_CHAR_UUID    "6E400004-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_COMMAND_CHAR_UUID   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_BATTERY_CHAR_UUID   "6E400005-B5A3-F393-E0A9-E50E24DCCA9E" // read+notify, 1 Byte: 0-100 = Prozent, 0xFF = unbekannt (USB gesteckt, siehe Battery::readPercent())
#define BLE_VERSION_CHAR_UUID   "6E400006-B5A3-F393-E0A9-E50E24DCCA9E" // read, UTF-8-String (FIRMWARE_VERSION)

// Geraete-Info: read-only, UTF-8-JSON - der Kern der App-seitigen
// Modellerkennung. Die App liest die Characteristic einmal beim Verbinden und
// stellt ihre Oberflaeche darauf ein (siehe DeviceInfo/WeightSource im
// App-Repo). Inhalt, hier am Beispiel der Basis:
//   {"model":"t-oi-plus","name":"Massarbeit","fw":"1.10.0",
//    "variant":"t-oi-plus",
//    "caps":{"display":false,"battery":true,"buttons":0,"led":true,"ota":true}}
// JSON statt einer kompakten Byte-Bitmaske ist Absicht: es laesst sich um ein
// Feld erweitern, ohne dass App und Firmware gleichzeitig aktualisiert werden
// muessen. Auf der Firmware reicht dafuer ein snprintf, kein Parser.
// Fehlt die Characteristic (aeltere Firmware), nimmt die App "Vision mit
// Display" an - alte Geraete funktionieren unveraendert weiter.
#define BLE_DEVICE_INFO_CHAR_UUID "6E40000A-B5A3-F393-E0A9-E50E24DCCA9E" // read, UTF-8-JSON

// Kalibrierung per App statt (ausschliesslich) ueber die Serial-Routine
// (siehe CalibrationRoutine.h, weiterhin als Fallback ohne Handy vorhanden).
// Rohwert-Characteristic: read+notify, int32 LE, HX711-Rohwert (Mittel aus 10
// Messungen) - von der App nach COMMAND_CALIBRATION_GET_RAW gelesen, siehe
// COMMAND_CALIBRATION_SET_FACTOR weiter unten fuer den Rueckweg.
#define BLE_CALIBRATION_CHAR_UUID "6E40000B-B5A3-F393-E0A9-E50E24DCCA9E" // read+notify, int32 LE, Rohwert

// Firmware-Update per BLE (siehe OtaUpdater.h) - loest die geplante
// WLAN-freie Ablösung des reinen Entwicklungs-OTA (siehe Abschnitt weiter
// unten) ein: die App laedt die .bin aus dem GitHub-Repo herunter und
// ueberträgt sie in Chunks direkt per Bluetooth, kein WLAN am Partyort noetig.
#define BLE_OTA_CONTROL_CHAR_UUID "6E400007-B5A3-F393-E0A9-E50E24DCCA9E" // write (mit Response)
#define BLE_OTA_DATA_CHAR_UUID    "6E400008-B5A3-F393-E0A9-E50E24DCCA9E" // write (mit Response), rohe Firmware-Chunks
#define BLE_OTA_STATUS_CHAR_UUID  "6E400009-B5A3-F393-E0A9-E50E24DCCA9E" // read+notify, 3 Byte: [state][percent][errorCode], siehe OtaUpdater.h
// Kommandos auf BLE_OTA_CONTROL_CHAR_UUID:
//   0x01 <uint32 LE Gesamtgroesse><16 Byte MD5>  Update starten (21 Byte Payload)
//   0x02                                          Laufendes Update abbrechen
// Rohe Firmware-Bytes werden in der Reihenfolge, in der sie erzeugt wurden,
// als Chunks auf BLE_OTA_DATA_CHAR_UUID geschrieben (keine Sequenznummer
// noetig - BLE ATT garantiert Zustellreihenfolge pro Verbindung bei
// Write-mit-Response). Schlaegt Groesse/MD5 nicht 1:1, bleibt die bisherige
// Firmware aktiv - siehe OtaUpdater.h fuer die Sicherheitsargumentation.

// Kommandos auf BLE_COMMAND_CHAR_UUID (siehe BleWeightService.h):
//   0x01               Tare
//   0x10               Display: zurueck zur normalen Gewichtsanzeige
//   0x11               Display: "Bereit, jetzt trinken" (Turn-Readiness-
//                       Ritual der App, siehe useSipDetector/TurnReadiness-
//                       Indicator im App-Repo)
//   0x12 <byte>         Display: Ergebnis-Guete des letzten Schlucks
//                       (0 = daneben, 1 = nah dran, 2 = Volltreffer) -
//                       Auf der LED-Leiste/dem Ring je Stufe ein eigenes
//                       Muster (siehe LedRing::renderCue()); auf dem TFT
//                       bislang nur Text/Farbe.
//   0x13 <gameId>       Display: Glas komplett von der Waage gehoben
//                       ("Abschlag") - laeuft als Endlos-Animation, bis
//                       0x10/0x12 kommt. Welche Animation (Ball/Pfeil/Karte/
//                       Block) haengt von <gameId> ab, siehe GameKind in
//                       DeviceUiTypes.h. Wird gesendet, wenn useSipDetector
//                       (App) einen zu grossen Gewichtsabfall erkennt, um
//                       ihn NICHT als normalen Schluck zu werten, sondern
//                       als "Glas weg, wird woanders getrunken" - die
//                       tatsaechliche Menge wird erst beim Zurueckstellen
//                       berechnet.
//   0x14 <gameId><r><g><b><nameLen><name>
//                       Zug gestartet: aktiver Spieler + Spiel fuers
//                       Display (Namens-/Farb-Badge oben auf dem Gewichts-
//                       screen, siehe DeviceUi::setActivePlayer()).
//                       <nameLen> = Anzahl der folgenden UTF-8-Bytes
//                       (auf dem Display ohnehin auf ca. 10 Zeichen
//                       abgeschnitten).
//   0x15               Zug beendet / kein aktiver Spieler - Badge weg.
//   0x20               Kalibrierung: aktuellen HX711-Rohwert messen (Mittel aus
//                       10 Messungen, dauert ~1s und blockiert currentWeight-
//                       Updates so lange) und ueber BLE_CALIBRATION_CHAR_UUID
//                       als int32 LE zurueckmelden.
//   0x21 <float32 LE>   Kalibrierung: neuen Kalibrierfaktor setzen und im NVS
//                       speichern (Scale::set_scale(), Faktor = Rohwert /
//                       Referenzgewicht - siehe CalibrationRoutine.cpp fuer
//                       dieselbe Formel). Unplausible Werte (<=0, NaN/Inf)
//                       werden ignoriert, der bisherige Faktor bleibt aktiv.
//   0x30 <uint16 LE holdMs> [<uint16 LE lampIntervalMs> <uint8 lampCount>]
//                       Startampel (Formel 1, siehe ROADMAP.md Punkt 1):
//                       `lampCount` rote Lampen gehen im Abstand von
//                       `lampIntervalMs` nacheinander an (LedRing::
//                       startRaceLights()) - die erste nach einem Intervall,
//                       nicht sofort, genau wie in der App -, bleiben dann
//                       `holdMs` Millisekunden stehen und schalten von selbst
//                       gemeinsam auf Gruen - kein weiteres Kommando noetig.
//                       Die beiden hinteren Felder sind optional (fehlen sie
//                       oder sind 0: LED_RING_RACE_LAMP_INTERVAL_MS/_COUNT).
//                       holdMs = 0 heisst "die App gibt Gruen von sich aus"
//                       (siehe 0x31); dann bleiben alle Lampen stehen, bis
//                       0x31/0x32 kommt. Gruen bleibt stehen, bis das Glas
//                       abgehoben ist (siehe LED_RING_RACE_GO_MIN_MS). Ohne
//                       angehaengten LED-Ring (siehe MASSARBEIT_HAS_LED_RING
//                       im Board-Profil) ein No-Op - auf der Vision also
//                       aktuell wirkungslos, siehe ROADMAP.md.
//   0x31               Startampel: sofort Gruen (fuer holdMs = 0 oben, oder
//                       um eine laufende Haltezeit vorzeitig zu beenden).
//   0x32               Startampel: Fehlstart/Abbruch - kurzes rotes
//                       Warnblinken, danach zurueck in den normalen Zustand.
// 0x10-0x15 sowie 0x30-0x32 loesen KEINE eigene Gewichtslogik aus, sie
// steuern nur, was die Geraeteanzeige gerade zeigt (TFT-Vollbild bzw.
// LED-Muster auf der Basis) - die Waage selbst weiss nichts vom Spielzustand,
// die App entscheidet und schickt nur das Anzeige-Kommando.
//
// GameKind-Werte fuer <gameId> (0x13/0x14, siehe DeviceUiTypes.h):
//   0 = keins/generisch, 1 = Golf, 2 = Dart, 3 = Blackjack, 4 = Tower, 5 = Scale, 6 = Boxen

#define BLE_WEIGHT_NOTIFY_INTERVAL_MS 50 // 20x/s, wie im WeighMyBru2-Original
#define BLE_BATTERY_NOTIFY_INTERVAL_MS 5000 // Akkustand aendert sich langsam - alle 5s reicht, spart Funkverkehr

// ============================================================================
// Kalibrierung
// ----------------------------------------------------------------------------
// Platzhalter - ungueltig fuer die 3kg-Single-Point-Zelle! Muss ueber die
// Kalibrierroutine (Vision: Taste 2 lang; Basis: Doppelklick) mit einem
// bekannten Referenzgewicht neu ermittelt werden. Danach wird der echte Wert
// automatisch im NVS (Preferences, Namespace "scale") gespeichert und dieser
// Platzhalter nicht mehr benutzt.
// ============================================================================
#define DEFAULT_CALIBRATION_FACTOR 1.0f

// ============================================================================
// Entwicklungs-OTA (WLAN + ArduinoOTA)
// ----------------------------------------------------------------------------
// NUR fuer die Entwicklungsphase. Wird ausschliesslich aktiviert, wenn Taste 2
// beim Booten gehalten wird (siehe DevOta.cpp) - im normalen Party-Betrieb
// bleibt WLAN komplett aus, um Akku zu sparen.
//
// TODO: eigene WLAN-Zugangsdaten eintragen. NICHT committen, falls dieses
// Repo geteilt/veroeffentlicht wird - stattdessen z.B. per .gitignore
// ausschliessen oder ueber eine lokale, nicht versionierte Datei einbinden.
// Der geplante spaetere Ersatz ist BLE-OTA ueber die Handy-Web-App - dieser
// Block ist bewusst isoliert in DevOta.cpp/.h, damit er sich rueckstandslos
// entfernen laesst, sobald das steht.
// ============================================================================
#define DEV_WIFI_SSID     "DEIN-WLAN-NAME"
#define DEV_WIFI_PASSWORD "DEIN-WLAN-PASSWORT"
#define DEV_OTA_HOSTNAME  "massarbeit-waage"
#define DEV_OTA_PASSWORD  "massarbeit"     // beim Upload: --auth=massarbeit (bzw. angepasst)

// ============================================================================
// WS2812B-Lichtleiste / -ring
// ----------------------------------------------------------------------------
// Zeit- und Helligkeitswerte der LED-Anzeige (siehe src/LedRing.h). Sie
// gelten je Variante nur, wenn `MASSARBEIT_HAS_LED_RING` im Board-Profil auf
// 1 steht - sonst wird der komplette Ring-Code vom Compiler als toter Code
// verworfen. Aktiv ist das derzeit auf der Basis (8er-Leiste), auf der
// Vision noch nicht.
// ============================================================================

// Harte Helligkeitsobergrenze (0-255), angewandt in LedRing::show(). WS2812B
// ziehen bei Vollweiss bis zu ~60mA je LED - 16 LEDs waeren fast 1A und
// wuerden die 16340-Zelle der Basis in kurzer Zeit leeren. 40/255 ist im
// Partylicht immer noch deutlich sichtbar, kostet aber nur einen Bruchteil.
//
// Dieser Wert ist zugleich das Strombudget des ganzen Aufbaus - vor dem
// Hochdrehen also die Stromversorgung pruefen, nicht nur das Aussehen.
// Worst Case je LED (alle drei Farben voll = Weiss): 60mA * B/255, dazu
// ~1mA Ruhestrom des LED-Controllers:
//
//   B=40 (hier)  ~10.4mA je LED     B=128  ~31mA je LED
//   B=64         ~16mA je LED       B=255  ~61mA je LED
//
// Haengt der Ring am "5V"-Pin der Basis, laeuft sein Strom durch den
// Schiebeschalter SW2 des Boards - LilyGO gibt dafuer keine Belastbarkeit
// an, die Bauform ist typisch mit 0.3A spezifiziert, und der ESP32 selbst
// braucht davon schon 40-80mA. Fuer die LEDs bleiben also ~150-200mA:
//
//   bei B=40   ~16 LEDs bequem, ~20 als Obergrenze
//   bei B=64   ~12 LEDs
//   bei B=128  ~6 LEDs
//
// Mehr LEDs (z.B. ein 32er-Ring) NICHT ueber den 5V-Pin speisen, sondern
// direkt an der Zelle abgreifen - der Ring haengt ohnehin am
// GPIO-geschalteten MOSFET (Pins::LED_RING_POWER) und wird im Schlaf
// getrennt. Ein ueberlasteter Schiebeschalter stirbt nicht schlagartig,
// seine Kontakte werden nur langsam hochohmig - das aeussert sich als
// sporadische Resets, die man dann ewig woanders sucht.
#define LED_RING_MAX_BRIGHTNESS 40

// Bildrate des Rings. 20ms = 50 Bilder/s; jedes Bild sperrt waehrend der
// WS2812B-Uebertragung kurz die Interrupts (~30µs je LED), deshalb bewusst
// getaktet und nicht bei jedem loop()-Durchlauf.
#define LED_RING_FRAME_INTERVAL_MS 20

// Wiege-Balken: ab wann er ueberhaupt erscheint und wo Vollausschlag ist
// (ein volles grosses Glas). Darunter bleibt der Ring fuer Spieler-/
// Leerlauf-Anzeige frei, statt dauerhaft eine leere Skala zu zeigen.
#define LED_RING_WEIGH_MIN_G 5.0f
#define LED_RING_WEIGH_FULL_SCALE_G 400.0f

// Startampel "Formel 1" (siehe LedRing::startRaceLights()). Lampenzahl und
// -takt muessen exakt zur App passen (LIGHT_COUNT/LIGHT_INTERVAL_MS in
// Formel1Config.ts, App-Repo) - sonst geht die Waage zu einem anderen
// Zeitpunkt auf Gruen als der Bildschirm, und wer auf die Waage schaut,
// startet zu spaet. Die App schickt beide Werte deshalb im 0x30-Kommando mit;
// die Vorgaben hier greifen nur, wenn ein aelterer App-Stand sie weglaesst.
//
// Vier Lampen statt der fuenf am echten Startgalgen: vier teilt die acht
// LEDs der Leiste (und die 16 eines Rings) glatt, jede Lampe ist genau zwei
// (bzw. vier) LEDs breit - bei fuenf Lampen laege eine Lampe zwangslaeufig
// zwischen zwei LEDs oder waere schmaler als die anderen.
#define LED_RING_RACE_LAMP_COUNT 4
#define LED_RING_RACE_LAMP_INTERVAL_MS 700
// Gruen bleibt stehen, bis das Glas abgehoben ist (Gewicht unter
// LED_RING_WEIGH_MIN_G) - wie in der App, deren gruene Lampen bis zum
// Abheben stehen. Mindestens MIN, damit ein Abheben genau auf Gruen das
// Signal nicht auf einen einzigen Frame zusammenschrumpfen laesst;
// hoechstens MAX, falls niemand abhebt (gleiche Grenze wie "Bereit").
#define LED_RING_RACE_GO_MIN_MS 600
#define LED_RING_RACE_GO_MAX_MS 20000
// Fehlstart/Abbruch: Dauer des roten Warnblinkens.
#define LED_RING_RACE_ABORT_MS 1200

// ============================================================================
// Power Management (Deep Sleep)
// ----------------------------------------------------------------------------
// Deep Sleep gibt es nur auf Varianten MIT Taster - ohne Aufweck-Taster gaebe
// es keinen Weg zurueck. Auf der Vision: Taste 1 lang druecken (siehe
// Buttons.h) oder automatisch nach AUTO_SLEEP_TIMEOUT_MS ohne
// Gewichtsaenderung und ohne Tastendruck; aufgeweckt wird ueber Taste 2
// (Pins::WAKEUP_BUTTON), bewusst nicht ueber Taste 1/GPIO0 (Strapping-Pin,
// siehe main.cpp).
//
// Die Basis hat keinen Taster: sie wird ueber den Schiebeschalter des Boards
// ausgeschaltet - wirklich stromlos statt schlafend - und kennt deshalb
// weder Deep Sleep noch Auto-Sleep. Tara und Kalibrierung laufen dort ueber
// die App (BLE 0x01 bzw. 0x20/0x21).
// ============================================================================
#if MASSARBEIT_BUTTON_COUNT >= 1
    #define AUTO_SLEEP_TIMEOUT_MS (10UL * 60UL * 1000UL) // 10 Minuten
#else
    #define AUTO_SLEEP_TIMEOUT_MS 0 // ohne Taster kein Deep Sleep, siehe oben
#endif
#define SLEEP_ACTIVITY_THRESHOLD_G 1.0f // Gewichtsaenderung, die den Inaktivitaets-Timer zuruecksetzt

// ============================================================================
// Serial / Timing
// ============================================================================
#define SERIAL_BAUD_RATE 115200
