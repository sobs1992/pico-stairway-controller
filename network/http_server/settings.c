#define FILE_ID "NN04"

#include "global.h"
#include "settings.h"
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <stdarg.h>

#define JSON_KEY_BUF_SIZE 48

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
    const bool secret;      // value is never sent to the client, empty value means "don't change"
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
    {.name = "wifi_pass",
     .type = STYPE_STRING,
     .offset = offsetof(Settings, pass),
     .max_len = PASS_MAX_LEN,
     .secret = true},
};

// Returns pointer to the first non-space char of the value for "key", or NULL
static const char *json_find_value(const char *in, const char *key) {
    char pattern[JSON_KEY_BUF_SIZE];
    int len = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if ((len <= 0) || (len >= sizeof(pattern))) {
        return NULL;
    }
    const char *start = strstr(in, pattern);
    if (start == NULL) {
        return NULL;
    }
    start += len;
    while ((*start == ' ') || (*start == '\t') || (*start == '\r') || (*start == '\n')) {
        start++;
    }
    if (*start != ':') {
        return NULL;
    }
    start++;
    while ((*start == ' ') || (*start == '\t') || (*start == '\r') || (*start == '\n')) {
        start++;
    }
    return start;
}

static ErrCode json_parse_string(const char *val, char *out, uint32_t buf_size) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(*val != '"', ERR_PARAM_INVALID);
    val++;

    uint32_t len = 0;
    while (*val != '"') {
        char c = *val++;
        RETURN_IF_COND(c == '\0', ERR_PARAM_INVALID);
        if (c == '\\') {
            c = *val++;
            RETURN_IF_COND((c != '"') && (c != '\\') && (c != '/'), ERR_PARAM_INVALID);
        }
        RETURN_IF_COND(len + 1 >= buf_size, ERR_OVERFLOW);
        out[len++] = c;
    }
    out[len] = '\0';

    return err;
}

static ErrCode json_parse_bool(const char *val, bool *out) {
    ErrCode err = ERR_SUCCESS;

    if (strncmp(val, "true", sizeof("true") - 1) == 0) {
        *out = true;
    } else if (strncmp(val, "false", sizeof("false") - 1) == 0) {
        *out = false;
    } else {
        RETURN_IF_ERROR(ERR_PARAM_INVALID);
    }

    return err;
}

static ErrCode json_parse_uint(const char *val, uint32_t max, uint32_t *out) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND((*val < '0') || (*val > '9'), ERR_PARAM_INVALID);
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(val, &end, 10);
    RETURN_IF_COND((errno != 0) || (end == val) || (v > max), ERR_PARAM_INVALID);
    *out = v;

    return err;
}

// Appends string with JSON escaping
static ErrCode json_append_string(char *buf, uint32_t buf_size, uint32_t *pos, const char *str) {
    ErrCode err = ERR_SUCCESS;

    for (; *str; str++) {
        bool escape = (*str == '"') || (*str == '\\');
        RETURN_IF_COND(*pos + (escape ? 2 : 1) >= buf_size, ERR_OVERFLOW);
        if (escape) {
            buf[(*pos)++] = '\\';
        }
        buf[(*pos)++] = *str;
    }
    buf[*pos] = '\0';

    return err;
}

static ErrCode json_append(char *buf, uint32_t buf_size, uint32_t *pos, const char *fmt, ...) {
    ErrCode err = ERR_SUCCESS;

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf + *pos, buf_size - *pos, fmt, args);
    va_end(args);
    RETURN_IF_COND((len < 0) || (*pos + len >= buf_size), ERR_OVERFLOW);
    *pos += len;

    return err;
}

