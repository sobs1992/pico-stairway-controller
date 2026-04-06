#pragma once

#include "global.h"

#define MODBUS_MASTER_UART      uart1
#define MODBUS_MASTER_UART_IRQ  UART1_IRQ
#define MODBUS_MASTER_BAUD      9600
#define MODBUS_MASTER_STOP_BITS 1
#define MODBUS_MASTER_TX_PIN    8
#define MODBUS_MASTER_RX_PIN    9
#define MODBUS_MASTER_DIR_PIN   4

typedef enum { MODBUS_ERR_SUCCESS = 0, MODBUS_ERR_TIMEOUT, MODBUS_ERR_CRC, MODBUS_ERR_UNKNOWN } ModbusError;

typedef struct {
    uint8_t id;
    uint8_t func;
    uint16_t reg_addr;
    uint16_t reg_length;
    uint8_t *data;
    uint32_t timeout;
    ErrCode (*cb)(ModbusError status, uint8_t *raw_answer, uint16_t raw_len, uint8_t *payload, uint8_t payload_len);
} ModbusMasterRequest;

ErrCode modbus_master_init(void);
ErrCode modbus_master_send(const ModbusMasterRequest *request);
bool modbus_master_get_busy(void);
ErrCode modbus_master_routine(void);