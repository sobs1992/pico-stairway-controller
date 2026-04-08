#define FILE_ID "MM01"

#include "global.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "api/ws2812_pio_api.h"
#include "api/stairway_leds.h"
#include "api/stairway_sensors.h"
#include "api/light_sensor_api.h"

#define WS281x_PIN 3
#define LED_COUNT  18

#define LEDS_INTERVAL_MS    100
#define DISTANCE_TRIGGER_MM 100
#define LEDS_OFF_TIMEOUT_MS 10000

#define LIGHT_SENSOR_OFF 1200
#define LIGHT_SENSOR_ON  1400

#define EMERGENCY_BLOCK_MS 1000

inline uint32_t get_time_us(void) {
    return to_us_since_boot(get_absolute_time());
}
inline uint32_t get_time_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

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

typedef struct {
    bool current;
    bool trigger;
    uint32_t ts;
} SensorState;

typedef struct {
    DetectorState state;
    uint32_t ts;
} Detector;

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

static Detector detector[DETECTOR_TYPE_MAX] = {0};
static SensorState sensor_state[STAIRWAY_SENS_MAX] = {0};
static int32_t people_count = 0;
static int32_t prev_people_count = 0;
static uint64_t people_ts = 0;
static bool block_by_light = false;
static bool disable_light_sensor = false;
static bool disable_emergency = false;

