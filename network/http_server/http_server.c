#define FILE_ID "NN02"

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "http_server.h"
#include "content.h"
#include "pico/cyw43_arch.h"

#define TCP_PORT 80
// tcp_poll() interval is measured in TCP coarse timer ticks (500 ms)
#define POLL_INTERVAL_TICKS 2
// Close a connection without any rx/tx progress after this number of polls (~10 s)
#define IDLE_POLLS_MAX 10

#define HTTP_GET            "GET "
#define HTTP_POST           "POST "
#define HTTP_CONTENT_LENGTH "Content-Length:"

#define HTTP_REQUEST_MAX_SIZE 2048

typedef struct {
    char data[HTTP_REQUEST_MAX_SIZE + 1]; // + '\0'
    uint32_t data_len;
    uint32_t body_offset;
    uint32_t content_len;
    bool header_complete;
    bool processed;
} HttpRequest;

typedef struct TCP_CONNECT_STATE_T_ {
    struct tcp_pcb *pcb;
    HttpRequest request;
    ContentResponse response;
    uint32_t queued_len; // bytes passed to tcp_write()
    uint32_t acked_len;  // bytes acknowledged by the client
    uint32_t idle_polls;
    ip_addr_t *gw;
} TCP_CONNECT_STATE_T;

static void con_state_free(TCP_CONNECT_STATE_T *con_state) {
    if (con_state) {
        content_response_free(&con_state->response);
        free(con_state);
    }
}

// Closes the client connection and frees its state. Returns the value the lwIP callback must return.
static err_t tcp_close_client_connection(TCP_CONNECT_STATE_T *con_state, struct tcp_pcb *client_pcb) {
    err_t ret = ERR_OK;
    if (client_pcb) {
        tcp_arg(client_pcb, NULL);
        tcp_poll(client_pcb, NULL, 0);
        tcp_sent(client_pcb, NULL);
        tcp_recv(client_pcb, NULL);
        tcp_err(client_pcb, NULL);
        err_t err = tcp_close(client_pcb);
        if (err != ERR_OK) {
            INFO("Close failed %d, calling abort", err);
            tcp_abort(client_pcb);
            ret = ERR_ABRT;
        }
    }
    con_state_free(con_state);
    return ret;
}

static uint32_t response_total_len(const ContentResponse *r) {
    return r->header_len + r->body_len;
}

// Queues as much of the response as the TCP send buffer allows. The rest is sent from the sent callback.
static err_t send_more(TCP_CONNECT_STATE_T *con_state) {
    ContentResponse *r = &con_state->response;
    uint32_t total = response_total_len(r);

    while (con_state->queued_len < total) {
        uint32_t avail = tcp_sndbuf(con_state->pcb);
        if (avail == 0) {
            break;
        }

        const char *data = NULL;
        uint32_t len = 0;
        u8_t flags = TCP_WRITE_FLAG_COPY;
        if (con_state->queued_len < r->header_len) {
            data = &r->header[con_state->queued_len];
            len = r->header_len - con_state->queued_len;
        } else {
            uint32_t offset = con_state->queued_len - r->header_len;
            data = &r->body[offset];
            len = r->body_len - offset;
            if (r->body_static) {
                flags = 0; // data stays valid, no need to copy it
            }
        }
        if (len > avail) {
            len = avail;
        }
        if (len > UINT16_MAX) {
            len = UINT16_MAX;
        }
        if (con_state->queued_len + len < total) {
            flags |= TCP_WRITE_FLAG_MORE;
        }

        err_t tcp_err = tcp_write(con_state->pcb, data, len, flags);
        if (tcp_err == ERR_MEM) {
            break; // queue is full, continue from the sent/poll callback
        }
        if (tcp_err != ERR_OK) {
            INFO("tcp_write error: %d", tcp_err);
            return tcp_err;
        }
        con_state->queued_len += len;
    }

    return tcp_output(con_state->pcb);
}

static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T *)arg;
    con_state->acked_len += len;
    con_state->idle_polls = 0;

    if (con_state->acked_len >= response_total_len(&con_state->response)) {
        return tcp_close_client_connection(con_state, pcb);
    }
    if (send_more(con_state) != ERR_OK) {
        return tcp_close_client_connection(con_state, pcb);
    }
    return ERR_OK;
}

static uint32_t get_content_length(const char *header) {
    const char *line = header;
    while (line && *line) {
        if (strncasecmp(line, HTTP_CONTENT_LENGTH, sizeof(HTTP_CONTENT_LENGTH) - 1) == 0) {
            return strtoul(line + sizeof(HTTP_CONTENT_LENGTH) - 1, NULL, 10);
        }
        line = strchr(line, '\n');
        if (line) {
            line++;
        }
    }
    return 0;
}

// Cuts "METHOD /path HTTP/1.1" to "/path" (null terminated) and returns it
static char *get_request_path(char *header, size_t method_len) {
    char *path = header + method_len;
    char *end = strpbrk(path, " \r\n");
    if (end) {
        *end = '\0';
    }
    return path;
}

static void free_get_params(ContentGetRequest *req) {
    if (req->params) {
        for (uint32_t i = 0; i < req->params_n; i++) {
            free(req->params[i]);
        }
        free(req->params);
        req->params = NULL;
    }
    req->params_n = 0;
}

