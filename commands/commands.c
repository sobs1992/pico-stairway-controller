#define FILE_ID "CM01"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <inttypes.h>
#include "api/commands_api.h"
#include "api/settings_api.h"
#include "api/network_api.h"
#include "api/cli_api.h"

typedef enum {
    CTYPE_UINT32,
    CTYPE_UINT16,
    CTYPE_UINT8,
    CTYPE_BOOL,
    CTYPE_STRING,
} CommandType;

typedef struct {
    const char *name;
    const char *help;
    const char *label; // printed after the value is changed
    CommandType type;
    size_t offset;
    size_t size; // relevant for CTYPE_STRING only
    bool secret; // value is not printed
} SettingCommand;

#define SETTING_FIELD(field) .offset = offsetof(Settings, field)

static const SettingCommand setting_commands[] = {
    {"light_state", "Set state of light sensor (arg0: 0 - disable, 1 - enable)", "Light sensor", CTYPE_BOOL,
     SETTING_FIELD(use_light_sensor)},
    {"light_day", "Set day light value (arg0: value)", "Day light value", CTYPE_UINT32,
     SETTING_FIELD(light_sensor_day_value)},
    {"light_night", "Set night light value (arg0: value)", "Night light value", CTYPE_UINT32,
     SETTING_FIELD(light_sensor_night_value)},

    {"dist_up_first", "Set trigger distance for UP FIRST sensor (arg0: value)", "Sensor UP FIRST trigger value",
     CTYPE_UINT16, SETTING_FIELD(dist_trigger[STAIRWAY_SENS_UP_FIRST])},
    {"dist_up_second", "Set trigger distance for UP SECOND sensor (arg0: value)", "Sensor UP SECOND trigger value",
     CTYPE_UINT16, SETTING_FIELD(dist_trigger[STAIRWAY_SENS_UP_SECOND])},
    {"dist_down_first", "Set trigger distance for DOWN FIRST sensor (arg0: value)", "Sensor DOWN FIRST trigger value",
     CTYPE_UINT16, SETTING_FIELD(dist_trigger[STAIRWAY_SENS_DOWN_FIRST])},
    {"dist_down_second", "Set trigger distance for DOWN SECOND sensor (arg0: value)",
     "Sensor DOWN SECOND trigger value", CTYPE_UINT16, SETTING_FIELD(dist_trigger[STAIRWAY_SENS_DOWN_SECOND])},
    {"sens_debouce", "Set sensors debounce time (arg0: value)", "Sensor debounce time, ms", CTYPE_UINT32,
     SETTING_FIELD(sensor_debouce_time)},
    {"sens_up_swap", "Swap UP sensors (arg0: 0 - no swap, 1 - swap)", "Swap UP sensor", CTYPE_BOOL,
     SETTING_FIELD(sensor_up_swap)},
    {"sens_down_swap", "Swap DOWN sensors (arg0: 0 - no swap, 1 - swap)", "Swap DOWN sensor", CTYPE_BOOL,
     SETTING_FIELD(sensor_down_swap)},

    {"leds_count", "Set leds count (arg0: value)", "Leds count", CTYPE_UINT32, SETTING_FIELD(led_count)},
    {"leds_time_interval", "Set leds On/Off interval (arg0: value in ms)", "Leds On/Off time interval, ms",
     CTYPE_UINT32, SETTING_FIELD(leds_time_interval)},
    {"leds_off_timeout", "Set leds Off timeout (arg0: value in ms)", "Leds Off timeout, ms", CTYPE_UINT32,
     SETTING_FIELD(leds_off_timeout)},
    {"leds_on_value", "Set leds On PWM value (arg0: value)", "Leds On value", CTYPE_UINT8, SETTING_FIELD(led_on_value)},
    {"leds_off_value", "Set leds Off PWM value (arg0: value)", "Leds Off value", CTYPE_UINT8,
     SETTING_FIELD(led_off_value)},
    {"leds_on_step", "Set leds On step size (arg0: value)", "Leds On step", CTYPE_UINT8, SETTING_FIELD(led_on_step)},
    {"leds_off_step", "Set leds Off step size (arg0: value)", "Leds Off step", CTYPE_UINT8,
     SETTING_FIELD(led_off_step)},

    {"em_state", "Set state of emergency leds (arg0: 0 - disable, 1 - enable)", "Emergency leds", CTYPE_BOOL,
     SETTING_FIELD(use_emergency)},
    {"em_block_time", "Set emergency leds blocking time (after SECOND sensor triggered) (arg0: value ms)",
     "Emergency blocking time, ms", CTYPE_UINT32, SETTING_FIELD(emergency_block_ms)},
    {"em_up_cnt", "Set emergency UP leds count (arg0: value)", "Emergency UP leds count", CTYPE_UINT32,
     SETTING_FIELD(emergency_cnt[EMERGENCY_UP])},
    {"em_down_cnt", "Set emergency DOWN leds count (arg0: value)", "Emergency DOWN leds count", CTYPE_UINT32,
     SETTING_FIELD(emergency_cnt[EMERGENCY_DOWN])},

    {"ssid", "Set wifi SSID (arg0: SSID string, applied after restart)", "WiFi SSID", CTYPE_STRING, SETTING_FIELD(ssid),
     .size = SSID_MAX_LEN},
    {"pass", "Set wifi PASS (arg0: PASS string, 8+ chars, applied after restart)", "WiFi PASS", CTYPE_STRING,
     SETTING_FIELD(pass), .size = PASS_MAX_LEN, .secret = true},
};

