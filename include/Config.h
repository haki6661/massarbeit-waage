#pragma once

// ============================================================================
// Firmware-Version
// ----------------------------------------------------------------------------
// Bei jedem Release-Build hochzaehlen (SemVer) - wird per BLE_VERSION_CHAR_UUID
// ausgelesen, damit die App in den Einstellungen erkennt, ob eine neuere
// Firmware im GitHub-Repo verfuegbar ist (siehe BLE_OTA_*-Abschnitt unten
// und SettingsScreen im App-Repo).
// ============================================================================
#define FIRMWARE_VERSION "1.2.0"

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

#define BLE_DEVICE_NAME "Massarbeit-Waage"

#define BLE_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_WEIGHT_CHAR_UUID    "6E400004-B5A3-F393-E0A9-E50E24DCCA9E" // read+notify, float32 LE, Gramm
#define BLE_COMMAND_CHAR_UUID   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_BATTERY_CHAR_UUID   "6E400005-B5A3-F393-E0A9-E50E24DCCA9E" // read+notify, 1 Byte: 0-100 = Prozent, 0xFF = unbekannt (USB gesteckt, siehe Battery::readPercent())
#define BLE_VERSION_CHAR_UUID   "6E400006-B5A3-F393-E0A9-E50E24DCCA9E" // read, UTF-8-String (FIRMWARE_VERSION)

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
//                       Grundgeruest fuer spaeter geplante Animationen je
//                       Guete-Stufe, aktuell nur Text/Farbe als Platzhalter.
//   0x13 <gameId>       Display: Glas komplett von der Waage gehoben
//                       ("Abschlag") - laeuft als Endlos-Animation, bis
//                       0x10/0x12 kommt. Welche Animation (Ball/Pfeil/Karte/
//                       Block) haengt von <gameId> ab, siehe GameKind in
//                       TftDisplay.h. Wird gesendet, wenn useSipDetector
//                       (App) einen zu grossen Gewichtsabfall erkennt, um
//                       ihn NICHT als normalen Schluck zu werten, sondern
//                       als "Glas weg, wird woanders getrunken" - die
//                       tatsaechliche Menge wird erst beim Zurueckstellen
//                       berechnet.
//   0x14 <gameId><r><g><b><nameLen><name>
//                       Zug gestartet: aktiver Spieler + Spiel fuers
//                       Display (Namens-/Farb-Badge oben auf dem Gewichts-
//                       screen, siehe TftDisplay::setActivePlayer()).
//                       <nameLen> = Anzahl der folgenden UTF-8-Bytes
//                       (auf dem Display ohnehin auf ca. 10 Zeichen
//                       abgeschnitten).
//   0x15               Zug beendet / kein aktiver Spieler - Badge weg.
// 0x10-0x15 loesen KEINE eigene Gewichtslogik aus, sie steuern nur, was
// TftDisplay gerade zeigt - die Waage selbst weiss nichts vom Spielzustand,
// die App entscheidet und schickt nur das Anzeige-Kommando.
//
// GameKind-Werte fuer <gameId> (0x13/0x14, siehe TftDisplay.h):
//   0 = keins/generisch, 1 = Golf, 2 = Dart, 3 = Blackjack, 4 = Tower, 5 = Scale

#define BLE_WEIGHT_NOTIFY_INTERVAL_MS 50 // 20x/s, wie im WeighMyBru2-Original
#define BLE_BATTERY_NOTIFY_INTERVAL_MS 5000 // Akkustand aendert sich langsam - alle 5s reicht, spart Funkverkehr

// ============================================================================
// Kalibrierung
// ----------------------------------------------------------------------------
// Platzhalter - ungueltig fuer die 3kg-Single-Point-Zelle! Muss ueber die
// Kalibrierroutine (Taste 2 lang druecken) mit einem bekannten Referenz-
// gewicht neu ermittelt werden. Danach wird der echte Wert automatisch im
// NVS (Preferences, Namespace "scale") gespeichert und dieser Platzhalter
// nicht mehr benutzt.
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
// Power Management (Deep Sleep)
// ----------------------------------------------------------------------------
// Taste 1 LANG druecken (siehe Buttons.h) -> sofort in Deep Sleep. Zusaetzlich
// automatisch nach AUTO_SLEEP_TIMEOUT_MS ohne Gewichtsaenderung UND ohne
// Tastendruck. Aufwachen immer ueber Taste 2 (GPIO14), siehe main.cpp fuer
// die Begruendung (Taste 1/GPIO0 ist ein Strapping-Pin).
// ============================================================================
#define AUTO_SLEEP_TIMEOUT_MS (10UL * 60UL * 1000UL) // 10 Minuten
#define SLEEP_ACTIVITY_THRESHOLD_G 1.0f // Gewichtsaenderung, die den Inaktivitaets-Timer zuruecksetzt

// ============================================================================
// Serial / Timing
// ============================================================================
#define SERIAL_BAUD_RATE 115200
