#define FILE_ID "NN03"

#include "content.h"
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "settings.h"
#include "api/settings_api.h"

#define PRINT_REQUEST 0

#define HTTP_RESPONSE_HEADER                                                                                           \
    "HTTP/1.1 %d %s\r\nContent-Length: %" PRIu32 "\r\nContent-Type: %s\r\nConnection: close\r\n\r\n"
#define HTTP_RESPONSE_REDIRECT                                                                                         \
    "HTTP/1.1 302 Found\r\nLocation: http://%s/index.html\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"

#define CONTENT_TYPE_HTML "text/html; charset=utf-8"
#define CONTENT_TYPE_JSON "application/json"

#define JSON_STATUS_OK    "{\"status\":\"ok\"}"
#define JSON_STATUS_ERROR "{\"status\":\"error\"}"

#define JSON_BUF_SIZE_MAX 1024

#define STATUS_JSON                                                                                                    \
    "{\"peopleCount\": %" PRId32 ",\n\"lightValue\": %" PRIu32 ",\n\"sen0dist\": %" PRIu16 ",\n\"sen1dist\": %" PRIu16 \
    ",\n\"sen2dist\": %" PRIu16 ",\n\"sen3dist\": %" PRIu16 ",\n\"sen0err\": %" PRIu32 ",\n\"sen1err\": %" PRIu32      \
    ",\n\"sen2err\": %" PRIu32 ",\n\"sen3err\": %" PRIu32 "}"

static const char index_html[] = {
#include "generated/index.html.h"
    0x00,
};

void content_response_free(ContentResponse *response) {
    if (response == NULL) {
        return;
    }
    free(response->header);
    if (!response->body_static) {
        free(response->body);
    }
    memset(response, 0, sizeof(*response));
}

static ErrCode set_body_copy(ContentResponse *response, const char *data, uint32_t len) {
    ErrCode err = ERR_SUCCESS;

    response->body = malloc(len);
    RETURN_IF_COND(response->body == NULL, ERR_MEM_ALLOC_FAIL);
    memcpy(response->body, data, len);
    response->body_len = len;
    response->body_static = false;

    return err;
}

static void set_body_static(ContentResponse *response, const char *data, uint32_t len) {
    response->body = (char *)data;
    response->body_len = len;
    response->body_static = true;
}

// Must be called after the body is set, as the header contains its length
static ErrCode set_header(ContentResponse *response, int code, const char *reason, const char *content_type) {
    ErrCode err = ERR_SUCCESS;

    response->header = malloc(HTTP_HEADER_MAX_SIZE);
    RETURN_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
    int len = snprintf(response->header, HTTP_HEADER_MAX_SIZE, HTTP_RESPONSE_HEADER, code, reason, response->body_len,
                       content_type);
    RETURN_IF_COND((len <= 0) || (len >= HTTP_HEADER_MAX_SIZE), ERR_OVERFLOW);
    response->header_len = len;

    return err;
}

static ErrCode set_redirect(ContentResponse *response, ip_addr_t *gw) {
    ErrCode err = ERR_SUCCESS;

    response->header = malloc(HTTP_HEADER_MAX_SIZE);
    RETURN_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
    int len = snprintf(response->header, HTTP_HEADER_MAX_SIZE, HTTP_RESPONSE_REDIRECT, ipaddr_ntoa(gw));
    RETURN_IF_COND((len <= 0) || (len >= HTTP_HEADER_MAX_SIZE), ERR_OVERFLOW);
    response->header_len = len;

    return err;
}

static ErrCode set_json_status(ContentResponse *response, bool ok) {
    ErrCode err = ERR_SUCCESS;

    if (ok) {
        set_body_static(response, JSON_STATUS_OK, sizeof(JSON_STATUS_OK) - 1);
        RETURN_IF_ERROR(set_header(response, 200, "OK", CONTENT_TYPE_JSON));
    } else {
        set_body_static(response, JSON_STATUS_ERROR, sizeof(JSON_STATUS_ERROR) - 1);
        RETURN_IF_ERROR(set_header(response, 400, "Bad Request", CONTENT_TYPE_JSON));
    }

    return err;
}

