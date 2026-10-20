#ifndef WAAGE_POWER_H
#define WAAGE_POWER_H

#include <driver/rtc_io.h>
#include "waage_config.h"
#include "waage_display.h"

// Schaltet das Display ab und versetzt den Mikrocontroller in den Deep Sleep Modus. Das Aufwachen erfolgt über den Encoder Taster, wodurch das Programm in der setup() Routine neu startet.
inline void geheInDeepSleep() {
    displayLoeschen();

    // Aktiviert den internen Pull up Widerstand im RTC Bereich für den Deep Sleep
    rtc_gpio_pullup_en((gpio_num_t)ENCODER_SW);
    rtc_gpio_pulldown_dis((gpio_num_t)ENCODER_SW);

    esp_sleep_enable_ext0_wakeup((gpio_num_t)ENCODER_SW, 0);  // 0 = Pegel bei gedrücktem Taster
    esp_deep_sleep_start();
}

#endif