static ErrCode parse_get_params(char *params, ContentGetRequest *req) {
    ErrCode err = ERR_SUCCESS;

    uint32_t params_cnt = 1;
    for (char *c = params; *c; c++) {
        if (*c == '&') {
            params_cnt++;
            *c = '\0';
        }
    }
    req->params = calloc(params_cnt, sizeof(req->params[0]));
    RETURN_IF_COND(req->params == NULL, ERR_MEM_ALLOC_FAIL);
    for (uint32_t i = 0; i < params_cnt; i++) {
        size_t len = strlen(params);
        req->params[i] = strdup(params);
        RETURN_IF_COND(req->params[i] == NULL, ERR_MEM_ALLOC_FAIL);
        req->params_n++;
        params += len + 1;
    }

    return err;
}

static ErrCode handle_request(TCP_CONNECT_STATE_T *con_state) {
    ErrCode err = ERR_SUCCESS;
    HttpRequest *request = &con_state->request;
    char *header = request->data;
    ContentGetRequest req_get = {0};

    if (strncmp(header, HTTP_GET, sizeof(HTTP_GET) - 1) == 0) {
        req_get.request = get_request_path(header, sizeof(HTTP_GET) - 1);
        char *params = strchr(req_get.request, '?');
        if (params) {
            *params++ = '\0';
            TO_EXIT_IF_ERROR(parse_get_params(params, &req_get));
        }
        TO_EXIT_IF_ERROR(get_content(con_state->gw, &req_get, &con_state->response));
    } else if (strncmp(header, HTTP_POST, sizeof(HTTP_POST) - 1) == 0) {
        ContentPostRequest req_post = {
            .request = get_request_path(header, sizeof(HTTP_POST) - 1),
            .body = request->data + request->body_offset,
            .body_len = request->content_len,
        };
        TO_EXIT_IF_ERROR(post_content(con_state->gw, &req_post, &con_state->response));
    } else {
        TO_EXIT_IF_ERROR(ERR_PARAM_INVALID);
    }
    TO_EXIT_IF_COND(con_state->response.header_len == 0, ERR_FAIL);

EXIT:
    free_get_params(&req_get);
    return err;
}

static err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t tcp_err) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T *)arg;

    if (!p) {
        return tcp_close_client_connection(con_state, pcb);
    }
    tcp_recved(pcb, p->tot_len);
    if ((tcp_err != ERR_OK) || (con_state == NULL)) {
        pbuf_free(p);
        return tcp_close_client_connection(con_state, pcb);
    }
    con_state->idle_polls = 0;

    HttpRequest *request = &con_state->request;
    if (request->processed) {
        // Response is already being sent, ignore the rest
        pbuf_free(p);
        return ERR_OK;
    }

    if (p->tot_len > HTTP_REQUEST_MAX_SIZE - request->data_len) {
        INFO("HTTP request is too large");
        pbuf_free(p);
        return tcp_close_client_connection(con_state, pcb);
    }
    pbuf_copy_partial(p, request->data + request->data_len, p->tot_len, 0);
    request->data_len += p->tot_len;
    request->data[request->data_len] = '\0';
    pbuf_free(p);

    if (!request->header_complete) {
        size_t sep_len = sizeof("\r\n\r\n") - 1;
        char *header_end = strstr(request->data, "\r\n\r\n");
        if (header_end == NULL) {
            sep_len = sizeof("\n\n") - 1;
            header_end = strstr(request->data, "\n\n");
        }
        if (header_end == NULL) {
            return ERR_OK; // wait for the rest of the header
        }
        *header_end = '\0';
        request->body_offset = (header_end - request->data) + sep_len;
        request->content_len = get_content_length(request->data);
        if (request->content_len > HTTP_REQUEST_MAX_SIZE - request->body_offset) {
            INFO("HTTP request body is too large");
            return tcp_close_client_connection(con_state, pcb);
        }
        request->header_complete = true;
    }

    if (request->data_len - request->body_offset < request->content_len) {
        return ERR_OK; // wait for the rest of the body
    }
    request->data[request->body_offset + request->content_len] = '\0';
    request->processed = true;

    if ((handle_request(con_state) != ERR_SUCCESS) || (send_more(con_state) != ERR_OK)) {
        return tcp_close_client_connection(con_state, pcb);
    }

    return ERR_OK;
}

static void tcp_server_close(TCP_SERVER_T *state) {
    if (state->server_pcb) {
        tcp_arg(state->server_pcb, NULL);
        tcp_close(state->server_pcb);
        state->server_pcb = NULL;
    }
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T *)arg;
    if ((con_state == NULL) || (++con_state->idle_polls > IDLE_POLLS_MAX)) {
        return tcp_close_client_connection(con_state, pcb);
    }
    if ((con_state->request.processed) && (send_more(con_state) != ERR_OK)) {
        return tcp_close_client_connection(con_state, pcb);
    }
    return ERR_OK;
}

static void tcp_server_err(void *arg, err_t err) {
    // pcb is already freed by lwIP here, only the connection state must be released
    if (err != ERR_ABRT) {
        INFO("tcp_client_err_fn %d", err);
    }
    con_state_free((TCP_CONNECT_STATE_T *)arg);
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
        tcp_abort(client_pcb);
        return ERR_ABRT;
    }
    con_state->pcb = client_pcb;
    con_state->gw = &state->gw;

    // setup connection to client
    tcp_arg(client_pcb, con_state);
    tcp_sent(client_pcb, tcp_server_sent);
    tcp_recv(client_pcb, tcp_server_recv);
    tcp_poll(client_pcb, tcp_server_poll, POLL_INTERVAL_TICKS);
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

    RETURN_IF_COND(tcp_server == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_ERROR(tcp_server_open(tcp_server));

    return err;
}

ErrCode http_server_deinit(TCP_SERVER_T *tcp_server) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(tcp_server == NULL, ERR_PARAM_IS_NULL);
    tcp_server_close(tcp_server);
    return err;
}
