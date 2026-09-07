static const Settings default_settings = {
    .magic = 0xDEADBEEF,
    .use_light_sensor = true,
    .light_sensor_day_value = 1500,
    .light_sensor_night_value = 1400,
    .use_emergency = true,
    .dist_trigger =
        {
            [STAIRWAY_SENS_UP_FIRST] = 1200,
            [STAIRWAY_SENS_UP_SECOND] = 1200,
            [STAIRWAY_SENS_DOWN_FIRST] = 1000,
            [STAIRWAY_SENS_DOWN_SECOND] = 1000,
        },
    .leds_time_interval = 20,
    .leds_off_timeout = 20000,
    .sensor_debouce_time = 2000,
    .emergency_block_ms = 1000,
    .emergency_cnt =
        {
            [EMERGENCY_UP] = 2,
            [EMERGENCY_DOWN] = 2,
        },
    .led_on_value = 255,
    .led_off_value = 0,
    .led_on_step = 50,
    .led_off_step = 50,
    .led_count = 18,
    .sensor_up_swap = true,
    .sensor_down_swap = false,

    .ssid = "Stairway",
    .pass = "12345678",
    .ap_state = true,
};