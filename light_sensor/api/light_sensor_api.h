#pragma once

#include <global.h>

ErrCode light_sensor_init(uint32_t pin);
ErrCode light_sensor_get_data(bool *state, uint32_t *raw);