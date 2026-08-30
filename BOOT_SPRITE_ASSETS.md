# Boot-Sprite-Assets

Pixel-Art-Bootanimation (Tavernen-Charakter trinkt Bier, endet mit dem
"Maßarbeit"-Schriftzug) - wird von `TftDisplay::playBootSprite()` beim Start
gezeigt, parallel zur eigentlichen Initialisierung (siehe `main.cpp`,
`runNextBootStep()`).

## Format

- `f000.raw` … `f033.raw`: 34 Frames, je 320×170px, 8-Bit indiziert
  (1 Byte/Pixel = Palette-Index), roh ohne Header.
- `pal.raw`: gemeinsame 256-Farben-Palette, 256 × `uint16_t` RGB565
  Little-Endian (512 Byte).

Indiziert statt direkt RGB565, weil `Arduino_GFX::drawIndexedBitmap()`
denselben Pro-Pixel-Zeichenaufwand hat wie `draw16bitRGBBitmap()` (siehe
`Arduino_GFX.cpp`) - bei gleicher Zeichengeschwindigkeit passen so doppelt so
viele Frames in den verfügbaren Flash-Speicher. Liegt auf SPIFFS (siehe
`platformio.ini`, `board_build.filesystem = spiffs`) statt als C-Array im
Programmcode, weil 50 Frames à 320×170 Byte als Text-Quellcode mehrere MB
groß wären.

**Frame-Anzahl (34 statt anfangs 50):** SPIFFS' nutzbare Kapazität liegt
deutlich unter der rohen Partitionsgröße (3,5MB) - Dateisystem-Overhead/
Wear-Leveling-Reserve. 50 Frames (~2,7MB) passten beim lokalen Bauen des
Images nicht mehr rein (`SPIFFS_write error(-10001): File system is full`),
34 Frames (~1,85MB) passen komfortabel. Bei Bedarf mit `STEP` im
Erzeugungs-Skript (unten) anpassen - `pio run -t uploadfs` schlägt beim
Bauen des Images fehl, falls es wieder zu voll wird, ohne dass etwas auf
die Waage geschrieben wird (sicherer Fehlschlag).

**Bewusst flach, kein Unterordner** (nicht `data/boot/…`): SPIFFS' Pfad-
aufloesung fuer Unterordner ist unzuverlaessig - ein `stat()` auf einen Pfad
mit Unterordner kann fehlschlagen und dann still auf einen (leeren)
Directory-Handle zurueckfallen. Der gibt sich als "erfolgreich geoeffnet"
aus, liefert bei jedem `read()` aber 0 Byte - kein Fehler im Log, nur eine
leere/schwarze Palette und einen leeren Frame-Puffer. Genau das ist beim
ersten Test passiert (Boot-Log ueber USB komplett fehlerfrei, Display auf
dem echten Geraet trotzdem durchgehend schwarz) - seitdem alles flach unter
`data/` statt `data/boot/`.

## Herkunft / Neu erzeugen

Quelle: vom Nutzer bereitgestelltes GIF (320×170px, 100 Frames, 10fps,
Lizenz/Urheberschaft liegt beim Nutzer). Erzeugt mit einem Einmal-Skript
(nicht Teil des Repos) über Pillow:

1. Jeden 3. Frame des Original-GIFs auswählen (0, 3, 6, …, 99 → 34 Frames,
   `STEP = 3`).
2. Eine gemeinsame 256-Farben-Palette aus einem Komposit von Start-,
   Mittel- und End-Frame bilden (`Image.quantize(colors=256,
   method=Image.MEDIANCUT)`), damit die Farben über alle Frames konsistent
   bleiben (kein Palette-Flackern).
3. Jeden ausgewählten Frame mit Floyd-Steinberg-Dithering auf diese Palette
   quantisieren, als rohe Index-Bytes speichern.
4. Palette als RGB565 speichern.

Nach Änderungen an den Frames: `pio run -t uploadfs --upload-port <PORT>`
nicht vergessen - das normale Firmware-Flashen (`-t upload`) überträgt nur
den Programmcode, nicht die SPIFFS-Partition.
