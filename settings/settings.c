#define FILE_ID "ST01"

#include "api/settings_api.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include <string.h>
#include "settings_list.h"
#include "pico/time.h"

static Settings global_settings = {0};

const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + SETTINGS_OFFSET);

ErrCode settings_init(void) {
    ErrCode err = ERR_SUCCESS;

    uint32_t magic = *(uint32_t *)flash_target_contents;
    memcpy(&global_settings, &default_settings, sizeof(Settings));
    if (magic != default_settings.magic) {
        RETURN_IF_ERROR(settings_write());
    }
    memcpy(&global_settings, flash_target_contents, sizeof(Settings));

    return err;
}

ErrCode settings_default(void) {
    ErrCode err = ERR_SUCCESS;
    memcpy(&global_settings, &default_settings, sizeof(Settings));
    RETURN_IF_ERROR(settings_write());
    printf("Device will reboot!\n");
    sleep_ms(1000);
    watchdog_reboot(0, 0, 0);
    return err;
}

Settings *settings_get(void) {
    return &global_settings;
}

// This function will be called when it's safe to call flash_range_erase
static void call_flash_range_erase(void *param) {
    uint32_t offset = (uint32_t)param;
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
}

// This function will be called when it's safe to call flash_range_program
static void call_flash_range_program(void *param) {
    uint32_t offset = ((uintptr_t *)param)[0];
    const uint8_t *data = (const uint8_t *)((uintptr_t *)param)[1];
    flash_range_program(offset, data, FLASH_PAGE_SIZE);
}

ErrCode settings_write(void) {
    ErrCode err = ERR_SUCCESS;

    if (memcmp(flash_target_contents, &global_settings, sizeof(Settings)) != 0) {
        INFO("Erasing target region...");
        RETURN_IF_COND(flash_safe_execute(call_flash_range_erase, (void *)SETTINGS_OFFSET, UINT32_MAX) != PICO_OK,
                       ERR_FAIL);

        INFO("Done. Programming...");
        uintptr_t params[] = {SETTINGS_OFFSET, (uintptr_t)&global_settings};
        RETURN_IF_COND(flash_safe_execute(call_flash_range_program, params, UINT32_MAX) != PICO_OK, ERR_FAIL);
        INFO("Done! Verify...");
        RETURN_IF_COND(memcmp(flash_target_contents, &global_settings, sizeof(Settings)) != 0, ERR_FAIL);
        INFO("Done!");
    }
    INFO("Done!");

    return err;
}
