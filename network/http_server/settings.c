#define FILE_ID "NN04"

#include "global.h"
#include "api/settings_api.h"
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#define JSON_TMP_BUF_SIZE 100

typedef enum {
    STYPE_MIN = 0,
    STYPE_UINT32 = STYPE_MIN,
    STYPE_UINT16,
    STYPE_UINT8,
    STYPE_BOOL,
    STYPE_STRING,
    STYPE_MAX,
} SettingsType;

typedef struct {
    const char *name;
    const SettingsType type;
    size_t offset;
    const uint32_t max_len; // relevant for STYPE_STRING only
} SettingsMap;

static const SettingsMap settings_map[] = {
    {.name = "use_light_sensor", .type = STYPE_BOOL, .offset = offsetof(Settings, use_light_sensor)},
    {.name = "light_sensor_day", .type = STYPE_UINT32, .offset = offsetof(Settings, light_sensor_day_value)},
    {.name = "light_sensor_night", .type = STYPE_UINT32, .offset = offsetof(Settings, light_sensor_night_value)},
    {.name = "dist_trigger_down_first",
     .type = STYPE_UINT16,
     .offset = offsetof(Settings, dist_trigger[STAIRWAY_SENS_DOWN_FIRST])},
    {.name = "dist_trigger_down_second",
     .type = STYPE_UINT16,
     .offset = offsetof(Settings, dist_trigger[STAIRWAY_SENS_DOWN_SECOND])},
    {.name = "sensor_down_swap", .type = STYPE_BOOL, .offset = offsetof(Settings, sensor_down_swap)},
    {.name = "dist_trigger_up_first",
     .type = STYPE_UINT16,
     .offset = offsetof(Settings, dist_trigger[STAIRWAY_SENS_UP_FIRST])},
    {.name = "dist_trigger_up_second",
     .type = STYPE_UINT16,
     .offset = offsetof(Settings, dist_trigger[STAIRWAY_SENS_UP_SECOND])},
    {.name = "sensor_up_swap", .type = STYPE_BOOL, .offset = offsetof(Settings, sensor_up_swap)},
    {.name = "sensor_debouce_time", .type = STYPE_UINT32, .offset = offsetof(Settings, sensor_debouce_time)},
    {.name = "led_count", .type = STYPE_UINT32, .offset = offsetof(Settings, led_count)},
    {.name = "leds_time_interval", .type = STYPE_UINT32, .offset = offsetof(Settings, leds_time_interval)},
    {.name = "leds_off_timeout", .type = STYPE_UINT32, .offset = offsetof(Settings, leds_off_timeout)},
    {.name = "led_on_value", .type = STYPE_UINT8, .offset = offsetof(Settings, led_on_value)},
    {.name = "led_off_value", .type = STYPE_UINT8, .offset = offsetof(Settings, led_off_value)},
    {.name = "led_on_step", .type = STYPE_UINT8, .offset = offsetof(Settings, led_on_step)},
    {.name = "led_off_step", .type = STYPE_UINT8, .offset = offsetof(Settings, led_off_step)},
    {.name = "use_emergency", .type = STYPE_BOOL, .offset = offsetof(Settings, use_emergency)},
    {.name = "emergency_cnt_down", .type = STYPE_UINT32, .offset = offsetof(Settings, emergency_cnt[EMERGENCY_DOWN])},
    {.name = "emergency_cnt_up", .type = STYPE_UINT32, .offset = offsetof(Settings, emergency_cnt[EMERGENCY_UP])},
    {.name = "emergency_block_ms", .type = STYPE_UINT32, .offset = offsetof(Settings, emergency_block_ms)},
    {.name = "wifi_ssid", .type = STYPE_STRING, .offset = offsetof(Settings, ssid), .max_len = SSID_MAX_LEN},
    {.name = "wifi_pass", .type = STYPE_STRING, .offset = offsetof(Settings, pass), .max_len = PASS_MAX_LEN},
};