static Settings *settings = NULL;
static Status *status = NULL;

static ErrCode parse_uint(const char *str, uint32_t max, uint32_t *out) {
    ErrCode err = ERR_SUCCESS;

    char *end = NULL;
    RETURN_IF_COND((*str < '0') || (*str > '9'), ERR_PARAM_INVALID);
    unsigned long val = strtoul(str, &end, 10);
    RETURN_IF_COND((*end != '\0') || (val > max), ERR_PARAM_INVALID);
    *out = val;

    return err;
}

static void print_setting(const SettingCommand *cmd, const Settings *s) {
    const void *field = (const uint8_t *)s + cmd->offset;
    switch (cmd->type) {
        case CTYPE_UINT32:
            printf("%s: %" PRIu32 "\n", cmd->label, *(const uint32_t *)field);
            break;
        case CTYPE_UINT16:
            printf("%s: %" PRIu16 "\n", cmd->label, *(const uint16_t *)field);
            break;
        case CTYPE_UINT8:
            printf("%s: %" PRIu8 "\n", cmd->label, *(const uint8_t *)field);
            break;
        case CTYPE_BOOL:
            printf("%s: %s\n", cmd->label, (*(const bool *)field) ? "yes" : "no");
            break;
        case CTYPE_STRING:
            printf("%s: %s\n", cmd->label, cmd->secret ? "********" : (const char *)field);
            break;
    }
}

static void set_setting(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    const SettingCommand *cmd = (const SettingCommand *)context;
    Settings new_settings = *settings;
    void *field = (uint8_t *)&new_settings + cmd->offset;
    uint32_t val = 0;

    TO_EXIT_IF_COND_(embeddedCliGetTokenCount(args) != 1, ERR_PARAM_INVALID);
    const char *arg = embeddedCliGetToken(args, 1);

    switch (cmd->type) {
        case CTYPE_UINT32:
            TO_EXIT_IF_ERROR_(parse_uint(arg, UINT32_MAX, &val));
            *(uint32_t *)field = val;
            break;
        case CTYPE_UINT16:
            TO_EXIT_IF_ERROR_(parse_uint(arg, UINT16_MAX, &val));
            *(uint16_t *)field = val;
            break;
        case CTYPE_UINT8:
            TO_EXIT_IF_ERROR_(parse_uint(arg, UINT8_MAX, &val));
            *(uint8_t *)field = val;
            break;
        case CTYPE_BOOL:
            TO_EXIT_IF_ERROR_(parse_uint(arg, 1, &val));
            *(bool *)field = val;
            break;
        case CTYPE_STRING:
            TO_EXIT_IF_COND_(strlen(arg) >= cmd->size, ERR_PARAM_INVALID);
            strcpy((char *)field, arg);
            break;
    }

    TO_EXIT_IF_ERROR_(settings_validate(&new_settings));
    TO_EXIT_IF_ERROR(settings_apply(&new_settings));
    print_setting(cmd, settings);
    if (cmd->type == CTYPE_STRING) {
        printf("Need to restart device to apply new WiFi settings\n");
    }
    return;
EXIT:
    printf("Invalid argument\n");
}

