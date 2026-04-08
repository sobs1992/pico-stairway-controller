static const Settings default_settings = {
    .magic = 0xDEADBEEF,
    .disable_light_sensor = false,
    .light_sensor_day_value = 1400,
    .light_sensor_night_value = 1200,
    .disable_emergency = false,
    .dist_trigger =
        {
            [STAIRWAY_SENS_UP_FIRST] = 100,
            [STAIRWAY_SENS_UP_SECOND] = 100,
            [STAIRWAY_SENS_DOWN_FIRST] = 100,
            [STAIRWAY_SENS_DOWN_SECOND] = 100,
        },
    .leds_time_interval = 100,
    .leds_off_timeout = 10000,
    .sensor_debouce_time = 1000,
    .emergency_block_ms = 1000,
    .emergency_cnt =
        {
            [EMERGENCY_UP] = 2,
            [EMERGENCY_DOWN] = 2,
        },
    .led_on_value = 255,
    .led_off_value = 0,
    .led_on_step = 16,
    .led_off_step = 16,
    .led_count = 18,
    .sensor_up_swap = false,
    .sensor_down_swap = true,
};