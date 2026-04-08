#pragma once

#include "global.h"

typedef struct {
    uint16_t trigger_value[STAIRWAY_SENS_MAX];
} StairwaySensorsSettings;

typedef struct {
    uint16_t dist[STAIRWAY_SENS_MAX];
    bool state[STAIRWAY_SENS_MAX];
    bool error[STAIRWAY_SENS_MAX];
} StairwaySensorsGet;

ErrCode stairway_sensors_init(StairwaySensorsSettings *s);
ErrCode stairway_sensors_routine(void);
ErrCode stairway_sensors_get(StairwaySensorsGet *value);