#pragma once

#include <stdint.h>

ErrCode stairway_leds_init(uint32_t pin, uint32_t led_n);
ErrCode stairway_leds_deinit(void);
ErrCode stairway_leds_set_state(uint32_t index, bool state);
ErrCode stairway_leds_refresh(void);