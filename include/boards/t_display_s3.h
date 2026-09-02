#pragma once

// Board-Profil "Pro": LilyGO T-Display S3 - die Vision mit TFT.
// Quelle: offizielles Repo https://github.com/Xinyuan-LilyGO/T-Display-S3
// (README-Pinout-Tabelle) - NICHT von einem generischen ESP32-S3-Devkit
// oder von WeighMyBru2s esp32s3-supermini/xiao-Pins uebernehmen, beide
// weichen ab.
//
// Wird nie direkt eingebunden, immer ueber include/BoardConfig.h.

#include <Arduino.h>

// --- Faehigkeits-Steckbrief ------------------------------------------------
// Steuert die #if-Zweige im restlichen Code UND wird per BLE an die App
// gemeldet, damit sie sich zur Laufzeit auf das Modell einstellen kann
// (siehe BleWeightService::buildDeviceInfoJson()).
#define MASSARBEIT_MODEL_ID        "t-display-s3"
#define MASSARBEIT_MODEL_NAME      "Massarbeit Vision"
#define MASSARBEIT_BLE_NAME        "Massarbeit-Vision"
#define MASSARBEIT_HAS_TFT         1
#define MASSARBEIT_HAS_STATUS_LED  0
#define MASSARBEIT_STATUS_LED_RGB  0
#define MASSARBEIT_HAS_BATTERY     1
#define MASSARBEIT_BUTTON_COUNT    2
#define MASSARBEIT_HAS_POWER_ON    1
#define MASSARBEIT_HAS_DEV_OTA     1

// Der S3 kann per ext0 aus dem Deep Sleep geweckt werden (der C3 der Basis
// nicht, siehe dortiges Profil).
#define MASSARBEIT_WAKEUP_USES_EXT0 1

// Spannungsteiler vor dem Batterie-ADC (Faktor 2), siehe LilyGOs
// GetBatteryVoltage-Beispiel.
#define MASSARBEIT_BATTERY_DIVIDER 2

namespace Pins {

// --- Display: 8-Bit-Parallel-Bus (Intel-8080-Timing), KEIN SPI! ------------
constexpr uint8_t LCD_BL  = 38;
constexpr uint8_t LCD_D0  = 39;
constexpr uint8_t LCD_D1  = 40;
constexpr uint8_t LCD_D2  = 41;
constexpr uint8_t LCD_D3  = 42;
constexpr uint8_t LCD_D4  = 45;
constexpr uint8_t LCD_D5  = 46;
constexpr uint8_t LCD_D6  = 47;
constexpr uint8_t LCD_D7  = 48;
constexpr uint8_t LCD_RST = 5;
constexpr uint8_t LCD_CS  = 6;
constexpr uint8_t LCD_DC  = 7;
constexpr uint8_t LCD_WR  = 8;
constexpr uint8_t LCD_RD  = 9;

// Peripherie-Power-Enable. MUSS beim Boot HIGH sein, sonst bleiben Display
// und andere Peripherie tot - laut offiziellem README auch am USB-Kabel,
// nicht nur im Batteriebetrieb.
constexpr uint8_t POWER_ON = 15;

// Physische Taster (ersetzen die kapazitiven Touch-Pads aus WeighMyBru2).
constexpr uint8_t BUTTON_1 = 0;  // auf dem Board mit "BOOT" beschriftet
constexpr uint8_t BUTTON_2 = 14;

// Welcher Taster aus dem Deep Sleep aufweckt. Bewusst NICHT Taste 1/GPIO0:
// das ist der BOOT-Strapping-Pin, waere er beim Aufwach-Boot noch gedrueckt,
// koennte der Chip in den Flash-Download-Modus starten (siehe main.cpp).
constexpr uint8_t WAKEUP_BUTTON = BUTTON_2;

// Batteriespannung: nur messbar, wenn USB-C NICHT gesteckt ist.
constexpr uint8_t BATTERY_ADC = 4;

// --- HX711 -------------------------------------------------------------
// WeighMyBru2 nutzt hierfuer GPIO5/6 - auf dem T-Display S3 sind das aber
// LCD_RST/LCD_CS! Wir weichen daher auf zwei laut offizieller Pin-Tabelle
// komplett freie, funktionslose Header-Pins aus. Vor dem Festloeten kurz
// mit einem Multimeter/Scope gegenpruefen, falls sich am Board-Revisionsstand
// etwas geaendert hat.
constexpr uint8_t HX711_DOUT = 1;
constexpr uint8_t HX711_SCK  = 2;

} // namespace Pins