ErrCode get_settings_json(char *buf, uint32_t buf_size) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(buf == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(buf_size == 0, ERR_PARAM_INVALID);

    const uint8_t *p_settings = (const uint8_t *)settings_get();
    uint32_t pos = 0;
    buf[0] = '\0';

    RETURN_IF_ERROR(json_append(buf, buf_size, &pos, "{"));
    for (uint32_t i = 0; i < ARRAY_SIZE(settings_map); i++) {
        const SettingsMap *m = &settings_map[i];
        const void *field = p_settings + m->offset;
        const char *sep = (i < ARRAY_SIZE(settings_map) - 1) ? "," : "";
        switch (m->type) {
            case STYPE_UINT32:
                RETURN_IF_ERROR(
                    json_append(buf, buf_size, &pos, "\"%s\":%" PRIu32 "%s", m->name, *(const uint32_t *)field, sep));
                break;
            case STYPE_UINT16:
                RETURN_IF_ERROR(
                    json_append(buf, buf_size, &pos, "\"%s\":%" PRIu16 "%s", m->name, *(const uint16_t *)field, sep));
                break;
            case STYPE_UINT8:
                RETURN_IF_ERROR(
                    json_append(buf, buf_size, &pos, "\"%s\":%" PRIu8 "%s", m->name, *(const uint8_t *)field, sep));
                break;
            case STYPE_BOOL:
                RETURN_IF_ERROR(json_append(buf, buf_size, &pos, "\"%s\":%s%s", m->name,
                                            (*(const bool *)field) ? "true" : "false", sep));
                break;
            case STYPE_STRING:
                RETURN_IF_ERROR(json_append(buf, buf_size, &pos, "\"%s\":\"", m->name));
                if (!m->secret) {
                    RETURN_IF_ERROR(json_append_string(buf, buf_size, &pos, (const char *)field));
                }
                RETURN_IF_ERROR(json_append(buf, buf_size, &pos, "\"%s", sep));
                break;
            default:
                RETURN_IF_ERROR(ERR_FAIL);
                break;
        }
    }
    RETURN_IF_ERROR(json_append(buf, buf_size, &pos, "}"));

    return err;
}

ErrCode set_settings_json(const char *json) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(json == NULL, ERR_PARAM_IS_NULL);

    // Parse into a copy: active settings are changed only if everything is valid
    Settings new_settings = *settings_get();
    uint8_t *p_settings = (uint8_t *)&new_settings;

    for (uint32_t i = 0; i < ARRAY_SIZE(settings_map); i++) {
        const SettingsMap *m = &settings_map[i];
        void *field = p_settings + m->offset;
        const char *val = json_find_value(json, m->name);
        if (val == NULL) {
            INFO("Param %s not found", m->name);
            continue;
        }

        uint32_t num = 0;
        switch (m->type) {
            case STYPE_UINT32:
                TO_EXIT_IF_ERROR(json_parse_uint(val, UINT32_MAX, &num));
                *(uint32_t *)field = num;
                break;
            case STYPE_UINT16:
                TO_EXIT_IF_ERROR(json_parse_uint(val, UINT16_MAX, &num));
                *(uint16_t *)field = num;
                break;
            case STYPE_UINT8:
                TO_EXIT_IF_ERROR(json_parse_uint(val, UINT8_MAX, &num));
                *(uint8_t *)field = num;
                break;
            case STYPE_BOOL:
                TO_EXIT_IF_ERROR(json_parse_bool(val, (bool *)field));
                break;
            case STYPE_STRING: {
                char str[PASS_MAX_LEN > SSID_MAX_LEN ? PASS_MAX_LEN : SSID_MAX_LEN];
                TO_EXIT_IF_ERROR(json_parse_string(val, str, m->max_len));
                if ((m->secret) && (str[0] == '\0')) {
                    break; // keep current value
                }
                strcpy((char *)field, str);
            } break;
            default:
                TO_EXIT_IF_ERROR(ERR_FAIL);
                break;
        }
    }

    TO_EXIT_IF_ERROR(settings_apply(&new_settings));
    return err;

EXIT:
    INFO("Settings are not applied");
    return err;
}
