#pragma once

#include "global.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

typedef struct {
    struct {
        PIO pio;
        uint sm;
        uint offset;
    } pio;
    uint32_t pin;
    uint32_t led_count;
    uint32_t *mem;
    uint32_t mem_size;
} Ws2812_Header;

ErrCode ws2812_init(uint32_t pin, uint32_t led_count, Ws2812_Header *h);
ErrCode ws2812_deinit(Ws2812_Header *h);
ErrCode ws2812_clear(Ws2812_Header *h);
ErrCode ws2812_set_color(Ws2812_Header *h, uint32_t index, uint8_t r, uint8_t g, uint8_t b);
ErrCode ws2812_refresh(Ws2812_Header *h);