static ErrCode jsonFindString(const char *in, const char *key, char *value, uint32_t buf_size) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(in == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(key == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(value == NULL, ERR_PARAM_IS_NULL);

    char *start = strstr(in, key);
    RETURN_IF_COND(start == NULL, ERR_FAIL);
    char *div = strchr(start, ':');
    char *val_start = div + 2;
    char *val_end = strpbrk(val_start, "\"\r\n,}\0");
    RETURN_IF_COND(val_end == NULL, ERR_FAIL);
    int len = val_end - val_start;
    RETURN_IF_COND(len >= buf_size, ERR_FAIL);
    memcpy(value, val_start, len);
    value[len] = '\0';

    return err;
}

static ErrCode jsonFindBool(const char *in, const char *key, bool *value) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(in == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(key == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(value == NULL, ERR_PARAM_IS_NULL);

    char *start = strstr(in, key);
    RETURN_IF_COND(start == NULL, ERR_FAIL);
    char *div = strchr(start, ':');
    if (strncmp(div + 1, "true", strlen("true")) == 0) {
        *value = true;
    } else if (strncmp(div + 1, "false", strlen("false")) == 0) {
        *value = false;
    } else {
        RETURN_IF_ERROR(ERR_FAIL);
    }

    return err;
}

static ErrCode jsonFindUint8(const char *in, const char *key, uint8_t *value) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(in == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(key == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(value == NULL, ERR_PARAM_IS_NULL);

    char *start = strstr(in, key);
    RETURN_IF_COND(start == NULL, ERR_FAIL);
    char *div = strchr(start, ':');
    RETURN_IF_COND(div == NULL, ERR_FAIL);
    *value = (uint8_t)strtoul(div + 1, NULL, 10);

    return err;
}

static ErrCode jsonFindUint16(const char *in, const char *key, uint16_t *value) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(in == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(key == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(value == NULL, ERR_PARAM_IS_NULL);

    char *start = strstr(in, key);
    RETURN_IF_COND(start == NULL, ERR_FAIL);
    char *div = strchr(start, ':');
    RETURN_IF_COND(div == NULL, ERR_FAIL);
    *value = (uint16_t)strtoul(div + 1, NULL, 10);

    return err;
}

static ErrCode jsonFindUint32(const char *in, const char *key, uint32_t *value) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(in == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(key == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(value == NULL, ERR_PARAM_IS_NULL);

    char *start = strstr(in, key);
    RETURN_IF_COND(start == NULL, ERR_FAIL);
    char *div = strchr(start, ':');
    RETURN_IF_COND(div == NULL, ERR_FAIL);
    *value = (uint32_t)strtoul(div + 1, NULL, 10);

    return err;
}

ErrCode get_settings_json(char *buf, uint32_t buf_size) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(buf == NULL, ERR_PARAM_IS_NULL);

    Settings *settings = settings_get();
    uint8_t *p_settings = (uint8_t *)settings;
    char tmp[JSON_TMP_BUF_SIZE] = {0};
    uint32_t json_size = 0;

    strcpy(buf, "{");
    json_size++;
    for (uint32_t i = 0; i < ARRAY_SIZE(settings_map); i++) {
        switch (settings_map[i].type) {
            case STYPE_UINT32:
                snprintf(tmp, sizeof(tmp), "\"%s\":%" PRIu32, settings_map[i].name,
                         *(uint32_t *)(p_settings + settings_map[i].offset));
                json_size += strlen(tmp);
                RETURN_IF_COND(json_size > buf_size, ERR_FAIL);
                strcat(buf, tmp);
                break;
            case STYPE_UINT16:
                snprintf(tmp, sizeof(tmp), "\"%s\":%" PRIu16, settings_map[i].name,
                         *(uint16_t *)(p_settings + settings_map[i].offset));
                json_size += strlen(tmp);
                RETURN_IF_COND(json_size > buf_size, ERR_FAIL);
                strcat(buf, tmp);
                break;
            case STYPE_UINT8:
                snprintf(tmp, sizeof(tmp), "\"%s\":%" PRIu8, settings_map[i].name,
                         *(uint8_t *)(p_settings + settings_map[i].offset));
                json_size += strlen(tmp);
                RETURN_IF_COND(json_size > buf_size, ERR_FAIL);
                strcat(buf, tmp);
                break;
            case STYPE_BOOL:
                snprintf(tmp, sizeof(tmp), "\"%s\":%s", settings_map[i].name,
                         (*(bool *)(p_settings + settings_map[i].offset)) ? "true" : "false");
                json_size += strlen(tmp);
                RETURN_IF_COND(json_size > buf_size, ERR_FAIL);
                strcat(buf, tmp);
                break;
            case STYPE_STRING:
                snprintf(tmp, sizeof(tmp), "\"%s\":\"%s\"", settings_map[i].name,
                         ((char *)(p_settings + settings_map[i].offset)));
                json_size += strlen(tmp);
                RETURN_IF_COND(json_size > buf_size, ERR_FAIL);
                strcat(buf, tmp);
                break;
            default:
                LOG_IF_ERROR(ERR_FAIL);
                break;
        }
        if (i < (ARRAY_SIZE(settings_map) - 1)) {
            json_size++;
            RETURN_IF_COND(json_size > buf_size, ERR_FAIL);
            strcat(buf, ",");
        }
    }
    json_size++;
    RETURN_IF_COND(json_size > buf_size, ERR_FAIL);
    strcat(buf, "}");

    return err;
}

ErrCode set_settings_json(char *buf, uint32_t buf_size) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(buf == NULL, ERR_PARAM_IS_NULL);

    Settings *settings = settings_get();
    uint8_t *p_settings = (uint8_t *)settings;

    for (uint32_t i = 0; i < ARRAY_SIZE(settings_map); i++) {
        switch (settings_map[i].type) {
            case STYPE_UINT32:
                LOG_IF_ERROR(
                    jsonFindUint32(buf, settings_map[i].name, (uint32_t *)(p_settings + settings_map[i].offset)));
                break;
            case STYPE_UINT16:
                LOG_IF_ERROR(
                    jsonFindUint16(buf, settings_map[i].name, (uint16_t *)(p_settings + settings_map[i].offset)));
                break;
            case STYPE_UINT8:
                LOG_IF_ERROR(
                    jsonFindUint8(buf, settings_map[i].name, (uint8_t *)(p_settings + settings_map[i].offset)));
                break;
            case STYPE_BOOL:
                LOG_IF_ERROR(jsonFindBool(buf, settings_map[i].name, (bool *)(p_settings + settings_map[i].offset)));
                break;
            case STYPE_STRING:
                LOG_IF_ERROR(jsonFindString(buf, settings_map[i].name, (char *)(p_settings + settings_map[i].offset),
                                            settings_map[i].max_len));
                break;
            default:
                LOG_IF_ERROR(ERR_FAIL);
                break;
        }
        if (err != ERR_SUCCESS) {
            INFO("Param %s not found", settings_map[i].name);
        }
    }

    RETURN_IF_ERROR(settings_write());

    return err;
}
