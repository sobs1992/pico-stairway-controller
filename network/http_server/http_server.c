#define FILE_ID "NN02"

#include <string.h>
#include "http_server.h"
#include "content.h"
#include "pico/cyw43_arch.h"

#define TCP_PORT    80
#define POLL_TIME_S 5
#define HTTP_GET    "GET"
#define HTTP_POST   "POST"

#define CHUNK_SIZE 1024

typedef struct TCP_CONNECT_STATE_T_ {
    struct tcp_pcb *pcb;
    int sent_len;
    char headers[HTTP_HEADER_MAX_SIZE];
    ContentResponse response;
    ip_addr_t *gw;
} TCP_CONNECT_STATE_T;

static err_t tcp_close_client_connection(TCP_CONNECT_STATE_T *con_state, struct tcp_pcb *client_pcb, err_t close_err);

static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    ErrCode err = ERR_SUCCESS;

    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T *)arg;
    con_state->sent_len += len;

    char *data = NULL;
    uint32_t data_len = 0;
    if (con_state->sent_len < con_state->response.header_len) {
        data_len = con_state->response.header_len - con_state->sent_len;
        if (data_len > CHUNK_SIZE) {
            data_len = CHUNK_SIZE;
        }
        data = &con_state->response.header[con_state->sent_len];
    } else {
        uint32_t sent_body_len = con_state->sent_len - con_state->response.header_len;
        if (sent_body_len < con_state->response.body_len) {
            data_len = con_state->response.body_len - sent_body_len;
            if (data_len > CHUNK_SIZE) {
                data_len = CHUNK_SIZE;
            }
            data = &con_state->response.body[sent_body_len];
        } else {
            if (con_state->response.header) {
                free(con_state->response.header);
            }
            if (con_state->response.body) {
                free(con_state->response.body);
            }
            memset(&con_state->response, 0, sizeof(con_state->response));
            return tcp_close_client_connection(con_state, pcb, ERR_OK);
        }
    }

    TO_EXIT_IF_COND(data == NULL, ERR_FAIL);
    err_t tcp_err = tcp_write(con_state->pcb, data, data_len, TCP_WRITE_FLAG_COPY);
    if (tcp_err == ERR_OK) {
        tcp_output(con_state->pcb);
        return ERR_OK;
    } else {
        INFO("tcp_write error: %d", tcp_err);
        TO_EXIT_IF_ERROR(ERR_FAIL);
    }

    return ERR_OK;

EXIT:
    if (con_state->response.header) {
        free(con_state->response.header);
    }
    if (con_state->response.body) {
        free(con_state->response.body);
    }
    memset(&con_state->response, 0, sizeof(con_state->response));
    return tcp_close(con_state->pcb);
}

static err_t send_tcp_data(TCP_CONNECT_STATE_T *con_state) {
    ErrCode err = ERR_SUCCESS;
    err_t tcp_err = ERR_OK;

    con_state->sent_len = 0;
    size_t first_chunk = con_state->response.header_len;
    if (con_state->response.header_len > CHUNK_SIZE) {
        first_chunk = CHUNK_SIZE;
    }

    tcp_err = tcp_write(con_state->pcb, con_state->response.header, first_chunk, TCP_WRITE_FLAG_COPY);
    TO_EXIT_IF_COND(tcp_err != ERR_OK, ERR_FAIL);

    tcp_err = tcp_output(con_state->pcb);
    TO_EXIT_IF_COND(tcp_err != ERR_OK, ERR_FAIL);
#if 0
    INFO("Started sending %" PRIu32 " bytes (first chunk: %d)",
         con_state->response.header_len + con_state->response.body_len, first_chunk);
#endif
    return ERR_OK;
EXIT:
    if (con_state->response.header) {
        free(con_state->response.header);
    }
    if (con_state->response.body) {
        free(con_state->response.body);
    }
    memset(&con_state->response, 0, sizeof(con_state->response));
    return tcp_err;
}

