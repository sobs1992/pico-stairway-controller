#pragma once

#include <global.h>
#include "api/stairway_leds.h"
#include "api/stairway_sensors.h"

#define FLASH_SIZE      2 * 1024 * 1024
#define SETTINGS_OFFSET (FLASH_SIZE - FLASH_SECTOR_SIZE)

typedef struct {
    uint32_t magic;

    bool disable_light_sensor;
    uint32_t light_sensor_day_value;
    uint32_t light_sensor_night_value;

    uint16_t dist_trigger[STAIRWAY_SENS_MAX];
    uint32_t sensor_debouce_time;
    bool sensor_up_swap;
    bool sensor_down_swap;

    uint32_t leds_time_interval;
    uint32_t leds_off_timeout;
    uint8_t led_on_value;
    uint8_t led_off_value;
    uint8_t led_on_step;
    uint8_t led_off_step;
    uint32_t led_count;

    bool disable_emergency;
    uint32_t emergency_block_ms;
    uint32_t emergency_cnt[EMERGENCY_MAX];
} Settings;

ErrCode settings_init(void);
ErrCode settings_default(void);
Settings *settings_get(void);
ErrCode settings_write(void);