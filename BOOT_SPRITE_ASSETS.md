# Boot-Sprite-Assets

Bootanimation (Dart fliegt auf eine dunkle Flaeche, endet mit dem
"MaßArbeit"-Schriftzug) - wird von `TftDisplay::playBootSprite()` beim Start
gezeigt, parallel zur eigentlichen Initialisierung (siehe `main.cpp`,
`runNextBootStep()`).

## Format

- `f000.raw` … `f046.raw`: 47 Frames, je 320×170px, 8-Bit indiziert
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

**Frame-Anzahl (47):** SPIFFS' nutzbare Kapazität liegt deutlich unter der
rohen Partitionsgröße (3,5MB) - Dateisystem-Overhead/Wear-Leveling-Reserve.
Per Binärsuche mit `pio run -t buildfs` (baut nur das FS-Image lokal, ohne
Geraet) ermittelt: 47 Frames (~2,55MB) passen, 48 Frames (~2,61MB) schlagen
mit `SPIFFS_write error(-10001): File system is full` fehl - 47 ist also
das Maximum. Bei Bedarf mit `FRAME_COUNT` im Erzeugungs-Skript (unten)
anpassen und die Grenze erneut per `pio run -t buildfs` pruefen, bevor
`uploadfs` real aufs Geraet schreibt.

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

Quelle: vom Nutzer bereitgestelltes GIF `animation/boot.gif` (640×360px,
101 Frames, 10fps, Lizenz/Urheberschaft liegt beim Nutzer). Erzeugt mit
einem Einmal-Skript (nicht Teil des Repos) über Pillow:

1. Jeden Frame auf 320×170 skalieren (`Image.resize(..., Image.LANCZOS)`,
   direkt gestaucht statt Crop - bei diesem dunklen, zentrierten Motiv
   nicht sichtbar, siehe Diskussion im PR).
2. `FRAME_COUNT` (47, siehe oben) gleichmäßig über alle Quell-Frames
   verteilte Indizes auswählen (`round(i * (total-1) / (FRAME_COUNT-1))`),
   Start- und End-Frame sind dabei garantiert enthalten.
3. Eine gemeinsame 256-Farben-Palette aus einem Komposit von Start-,
   Mittel- und End-Frame bilden (`Image.quantize(colors=256,
   method=Image.MEDIANCUT)`), damit die Farben über alle Frames konsistent
   bleiben (kein Palette-Flackern).
4. Jeden ausgewählten Frame mit Floyd-Steinberg-Dithering auf diese Palette
   quantisieren, als rohe Index-Bytes speichern.
5. Palette als RGB565 speichern.

Nach Änderungen an den Frames: `pio run -t uploadfs --upload-port <PORT>`
nicht vergessen - das normale Firmware-Flashen (`-t upload`) überträgt nur
den Programmcode, nicht die SPIFFS-Partition.
