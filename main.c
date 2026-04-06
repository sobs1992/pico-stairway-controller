#define FILE_ID "MM01"

#include "global.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "api/ws2812_pio_api.h"
#include "api/stairway_leds.h"
#include "api/stairway_sensors.h"

#define WS281x_PIN 3
#define LED_COUNT  18

#define LEDS_INTERVAL_MS    100
#define DISTANCE_TRIGGER_MM 100
#define LEDS_OFF_TIMEOUT_MS 10000

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
    uint64_t ts;
} Detector;

static Detector detector[DETECTOR_TYPE_MAX] = {0};
static SensorState sensor_state[STAIRWAY_SENS_MAX] = {0};

int main() {
    static int32_t people_count = 0;
    static int32_t prev_people_count = 0;
    static uint64_t people_ts = 0;

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

    stairway_leds_init(WS281x_PIN, LED_COUNT);
    stairway_sensors_init(&sen_settings);

    while (1) {
        stairway_sensors_routine();
        stairway_sensors_get(&sen_values);
#if 0
        printf("%d|%d|%d|%d\n", sen_values.state[STAIRWAY_SENS_UP_FIRST], sen_values.state[STAIRWAY_SENS_UP_SECOND],
               sen_values.state[STAIRWAY_SENS_DOWN_FIRST], sen_values.state[STAIRWAY_SENS_DOWN_SECOND]);
#endif

        for (uint32_t i = 0; i < STAIRWAY_SENS_MAX; i++) {
            if (sen_values.error[i]) {
                printf("Sensor %d error\n", i);
                sensor_state[i].current = 0;
            } else {
                sensor_state[i].current = sen_values.state[i];
            }
            if (sensor_state[i].current) {
                sensor_state[i].trigger = true;
            }
            sensor_state[i].ts = get_time_ms();
        }

        for (uint32_t i = 0; i < DETECTOR_TYPE_MAX; i++) {
            uint32_t sens_first = (i == DETECTOR_TYPE_UP) ? STAIRWAY_SENS_UP_FIRST : STAIRWAY_SENS_DOWN_FIRST;
            uint32_t sens_second = (i == DETECTOR_TYPE_UP) ? STAIRWAY_SENS_UP_SECOND : STAIRWAY_SENS_DOWN_SECOND;

            switch (detector[i].state) {
                case DETECTOR_STATE_IDLE: {
                    if (sensor_state[sens_first].current) {
                        printf("Detector %d set DETECTOR_STATE_FIRST_LOCK\n", i);
                        detector[i].state = DETECTOR_STATE_FIRST_LOCK;
                        detector[i].ts = get_time_ms();
                    } else if (sensor_state[sens_second].current) {
                        printf("Detector %d set DETECTOR_STATE_SECOND_LOCK\n", i);
                        detector[i].state = DETECTOR_STATE_SECOND_LOCK;
                        detector[i].ts = get_time_ms();
                    }
                } break;
                case DETECTOR_STATE_FIRST_LOCK: {
                    if ((!sensor_state[sens_first].current) && ((get_time_ms() - detector[i].ts)) > 1000) {
                        printf("Detector %d set DETECTOR_STATE_IDLE\n", i);
                        detector[i].state = DETECTOR_STATE_IDLE;
                        detector[i].ts = get_time_ms();
                    } else if ((sensor_state[sens_second].current) /*&& ((get_time_ms() - detector[i].ts)) < 1000*/) {
                        printf("Detector %d set DETECTOR_STATE_INC\n", i);
                        detector[i].state = DETECTOR_STATE_INC;
                        detector[i].ts = get_time_ms();
                        people_count++;
                        people_ts = detector[i].ts;
                        printf("People: %d\n", people_count);
                    }
                } break;
                case DETECTOR_STATE_SECOND_LOCK: {
                    if ((!sensor_state[sens_second].current) && ((get_time_ms() - detector[i].ts)) > 1000) {
                        printf("Detector %d set DETECTOR_STATE_IDLE\n", i);
                        detector[i].state = DETECTOR_STATE_IDLE;
                        detector[i].ts = get_time_ms();
                    } else if ((sensor_state[sens_first].current) /*&& ((get_time_ms() - detector[i].ts)) < 1000*/) {
                        printf("Detector %d set DETECTOR_STATE_DEC\n", i);
                        detector[i].state = DETECTOR_STATE_DEC;
                        detector[i].ts = get_time_ms();
                        if (people_count) {
                            people_count--;
                            people_ts = detector[i].ts;
                            printf("People: %d\n", people_count);
                        }
                    }
                } break;
                case DETECTOR_STATE_INC:
                case DETECTOR_STATE_DEC: {
                    if ((!sensor_state[sens_first].current) && (!sensor_state[sens_second].current) &&
                        ((get_time_ms() - detector[i].ts) > 1000)) {
                        printf("Detector %d set DETECTOR_STATE_IDLE\n", i);
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
            printf("Timeout done!\n");
            prev_people_count = 0;
            people_count = 0;
            printf("People: %d\n", people_count);
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
        if ((ts - prev_ts) > LEDS_INTERVAL_MS) {
            prev_ts = ts;

            if (sensor_state[STAIRWAY_SENS_UP_FIRST].current) {
                stairway_emergency_leds(EMERGENCY_UP, true);
            } else {
                stairway_emergency_leds(EMERGENCY_UP, false);
            }
            if (sensor_state[STAIRWAY_SENS_DOWN_FIRST].current) {
                stairway_emergency_leds(EMERGENCY_DOWN, true);
            } else {
                stairway_emergency_leds(EMERGENCY_DOWN, false);
            }

            if (led_on_up) {
                if (led_on_up_cnt < LED_COUNT) {
                    //    printf("led_on_up_cnt: %d\n", led_on_up_cnt);
                    stairway_leds_set_state(led_on_up_cnt, true, led_on_up_ts);
                    led_on_up_cnt++;
                } else {
                    led_on_up_cnt = 0;
                    led_on_up = false;
                }
            }
            if (led_off_up) {
                if (led_off_up_cnt < LED_COUNT) {
                    //    printf("led_off_up_cnt: %d\n", led_off_up_cnt);
                    stairway_leds_set_state(LED_COUNT - led_off_up_cnt - 1, false, led_off_up_ts);
                    led_off_up_cnt++;
                } else {
                    led_off_up_cnt = 0;
                    led_off_up = false;
                }
            }
            if (led_on_down) {
                if (led_on_down_cnt < LED_COUNT) {
                    //    printf("led_on_down_cnt: %d\n", led_on_down_cnt);
                    stairway_leds_set_state(LED_COUNT - led_on_down_cnt - 1, true, led_on_down_ts);
                    led_on_down_cnt++;
                } else {
                    led_on_down_cnt = 0;
                    led_on_down = false;
                }
            }
            if (led_off_down) {
                if (led_off_down_cnt < LED_COUNT) {
                    //    printf("led_off_down_cnt: %d\n", led_off_down_cnt);
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