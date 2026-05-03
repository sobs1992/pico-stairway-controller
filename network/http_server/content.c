#define FILE_ID "NN03"

#include "content.h"
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "api/settings_api.h"

#define PRINT_REQUEST  0
#define PRINT_RESPONSE 0

#define CHECK_REQUEST(req, param) (strncmp(req, param, sizeof(param) - 1) == 0)
#define HTML_RESPONSE_HEADER                                                                                           \
    "HTTP/1.1 %d OK\nContent-Length: %ld\nContent-Type: text/html; charset=utf-8\nConnection: close\n\n"
#define HTTP_ERROR_HEADER      "HTTP/1.1 %d Not Found\nContent-Type: text/html\nConnection: close\n\n"
#define HTTP_RESPONSE_REDIRECT "HTTP/1.1 302 %s\nLocation: http://%s%s\n\n"
#define JSON_RESPONSE_HEADER                                                                                           \
    "HTTP/1.1 %d OK\nContent-Length: %ld\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\n\n"

#define JSON_BUF_SIZE_MAX 512

#define STATUS_JSON "{\"peopleCount\": %" PRId32 ",\n\"lightValue\": %" PRIu32 "}"

#define SETTINGS_JSON                                                                                                  \
    "{\"lightSensorEnabled\":%s,"                                                                                      \
    "\"dayThreshold\":%" PRIu32 ","                                                                                    \
    "\"nightThreshold\":%" PRIu32 ","                                                                                  \
    "\"bottomSensorA\":%" PRIu16 ","                                                                                   \
    "\"bottomSensorB\":%" PRIu16 ","                                                                                   \
    "\"swapBottomCheckbox\":%s,"                                                                                       \
    "\"topSensorA\":%" PRIu16 ","                                                                                      \
    "\"topSensorB\":%" PRIu16 ","                                                                                      \
    "\"swapTopCheckbox\":%s,"                                                                                          \
    "\"sensorDelay\":%" PRIu32 ","                                                                                     \
    "\"ledCount\":%" PRIu32 ","                                                                                        \
    "\"ledDelay\":%" PRIu32 ","                                                                                        \
    "\"ledTimeout\":%" PRIu32 ","                                                                                      \
    "\"brightnessOn\":%" PRIu8 ","                                                                                     \
    "\"brightnessOff\":%" PRIu8 ","                                                                                    \
    "\"pwmStepUp\":%" PRIu8 ","                                                                                        \
    "\"pwmStepDown\":%" PRIu8 ","                                                                                      \
    "\"standbyEnabled\":%s,"                                                                                           \
    "\"standbyStepsBottom\":%" PRIu32 ","                                                                              \
    "\"standbyStepsTop\":%" PRIu32 ","                                                                                 \
    "\"standbyDelay\":%" PRIu32 "}"

static ErrCode prepare_json_status(char **out, uint32_t *len) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(out == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(len == NULL, ERR_PARAM_IS_NULL);
    char json_buf[JSON_BUF_SIZE_MAX] = {0};

    Status *status = status_get();
    sprintf(json_buf, STATUS_JSON, status->people_count, status->light_value);
    *len = strlen(json_buf);
    *out = malloc(*len);
    RETURN_IF_COND(*out == NULL, ERR_MEM_ALLOC_FAIL);
    memcpy(*out, json_buf, *len);

    return err;
}

