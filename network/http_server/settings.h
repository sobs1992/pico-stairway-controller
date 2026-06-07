#pragma once

#include "global.h"
#include "api/settings_api.h"

ErrCode get_settings_json(char *buf, uint32_t buf_size);
ErrCode set_settings_json(char *buf, uint32_t buf_size);