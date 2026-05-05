#include "XClk.h"
#include "driver/ledc.h"

bool ClockEnable(int pin, int Hz)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_1_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = Hz,
        .clk_cfg = LEDC_AUTO_CLK   // 🔥 ESSENCIAL (corrige o crash)
    };

    if (ledc_timer_config(&timer_conf) != ESP_OK) {
        return false;
    }

    ledc_channel_config_t ch_conf = {
        .gpio_num = pin,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 1,
        .hpoint = 0
    };

    if (ledc_channel_config(&ch_conf) != ESP_OK) {
        return false;
    }

    return true;
}

void ClockDisable()
{
    ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
}