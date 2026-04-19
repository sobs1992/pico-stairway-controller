#pragma once

#include "global.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    ip_addr_t gw;
} TCP_SERVER_T;

ErrCode http_server_init(TCP_SERVER_T *tcp_server);
ErrCode http_server_deinit(TCP_SERVER_T *tcp_server);