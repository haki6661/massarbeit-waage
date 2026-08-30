#include "Buttons.h"
#include "BoardConfig.h"

Buttons::Buttons()
    : button1_(Pins::BUTTON_1, true, true),
      button2_(Pins::BUTTON_2, true, true) {}

void Buttons::begin() {
    button1_.setDebounceMs(50);
    button2_.setDebounceMs(50);
    button1_.setPressMs(2000); // 2s bis "lang gedrueckt" gilt - bewusst laenger als Taste 2,
                               // damit Deep Sleep nicht aus Versehen beim Tarieren ausgeloest wird
    button2_.setPressMs(1500); // 1.5s bis "lang gedrueckt" gilt
}

void Buttons::update() {
    button1_.tick();
    button2_.tick();
}

void Buttons::onTare(void (*callback)()) {
    button1_.attachClick(callback);
}

void Buttons::onSleepLongPress(void (*callback)()) {
    button1_.attachLongPressStart(callback);
}

void Buttons::onModeClick(void (*callback)()) {
    button2_.attachClick(callback);
}

void Buttons::onCalibrationLongPress(void (*callback)()) {
    button2_.attachLongPressStart(callback);
}
