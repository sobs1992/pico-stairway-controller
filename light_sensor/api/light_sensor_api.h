#pragma once

#include <global.h>

ErrCode light_sensor_init(uint32_t pin, uint32_t off_voltage, uint32_t on_voltage);
ErrCode light_sensor_get_data(bool *state, uint32_t *raw);