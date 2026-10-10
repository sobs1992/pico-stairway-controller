#pragma once

#include <global.h>

#define MAX_SSID_LEN 33 /* 32 chars + '\0' */
#define MIN_PASS_LEN 8
#define MAX_PASS_LEN 64 /* 63 chars + '\0' */

ErrCode net_ap_init(const char *ssid, const char *pass);
ErrCode net_ap_deinit(void);
ErrCode net_ap_set_state(bool en);
bool net_ap_get_state(void);
ErrCode net_poll(void);