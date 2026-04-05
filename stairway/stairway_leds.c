#define FILE_ID "SW01"

#include "global.h"
#include "api/stairway_leds.h"
#include "api/ws2812_pio_api.h"
#include <stdlib.h>
#include <string.h>

#define LED_ON           250
#define LED_OFF          0
#define LED_ON_STEP_CNT  10
#define LED_OFF_STEP_CNT 10

typedef struct {
    uint8_t led_value;
    bool state;
} LedState;

static Ws2812_Header ws2812_handler = {0};
static LedState *buf = {NULL};
static uint8_t led_on_step = 1;
static uint8_t led_off_step = 1;

ErrCode stairway_leds_init(uint32_t pin, uint32_t led_n) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(led_n == 0, ERR_PARAM_INVALID);
    RETURN_IF_ERROR(ws2812_init(pin, led_n, &ws2812_handler));
    buf = malloc(sizeof(LedState) * led_n);
    TO_EXIT_IF_COND(buf == NULL, ERR_MEM_ALLOC_FAIL);
    memset(buf, 0, sizeof(LedState) * led_n);

    led_on_step = ((LED_ON - LED_OFF) / LED_ON_STEP_CNT);
    led_off_step = ((LED_ON - LED_OFF) / LED_OFF_STEP_CNT);
    if (led_on_step == 0) {
        led_on_step = 1;
    }
    if (led_off_step == 0) {
        led_off_step = 1;
    }

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

ErrCode stairway_leds_set_state(uint32_t index, bool state) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(index >= ws2812_handler.led_count, ERR_PARAM_INVALID);

    buf[index].state = state;

    return err;
}

ErrCode stairway_leds_refresh(void) {
    ErrCode err = ERR_SUCCESS;

    for (uint32_t i = 0; i < ws2812_handler.led_count; i++) {
        if ((buf[i].state) && (buf[i].led_value < LED_ON)) {
            int16_t temp = (int16_t)buf[i].led_value + led_on_step;
            if (temp > LED_ON) {
                buf[i].led_value = LED_ON;
            } else {
                buf[i].led_value = temp;
            }
        }
        if ((!buf[i].state) && (buf[i].led_value > LED_OFF)) {
            int16_t temp = (int16_t)buf[i].led_value - led_off_step;
            if (temp < LED_OFF) {
                buf[i].led_value = LED_OFF;
            } else {
                buf[i].led_value = temp;
            }
        }
        RETURN_IF_ERROR(ws2812_set_color(&ws2812_handler, i, buf[i].led_value, buf[i].led_value, buf[i].led_value));
    }

    RETURN_IF_ERROR(ws2812_refresh(&ws2812_handler));

    return err;
}
