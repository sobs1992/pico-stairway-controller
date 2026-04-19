#pragma once

#include <global.h>

#define MAX_SSID_LEN 16
#define MAX_PASS_LEN 16

ErrCode net_ap_init(const char *ssid, const char *pass);
ErrCode net_ap_deinit(void);
ErrCode net_ap_set_state(bool en);
ErrCode net_poll(void);