#pragma once

#include "global.h"
#include "http_server.h"

typedef struct {
    char *request;
    char **params;
    uint32_t params_n;
} ContentGetRequest;

typedef struct {
    char *request;
    char *body;
    uint32_t body_len;
} ContentPostRequest;

typedef struct {
    char *header;
    uint32_t header_len;
    char *body;
    uint32_t body_len;
    bool body_static; // body points to constant data and must not be freed
} ContentResponse;

void content_response_free(ContentResponse *response);

ErrCode get_content(ip_addr_t *gw, ContentGetRequest *request, ContentResponse *response);
ErrCode post_content(ip_addr_t *gw, ContentPostRequest *request, ContentResponse *response);