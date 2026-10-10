#pragma once

#include "global.h"
#include "api/settings_api.h"

ErrCode get_settings_json(char *buf, uint32_t buf_size);
// Parses settings from JSON, validates and applies them. Nothing is changed on error.
ErrCode set_settings_json(const char *json);