static ErrCode prepare_json_status(ContentResponse *response) {
    ErrCode err = ERR_SUCCESS;
    char json_buf[JSON_BUF_SIZE_MAX] = {0};

    Status *status = status_get();
    int len = snprintf(json_buf, sizeof(json_buf), STATUS_JSON, status->people_count, status->light_value,
                       status->sensor_distance[0], status->sensor_distance[1], status->sensor_distance[2],
                       status->sensor_distance[3], status->sensor_error_cnt[0], status->sensor_error_cnt[1],
                       status->sensor_error_cnt[2], status->sensor_error_cnt[3]);
    RETURN_IF_COND((len <= 0) || (len >= sizeof(json_buf)), ERR_OVERFLOW);
    RETURN_IF_ERROR(set_body_copy(response, json_buf, len));

    return err;
}

static ErrCode prepare_json_settings(ContentResponse *response) {
    ErrCode err = ERR_SUCCESS;
    char json_buf[JSON_BUF_SIZE_MAX] = {0};

    RETURN_IF_ERROR(get_settings_json(json_buf, sizeof(json_buf)));
    RETURN_IF_ERROR(set_body_copy(response, json_buf, strlen(json_buf)));

    return err;
}

ErrCode get_content(ip_addr_t *gw, ContentGetRequest *request, ContentResponse *response) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(gw == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(request == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(response == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(request->request == NULL, ERR_PARAM_IS_NULL);

#if PRINT_REQUEST
    INFO("GET Request: %s", request->request);
    for (uint32_t i = 0; i < request->params_n; i++) {
        INFO("Param %" PRIu32 ": %s", i, request->params[i]);
    }
#endif

    content_response_free(response);

    if ((strcmp(request->request, "/") == 0) || (strcmp(request->request, "/index.html") == 0)) {
        set_body_static(response, index_html, sizeof(index_html) - 1);
        TO_EXIT_IF_ERROR(set_header(response, 200, "OK", CONTENT_TYPE_HTML));
    } else if (strcmp(request->request, "/api/info") == 0) {
        TO_EXIT_IF_ERROR(prepare_json_status(response));
        TO_EXIT_IF_ERROR(set_header(response, 200, "OK", CONTENT_TYPE_JSON));
    } else if (strcmp(request->request, "/api/settings") == 0) {
        TO_EXIT_IF_ERROR(prepare_json_settings(response));
        TO_EXIT_IF_ERROR(set_header(response, 200, "OK", CONTENT_TYPE_JSON));
    } else {
        // Captive portal: any other page (generate_204, hotspot-detect.html, connecttest.txt, ...) goes to index
        TO_EXIT_IF_ERROR(set_redirect(response, gw));
    }

    return err;
EXIT:
    content_response_free(response);
    return err;
}

ErrCode post_content(ip_addr_t *gw, ContentPostRequest *request, ContentResponse *response) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(request == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(response == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(request->request == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(request->body == NULL, ERR_PARAM_IS_NULL);

#if PRINT_REQUEST
    INFO("POST Request: %s", request->request);
    INFO("POST Body: %s", request->body);
#endif

    content_response_free(response);

    if (strcmp(request->request, "/api/settings") == 0) {
        TO_EXIT_IF_ERROR(set_json_status(response, set_settings_json(request->body) == ERR_SUCCESS));
    } else if (strcmp(request->request, "/api/test_on") == 0) {
        status_get()->test_mode = true;
        TO_EXIT_IF_ERROR(set_json_status(response, true));
    } else if (strcmp(request->request, "/api/test_off") == 0) {
        status_get()->test_mode = false;
        TO_EXIT_IF_ERROR(set_json_status(response, true));
    } else if (strcmp(request->request, "/api/reset") == 0) {
        TO_EXIT_IF_ERROR(set_json_status(response, settings_default(false) == ERR_SUCCESS));
    } else if (strcmp(request->request, "/api/restart") == 0) {
        // Give the response time to reach the client before reboot
        reboot_request(500);
        TO_EXIT_IF_ERROR(set_json_status(response, true));
    } else {
        set_body_static(response, "", 0);
        TO_EXIT_IF_ERROR(set_header(response, 404, "Not Found", CONTENT_TYPE_JSON));
    }

    return err;
EXIT:
    content_response_free(response);
    return err;
}
