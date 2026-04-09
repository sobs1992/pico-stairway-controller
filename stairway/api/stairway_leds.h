#pragma once

#include <stdint.h>

typedef enum { EMERGENCY_MIN = 0, EMERGENCY_UP = EMERGENCY_MIN, EMERGENCY_DOWN, EMERGENCY_MAX } EmergencyType;

ErrCode stairway_leds_init(uint32_t pin);
ErrCode stairway_leds_deinit(void);
ErrCode stairway_leds_set_state(uint32_t index, bool state, uint64_t event_ts);
ErrCode stairway_emergency_leds(EmergencyType type, bool state);
ErrCode stairway_leds_refresh(void);