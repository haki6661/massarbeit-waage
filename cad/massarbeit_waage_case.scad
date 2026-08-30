// Maßarbeit Waage - Gehäuse (v1, zum Testdrucken)
//
// Eigenes, parametrisches Design - NICHT von WeighMyBru2s Original-CAD
// (cad/step im weighmybru2-Repo) abgeleitet. Das war dort auf komplett
// andere Hardware zugeschnitten (separates ESP32-Board + separates Display
// + kapazitive Touch-Pads, je eigener "Clamp"), passt geometrisch nicht zu
// unserer Hardware (LilyGO T-Display S3 in einem Stück, physische Taster,
// groessere 3kg-Waegezelle). Uebernommen wurde nur das Grundkonzept
// (Bottom/Top-Split, Waegezelle einseitig fest verschraubt, freies Ende
// traegt die Plattform).
//
// Alle Bauteil-Masse stammen aus echten Messungen (siehe Chat-Verlauf),
// nicht aus Datenblaettern - fuer ein eng anliegendes 3D-Druck-Gehaeuse
// zuverlaessiger als Referenzwerte, die je nach Fertigungscharge abweichen
// koennen.
//
// Mechanik: Standard-Single-Point-Aufbau, jedes Ende hat ZWEI Loecher
// (verhindert Verdrehen/Kippeln):
//   - Festes Ende (2x M4, Kabelaustritt): liegt auf einem Sockel in der
//     Bodenschale, Schrauben kommen von unten durch den Sockel in die Zelle.
//   - Freies Ende (2x M3): traegt die separate Plattform. Die Plattform hat
//     KEINEN Kontakt zur Bodenschale - nur zur Waegezelle - sonst verfaelscht
//     das die Messung. Schrauben kommen von oben durch die Plattform in die
//     Zelle (siehe platform()-Modul).
//
// Rendern (Kommandozeile):
//   openscad -o base.stl     -D part=\"base\"     massarbeit_waage_case.scad
//   openscad -o platform.stl -D part=\"platform\" massarbeit_waage_case.scad
//   openscad -o preview.png  -D part=\"preview\"  --imgsize=1400,1000 --autocenter --viewall massarbeit_waage_case.scad

part = "preview"; // "base" | "platform" | "preview" (beide zusammen, Sichtkontrolle)

$fn = 48;

// ---------------------------------------------------------------------
// Gemessene Bauteil-Masse
// ---------------------------------------------------------------------
wall = 2.4;  // Wandstaerke
clr  = 0.4;  // allgemeines Spiel pro Seite (Steckpassung)

// LilyGO T-Display S3 (L x B x H, H = hoechster Punkt: USB-C-Buchse)
board_l = 62;
board_w = 26;
board_h = 10;

// HX711-Modul
hx_l = 35;
hx_w = 20;
hx_h = 10; // Annahme (Pinleisten/Bauteile) - bei Bedarf anpassen

// Waegezelle YZC-133 (12.7 x 12.7 x 80mm Alu-Balken)
lc_l = 80;
lc_w = 12.7;
lc_h = 12.7;
// Jedes Ende hat ZWEI Loecher (verhindert Verdrehen): eines nahe der Kante,
// eines bei 20mm - auf beiden Enden identisch beabstandet.
lc_fixed_hole_d       = 4.3;  // M4-Durchgangsloch fuer die Schraube
lc_fixed_hole_offset1 = 5.0;  // Kabelaustritts-Ende -> Lochmitte, Loch 1
lc_fixed_hole_offset2 = 20.0; // Kabelaustritts-Ende -> Lochmitte, Loch 2
lc_load_hole_d        = 3.4;  // M3-Durchgangsloch
lc_load_hole_offset1  = 5.25; // freies Ende -> Lochmitte, Loch 1
lc_load_hole_offset2  = 20.0; // freies Ende -> Lochmitte, Loch 2

// Akku 3000mAh LiPo (Zellcode 103665 = 10 x 36 x 65mm, T x B x L)
bat_l = 65;
bat_w = 36;
bat_h = 10;

