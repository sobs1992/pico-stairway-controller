#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>

int main() {
    uint8_t cnt = 0;
    stdio_init_all();
    if (cyw43_arch_init()) {
        return -1;
    }

    while (true) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, cnt & 1);
        printf("Hello, world!\n");
        sleep_ms(100);
        cnt++;
    }
    return 0;
}