static err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t tcp_err) {
    ErrCode err = ERR_SUCCESS;
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T *)arg;
    ContentGetRequest req_get = {0};
    ContentPostRequest req_post = {0};
    memset(&req_get, 0, sizeof(req_get));
    memset(&req_post, 0, sizeof(req_post));

    if (!p) {
        INFO("Connection closed");
        return tcp_close_client_connection(con_state, pcb, ERR_OK);
    }
    assert(con_state && con_state->pcb == pcb);
    if (p->tot_len > 0) {
        // Copy the request into the buffer
        pbuf_copy_partial(p, con_state->headers,
                          p->tot_len > sizeof(con_state->headers) - 1 ? sizeof(con_state->headers) - 1 : p->tot_len, 0);
#if 0
        INFO("Receive header: %s", con_state->headers);
#endif
        // Handle GET request
        if (strncmp(HTTP_GET, con_state->headers, sizeof(HTTP_GET) - 1) == 0) {
            req_get.request = con_state->headers + sizeof(HTTP_GET); // + space;
            char *end_req = strstr(req_get.request, "HTTP/");
            if (end_req) {
                *end_req = 0;
            }

            char *params = strchr(req_get.request, '?');
            if (params) {
                *params++ = 0;
                uint32_t params_cnt = 1;
                uint32_t len = strlen(params);
                for (uint32_t i = 0; i < len; i++) {
                    if (params[i] == '&') {
                        params_cnt++;
                        params[i] = 0;
                    }
                }
                req_get.params = malloc(sizeof(req_get.params[0]) * params_cnt);
                TO_EXIT_IF_COND(req_get.params == NULL, ERR_MEM_ALLOC_FAIL);
                memset(req_get.params, 0, sizeof(req_get.params[0]) * params_cnt);
                for (uint32_t i = 0; i < params_cnt; i++) {
                    len = strlen(params);
                    req_get.params[i] = malloc(len);
                    TO_EXIT_IF_COND(req_get.params[i] == NULL, ERR_MEM_ALLOC_FAIL);
                    strcpy(req_get.params[req_get.params_n], params);
                    params += len + 1;
                    req_get.params_n++;
                }
            }

            // Generate content
            TO_EXIT_IF_ERROR(get_content(con_state->gw, &req_get, &con_state->response));

            if (req_get.params) {
                for (uint32_t i = 0; i < req_get.params_n; i++) {
                    if (req_get.params[i]) {
                        free(req_get.params[i]);
                    }
                }
                free(req_get.params);
            }
        } else if (strncmp(HTTP_POST, con_state->headers, sizeof(HTTP_POST) - 1) == 0) {
            req_post.request = con_state->headers + sizeof(HTTP_POST); // + space;
            INFO("POST REQUEST: %s\n", req_post.request);
            char *end_req = strstr(req_post.request, "HTTP/");
            if (end_req) {
                *end_req++ = 0;
            }
            char *body_start = strstr(end_req, "\n\n");
            if (body_start) {
                body_start += sizeof("\n\n") - 1;
            } else
                body_start = strstr(end_req, "\r\n\r\n");
            if (body_start) {
                body_start += sizeof("\r\n\r\n") - 1;
            }
            TO_EXIT_IF_COND(body_start == NULL, ERR_FAIL);
            req_post.body = body_start;
            req_post.body_len = strlen(body_start);

            // Generate content
            TO_EXIT_IF_ERROR(post_content(con_state->gw, &req_post, &con_state->response));
        }
        err_t send_err = send_tcp_data(con_state);
        TO_EXIT_IF_COND(send_err != ERR_OK, ERR_FAIL);
        tcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
EXIT:
    if (req_get.params) {
        for (uint32_t i = 0; i < req_get.params_n; i++) {
            if (req_get.params[i]) {
                free(req_get.params[i]);
            }
        }
        free(req_get.params);
    }
    return tcp_close_client_connection(con_state, pcb, ERR_CLSD);
}

static void tcp_server_close(TCP_SERVER_T *state) {
    if (state->server_pcb) {
        tcp_arg(state->server_pcb, NULL);
        tcp_close(state->server_pcb);
        state->server_pcb = NULL;
    }
}

static err_t tcp_close_client_connection(TCP_CONNECT_STATE_T *con_state, struct tcp_pcb *client_pcb, err_t close_err) {
    if (client_pcb) {
        assert(con_state && con_state->pcb == client_pcb);
        tcp_arg(client_pcb, NULL);
        tcp_poll(client_pcb, NULL, 0);
        tcp_sent(client_pcb, NULL);
        tcp_recv(client_pcb, NULL);
        tcp_err(client_pcb, NULL);
        err_t err = tcp_close(client_pcb);
        if (err != ERR_OK) {
            INFO("Close failed %d, calling abort", err);
            tcp_abort(client_pcb);
            close_err = ERR_ABRT;
        }
        if (con_state) {
            free(con_state);
        }
    }
    return close_err;
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T *)arg;
    return tcp_close_client_connection(con_state, pcb, ERR_OK); // Just disconnect clent?
}

static void tcp_server_err(void *arg, err_t err) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T *)arg;
    if (err != ERR_ABRT) {
        INFO("tcp_client_err_fn %d", err);
        tcp_close_client_connection(con_state, con_state->pcb, err);
    }
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T *)arg;
    if (err != ERR_OK || client_pcb == NULL) {
        INFO("Failure in accept");
        return ERR_VAL;
    }

    // Create the state for the connection
    TCP_CONNECT_STATE_T *con_state = calloc(1, sizeof(TCP_CONNECT_STATE_T));
    if (!con_state) {
        INFO("Failed to allocate connect state");
        return ERR_MEM;
    }
    con_state->pcb = client_pcb; // for checking
    con_state->gw = &state->gw;

    // setup connection to client
    tcp_arg(client_pcb, con_state);
    tcp_sent(client_pcb, tcp_server_sent);
    tcp_recv(client_pcb, tcp_server_recv);
    tcp_poll(client_pcb, tcp_server_poll, POLL_TIME_S);
    tcp_err(client_pcb, tcp_server_err);

    return ERR_OK;
}

static ErrCode tcp_server_open(TCP_SERVER_T *tcp_server) {
    ErrCode err = ERR_SUCCESS;

    INFO("Starting server on port %d", TCP_PORT);

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    RETURN_IF_COND(pcb == NULL, ERR_FAIL);

    err_t bind_err = tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT);
    if (bind_err) {
        INFO("Failed to bind to port %d", TCP_PORT);
        TO_EXIT_IF_COND(bind_err, ERR_FAIL);
    }

    tcp_server->server_pcb = tcp_listen_with_backlog(pcb, 1);
    TO_EXIT_IF_COND(!tcp_server->server_pcb, ERR_FAIL);

    tcp_arg(tcp_server->server_pcb, (void *)tcp_server);
    tcp_accept(tcp_server->server_pcb, tcp_server_accept);

    INFO("TCP server created!");
    return err;

EXIT:
    if (pcb) {
        tcp_close(pcb);
    }
    return err;
}

ErrCode http_server_init(TCP_SERVER_T *tcp_server) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_ERROR(tcp_server_open(tcp_server));

    return err;
}

ErrCode http_server_deinit(TCP_SERVER_T *tcp_server) {
    ErrCode err = ERR_SUCCESS;
    tcp_server_close(tcp_server);
    return err;
}