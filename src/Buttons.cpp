#include "Buttons.h"
#include "BoardConfig.h"

Buttons::Buttons()
    : button1_(Pins::BUTTON_1, true, true)
#if MASSARBEIT_BUTTON_COUNT >= 2
      , button2_(Pins::BUTTON_2, true, true)
#endif
{}

void Buttons::begin() {
    button1_.setDebounceMs(50);
    button1_.setPressMs(2000); // 2s bis "lang gedrueckt" gilt - bewusst laenger als Taste 2,
                               // damit Deep Sleep nicht aus Versehen beim Tarieren ausgeloest wird
#if MASSARBEIT_BUTTON_COUNT >= 2
    button2_.setDebounceMs(50);
    button2_.setPressMs(1500); // 1.5s bis "lang gedrueckt" gilt
#endif
}

void Buttons::update() {
    button1_.tick();
#if MASSARBEIT_BUTTON_COUNT >= 2
    button2_.tick();
#endif
}

void Buttons::onButton1Click(void (*callback)()) {
    button1_.attachClick(callback);
}

void Buttons::onSleepLongPress(void (*callback)()) {
    button1_.attachLongPressStart(callback);
}

#if MASSARBEIT_BUTTON_COUNT >= 2

void Buttons::onButton2Click(void (*callback)()) {
    button2_.attachClick(callback);
}

void Buttons::onCalibrationLongPress(void (*callback)()) {
    button2_.attachLongPressStart(callback);
}

#else

void Buttons::onCalibrationDoubleClick(void (*callback)()) {
    button1_.attachDoubleClick(callback);
}

#endif
