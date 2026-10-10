#pragma once

#include <global.h>
#include "api/stairway_leds.h"
#include "api/stairway_sensors.h"

#define SETTINGS_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

#define SETTINGS_MAGIC   0xDEADBEED
#define SETTINGS_VERSION 2

#define SSID_MAX_LEN 33 /* 32 chars + '\0' */
#define PASS_MIN_LEN 8  /* WPA2 requirement */
#define PASS_MAX_LEN 64 /* 63 chars + '\0' */

#define LED_COUNT_MAX 1000

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;

    bool use_light_sensor;
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

    bool use_emergency;
    uint32_t emergency_block_ms;
    uint32_t emergency_cnt[EMERGENCY_MAX];

    char ssid[SSID_MAX_LEN];
    char pass[PASS_MAX_LEN];

    bool ap_state;

    uint32_t crc; // must be the last field
} Settings;

ErrCode settings_init(void);
ErrCode settings_default(bool restart);
Settings *settings_get(void);
ErrCode settings_write(void);
// Checks values of s for consistency. Returns ERR_SUCCESS if settings can be applied.
ErrCode settings_validate(const Settings *s);
// Validates s, copies it to the active settings and writes them to flash.
ErrCode settings_apply(const Settings *s);