// ---------------------------------------------------------------------
// Abgeleitetes Layout
// ---------------------------------------------------------------------
lc_boss_h      = 8; // Sockelhoehe am festen Ende - Zelle liegt OBEN drauf
lc_clear_under = 5; // Luftspalt unter der freien Balkenlaenge (Durchbiegung
                     // unter 3kg ist Bruchteile eines mm - 5mm ist
                     // grosszuegig, laesst noch Platz fuer Kabel)
lc_boss_len    = lc_fixed_hole_offset2 + 6; // deckt beide Loecher + Rand ab
margin         = 4; // Luft zwischen Zellenenden und Buchtwaenden

sensor_bay_w = max(lc_w + 16, board_w + 6);
sensor_bay_l = lc_l + 2*margin;

elec_bay_w    = bat_w + 2*clr + 4;
elec_bay_l    = max(bat_l, hx_l) + 2*clr + 6;
hx_standoff_h = 3; // Stand zwischen Akku-Oberseite und HX711
elec_bay_h    = bat_h + hx_standoff_h + hx_h + clr;

// Anzeige-Turm hinten: Board steht senkrecht, Bildschirm zeigt nach oben/vorn
// - USB-C+Taster-Kante zeigt nach hinten aus dem Gehaeuse. (Schraege Neigung
// ist fuer v2 vorgesehen, fuer den Testdruck erstmal gerade - einfacher zu
// pruefen, ob Mechanik/Masse passen.)
tower_w     = board_w + 2*wall + 2*clr;
tower_depth = 18;

base_int_w = max(sensor_bay_w, elec_bay_w, tower_w);
base_int_l = sensor_bay_l + elec_bay_l;
base_int_h = max(lc_boss_h + lc_h + 3, elec_bay_h + wall);

base_ext_w = base_int_w + 2*wall;
base_ext_l = base_int_l + 2*wall;
base_ext_h = base_int_h + wall;

// X-Koordinaten der Waegezelle innerhalb der Bodenschale: freies Ende zeigt
// nach vorn (kleines X), festes Ende (Kabelaustritt) nach hinten zur
// Elektronik-Bucht (grosses X).
lc_x_free_edge  = wall + margin;
lc_x_fixed_edge = lc_x_free_edge + lc_l;
lc_y_center     = base_ext_w / 2;

hole_fixed1_x = lc_x_fixed_edge - lc_fixed_hole_offset1;
hole_fixed2_x = lc_x_fixed_edge - lc_fixed_hole_offset2;
hole_load1_x  = lc_x_free_edge + lc_load_hole_offset1;
hole_load2_x  = lc_x_free_edge + lc_load_hole_offset2;

// Plattform (lose Deckplatte, traegt Glas/Becher)
platform_l   = 100;
platform_w   = 80;
platform_t   = 4;
platform_boss_w = lc_w + 5;
platform_boss_len = lc_load_hole_offset2 - lc_load_hole_offset1 + 10; // deckt beide Loecher ab
platform_boss_h = base_ext_h - (wall + lc_boss_h + lc_h); // bis exakt auf Zellen-Oberkante
platform_hole1_x = 15;                              // lokale Plattform-Koordinate
platform_hole2_x = platform_hole1_x + (lc_load_hole_offset2 - lc_load_hole_offset1);