static ErrCode prepare_json_settings(char **out, uint32_t *len) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(out == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(len == NULL, ERR_PARAM_IS_NULL);
    char json_buf[JSON_BUF_SIZE_MAX] = {0};

    Settings *settings = settings_get();

    sprintf(
        json_buf, SETTINGS_JSON, (settings->disable_light_sensor) ? "false" : "true", settings->light_sensor_day_value,
        settings->light_sensor_night_value, settings->dist_trigger[STAIRWAY_SENS_DOWN_FIRST],
        settings->dist_trigger[STAIRWAY_SENS_DOWN_SECOND], (settings->sensor_down_swap) ? "true" : "false",
        settings->dist_trigger[STAIRWAY_SENS_UP_FIRST], settings->dist_trigger[STAIRWAY_SENS_UP_SECOND],
        (settings->sensor_up_swap) ? "true" : "false", settings->sensor_debouce_time, settings->led_count,
        settings->leds_time_interval, settings->leds_off_timeout, settings->led_on_value, settings->led_off_value,
        settings->led_on_step, settings->led_off_step, (settings->disable_emergency) ? "false" : "true",
        settings->emergency_cnt[EMERGENCY_DOWN], settings->emergency_cnt[EMERGENCY_UP], settings->emergency_block_ms);
    *len = strlen(json_buf);
    *out = malloc(*len);
    RETURN_IF_COND(*out == NULL, ERR_MEM_ALLOC_FAIL);
    memcpy(*out, json_buf, *len);

    return err;
}
#if 1
static ErrCode apply_json_settings(char *in) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(in == NULL, ERR_PARAM_IS_NULL);

    Settings *settings = settings_get();
    char light_sensor_state[6] = {0};
    char sensors_swap_up[6] = {0};
    char sensors_swap_down[6] = {0};
    char emergency_state[6] = {0};

    int fields =
        sscanf(in, SETTINGS_JSON, light_sensor_state, &settings->light_sensor_day_value,
               &settings->light_sensor_night_value, &settings->dist_trigger[STAIRWAY_SENS_DOWN_FIRST],
               &settings->dist_trigger[STAIRWAY_SENS_DOWN_SECOND], sensors_swap_down,
               &settings->dist_trigger[STAIRWAY_SENS_UP_FIRST], &settings->dist_trigger[STAIRWAY_SENS_UP_SECOND],
               sensors_swap_up, &settings->sensor_debouce_time, &settings->led_count, &settings->leds_time_interval,
               &settings->leds_off_timeout, &settings->led_on_value, &settings->led_off_value, &settings->led_on_step,
               &settings->led_off_step, emergency_state, &settings->emergency_cnt[EMERGENCY_DOWN],
               &settings->emergency_cnt[EMERGENCY_UP], &settings->emergency_block_ms);

    if (strncmp(light_sensor_state, "false", sizeof(light_sensor_state)) == 0) {
        settings->disable_light_sensor = true;
    } else {
        settings->disable_light_sensor = false;
    }
    if (strncmp(sensors_swap_up, "false", sizeof(sensors_swap_up)) == 0) {
        settings->sensor_up_swap = false;
    } else {
        settings->sensor_up_swap = true;
    }
    if (strncmp(sensors_swap_down, "false", sizeof(sensors_swap_down)) == 0) {
        settings->sensor_down_swap = false;
    } else {
        settings->sensor_down_swap = true;
    }
    if (strncmp(emergency_state, "false", sizeof(emergency_state)) == 0) {
        settings->disable_emergency = true;
    } else {
        settings->disable_emergency = false;
    }

    INFO("Fields: %d, light_sensor_state: %s", fields, light_sensor_state);

    RETURN_IF_ERROR(settings_write());

    return err;
}
#endif

static const char index_html[] = {
#include "generated/index.html.h"
    0x00,
};