static void get_status(EmbeddedCli *cli, char *args, void *context) {
    printf("People detected: %" PRId32 "\n", status->people_count);

    printf("Light sensor: %s\n", (settings->use_light_sensor) ? "Enabled" : "Disabled");
    printf("\tCurrent state: %s\n"
           "\tCurrent light: %" PRIu32 "\n"
           "\tDay value: %" PRIu32 "\n"
           "\tNight value: %" PRIu32 "\n",
           (status->light_state) ? "Day" : "Night", status->light_value, settings->light_sensor_day_value,
           settings->light_sensor_night_value);

    printf("Sensors:\n"
           "\tTrigger UP FIRST: %" PRIu16 " mm\n"
           "\tTrigger UP SECOND: %" PRIu16 " mm\n"
           "\tTrigger DOWN FIRST: %" PRIu16 " mm\n"
           "\tTrigger DOWN SECOND: %" PRIu16 " mm\n",
           settings->dist_trigger[STAIRWAY_SENS_UP_FIRST], settings->dist_trigger[STAIRWAY_SENS_UP_SECOND],
           settings->dist_trigger[STAIRWAY_SENS_DOWN_FIRST], settings->dist_trigger[STAIRWAY_SENS_DOWN_SECOND]);
    printf("\tDebounce time: %" PRIu32 " ms\n", settings->sensor_debouce_time);
    printf("\tSwap UP: %s\n"
           "\tSwap DOWN: %s\n",
           (settings->sensor_up_swap) ? "yes" : "no", (settings->sensor_down_swap) ? "yes" : "no");

    printf("Leds:\n");
    printf("\tLeds count: %" PRIu32 "\n", settings->led_count);
    printf("\tOn/off interval: %" PRIu32 " ms\n", settings->leds_time_interval);
    printf("\tOff timeout: %" PRIu32 " ms\n", settings->leds_off_timeout);
    printf("\tOn value: %" PRIu8 "\n"
           "\tOn step: %" PRIu8 "\n",
           settings->led_on_value, settings->led_on_step);
    printf("\tOff value: %" PRIu8 "\n"
           "\tOff step: %" PRIu8 "\n",
           settings->led_off_value, settings->led_off_step);

    printf("Emergency leds: %s\n", (settings->use_emergency) ? "Enabled" : "Disabled");
    printf("\tEmergency up count: %" PRIu32 "\n"
           "\tEmergency down leds count: %" PRIu32 "\n"
           "\tEmergency block: %" PRIu32 " ms\n",
           settings->emergency_cnt[EMERGENCY_UP], settings->emergency_cnt[EMERGENCY_DOWN],
           settings->emergency_block_ms);

    printf("WiFi AP: %s\n", (net_ap_get_state()) ? "Started" : "Stopped");
    printf("\tSSID: %s\n", settings->ssid);
}

static void set_default(EmbeddedCli *cli, char *args, void *context) {
    settings_default(true);
}

static void set_wifi_state(EmbeddedCli *cli, char *args, void *context) {
    ErrCode err = ERR_SUCCESS;
    uint32_t val = 0;

    TO_EXIT_IF_COND_(embeddedCliGetTokenCount(args) != 1, ERR_PARAM_INVALID);
    TO_EXIT_IF_ERROR_(parse_uint(embeddedCliGetToken(args, 1), 1, &val));
    // The main loop syncs AP state with settings
    settings->ap_state = val;
    TO_EXIT_IF_ERROR(settings_write());
    printf("WiFi AP: %s\n", (settings->ap_state) ? "on" : "off");
    return;
EXIT:
    printf("Invalid argument\n");
}

ErrCode commands_init(void) {
    ErrCode err = ERR_SUCCESS;

    settings = settings_get();
    status = status_get();

    RETURN_IF_ERROR(cli_init(ARRAY_SIZE(setting_commands) + 3));

    RETURN_IF_ERROR(cli_add_command("status", "Get system status", get_status, NULL));
    RETURN_IF_ERROR(cli_add_command("default", "Restore default settings (no args)", set_default, NULL));
    RETURN_IF_ERROR(cli_add_command("wifi", "Set wifi state (arg0: 0 - wifi off, 1 - wifi on)", set_wifi_state, NULL));
    for (uint32_t i = 0; i < ARRAY_SIZE(setting_commands); i++) {
        RETURN_IF_ERROR(cli_add_command(setting_commands[i].name, setting_commands[i].help, set_setting,
                                        (void *)&setting_commands[i]));
    }

    return err;
}