int main() {
    StairwaySensorsGet sen_values = {0};
    StairwaySensorsSettings sen_settings = {
        .trigger_value =
            {
                [STAIRWAY_SENS_UP_FIRST] = DISTANCE_TRIGGER_MM,
                [STAIRWAY_SENS_UP_SECOND] = DISTANCE_TRIGGER_MM,
                [STAIRWAY_SENS_DOWN_FIRST] = DISTANCE_TRIGGER_MM,
                [STAIRWAY_SENS_DOWN_SECOND] = DISTANCE_TRIGGER_MM,
            },
    };

    stdio_init_all();
    if (cyw43_arch_init()) {
        return -1;
    }

    light_sensor_init(0, LIGHT_SENSOR_OFF, LIGHT_SENSOR_ON);

    stairway_leds_init(WS281x_PIN, LED_COUNT);
    stairway_sensors_init(&sen_settings);

    while (1) {
        bool light_state = false;
        uint32_t light_value = 0;
        light_sensor_get_data(&light_state, &light_value);

        stairway_sensors_routine();
        stairway_sensors_get(&sen_values);

        for (uint32_t i = 0; i < STAIRWAY_SENS_MAX; i++) {
            if (sen_values.error[i]) {
                ANTISPAM_BEGIN(1000)
                INFO("Sensor %s error", sensors_name[i]);
                ANTISPAM_END
                sensor_state[i].current = 0;
            } else {
                sensor_state[i].current = sen_values.state[i];
            }
            if (sensor_state[i].current) {
                sensor_state[i].trigger = true;
            }
            sensor_state[i].ts = get_time_ms();
        }

        if ((light_state) && (!disable_light_sensor)) {
            if ((people_count == 0) && (!block_by_light)) {
                INFO("Block by light (value: %d)", light_value);
                block_by_light = true;
            }
        } else {
            if (block_by_light) {
                INFO("Unlock by light (value: %d)", light_value);
                block_by_light = false;
            }
        }

        for (uint32_t i = 0; i < DETECTOR_TYPE_MAX; i++) {
            if (block_by_light) {
                detector[i].state = DETECTOR_STATE_IDLE;
                detector[i].ts = get_time_ms();
                continue;
            }

            uint32_t sens_first = (i == DETECTOR_TYPE_UP) ? STAIRWAY_SENS_UP_FIRST : STAIRWAY_SENS_DOWN_FIRST;
            uint32_t sens_second = (i == DETECTOR_TYPE_UP) ? STAIRWAY_SENS_UP_SECOND : STAIRWAY_SENS_DOWN_SECOND;

            switch (detector[i].state) {
                case DETECTOR_STATE_IDLE: {
                    if (sensor_state[sens_first].current) {
                        INFO("Detector %s first sensor is triggered", detector_name[i]);
                        detector[i].state = DETECTOR_STATE_FIRST_LOCK;
                        detector[i].ts = get_time_ms();
                    } else if (sensor_state[sens_second].current) {
                        INFO("Detector %s second sensor is triggered", detector_name[i]);
                        detector[i].state = DETECTOR_STATE_SECOND_LOCK;
                        detector[i].ts = get_time_ms();
                    }
                } break;
                case DETECTOR_STATE_FIRST_LOCK: {
                    if ((!sensor_state[sens_first].current) && ((get_time_ms() - detector[i].ts)) > 1000) {
                        INFO("Detector %s go to IDLE", detector_name[i]);
                        detector[i].state = DETECTOR_STATE_IDLE;
                        detector[i].ts = get_time_ms();
                    } else if ((sensor_state[sens_second].current) /*&& ((get_time_ms() - detector[i].ts)) < 1000*/) {
                        INFO("Detector %s RUN!", detector_name[i]);
                        detector[i].state = DETECTOR_STATE_INC;
                        detector[i].ts = get_time_ms();
                        people_count++;
                        people_ts = detector[i].ts;
                        INFO("People: %d", people_count);
                    }
                } break;
                case DETECTOR_STATE_SECOND_LOCK: {
                    if ((!sensor_state[sens_second].current) && ((get_time_ms() - detector[i].ts)) > 1000) {
                        INFO("Detector %s go to IDLE", detector_name[i]);
                        detector[i].state = DETECTOR_STATE_IDLE;
                        detector[i].ts = get_time_ms();
                    } else if ((sensor_state[sens_first].current) /*&& ((get_time_ms() - detector[i].ts)) < 1000*/) {
                        INFO("Detector %s RUN!", detector_name[i]);
                        detector[i].state = DETECTOR_STATE_DEC;
                        detector[i].ts = get_time_ms();
                        if (people_count) {
                            people_count--;
                            people_ts = detector[i].ts;
                            INFO("People: %d", people_count);
                        }
                    }
                } break;
                case DETECTOR_STATE_INC:
                case DETECTOR_STATE_DEC: {
                    if ((!sensor_state[sens_first].current) && (!sensor_state[sens_second].current) &&
                        ((get_time_ms() - detector[i].ts) > 1000)) {
                        INFO("Detector %s go to IDLE", detector_name[i]);
                        detector[i].state = DETECTOR_STATE_IDLE;
                        detector[i].ts = get_time_ms();
                    }
                } break;
                default:
                    break;
            }
        }

        static bool led_on_up = false;
        static bool led_on_down = false;
        static bool led_off_up = false;
        static bool led_off_down = false;

        static uint32_t led_on_up_ts = 0;
        static uint32_t led_on_down_ts = 0;
        static uint32_t led_off_up_ts = 0;
        static uint32_t led_off_down_ts = 0;

        static uint32_t led_on_up_cnt = 0;
        static uint32_t led_off_up_cnt = 0;
        static uint32_t led_on_down_cnt = 0;
        static uint32_t led_off_down_cnt = 0;

        static uint32_t last_det = 0;

        if (prev_people_count != people_count) {
            if (people_count > prev_people_count) {
                if (detector[DETECTOR_TYPE_UP].state == DETECTOR_STATE_INC) {
                    last_det = DETECTOR_TYPE_UP;
                    if (led_on_up_cnt != LED_COUNT) {
                        led_on_up = true;
                        led_on_up_ts = get_time_ms();
                    }
                }
                if (detector[DETECTOR_TYPE_DOWN].state == DETECTOR_STATE_INC) {
                    last_det = DETECTOR_TYPE_DOWN;
                    if (led_on_down_cnt != LED_COUNT) {
                        led_on_down = true;
                        led_on_down_ts = get_time_ms();
                    }
                }
            } else if (people_count < prev_people_count) {
                if (detector[DETECTOR_TYPE_UP].state == DETECTOR_STATE_DEC) {
                    if (people_count == 0) {
                        if (led_off_up_cnt != LED_COUNT) {
                            led_off_up = true;
                            led_off_up_ts = get_time_ms();
                        }
                    }
                }
                if (detector[DETECTOR_TYPE_DOWN].state == DETECTOR_STATE_DEC) {
                    if (people_count == 0) {
                        if (led_off_down_cnt != LED_COUNT) {
                            led_off_down = true;
                            led_off_down_ts = get_time_ms();
                        }
                    }
                }
            }
            prev_people_count = people_count;
        }
        if ((people_count > 0) && (!led_on_up) && (!led_on_down) && (!led_off_up) && (!led_off_down) &&
            ((get_time_ms() - people_ts) > LEDS_OFF_TIMEOUT_MS)) {
            prev_people_count = 0;
            people_count = 0;
            INFO("Timeout done!\nPeople: %d", people_count);
            if (last_det == DETECTOR_TYPE_UP) {
                led_off_down = true;
                led_off_down_ts = get_time_ms();
            } else {
                led_off_up = true;
                led_off_up_ts = get_time_ms();
            }
        }

        static uint32_t prev_ts = 0;
        uint32_t ts = get_time_ms();
        static uint32_t detector_block_ts[DETECTOR_TYPE_MAX] = {0};
        if ((ts - prev_ts) > LEDS_INTERVAL_MS) {
            prev_ts = ts;

            for (uint32_t i = 0; i < DETECTOR_TYPE_MAX; i++) {
                if (detector[i].state == DETECTOR_STATE_SECOND_LOCK) {
                    detector_block_ts[i] = get_time_ms();
                }
            }

            if (sensor_state[STAIRWAY_SENS_UP_FIRST].current) {
                if ((!block_by_light) && (!disable_emergency) &&
                    ((get_time_ms() - detector_block_ts[DETECTOR_TYPE_UP]) > EMERGENCY_BLOCK_MS)) {
                    stairway_emergency_leds(EMERGENCY_UP, true);
                }
            } else {
                stairway_emergency_leds(EMERGENCY_UP, false);
            }
            if (sensor_state[STAIRWAY_SENS_DOWN_FIRST].current) {
                if ((!block_by_light) && (!disable_emergency) &&
                    ((get_time_ms() - detector_block_ts[DETECTOR_TYPE_DOWN]) > EMERGENCY_BLOCK_MS)) {
                    stairway_emergency_leds(EMERGENCY_DOWN, true);
                }
            } else {
                stairway_emergency_leds(EMERGENCY_DOWN, false);
            }

            if (led_on_up) {
                if (led_on_up_cnt < LED_COUNT) {
                    //    INFO("led_on_up_cnt: %d", led_on_up_cnt);
                    stairway_leds_set_state(led_on_up_cnt, true, led_on_up_ts);
                    led_on_up_cnt++;
                } else {
                    led_on_up_cnt = 0;
                    led_on_up = false;
                }
            }
            if (led_off_up) {
                if (led_off_up_cnt < LED_COUNT) {
                    //    INFO("led_off_up_cnt: %d", led_off_up_cnt);
                    stairway_leds_set_state(LED_COUNT - led_off_up_cnt - 1, false, led_off_up_ts);
                    led_off_up_cnt++;
                } else {
                    led_off_up_cnt = 0;
                    led_off_up = false;
                }
            }
            if (led_on_down) {
                if (led_on_down_cnt < LED_COUNT) {
                    //    INFO("led_on_down_cnt: %d", led_on_down_cnt);
                    stairway_leds_set_state(LED_COUNT - led_on_down_cnt - 1, true, led_on_down_ts);
                    led_on_down_cnt++;
                } else {
                    led_on_down_cnt = 0;
                    led_on_down = false;
                }
            }
            if (led_off_down) {
                if (led_off_down_cnt < LED_COUNT) {
                    //    INFO("led_off_down_cnt: %d", led_off_down_cnt);
                    stairway_leds_set_state(led_off_down_cnt, false, led_off_down_ts);
                    led_off_down_cnt++;
                } else {
                    led_off_down_cnt = 0;
                    led_off_down = false;
                }
            }
        }

        static uint32_t rdt = 0;
        if (get_time_ms() - rdt > 20) {
            rdt = get_time_ms();
            stairway_leds_refresh();
        }
    }
    return 0;
}