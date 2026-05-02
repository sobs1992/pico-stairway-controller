#define FILE_ID "NN03"

#include "content.h"
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define PRINT_REQUEST  1
#define PRINT_RESPONSE 1

#define CHECK_REQUEST(req, param) (strncmp(req, param, sizeof(param) - 1) == 0)
#define HTTP_RESPONSE_HEADER                                                                                           \
    "HTTP/1.1 %d OK\nContent-Length: %ld\nContent-Type: text/html; charset=utf-8\nConnection: close\n\n"
#define HTTP_ERROR_HEADER      "HTTP/1.1 %d Not Found\nContent-Type: text/html\nConnection: close\n\n"
#define HTTP_RESPONSE_REDIRECT "HTTP/1.1 302 Redirect\nLocation: http://%s%s\n\n"

static const char index_html[] = {
#include "generated/index.html.h"
    0x00,
};

ErrCode generate_content(ip_addr_t *gw, ContentRequest *request, ContentResponse *response) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(request == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(response == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(request->request == NULL, ERR_PARAM_IS_NULL);
    for (uint32_t i = 0; i < request->params_n; i++) {
        TO_EXIT_IF_COND(request->params[i] == NULL, ERR_PARAM_INVALID);
    }

#if PRINT_REQUEST
    INFO("Request: %s", request->request);
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
            snprintf(response->header, HTTP_HEADER_MAX_SIZE, HTTP_RESPONSE_HEADER, 200, response->body_len);
    } else if (CHECK_REQUEST(request->request, "/ ")) {
        response->header = malloc(HTTP_HEADER_MAX_SIZE);
        TO_EXIT_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
        response->header_len =
            snprintf(response->header, HTTP_HEADER_MAX_SIZE, HTTP_RESPONSE_REDIRECT, ipaddr_ntoa(gw), "/index.html");

    } else {
        response->header = malloc(HTTP_HEADER_MAX_SIZE);
        TO_EXIT_IF_COND(response->header == NULL, ERR_MEM_ALLOC_FAIL);
        response->header_len = snprintf(response->header, HTTP_HEADER_MAX_SIZE, HTTP_ERROR_HEADER, 404);
    }

#if PRINT_RESPONSE
    INFO("Response:");
    printf("\tHeader: %s", response->header);
    if (response->body) {
        printf("\tBody: %s", response->body);
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