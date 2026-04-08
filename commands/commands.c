#define FILE_ID "CM01"

#include <stdlib.h>
#include "api/commands_api.h"
#include "api/settings_api.h"
#include "api/cli_api.h"
#include "hardware/watchdog.h"
#include "pico/time.h"

static Settings *settings = NULL;
static Status *status = NULL;

static void get_status(EmbeddedCli *cli, char *args, void *context) {
    printf("People detected: %d\n", status->people_count);

    printf("Light sensor: %s\n", (settings->disable_light_sensor) ? "Disabled" : "Enabled");
    printf("\tCurrent state: %s\n"
           "\tCurrent light: %d\n"
           "\tDay value: %d\n"
           "\tNight value: %d\n",
           (status->light_state) ? "Day" : "Night", status->light_value, settings->light_sensor_day_value,
           settings->light_sensor_night_value);

    printf("Sensors:\n"
           "\tTrigger UP FIRST: %d mm\n"
           "\tTrigger UP SECOND: %d mm\n"
           "\tTrigger DOWN FIRST: %d mm\n"
           "\tTrigger DOWN SECOND: %d mm\n",
           settings->dist_trigger[STAIRWAY_SENS_UP_FIRST], settings->dist_trigger[STAIRWAY_SENS_UP_SECOND],
           settings->dist_trigger[STAIRWAY_SENS_DOWN_FIRST], settings->dist_trigger[STAIRWAY_SENS_DOWN_SECOND]);
    printf("\tDebounce time: %d ms\n", settings->sensor_debouce_time);
    printf("\tSwap UP: %s\n"
           "\tSwap DOWN: %s\n",
           (settings->sensor_up_swap) ? "yes" : "no", (settings->sensor_down_swap) ? "yes" : "no");

    printf("Leds:\n");
    printf("\tLeds count: %d\n", settings->led_count);
    printf("\tOn/off interval: %d ms\n", settings->leds_time_interval);
    printf("\tOff timeout: %d ms\n", settings->leds_off_timeout);
    printf("\tOn value: %d\n"
           "\tOn step: %d\n",
           settings->led_on_value, settings->led_on_step);
    printf("\tOff value: %d\n"
           "\tOff step: %d\n",
           settings->led_off_value, settings->led_off_step);

    printf("Emergency leds: %s\n", (settings->disable_emergency) ? "Disabled" : "Enabled");
    printf("\tEmergency up count: %d\n"
           "\tEmergency down leds count: %d\n"
           "\tEmergency block: %d ms\n",
           settings->emergency_cnt[EMERGENCY_UP], settings->emergency_cnt[EMERGENCY_DOWN],
           settings->emergency_block_ms);
}

static void set_default(EmbeddedCli *cli, char *args, void *context) {
    settings_default();
}

