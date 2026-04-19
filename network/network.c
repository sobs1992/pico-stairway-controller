/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#define FILE_ID "NN01"

#include "global.h"

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "dhcpserver.h"
#include "dnsserver.h"
#include "http_server.h"

#include "api/network_api.h"

#define AP_AUTH CYW43_AUTH_WPA2_AES_PSK

static bool ap_inited = false;
static bool ap_started = false;

static TCP_SERVER_T *tcp_server_state = NULL;
static dhcp_server_t dhcp_server = {};
static dns_server_t dns_server = {};

static char ap_ssid[MAX_SSID_LEN] = {};
static char ap_pass[MAX_PASS_LEN] = {};

ErrCode net_ap_init(const char *ssid, const char *pass) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(ap_inited, ERR_ALREADY_INITED);
    RETURN_IF_COND(ssid == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(pass == NULL, ERR_PARAM_IS_NULL);
    size_t ssid_len = strlen(ssid);
    size_t pass_len = strlen(pass);
    RETURN_IF_COND(ssid_len >= MAX_SSID_LEN, ERR_PARAM_INVALID);
    RETURN_IF_COND(pass_len >= MAX_PASS_LEN, ERR_PARAM_INVALID);

    tcp_server_state = calloc(1, sizeof(TCP_SERVER_T));
    RETURN_IF_COND(tcp_server_state == NULL, ERR_MEM_ALLOC_FAIL);

#if LWIP_IPV6
#define IP(x) ((x).u_addr.ip4)
#else
#define IP(x) (x)
#endif
    ip4_addr_t mask;
    IP(tcp_server_state->gw).addr = PP_HTONL(CYW43_DEFAULT_IP_AP_ADDRESS);
    IP(mask).addr = PP_HTONL(CYW43_DEFAULT_IP_MASK);
#undef IP

    // Start the dhcp server
    dhcp_server_init(&dhcp_server, &tcp_server_state->gw, &mask);

    // Start the dns server
    dns_server_init(&dns_server, &tcp_server_state->gw);

    TO_EXIT_IF_ERROR(http_server_init(tcp_server_state));

    strcpy(ap_ssid, ssid);
    strcpy(ap_pass, pass);
    ap_inited = true;

    return err;
EXIT:
    net_ap_deinit();
    return err;
}

ErrCode net_poll(void) {
    ErrCode err = ERR_SUCCESS;

#if PICO_CYW43_ARCH_POLL
    if (ap_inited && ap_started) {
        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer interrupt) to check for Wi-Fi driver or lwIP work that needs to be done.
        cyw43_arch_poll();
        // you can poll as often as you like, however if you have nothing else to do you can
        // choose to sleep until either a specified time, or cyw43_arch_poll() has work to do:
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(1000));
    }
#endif

    return err;
}

ErrCode net_ap_deinit(void) {
    ErrCode err = ERR_SUCCESS;

    net_ap_set_state(false);
    http_server_deinit(tcp_server_state);
    dns_server_deinit(&dns_server);
    dhcp_server_deinit(&dhcp_server);
    memset(ap_ssid, 0, sizeof(ap_ssid));
    memset(ap_pass, 0, sizeof(ap_pass));
    free(tcp_server_state);
    ap_inited = false;

    return err;
}

ErrCode net_ap_set_state(bool en) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(!ap_inited, ERR_NEED_INIT);

    if (ap_started != en) {
        if (en) {
            cyw43_arch_enable_ap_mode(ap_ssid, ap_pass, AP_AUTH);
            INFO("WiFi AP Started!");
        } else {
            cyw43_arch_lwip_begin();
            cyw43_arch_disable_ap_mode();
            cyw43_arch_lwip_end();
            INFO("WiFi AP Stopped!");
        }
        ap_started = en;
    }

    return err;
}
