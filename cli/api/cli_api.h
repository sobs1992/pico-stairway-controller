#pragma once

#include <global.h>
#include "embedded_cli.h"

ErrCode cli_init(uint32_t cmd_count);
ErrCode cli_putchar(char ch);
ErrCode cli_add_command(char *cmd_token, char *help, void (*func)(EmbeddedCli *cli, char *args, void *context));