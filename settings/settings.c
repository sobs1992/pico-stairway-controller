#define FILE_ID "ST01"

#include "api/settings_api.h"
#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include "settings_list.h"

static_assert(sizeof(Settings) <= FLASH_PAGE_SIZE, "Settings must fit into one flash page");

static Settings global_settings = {0};

static const uint8_t *flash_target_contents = (const uint8_t *)(XIP_BASE + SETTINGS_OFFSET);

static uint32_t settings_crc(const Settings *s) {
    const uint8_t *data = (const uint8_t *)s;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < offsetof(Settings, crc); i++) {
        crc ^= data[i];
        for (uint32_t j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
        }
    }
    return ~crc;
}

static bool settings_is_valid_header(const Settings *s) {
    return (s->magic == SETTINGS_MAGIC) && (s->version == SETTINGS_VERSION) && (s->size == sizeof(Settings)) &&
           (s->crc == settings_crc(s));
}

ErrCode settings_init(void) {
    ErrCode err = ERR_SUCCESS;

    memcpy(&global_settings, flash_target_contents, sizeof(Settings));
    if ((!settings_is_valid_header(&global_settings)) || (settings_validate(&global_settings) != ERR_SUCCESS)) {
        INFO("Settings in flash are invalid, restoring defaults");
        memcpy(&global_settings, &default_settings, sizeof(Settings));
        RETURN_IF_ERROR(settings_write());
    }

    return err;
}

ErrCode settings_default(bool restart) {
    ErrCode err = ERR_SUCCESS;
    memcpy(&global_settings, &default_settings, sizeof(Settings));
    RETURN_IF_ERROR(settings_write());
    if (restart) {
        printf("Device will reboot!\n");
        reboot_request(1000);
    }
    return err;
}

Settings *settings_get(void) {
    return &global_settings;
}

ErrCode settings_validate(const Settings *s) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(s == NULL, ERR_PARAM_IS_NULL);

    RETURN_IF_COND((s->led_count == 0) || (s->led_count > LED_COUNT_MAX), ERR_PARAM_INVALID);
    for (uint32_t i = 0; i < EMERGENCY_MAX; i++) {
        RETURN_IF_COND(s->emergency_cnt[i] > s->led_count, ERR_PARAM_INVALID);
    }
    RETURN_IF_COND((s->led_on_step == 0) || (s->led_off_step == 0), ERR_PARAM_INVALID);
    RETURN_IF_COND(s->light_sensor_day_value < s->light_sensor_night_value, ERR_PARAM_INVALID);

    size_t ssid_len = strnlen(s->ssid, sizeof(s->ssid));
    size_t pass_len = strnlen(s->pass, sizeof(s->pass));
    RETURN_IF_COND((ssid_len == 0) || (ssid_len >= sizeof(s->ssid)), ERR_PARAM_INVALID);
    RETURN_IF_COND((pass_len < PASS_MIN_LEN) || (pass_len >= sizeof(s->pass)), ERR_PARAM_INVALID);

    return err;
}

ErrCode settings_apply(const Settings *s) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(s == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_ERROR(settings_validate(s));

    if (s != &global_settings) {
        memcpy(&global_settings, s, sizeof(Settings));
    }
    RETURN_IF_ERROR(settings_write());

    return err;
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
    static uint8_t page[FLASH_PAGE_SIZE];

    global_settings.magic = SETTINGS_MAGIC;
    global_settings.version = SETTINGS_VERSION;
    global_settings.size = sizeof(Settings);
    global_settings.crc = settings_crc(&global_settings);

    if (memcmp(flash_target_contents, &global_settings, sizeof(Settings)) != 0) {
        INFO("Erasing target region...");
        RETURN_IF_COND(flash_safe_execute(call_flash_range_erase, (void *)SETTINGS_OFFSET, UINT32_MAX) != PICO_OK,
                       ERR_FAIL);

        INFO("Done. Programming...");
        memset(page, 0xFF, sizeof(page));
        memcpy(page, &global_settings, sizeof(Settings));
        uintptr_t params[] = {SETTINGS_OFFSET, (uintptr_t)page};
        RETURN_IF_COND(flash_safe_execute(call_flash_range_program, params, UINT32_MAX) != PICO_OK, ERR_FAIL);
        INFO("Done! Verify...");
        RETURN_IF_COND(memcmp(flash_target_contents, &global_settings, sizeof(Settings)) != 0, ERR_FAIL);
    }
    INFO("Settings saved");

    return err;
}
