#define FILE_ID "MM01"

#include "global.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "api/ws2812_pio_api.h"
#include "api/stairway_leds.h"
#include "api/stairway_sensors.h"
#include "api/light_sensor_api.h"
#include "api/cli_api.h"
#include "api/commands_api.h"
#include "api/settings_api.h"
#include <tusb.h>
#include "hardware/watchdog.h"

#define WS281x_PIN       3
#define LIGHT_SENSOR_PIN 0

static const char *detector_name[DETECTOR_TYPE_MAX] = {
    [DETECTOR_TYPE_UP] = "UP",
    [DETECTOR_TYPE_DOWN] = "DOWN",
};

static const char *sensors_name[STAIRWAY_SENS_MAX] = {
    [STAIRWAY_SENS_UP_FIRST] = "UP FIRST",
    [STAIRWAY_SENS_UP_SECOND] = "UP SECOND",
    [STAIRWAY_SENS_DOWN_FIRST] = "DOWN FIRST",
    [STAIRWAY_SENS_DOWN_SECOND] = "DOWN SECOND",
};

static Status status = {0};
static Settings *settings = NULL;
static uint64_t prev_ts = 0;
static uint64_t detector_block_ts[DETECTOR_TYPE_MAX] = {0};

static bool led_on_up = false;
static bool led_on_down = false;
static bool led_off_up = false;
static bool led_off_down = false;

static uint64_t led_on_up_ts = 0;
static uint64_t led_on_down_ts = 0;
static uint64_t led_off_up_ts = 0;
static uint64_t led_off_down_ts = 0;

static uint32_t led_on_up_cnt = 0;
static uint32_t led_off_up_cnt = 0;
static uint32_t led_on_down_cnt = 0;
static uint32_t led_off_down_cnt = 0;

static uint32_t last_det = 0;

Status *status_get(void) {
    return &status;
}

inline uint64_t get_time_us(void) {
    static uint32_t last_raw_time = 0;
    static uint64_t total_us = 0;
    uint32_t ts = to_us_since_boot(get_absolute_time());
    uint32_t delta = ts - last_raw_time;
    total_us += delta;
    last_raw_time = ts;
    return total_us;
}
inline uint64_t get_time_ms(void) {
    static uint32_t last_raw_time = 0;
    static uint64_t total_ms = 0;
    uint32_t ts = to_ms_since_boot(get_absolute_time());
    uint32_t delta = ts - last_raw_time;
    total_ms += delta;
    last_raw_time = ts;
    return total_ms;
}

