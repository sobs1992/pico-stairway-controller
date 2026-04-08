#define FILE_ID "MB01"

#include "api/modbus_master_api.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include <string.h>

#define PRINT_COMMUNICATION 0

#define LED_PIN 12

#define SEND_BUF_SIZE    32
#define RECEIVE_BUF_SIZE 256

static uint8_t send_buf[SEND_BUF_SIZE] = {0};
static uint32_t send_counter = 0;
static uint8_t receive_buf[RECEIVE_BUF_SIZE] = {0};
static uint32_t receive_counter = 0;
static uint32_t answer_len = 0;
static uint32_t send_ts = 0;
static bool busy = false;
static ModbusMasterRequest last_request = {0};

static ErrCode rx_timer_control(bool state);

static uint16_t crc_chk(uint8_t *dat, uint8_t length) {
    uint16_t j;
    uint16_t reg_crc = 0xFFFF;
    while (length--) {
        reg_crc ^= *dat++;
        for (j = 0; j < 8; j++) {
            if (reg_crc & 0x01)
                reg_crc = (reg_crc >> 1) ^ 0xA001; // LSB(b0)=1
            else
                reg_crc = reg_crc >> 1;
        }
    }
    return reg_crc;
}

static ErrCode decode_modbus(uint8_t *buf, uint16_t size) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(buf == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(size == 0, ERR_PARAM_INVALID);
    RETURN_IF_COND(size < answer_len, ERR_PARAM_INVALID);
#if PRINT_COMMUNICATION
    printf("Receive: ");
    for (uint32_t i = 0; i < size; i++) {
        printf("%02X ", buf[i]);
    }
    printf("\n");
#endif
    if ((buf[0] != last_request.id) || (buf[1] != last_request.func)) {
        if (last_request.cb) {
            last_request.cb(MODBUS_ERR_UNKNOWN, NULL, 0, NULL, 0);
        }
        return ERR_FAIL;
    }
    uint16_t crc = crc_chk(buf, answer_len - 2);
    if (crc != (buf[answer_len - 2] | (buf[answer_len - 1] << 8))) {
        if (last_request.cb) {
            last_request.cb(MODBUS_ERR_CRC, NULL, 0, NULL, 0);
        }
        return ERR_FAIL;
    }

    switch (buf[1]) {
        case 3:
        case 4: {
            if (last_request.cb) {
                last_request.cb(MODBUS_ERR_SUCCESS, buf, answer_len, &buf[3], buf[2]);
            }
        } break;
        case 6:
        case 16: {
            if (last_request.cb) {
                last_request.cb(MODBUS_ERR_SUCCESS, buf, answer_len, NULL, 0);
            }
        } break;
        default: {
        } break;
    }

    return err;
}

static void modbus_master_uart_rx_irq(void) {
    ErrCode err = ERR_SUCCESS;
    while (uart_is_readable(MODBUS_MASTER_UART)) {
        if (receive_counter < RECEIVE_BUF_SIZE - 1) {
            receive_buf[receive_counter++] = uart_getc(MODBUS_MASTER_UART);
        }
    }
}

ErrCode modbus_master_init(void) {
    ErrCode err = ERR_SUCCESS;

    uart_init(MODBUS_MASTER_UART, MODBUS_MASTER_BAUD);
    uart_set_format(MODBUS_MASTER_UART, 8, MODBUS_MASTER_STOP_BITS, UART_PARITY_NONE);
    gpio_set_function(MODBUS_MASTER_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(MODBUS_MASTER_RX_PIN, GPIO_FUNC_UART);
    gpio_pull_up(MODBUS_MASTER_RX_PIN);
    gpio_init(MODBUS_MASTER_DIR_PIN);
    gpio_set_dir(MODBUS_MASTER_DIR_PIN, true);
    gpio_put(MODBUS_MASTER_DIR_PIN, 0);
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, true);

    uart_set_fifo_enabled(MODBUS_MASTER_UART, false);

    irq_set_exclusive_handler(MODBUS_MASTER_UART_IRQ, modbus_master_uart_rx_irq);
    irq_set_enabled(MODBUS_MASTER_UART_IRQ, true);

    uart_set_irqs_enabled(MODBUS_MASTER_UART, true, false);

    return err;
}

ErrCode modbus_master_send(const ModbusMasterRequest *request) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(request == NULL, ERR_PARAM_IS_NULL);
    RETURN_IF_COND(busy, ERR_BUSY);

    send_buf[0] = request->id;
    switch (request->func) {
        case 3:
        case 4: {
            send_buf[1] = request->func;
            send_buf[2] = request->reg_addr >> 8;
            send_buf[3] = request->reg_addr;
            send_buf[4] = request->reg_length >> 8;
            send_buf[5] = request->reg_length;
            answer_len = 5 + request->reg_length * 2;
            send_counter = 8;
        } break;
        case 6: {
            send_buf[1] = request->func;
            send_buf[2] = request->reg_addr >> 8;
            send_buf[3] = request->reg_addr;
            send_buf[4] = request->data[0];
            send_buf[5] = request->data[1];
            answer_len = 8;
            send_counter = 8;
        } break;
        case 16: {
            send_buf[1] = request->func;
            send_buf[2] = request->reg_addr >> 8;
            send_buf[3] = request->reg_addr;
            send_buf[4] = request->reg_length >> 8;
            send_buf[5] = request->reg_length;
            answer_len = 8;
            send_buf[6] = request->reg_length * 2;
            for (uint8_t i = 0; i < send_buf[6]; i++) {
                send_buf[7 + i] = request->data[i];
            }
            send_counter = 9 + send_buf[6];
        } break;
        default: {
            RETURN_IF_ERROR(ERR_PARAM_INVALID);
        } break;
    };

    busy = true;

    uint16_t crc = crc_chk(send_buf, send_counter - 2);
    send_buf[send_counter - 2] = crc;
    send_buf[send_counter - 1] = crc >> 8;
#if PRINT_COMMUNICATION
    printf("Send: ");
    for (uint32_t i = 0; i < send_counter; i++) {
        printf("%02X ", send_buf[i]);
    }
    printf("\n");
#endif

    gpio_put(LED_PIN, 1);

    receive_counter = 0;
    memset(receive_buf, 0, sizeof(receive_buf));

    gpio_put(MODBUS_MASTER_DIR_PIN, 1);
    uart_write_blocking(MODBUS_MASTER_UART, send_buf, send_counter);
    while (uart_get_hw(MODBUS_MASTER_UART)->fr & UART_UARTFR_BUSY_BITS) {
    };
    gpio_put(MODBUS_MASTER_DIR_PIN, 0);

    last_request = *request;
    send_ts = get_time_ms();

    return err;
}

bool modbus_master_get_busy(void) {
    return busy;
}

ErrCode modbus_master_routine(void) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND_(!busy, err);
    uint32_t dt = get_time_ms() - send_ts;
    if ((receive_counter >= answer_len) || (receive_counter >= SEND_BUF_SIZE)) {
        //    printf("DT: %d\n", dt);
        decode_modbus(receive_buf, receive_counter);
        busy = false;
        gpio_put(LED_PIN, 0);
    } else if (dt > last_request.timeout) {
        //    printf("TIMEOUT (%d > %d)\n", dt, last_request.timeout);
        if (last_request.cb) {
            last_request.cb(MODBUS_ERR_TIMEOUT, NULL, 0, NULL, 0);
        }
        busy = false;
        gpio_put(LED_PIN, 0);
    }
    return err;
}