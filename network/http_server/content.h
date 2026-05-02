#pragma once

#include "global.h"
#include "http_server.h"

typedef struct {
    char *request;
    char **params;
    uint32_t params_n;
} ContentRequest;

typedef struct {
    char *header;
    uint32_t header_len;
    char *body;
    uint32_t body_len;
} ContentResponse;

ErrCode generate_content(ip_addr_t *gw, ContentRequest *request, ContentResponse *response);