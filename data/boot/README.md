# Boot-Sprite-Assets

Pixel-Art-Bootanimation (Tavernen-Charakter trinkt Bier, endet mit dem
"Maßarbeit"-Schriftzug) - wird von `TftDisplay::playBootSprite()` beim Start
gezeigt, parallel zur eigentlichen Initialisierung (siehe `main.cpp`,
`runNextBootStep()`).

## Format

- `f000.raw` … `f049.raw`: 50 Frames, je 320×170px, 8-Bit indiziert
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

## Herkunft / Neu erzeugen

Quelle: vom Nutzer bereitgestelltes GIF (320×170px, 100 Frames, 10fps,
Lizenz/Urheberschaft liegt beim Nutzer). Erzeugt mit einem Einmal-Skript
(nicht Teil des Repos) über Pillow:

1. Jeden 2. Frame des Original-GIFs auswählen (0, 2, 4, …, 98 → 50 Frames).
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