// ---------------------------------------------------------------------
// Bodenschale
// ---------------------------------------------------------------------
module base_shell() {
    difference() {
        union() {
            // Hauptkorpus
            cube([base_ext_l, base_ext_w, base_ext_h]);

            // Anzeige-Turm hinten, senkrecht (v1 - Neigung folgt in v2).
            // 0.5mm Ueberlappung nach unten - gleicher Grund wie bei den
            // Sockeln (siehe Kommentare weiter unten).
            translate([base_ext_l - tower_depth, (base_ext_w - tower_w) / 2, base_ext_h - 0.5])
                cube([tower_depth, tower_w, board_l * 0.6 + 0.5]);
        }

        // Innenraum aushoehlen (offen nach oben)
        translate([wall, wall, wall])
            cube([base_int_l, base_int_w, base_int_h + 1]);

        // Waegezellen-Fach unter Balkenhoehe absenken (ausser dort, wo der
        // Sockel fuer das feste Ende steht - der wird danach wieder
        // aufgesetzt, siehe union() unten).
        translate([wall, lc_y_center - lc_w/2 - clr - 1, wall + lc_boss_h - lc_clear_under])
            cube([sensor_bay_l, lc_w + 2*clr + 2, lc_clear_under + 1]);

        // Kabeldurchfuehrung vom Waegezellen-Fach in die Elektronik-Bucht
        translate([lc_x_fixed_edge - 3, lc_y_center - 4, wall])
            cube([12, 8, lc_boss_h + 4]);

        // Zugriffsloecher von unten fuers Verschrauben am festen Ende (2x)
        for (hx = [hole_fixed1_x, hole_fixed2_x]) {
            translate([hx, lc_y_center, -1])
                cylinder(d = lc_fixed_hole_d + 1.5, h = wall + lc_boss_h + 2);
        }

        // USB-C + Taster-Ausschnitt am Turm (kurze Kante der Platine zeigt
        // nach hinten raus)
        translate([base_ext_l - tower_depth - 1, (base_ext_w - (board_w + 2*clr)) / 2, base_ext_h + board_l * 0.3])
            cube([tower_depth + 2, board_w + 2*clr, board_h + 2*clr]);
    }

    // Sockel fuer das feste Waegezellen-Ende (Zelle liegt oben drauf).
    // Ueberlappt 0.5mm in den Boden hinein, sonst verschmilzt CGAL die
    // beiden Koerper nicht zu einem einzigen druckbaren Volumen (Boolean
    // Union auf exakt deckungsgleichen Flaechen ist nicht zuverlaessig).
    translate([lc_x_fixed_edge - lc_boss_len, lc_y_center - lc_w/2 - clr, wall - 0.5])
        cube([lc_boss_len, lc_w + 2*clr, lc_boss_h + 0.5]);
}

// ---------------------------------------------------------------------
// Lose Deckplatte / Plattform
// ---------------------------------------------------------------------
module platform() {
    difference() {
        union() {
            cube([platform_l, platform_w, platform_t]);
            // Auflage-/Schraubdom, trifft exakt auf die Zellen-Oberkante,
            // deckt beide Loecher des freien Endes ab. 0.5mm Ueberlappung
            // in die Platte hinein - gleicher Grund wie beim Sockel oben.
            translate([platform_hole1_x - 5, platform_w/2 - platform_boss_w/2, -platform_boss_h])
                cube([platform_boss_len, platform_boss_w, platform_boss_h + 0.5]);
        }
        // Schraubenloecher von oben durch Platte + Dom in die Zelle (2x M3)
        for (hx = [platform_hole1_x, platform_hole2_x]) {
            translate([hx, platform_w/2, -platform_boss_h - 1])
                cylinder(d = lc_load_hole_d + 0.6, h = platform_t + platform_boss_h + 2);
            // Senkung fuer den Schraubenkopf
            translate([hx, platform_w/2, platform_t - 2.2])
                cylinder(d1 = lc_load_hole_d + 0.6, d2 = 7, h = 2.21);
        }
    }
}

// ---------------------------------------------------------------------
// Ausgabe
// ---------------------------------------------------------------------
if (part == "base") {
    base_shell();
} else if (part == "platform") {
    platform();
} else {
    // Sichtkontrolle: beide Teile ungefaehr in eingebauter Position
    // (Feinausrichtung ist fuers Rendern irrelevant - beide Teile werden
    // separat gedruckt).
    color("SteelBlue") base_shell();
    color("Goldenrod", 0.85)
        translate([lc_x_free_edge - platform_hole1_x, (base_ext_w - platform_w) / 2, base_ext_h + lc_clear_under])
            platform();
}
