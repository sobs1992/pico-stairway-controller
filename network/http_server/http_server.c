#define FILE_ID "NN02"

#include <string.h>
#include "http_server.h"
#include "pico/cyw43_arch.h"

#define TCP_PORT    80
#define POLL_TIME_S 5
#define HTTP_GET    "GET"
#define HTTP_RESPONSE_HEADERS                                                                                          \
    "HTTP/1.1 %d OK\nContent-Length: %d\nContent-Type: text/html; charset=utf-8\nConnection: close\n\n"
#define HTTP_RESPONSE_REDIRECT "HTTP/1.1 302 Redirect\nLocation: http://%s" PAGE_NAME "\n\n"

#define PAGE_CONTENT "<html><body><h1>Hello from Pico!</h1></body></html>"
//#define PAGE_CONTENT                                                                                                   \
//    "<html><body><h1>Hello from Pico.</h1><p>Led is %s</p><p><a href=\"?led=%d\">Turn led %s</a></body></html>"

#define LED_PARAM "led=%d"

#define PAGE_NAME "/index.html"

typedef struct TCP_CONNECT_STATE_T_ {
    struct tcp_pcb *pcb;
    int sent_len;
    char headers[128];
    char result[512];
    int header_len;
    int result_len;
    ip_addr_t *gw;
} TCP_CONNECT_STATE_T;

static err_t tcp_close_client_connection(TCP_CONNECT_STATE_T *con_state, struct tcp_pcb *client_pcb, err_t close_err);

static int test_server_content(const char *request, const char *params, char *result, size_t max_result_len) {
    int len = 0;
    if (strncmp(request, PAGE_NAME, sizeof(PAGE_NAME) - 1) == 0) {
        // See if the user changed it
#if 0
        int led_state = cyw43_arch_gpio_get(CYW43_WL_GPIO_LED_PIN);
#endif
        if (params) {
#if 0
            int led_param = sscanf(params, LED_PARAM, &led_state);
            if (led_param == 1) {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
            }
#endif
        }
        // Generate result
#if 0
        len = snprintf(result, max_result_len, PAGE_CONTENT, led_state ? "ON" : "OFF", led_state ? 0 : 1, "TOGGLE");
#else
        strncpy(result, PAGE_CONTENT, max_result_len);
        len = strlen(PAGE_CONTENT);
#endif
    }
    return len;
}

err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T *)arg;
    if (!p) {
        INFO("Connection closed");
        return tcp_close_client_connection(con_state, pcb, ERR_OK);
    }
    assert(con_state && con_state->pcb == pcb);
    if (p->tot_len > 0) {
        INFO("tcp_server_recv %d err %d", p->tot_len, err);
#if 0
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            INFO("in: %.*s", q->len, q->payload);
        }
#endif
        // Copy the request into the buffer
        pbuf_copy_partial(p, con_state->headers,
                          p->tot_len > sizeof(con_state->headers) - 1 ? sizeof(con_state->headers) - 1 : p->tot_len, 0);

        // Handle GET request
        if (strncmp(HTTP_GET, con_state->headers, sizeof(HTTP_GET) - 1) == 0) {
            char *request = con_state->headers + sizeof(HTTP_GET); // + space
            char *params = strchr(request, '?');
            if (params) {
                if (*params) {
                    char *space = strchr(request, ' ');
                    *params++ = 0;
                    if (space) {
                        *space = 0;
                    }
                } else {
                    params = NULL;
                }
            }

            // Generate content
            con_state->result_len = test_server_content(request, params, con_state->result, sizeof(con_state->result));
            INFO("Request: %s?%s", request, params);
            INFO("Result: %d", con_state->result_len);

            // Check we had enough buffer space
            if (con_state->result_len > sizeof(con_state->result) - 1) {
                INFO("Too much result data %d", con_state->result_len);
                return tcp_close_client_connection(con_state, pcb, ERR_CLSD);
            }

            // Generate web page
            if (con_state->result_len > 0) {
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_HEADERS,
                                                 200, con_state->result_len);
                if (con_state->header_len > sizeof(con_state->headers) - 1) {
                    INFO("Too much header data %d", con_state->header_len);
                    return tcp_close_client_connection(con_state, pcb, ERR_CLSD);
                }
            } else {
                // Send redirect
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_REDIRECT,
                                                 ipaddr_ntoa(con_state->gw));
                INFO("Sending redirect %s", con_state->headers);
            }

            // Send the headers to the client
            con_state->sent_len = 0;
            err_t err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
            if (err != ERR_OK) {
                INFO("failed to write header data %d", err);
                return tcp_close_client_connection(con_state, pcb, err);
            }

            // Send the body to the client
            if (con_state->result_len) {
                err = tcp_write(pcb, con_state->result, con_state->result_len, 0);
                if (err != ERR_OK) {
                    INFO("failed to write result data %d", err);
                    return tcp_close_client_connection(con_state, pcb, err);
                }
            }
        }
        tcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
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

static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T *)arg;
    INFO("tcp_server_sent %u", len);
    con_state->sent_len += len;
    if (con_state->sent_len >= con_state->header_len + con_state->result_len) {
        INFO("All done");
        return tcp_close_client_connection(con_state, pcb, ERR_OK);
    }
    return ERR_OK;
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T *)arg;
    INFO("tcp_server_poll_fn");
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
    INFO("Client connected");

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