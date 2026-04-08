#pragma once

#include <stdint.h>

typedef enum {
    DETECTOR_STATE_MIN = 0,
    DETECTOR_STATE_IDLE = DETECTOR_STATE_MIN,
    DETECTOR_STATE_FIRST_LOCK,
    DETECTOR_STATE_SECOND_LOCK,
    DETECTOR_STATE_INC,
    DETECTOR_STATE_DEC,
} DetectorState;

typedef enum {
    DETECTOR_TYPE_MIN = 0,
    DETECTOR_TYPE_UP = DETECTOR_TYPE_MIN,
    DETECTOR_TYPE_DOWN,
    DETECTOR_TYPE_MAX,
} DetectorType;

typedef enum {
    STAIRWAY_SENS_MIN = 0,
    STAIRWAY_SENS_UP_FIRST = STAIRWAY_SENS_MIN,
    STAIRWAY_SENS_UP_SECOND,
    STAIRWAY_SENS_DOWN_FIRST,
    STAIRWAY_SENS_DOWN_SECOND,
    STAIRWAY_SENS_MAX,
} StairwaySensorType;

typedef struct {
    bool current;
    bool trigger;
    uint32_t ts;
} SensorState;

typedef struct {
    DetectorState state;
    uint32_t ts;
} Detector;

typedef struct {
    Detector detector[DETECTOR_TYPE_MAX];
    SensorState sensor_state[STAIRWAY_SENS_MAX];
    int32_t people_count;
    int32_t prev_people_count;
    uint32_t people_ts;
    bool block_by_light;
    bool light_state;
    uint32_t light_value;
} Status;

Status *status_get(void);
uint32_t get_time_us(void);
uint32_t get_time_ms(void);