static void set_light_state(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    TO_EXIT_IF_COND(val > 1, ERR_PARAM_INVALID);
    settings->disable_light_sensor = !val;
    printf("Light sensor: %s\n", (settings->disable_light_sensor) ? "Disabled" : "Enabled");
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_day_light_value(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->light_sensor_day_value = val;
    printf("Day light value: %d\n", settings->light_sensor_day_value);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_night_light_value(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->light_sensor_night_value = val;
    printf("Night light value: %d\n", settings->light_sensor_night_value);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_emergency_state(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    TO_EXIT_IF_COND(val > 1, ERR_PARAM_INVALID);
    settings->disable_emergency = !val;
    printf("Emergency leds: %s\n", (settings->disable_emergency) ? "Disabled" : "Enabled");
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_dist_up_first_value(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->dist_trigger[STAIRWAY_SENS_UP_FIRST] = val;
    printf("Sensor UP FIRST trigger value: %d\n", settings->dist_trigger[STAIRWAY_SENS_UP_FIRST]);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_dist_up_second_value(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->dist_trigger[STAIRWAY_SENS_UP_SECOND] = val;
    printf("Sensor UP SECOND trigger value: %d\n", settings->dist_trigger[STAIRWAY_SENS_UP_SECOND]);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_dist_down_first_value(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->dist_trigger[STAIRWAY_SENS_DOWN_FIRST] = val;
    printf("Sensor DOWN FIRST trigger value: %d\n", settings->dist_trigger[STAIRWAY_SENS_DOWN_FIRST]);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_dist_down_second_value(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->dist_trigger[STAIRWAY_SENS_DOWN_SECOND] = val;
    printf("Sensor DOWN SECOND trigger value: %d\n", settings->dist_trigger[STAIRWAY_SENS_DOWN_SECOND]);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_sens_debounce(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->sensor_debouce_time = val;
    printf("Sensor debounce time: %d ms\n", settings->sensor_debouce_time);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_up_sens_swap(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    TO_EXIT_IF_COND(val > 1, ERR_PARAM_INVALID);
    settings->sensor_up_swap = val;
    printf("Swap UP sensor: %s\n", (settings->sensor_up_swap) ? "yes" : "no");
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_down_sens_swap(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    TO_EXIT_IF_COND(val > 1, ERR_PARAM_INVALID);
    settings->sensor_down_swap = val;
    printf("Swap DOWN sensor: %s\n", (settings->sensor_down_swap) ? "yes" : "no");
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_leds_count(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->led_count = val;
    printf("Leds count: %d\n", settings->led_count);
    TO_EXIT_IF_ERROR(settings_write());
    printf("Device will reboot!\n");
    sleep_ms(1000);
    watchdog_reboot(0, 0, 0);
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_leds_time_interval(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->leds_time_interval = val;
    printf("Leds On/Off time interval: %d ms\n", settings->leds_time_interval);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_leds_off_timeout(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->leds_off_timeout = val;
    printf("Leds Off timeout: %d ms\n", settings->leds_off_timeout);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_leds_on_value(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->led_on_value = val;
    printf("Leds On value: %d\n", settings->led_on_value);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_leds_off_value(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->led_off_value = val;
    printf("Leds Off value: %d\n", settings->led_off_value);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_leds_on_step(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->led_on_step = val;
    printf("Leds On step: %d\n", settings->led_on_step);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_leds_off_step(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->led_off_step = val;
    printf("Leds Off step: %d\n", settings->led_off_step);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_emergency_block_time(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->emergency_block_ms = val;
    printf("Emergency blocking time: %d ms\n", settings->emergency_block_ms);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_emergency_up_cnt(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->emergency_cnt[EMERGENCY_UP] = val;
    printf("Emergency UP leds count: %d\n", settings->emergency_cnt[EMERGENCY_UP]);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

static void set_emergency_down_cnt(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint8_t count = embeddedCliGetTokenCount(args);
    TO_EXIT_IF_COND(count != 1, ERR_PARAM_INVALID);
    uint32_t val = strtoul(embeddedCliGetToken(args, 1), NULL, 10);
    settings->emergency_cnt[EMERGENCY_DOWN] = val;
    printf("Emergency DOWN leds count: %d\n", settings->emergency_cnt[EMERGENCY_DOWN]);
    TO_EXIT_IF_ERROR(settings_write());
    return;
EXIT:
    printf("Invalid argument\n");
    return;
}

ErrCode commands_init(void) {
    ErrCode err = ERR_SUCCESS;

    settings = settings_get();
    status = status_get();

    cli_init(32);

    cli_add_command("status", "Get system status", get_status);
    cli_add_command("default", "Restore default settings (no args)", set_default);
    cli_add_command("light_state", "Set state of light sensor (arg0: 0 - disable, 1 - enable)", set_light_state);
    cli_add_command("light_day", "Set day light value (arg0: value)", set_day_light_value);
    cli_add_command("light_night", "Set night light value (arg0: value)", set_night_light_value);

    cli_add_command("dist_up_first", "Set trigger distance for UP FIRST sensor (arg0: value)", set_dist_up_first_value);
    cli_add_command("dist_up_second", "Set trigger distance for UP SECOND sensor (arg0: value)",
                    set_dist_up_second_value);
    cli_add_command("dist_down_first", "Set trigger distance for DOWN FIRST sensor (arg0: value)",
                    set_dist_down_first_value);
    cli_add_command("dist_down_second", "Set trigger distance for DOWN SECOND sensor (arg0: value)",
                    set_dist_down_second_value);
    cli_add_command("sens_debouce", "Set sensors debounce time (arg0: value)", set_sens_debounce);
    cli_add_command("sens_up_swap", "Swap UP sensors (arg0: 0 - no swap, 1 - swap)", set_up_sens_swap);
    cli_add_command("sens_down_swap", "Swap DOWN sensors (arg0: 0 - no swap, 1 - swap)", set_down_sens_swap);

    cli_add_command("leds_count", "Set leds count (arg0: value)", set_leds_count);
    cli_add_command("leds_time_interval", "Set leds On/Off interval (arg0: value in ms)", set_leds_time_interval);
    cli_add_command("leds_off_timeout", "Set leds Off timeout (arg0: value in ms)", set_leds_off_timeout);
    cli_add_command("leds_on_value", "Set leds On PWM value (arg0: value)", set_leds_on_value);
    cli_add_command("leds_off_value", "Set leds Off PWM value (arg0: value)", set_leds_off_value);
    cli_add_command("leds_on_step", "Set leds On step size (arg0: value)", set_leds_on_step);
    cli_add_command("leds_off_step", "Set leds Off step size (arg0: value)", set_leds_off_step);

    cli_add_command("em_state", "Set state of emergency leds (arg0: 0 - disable, 1 - enable)", set_emergency_state);
    cli_add_command("em_block_time",
                    "Set emergency leds blocking time (after SECOND sensor triggered) (arg0: value ms)",
                    set_emergency_block_time);
    cli_add_command("em_up_cnt", "Set emergency UP leds count (arg0: value)", set_emergency_up_cnt);
    cli_add_command("em_down_cnt", "Set emergency DOWN leds count (arg0: value)", set_emergency_down_cnt);

    return err;
}