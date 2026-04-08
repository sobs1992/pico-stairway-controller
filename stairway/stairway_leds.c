#define FILE_ID "SW01"

#include "global.h"
#include "api/stairway_leds.h"
#include "api/ws2812_pio_api.h"
#include "api/settings_api.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t led_value;
    bool state;
    uint32_t ts;
} LedState;

static Ws2812_Header ws2812_handler = {0};
static LedState *buf = {NULL};
static bool emergency_up_state = false;
static bool emergency_down_state = false;
static bool emergency_state[EMERGENCY_MAX] = {false};
static Settings *settings = NULL;

ErrCode stairway_leds_init(uint32_t pin) {
    ErrCode err = ERR_SUCCESS;

    settings = settings_get();
    RETURN_IF_ERROR(ws2812_init(pin, settings->led_count, &ws2812_handler));
    buf = malloc(sizeof(LedState) * settings->led_count);
    TO_EXIT_IF_COND(buf == NULL, ERR_MEM_ALLOC_FAIL);
    memset(buf, 0, sizeof(LedState) * settings->led_count);

    return err;
EXIT:
    if (buf) {
        free(buf);
        buf = NULL;
    }
    ws2812_deinit(&ws2812_handler);
    return err;
}

ErrCode stairway_leds_deinit(void) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_ERROR(ws2812_deinit(&ws2812_handler));
    if (buf) {
        free(buf);
        buf = NULL;
    }

    return err;
}

ErrCode stairway_emergency_leds(EmergencyType type, bool state) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(type >= EMERGENCY_MAX, ERR_PARAM_INVALID);
    emergency_state[type] = state;

    return err;
}

ErrCode stairway_leds_set_state(uint32_t index, bool state, uint32_t event_ts) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(index >= ws2812_handler.led_count, ERR_PARAM_INVALID);

    if (buf[index].ts < event_ts) {
        buf[index].state = state;
        buf[index].ts = event_ts;
    }

    return err;
}

ErrCode stairway_leds_refresh(void) {
    ErrCode err = ERR_SUCCESS;
    bool leds_updated = false;

    for (uint32_t i = 0; i < ws2812_handler.led_count; i++) {
        if (((buf[i].state) && (buf[i].led_value != settings->led_on_value)) ||
            ((emergency_state[EMERGENCY_UP]) && (i < settings->emergency_cnt[EMERGENCY_UP])) ||
            ((emergency_state[EMERGENCY_DOWN]) &&
             (i > ws2812_handler.led_count - settings->emergency_cnt[EMERGENCY_DOWN] - 1))) {
            int16_t temp = (int16_t)buf[i].led_value + settings->led_on_step;
            if (temp > settings->led_on_value) {
                buf[i].led_value = settings->led_on_value;
            } else {
                buf[i].led_value = temp;
            }
            leds_updated = true;
        } else if ((!buf[i].state) && (buf[i].led_value != settings->led_off_value)) {
            int16_t temp = (int16_t)buf[i].led_value - settings->led_off_step;
            if (temp < settings->led_off_value) {
                buf[i].led_value = settings->led_off_value;
            } else {
                buf[i].led_value = temp;
            }
            leds_updated = true;
        }
        RETURN_IF_ERROR(ws2812_set_color(&ws2812_handler, i, buf[i].led_value, buf[i].led_value, buf[i].led_value));
    }

    if (leds_updated) {
        RETURN_IF_ERROR(ws2812_refresh(&ws2812_handler));
    }

    return err;
}