ErrCode get_content(ip_addr_t *gw, ContentGetRequest *request, ContentResponse *response) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(gw == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(request == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(response == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(request->request == NULL, ERR_PARAM_IS_NULL);
    for (uint32_t i = 0; i < request->params_n; i++) {
        TO_EXIT_IF_COND(request->params[i] == NULL, ERR_PARAM_INVALID);
    }

#if PRINT_REQUEST
    INFO("GET Request: %s", request->request);
    for (uint32_t i = 0; i < request->params_n; i++) {
        INFO("Param %" PRIu32 ": %s", i, request->params[i]);
    }
#endif

    response->header_len = 0;
    response->body_len = 0;
    if (response->header) {
        free(response->header);
        response->header = NULL;
    }
    if (response->body) {
        free(response->body);
        response->body = NULL;
    }

    if (CHECK_REQUEST(request->request, "/index.html")) {
        response->body_len = sizeof(index_html);
        response->body = malloc(response->body_len);
        TO_EXIT_IF_COND(response->body == NULL, ERR_MEM_ALLOC_FAIL);
        strcpy(response->body, index_html);

        response->header = malloc(HTTP_HEADER_MAX_SIZE);
        TO_EXIT_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
        response->header_len =
            snprintf(response->header, HTTP_HEADER_MAX_SIZE, HTML_RESPONSE_HEADER, 200, response->body_len);
    } else if (CHECK_REQUEST(request->request, "/api/info")) {
        TO_EXIT_IF_ERROR(prepare_json_status(&response->body, &response->body_len));
        response->header = malloc(HTTP_HEADER_MAX_SIZE);
        TO_EXIT_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
        response->header_len =
            snprintf(response->header, HTTP_HEADER_MAX_SIZE, JSON_RESPONSE_HEADER, 200, response->body_len);

    } else if (CHECK_REQUEST(request->request, "/api/settings")) {
        TO_EXIT_IF_ERROR(prepare_json_settings(&response->body, &response->body_len));
        // INFO("JSON size %" PRIu32 ", JSON: %.*s", response->body_len, (int)response->body_len, response->body);
        response->header = malloc(HTTP_HEADER_MAX_SIZE);
        TO_EXIT_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
        response->header_len =
            snprintf(response->header, HTTP_HEADER_MAX_SIZE, JSON_RESPONSE_HEADER, 200, response->body_len);

    } else if ((CHECK_REQUEST(request->request, "/generate_204")) ||
               (CHECK_REQUEST(request->request, "/generate204"))) {
        response->header = malloc(HTTP_HEADER_MAX_SIZE);
        TO_EXIT_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
        response->header_len = snprintf(response->header, HTTP_HEADER_MAX_SIZE, HTTP_RESPONSE_REDIRECT, "Found",
                                        ipaddr_ntoa(gw), "/index.html");
    } else if (CHECK_REQUEST(request->request, "/ ")) {
        response->header = malloc(HTTP_HEADER_MAX_SIZE);
        TO_EXIT_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
        response->header_len = snprintf(response->header, HTTP_HEADER_MAX_SIZE, HTTP_RESPONSE_REDIRECT, "Redirect",
                                        ipaddr_ntoa(gw), "/index.html");

    } else {
        response->header = malloc(HTTP_HEADER_MAX_SIZE);
        TO_EXIT_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
        response->header_len = snprintf(response->header, HTTP_HEADER_MAX_SIZE, HTTP_ERROR_HEADER, 404);
    }

#if PRINT_RESPONSE
    INFO("GET Response:");
    printf("\tHeader: %s", response->header);
    if (response->body) {
        printf("\tBody: %.*s\n", (int)response->body_len, response->body);
    }
#endif

    return err;
EXIT:
    response->header_len = 0;
    response->body_len = 0;
    if (response->header) {
        free(response->header);
        response->header = NULL;
    }
    if (response->body) {
        free(response->body);
        response->body = NULL;
    }

    return err;
}

ErrCode post_content(ip_addr_t *gw, ContentPostRequest *request, ContentResponse *response) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(request == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(response == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(request->request == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(request->body == NULL, ERR_PARAM_IS_NULL);
    //    RETURN_IF_COND(request->body_len == 0, ERR_PARAM_INVALID);

#if 1 // PRINT_REQUEST
    INFO("POST Request: %s", request->request);
    INFO("POST Body: %s", request->body);
#endif

    response->header_len = 0;
    response->body_len = 0;
    if (response->header) {
        free(response->header);
        response->header = NULL;
    }
    if (response->body) {
        free(response->body);
        response->body = NULL;
    }

    if (CHECK_REQUEST(request->request, "/api/settings")) {
        TO_EXIT_IF_ERROR(apply_json_settings(request->body))
        response->body_len = sizeof("{\"status\":\"ok\"}") - 1;
        response->body = malloc(response->body_len);
        TO_EXIT_IF_COND(response->body == NULL, ERR_MEM_ALLOC_FAIL);
        memcpy(response->body, "{\"status\":\"ok\"}", response->body_len);

        response->header = malloc(HTTP_HEADER_MAX_SIZE);
        TO_EXIT_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
        response->header_len =
            snprintf(response->header, HTTP_HEADER_MAX_SIZE, JSON_RESPONSE_HEADER, 200, response->body_len);
    } else {
        response->header = malloc(HTTP_HEADER_MAX_SIZE);
        TO_EXIT_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
        response->header_len = snprintf(response->header, HTTP_HEADER_MAX_SIZE, HTTP_ERROR_HEADER, 404);
    }

#if 1 // PRINT_RESPONSE
    INFO("POST Response:");
    printf("\tHeader: %s", response->header);
    if (response->body) {
        printf("\tBody: %.*s\n", (int)response->body_len, response->body);
    }
#endif

    return err;
EXIT:
    response->header_len = 0;
    response->body_len = 0;
    if (response->header) {
        free(response->header);
        response->header = NULL;
    }
    if (response->body) {
        free(response->body);
        response->body = NULL;
    }

    return err;
}