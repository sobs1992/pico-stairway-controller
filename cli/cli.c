#define FILE_ID "CL01"

#include "api/cli_api.h"

#define EMBEDDED_CLI_IMPL
#include "embedded_cli.h"

static EmbeddedCli *cli = NULL;

static void cli_write_char(EmbeddedCli *embeddedCli, char c) {
    putchar(c);
}

ErrCode cli_init(uint32_t cmd_count) {
    ErrCode err = ERR_SUCCESS;
    EmbeddedCliConfig *config = embeddedCliDefaultConfig();
    config->maxBindingCount = cmd_count;

    cli = embeddedCliNew(config);
    RETURN_IF_COND(cli == NULL, ERR_MEM_ALLOC_FAIL);
    cli->writeChar = cli_write_char;

    return err;
}

ErrCode cli_putchar(char ch) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(cli == NULL, ERR_NEED_INIT);

    embeddedCliReceiveChar(cli, ch);
    embeddedCliProcess(cli);

    return err;
}

ErrCode cli_add_command(const char *cmd_token, const char *help,
                        void (*func)(EmbeddedCli *cli, char *args, void *context), void *context) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(cli == NULL, ERR_NEED_INIT);

    CliCommandBinding cmd = {
        cmd_token, // command name (spaces are not allowed)
        help,      // Optional help for a command (NULL for no help)
        true,      // flag whether to tokenize arguments
        context,   // optional pointer to any application context
        func       // binding function
    };

    RETURN_IF_COND(!embeddedCliAddBinding(cli, cmd), ERR_OVERFLOW);

    return err;
}
