#define FILE_ID "LS01"

#include "global.h"
#include "hardware/adc.h"
#include "api/settings_api.h"

#define FILTER_N 32

static const uint8_t adc_gpio[] = {[0] = 26, [1] = 27, [2] = 28, [3] = 29, [4] = 4};
static int32_t filtered_adc = INT32_MAX;
static bool trigger = false;
static Settings *settings = NULL;

ErrCode light_sensor_init(uint32_t pin) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(pin >= ARRAY_SIZE(adc_gpio), ERR_PARAM_INVALID);

    settings = settings_get();

    adc_init();
    adc_gpio_init(adc_gpio[pin]);
    adc_select_input(pin);

    return err;
}

ErrCode light_sensor_get_data(bool *state, uint32_t *raw) {
    ErrCode err = ERR_SUCCESS;

    uint16_t adc = adc_read();
    if (filtered_adc != UINT32_MAX) {
        filtered_adc += ((int32_t)adc - filtered_adc) / FILTER_N;
    } else {
        filtered_adc = adc;
    }

    uint32_t voltage = 3300 * filtered_adc / (1 << 12);
    if (voltage > settings->light_sensor_day_value) {
        trigger = true;
    } else if (voltage < settings->light_sensor_night_value) {
        trigger = false;
    }

    if (state) {
        *state = trigger;
    }
    if (raw) {
        *raw = voltage;
    }

    return err;
}