#define FILE_ID "WS01"

#include <stdlib.h>
#include <string.h>
#include "ws2812.pio.h"
#include "api/ws2812_pio_api.h"

#define WS2812_FREQ 800000

ErrCode ws2812_init(uint32_t pin, uint32_t led_count, Ws2812_Header *h) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(h == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(pin >= NUM_BANK0_GPIOS, ERR_PARAM_INVALID);

    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &h->pio.pio, &h->pio.sm,
                                                                    &h->pio.offset, pin, 1, true);
    RETURN_IF_COND(!success, ERR_PARAM_INVALID);

    h->pin = pin;
    h->led_count = led_count;

    ws2812_program_init(h->pio.pio, h->pio.sm, h->pio.offset, h->pin, WS2812_FREQ, false);
    h->mem_size = h->led_count * sizeof(uint32_t);
    h->mem = malloc(h->mem_size);
    TO_EXIT_IF_COND(h->mem == NULL, ERR_MEM_ALLOC_FAIL);
    memset(h->mem, 0x00, h->mem_size);

    ws2812_clear(h);

    return err;
EXIT:
    pio_remove_program_and_unclaim_sm(&ws2812_program, h->pio.pio, h->pio.sm, h->pio.offset);
    if (h->mem) {
        free(h->mem);
        h->mem = NULL;
    }
    memset(h, 0, sizeof(*h));
    return err;
}

ErrCode ws2812_deinit(Ws2812_Header *h) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(h == NULL, ERR_PARAM_IS_NULL);
    pio_remove_program_and_unclaim_sm(&ws2812_program, h->pio.pio, h->pio.sm, h->pio.offset);
    if (h->mem) {
        free(h->mem);
    }

    memset(h, 0, sizeof(*h));

    return err;
}

ErrCode ws2812_clear(Ws2812_Header *h) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(h == NULL, ERR_PARAM_IS_NULL);
    for (uint32_t i = 0; i < h->led_count; i++) {
        ws2812_set_color(h, i, 0, 0, 0);
    }
    ws2812_refresh(h);

    return err;
}

ErrCode ws2812_set_color(Ws2812_Header *h, uint32_t index, uint8_t r, uint8_t g, uint8_t b) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(h == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(h->mem == NULL, ERR_NEED_INIT);
    RETURN_IF_COND(index >= h->led_count, ERR_PARAM_INVALID);

    h->mem[index] = (g << 16) | (r << 8) | (b);

    return err;
}

ErrCode ws2812_refresh(Ws2812_Header *h) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(h == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(h->mem == NULL, ERR_NEED_INIT);

    for (uint32_t i = 0; i < h->led_count; i++) {
        pio_sm_put_blocking(h->pio.pio, h->pio.sm, h->mem[i] << 8);
    }
    sleep_us(100);

    return err;
}