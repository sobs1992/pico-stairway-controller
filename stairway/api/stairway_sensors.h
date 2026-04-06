#pragma once

#include <stdint.h>

typedef enum {
    STAIRWAY_SENS_MIN = 0,
    STAIRWAY_SENS_UP_FIRST = STAIRWAY_SENS_MIN,
    STAIRWAY_SENS_UP_SECOND,
    STAIRWAY_SENS_DOWN_FIRST,
    STAIRWAY_SENS_DOWN_SECOND,
    STAIRWAY_SENS_MAX,
} StaiwaySensorType;

typedef struct {
    uint16_t trigger_value[STAIRWAY_SENS_MAX];
} StairwaySensorsSettings;

typedef struct {
    uint16_t dist[STAIRWAY_SENS_MAX];
    bool state[STAIRWAY_SENS_MAX];
    bool error[STAIRWAY_SENS_MAX];
} StairwaySensorsGet;

ErrCode stairway_sensors_init(StairwaySensorsSettings *settings);
ErrCode stairway_sensors_routine(void);
ErrCode stairway_sensors_get(StairwaySensorsGet *value);