int main() {
    bool error_led_state = false;
    StairwaySensorsGet sen_values = {0};

    watchdog_enable(1000, true);

    stdio_init_all();
    if (cyw43_arch_init()) {
        return -1;
    }

    settings_init();
    settings = settings_get();

    light_sensor_init(LIGHT_SENSOR_PIN);
    stairway_leds_init(WS281x_PIN);

    StairwaySensorsSettings sen_settings = {0};
    memcpy(sen_settings.trigger_value, settings->dist_trigger, sizeof(settings->dist_trigger));
    stairway_sensors_init(&sen_settings);

    commands_init();

    while (1) {
        watchdog_update();
        error_led_state = false;
        if (tud_cdc_available()) {
            cli_putchar(getchar());
        }

        light_sensor_get_data(&status.light_state, &status.light_value);

        stairway_sensors_routine();
        stairway_sensors_get(&sen_values);

        for (uint32_t i = 0; i < STAIRWAY_SENS_MAX; i++) {
            if (sen_values.error[i]) {
                ANTISPAM_BEGIN(1000)
                INFO("Sensor %s error", sensors_name[i]);
                ANTISPAM_END
                error_led_state = true;
                status.sensor_state[i].current = 0;
            } else {
                status.sensor_state[i].current = sen_values.state[i];
            }
            if (status.sensor_state[i].current) {
                status.sensor_state[i].trigger = true;
            }
            status.sensor_state[i].ts = get_time_ms();
        }

        if ((status.light_state) && (!settings->disable_light_sensor)) {
            if ((status.people_count == 0) && (!status.block_by_light)) {
                INFO("Block by light (value: %d)", status.light_value);
                status.block_by_light = true;
            }
        } else {
            if (status.block_by_light) {
                INFO("Unlock by light (value: %d)", status.light_value);
                status.block_by_light = false;
            }
        }

        for (uint32_t i = 0; i < DETECTOR_TYPE_MAX; i++) {
            if (status.block_by_light) {
                status.detector[i].state = DETECTOR_STATE_IDLE;
                status.detector[i].ts = get_time_ms();
                continue;
            }

            uint32_t sens_first = (i == DETECTOR_TYPE_UP) ? STAIRWAY_SENS_UP_FIRST : STAIRWAY_SENS_DOWN_FIRST;
            uint32_t sens_second = (i == DETECTOR_TYPE_UP) ? STAIRWAY_SENS_UP_SECOND : STAIRWAY_SENS_DOWN_SECOND;

            switch (status.detector[i].state) {
                case DETECTOR_STATE_IDLE: {
                    if (status.sensor_state[sens_first].current) {
                        INFO("Detector %s first sensor is triggered", detector_name[i]);
                        status.detector[i].state = DETECTOR_STATE_FIRST_LOCK;
                        status.detector[i].ts = get_time_ms();
                    } else if (status.sensor_state[sens_second].current) {
                        INFO("Detector %s second sensor is triggered", detector_name[i]);
                        status.detector[i].state = DETECTOR_STATE_SECOND_LOCK;
                        status.detector[i].ts = get_time_ms();
                    }
                } break;
                case DETECTOR_STATE_FIRST_LOCK: {
                    if ((!status.sensor_state[sens_first].current) &&
                        ((get_time_ms() - status.detector[i].ts)) > settings->sensor_debouce_time) {
                        INFO("Detector %s go to IDLE", detector_name[i]);
                        status.detector[i].state = DETECTOR_STATE_IDLE;
                        status.detector[i].ts = get_time_ms();
                    } else if ((status.sensor_state[sens_second]
                                    .current) /*&& ((get_time_ms() - detector[i].ts)) < 1000*/) {
                        INFO("Detector %s RUN!", detector_name[i]);
                        status.detector[i].state = DETECTOR_STATE_INC;
                        status.detector[i].ts = get_time_ms();
                        status.people_count++;
                        status.people_ts = status.detector[i].ts;
                        INFO("People: %d", status.people_count);
                    }
                } break;
                case DETECTOR_STATE_SECOND_LOCK: {
                    if ((!status.sensor_state[sens_second].current) &&
                        ((get_time_ms() - status.detector[i].ts)) > settings->sensor_debouce_time) {
                        INFO("Detector %s go to IDLE", detector_name[i]);
                        status.detector[i].state = DETECTOR_STATE_IDLE;
                        status.detector[i].ts = get_time_ms();
                    } else if ((status.sensor_state[sens_first]
                                    .current) /*&& ((get_time_ms() - detector[i].ts)) < 1000*/) {
                        INFO("Detector %s RUN!", detector_name[i]);
                        status.detector[i].state = DETECTOR_STATE_DEC;
                        status.detector[i].ts = get_time_ms();
                        if (status.people_count) {
                            status.people_count--;
                            status.people_ts = status.detector[i].ts;
                            INFO("People: %d", status.people_count);
                        }
                    }
                } break;
                case DETECTOR_STATE_INC:
                case DETECTOR_STATE_DEC: {
                    if ((!status.sensor_state[sens_first].current) && (!status.sensor_state[sens_second].current) &&
                        ((get_time_ms() - status.detector[i].ts) > settings->sensor_debouce_time)) {
                        INFO("Detector %s go to IDLE", detector_name[i]);
                        status.detector[i].state = DETECTOR_STATE_IDLE;
                        status.detector[i].ts = get_time_ms();
                    }
                } break;
                default:
                    break;
            }
        }

        if (status.prev_people_count != status.people_count) {
            if (status.people_count > status.prev_people_count) {
                if (status.detector[DETECTOR_TYPE_UP].state == DETECTOR_STATE_INC) {
                    last_det = DETECTOR_TYPE_UP;
                    if (led_on_up_cnt != settings->led_count) {
                        led_on_up = true;
                        led_on_up_ts = get_time_ms();
                    }
                }
                if (status.detector[DETECTOR_TYPE_DOWN].state == DETECTOR_STATE_INC) {
                    last_det = DETECTOR_TYPE_DOWN;
                    if (led_on_down_cnt != settings->led_count) {
                        led_on_down = true;
                        led_on_down_ts = get_time_ms();
                    }
                }
            } else if (status.people_count < status.prev_people_count) {
                if (status.detector[DETECTOR_TYPE_UP].state == DETECTOR_STATE_DEC) {
                    if (status.people_count == 0) {
                        if (led_off_up_cnt != settings->led_count) {
                            led_off_up = true;
                            led_off_up_ts = get_time_ms();
                        }
                    }
                }
                if (status.detector[DETECTOR_TYPE_DOWN].state == DETECTOR_STATE_DEC) {
                    if (status.people_count == 0) {
                        if (led_off_down_cnt != settings->led_count) {
                            led_off_down = true;
                            led_off_down_ts = get_time_ms();
                        }
                    }
                }
            }
            status.prev_people_count = status.people_count;
        }
        if ((status.people_count > 0) && (!led_on_up) && (!led_on_down) && (!led_off_up) && (!led_off_down) &&
            ((get_time_ms() - status.people_ts) > settings->leds_off_timeout)) {
            status.prev_people_count = 0;
            status.people_count = 0;
            INFO("Timeout done!");
            INFO("People: %d", status.people_count);
            if (last_det == DETECTOR_TYPE_UP) {
                led_off_down = true;
                led_off_down_ts = get_time_ms();
            } else {
                led_off_up = true;
                led_off_up_ts = get_time_ms();
            }
        }

        for (uint32_t i = 0; i < DETECTOR_TYPE_MAX; i++) {
            if (status.detector[i].state == DETECTOR_STATE_SECOND_LOCK) {
                detector_block_ts[i] = get_time_ms();
            }
        }
        if (status.sensor_state[STAIRWAY_SENS_UP_FIRST].current) {
            if ((!status.block_by_light) && (!settings->disable_emergency) &&
                ((get_time_ms() - detector_block_ts[DETECTOR_TYPE_UP]) > settings->emergency_block_ms)) {
                stairway_emergency_leds(EMERGENCY_UP, true);
            }
        }
        if (status.sensor_state[STAIRWAY_SENS_DOWN_FIRST].current) {
            if ((!status.block_by_light) && (!settings->disable_emergency) &&
                ((get_time_ms() - detector_block_ts[DETECTOR_TYPE_DOWN]) > settings->emergency_block_ms)) {
                stairway_emergency_leds(EMERGENCY_DOWN, true);
            }
        }

        uint64_t ts = get_time_ms();
        if ((ts - prev_ts) > settings->leds_time_interval) {
            prev_ts = ts;

            if (!status.sensor_state[STAIRWAY_SENS_UP_FIRST].current) {
                stairway_emergency_leds(EMERGENCY_UP, false);
            }

            if (!status.sensor_state[STAIRWAY_SENS_DOWN_FIRST].current) {
                stairway_emergency_leds(EMERGENCY_DOWN, false);
            }

            if (led_on_up) {
                if (led_on_up_cnt < settings->led_count) {
                    if (!settings->disable_emergency) {
                        for (; led_on_up_cnt < settings->emergency_cnt[EMERGENCY_UP]; led_on_up_cnt++) {
                            stairway_leds_set_state(led_on_up_cnt, true, led_on_up_ts);
                        }
                    }
                    stairway_leds_set_state(led_on_up_cnt, true, led_on_up_ts);
                    led_on_up_cnt++;
                } else {
                    led_on_up_cnt = 0;
                    led_on_up = false;
                }
            }
            if (led_off_up) {
                if (led_off_up_cnt < settings->led_count) {
                    stairway_leds_set_state(settings->led_count - led_off_up_cnt - 1, false, led_off_up_ts);
                    led_off_up_cnt++;
                } else {
                    led_off_up_cnt = 0;
                    led_off_up = false;
                }
            }
            if (led_on_down) {
                if (led_on_down_cnt < settings->led_count) {
                    if (!settings->disable_emergency) {
                        for (; led_on_down_cnt < settings->emergency_cnt[EMERGENCY_DOWN]; led_on_down_cnt++) {
                            stairway_leds_set_state(settings->led_count - led_on_down_cnt - 1, true, led_on_down_ts);
                        }
                    }
                    stairway_leds_set_state(settings->led_count - led_on_down_cnt - 1, true, led_on_down_ts);
                    led_on_down_cnt++;
                } else {
                    led_on_down_cnt = 0;
                    led_on_down = false;
                }
            }
            if (led_off_down) {
                if (led_off_down_cnt < settings->led_count) {
                    //    INFO("led_off_down_cnt: %d", led_off_down_cnt);
                    stairway_leds_set_state(led_off_down_cnt, false, led_off_down_ts);
                    led_off_down_cnt++;
                } else {
                    led_off_down_cnt = 0;
                    led_off_down = false;
                }
            }
        }

        static uint64_t rdt = 0;
        if (get_time_ms() - rdt > 20) {
            rdt = get_time_ms();
            stairway_leds_refresh();
        }

        static uint64_t led_error_ts = 0;
        if (error_led_state) {
            led_error_ts = get_time_ms();
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        } else if ((get_time_ms() - led_error_ts) > 1000) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        }
    }
    return 0;
}