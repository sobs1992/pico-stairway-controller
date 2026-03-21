#include "global.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "api/ws2812_pio_api.h"

#define WS281x_PIN 2
#define WS281x_N   18

int main() {
    uint8_t cnt = 0;
    stdio_init_all();
    if (cyw43_arch_init()) {
        return -1;
    }

    Ws2812_Header led_header = {0};
    ws2812_init(WS281x_PIN, WS281x_N, &led_header);
    ws2812_clear(&led_header);

    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, cnt & 1);
        ws2812_set_color(&led_header, cnt % led_header.led_count, 255 - cnt, cnt / 2, cnt);
        ws2812_refresh(&led_header);
        printf("Hello, world!\n");
        cnt++;
        sleep_ms(100);
    }
    return 0;
}