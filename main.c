#include "global.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "api/ws2812_pio_api.h"
#include "api/stairway_leds.h"

static inline uint32_t get_time_us(void) {
    return to_us_since_boot(get_absolute_time());
}
static inline uint32_t get_time_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}

#define WS281x_PIN 3
#define WS281x_N   18

int main() {
    uint32_t dt = 0;
    uint32_t rdt = 0;
    uint8_t cnt = 0;
    bool dir = false;
    stdio_init_all();
    if (cyw43_arch_init()) {
        return -1;
    }

    stairway_leds_init(WS281x_PIN, WS281x_N);

    while (1) {
        if (get_time_ms() - dt > 200) {
            dt = get_time_ms();
            if (cnt < WS281x_N) {
                stairway_leds_set_state(cnt, !dir);
                cnt++;
            } else {
                dir = !dir;
                cnt = 0;
            }
        }

        stairway_leds_refresh();
        sleep_ms(20);
    }
    return 0;
}