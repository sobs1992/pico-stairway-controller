#define FILE_ID "SW02"

#include "global.h"
#include "api/stairway_sensors.h"
#include "api/modbus_master_api.h"
#include <string.h>

#define DEV_0_SWAP 0
#define DEV_1_SWAP 1

typedef enum { DEV_ID_MIN = 0, DEV_ID_0 = DEV_ID_MIN, DEV_ID_1, DEV_ID_MAX } DevId;

typedef enum { SM_MIN = 0, SM_DEV_ID_0_REQ = SM_MIN, SM_DEV_ID_0_WAIT, SM_DEV_ID_1_REQ, SM_DEV_ID_1_WAIT, SM_MAX } SM;

static ErrCode sensor_0_cb(ModbusError status, uint8_t *raw_answer, uint16_t raw_len, uint8_t *payload,
                           uint8_t payload_len);
static ErrCode sensor_1_cb(ModbusError status, uint8_t *raw_answer, uint16_t raw_len, uint8_t *payload,
                           uint8_t payload_len);

static StairwaySensorsSettings sensors_settings = {0};
static StairwaySensorsGet sensors_values = {0};
static bool dev_error[DEV_ID_MAX] = {false};

static const ModbusMasterRequest request[DEV_ID_MAX] = {
    [DEV_ID_0] =
        {
            .id = 1,
            .func = 3,
            .reg_addr = 0,
            .reg_length = 6,
            .timeout = 100,
            .cb = sensor_0_cb,
        },
    [DEV_ID_1] =
        {
            .id = 2,
            .func = 3,
            .reg_addr = 0,
            .reg_length = 6,
            .timeout = 100,
            .cb = sensor_1_cb,
        },
};

static ErrCode sensor_0_cb(ModbusError status, uint8_t *raw_answer, uint16_t raw_len, uint8_t *payload,
                           uint8_t payload_len) {
    ErrCode err = ERR_SUCCESS;
    if (status != MODBUS_ERR_SUCCESS) {
        dev_error[DEV_ID_0] = true;
        RETURN_IF_ERROR(ERR_FAIL);
    }

    switch (raw_answer[1]) {
        case 3:
        case 4: {
            RETURN_IF_COND(payload_len == 0, ERR_FAIL);
            uint16_t error = (payload[10] << 8) | payload[11];
            sensors_values.dist[STAIRWAY_SENS_UP_FIRST] = (payload[2] << 8) | payload[3];
            sensors_values.dist[STAIRWAY_SENS_UP_SECOND] = (payload[4] << 8) | payload[5];
            sensors_values.error[STAIRWAY_SENS_UP_FIRST] = (error & 0x01) == 0x01;
            sensors_values.error[STAIRWAY_SENS_UP_SECOND] = (error & 0x02) == 0x02;
            sensors_values.state[STAIRWAY_SENS_UP_FIRST] =
                sensors_values.dist[STAIRWAY_SENS_UP_FIRST] < sensors_settings.trigger_value[STAIRWAY_SENS_UP_FIRST];
            sensors_values.state[STAIRWAY_SENS_UP_SECOND] =
                sensors_values.dist[STAIRWAY_SENS_UP_SECOND] < sensors_settings.trigger_value[STAIRWAY_SENS_UP_SECOND];
            dev_error[DEV_ID_0] = false;
        } break;
        default: {
        } break;
    }

    return err;
}

static ErrCode sensor_1_cb(ModbusError status, uint8_t *raw_answer, uint16_t raw_len, uint8_t *payload,
                           uint8_t payload_len) {
    ErrCode err = ERR_SUCCESS;
    if (status != MODBUS_ERR_SUCCESS) {
        dev_error[DEV_ID_1] = true;
        RETURN_IF_ERROR(ERR_FAIL);
    }

    switch (raw_answer[1]) {
        case 3:
        case 4: {
            RETURN_IF_COND(payload_len == 0, ERR_FAIL);
            uint16_t error = (payload[10] << 8) | payload[11];
            sensors_values.dist[STAIRWAY_SENS_DOWN_FIRST] = (payload[2] << 8) | payload[3];
            sensors_values.dist[STAIRWAY_SENS_DOWN_SECOND] = (payload[4] << 8) | payload[5];
            sensors_values.error[STAIRWAY_SENS_DOWN_FIRST] = (error & 0x01) == 0x01;
            sensors_values.error[STAIRWAY_SENS_DOWN_SECOND] = (error & 0x02) == 0x02;
            sensors_values.state[STAIRWAY_SENS_DOWN_FIRST] =
                sensors_values.dist[STAIRWAY_SENS_DOWN_FIRST] < sensors_settings.trigger_value[STAIRWAY_SENS_UP_FIRST];
            sensors_values.state[STAIRWAY_SENS_DOWN_SECOND] = sensors_values.dist[STAIRWAY_SENS_DOWN_SECOND] <
                                                              sensors_settings.trigger_value[STAIRWAY_SENS_UP_SECOND];
            dev_error[DEV_ID_1] = false;
        } break;
        default: {
        } break;
    }

    return err;
}

ErrCode stairway_sensors_init(StairwaySensorsSettings *settings) {
    ErrCode err = ERR_SUCCESS;

    RETURN_IF_COND(settings == NULL, ERR_PARAM_IS_NULL);

    sensors_settings = *settings;
    modbus_master_init();

    return err;
}

ErrCode stairway_sensors_routine(void) {
    ErrCode err = ERR_SUCCESS;
    static uint32_t dt = 0;
    static SM sm = SM_DEV_ID_0_REQ;

    modbus_master_routine();
    if (get_time_ms() - dt > 20) {
        switch (sm) {
            case SM_DEV_ID_0_REQ: {
                modbus_master_send(&request[DEV_ID_0]);
                sm = SM_DEV_ID_0_WAIT;
            } break;
            case SM_DEV_ID_0_WAIT: {
                if (!modbus_master_get_busy()) {
                    sm = SM_DEV_ID_1_REQ;
                }
            } break;
            case SM_DEV_ID_1_REQ: {
                modbus_master_send(&request[DEV_ID_1]);
                sm = SM_DEV_ID_1_WAIT;
            } break;
            case SM_DEV_ID_1_WAIT: {
                if (!modbus_master_get_busy()) {
                    sm = SM_DEV_ID_0_REQ;
                }
            } break;
            default: {
                sm = SM_DEV_ID_0_REQ;
            } break;
        }

        dt = get_time_ms();
    }

    return err;
}

ErrCode stairway_sensors_get(StairwaySensorsGet *value) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(value == NULL, ERR_PARAM_IS_NULL);

    if (dev_error[DEV_ID_0]) {
        value->error[STAIRWAY_SENS_UP_FIRST] = true;
        value->error[STAIRWAY_SENS_UP_SECOND] = true;
        value->state[STAIRWAY_SENS_UP_FIRST] = false;
        value->state[STAIRWAY_SENS_UP_SECOND] = false;
        value->dist[STAIRWAY_SENS_UP_FIRST] = 0;
        value->dist[STAIRWAY_SENS_UP_SECOND] = 0;
    } else {
#if !DEV_0_SWAP
        value->error[STAIRWAY_SENS_UP_FIRST] = sensors_values.error[STAIRWAY_SENS_UP_FIRST];
        value->error[STAIRWAY_SENS_UP_SECOND] = sensors_values.error[STAIRWAY_SENS_UP_SECOND];
        value->state[STAIRWAY_SENS_UP_FIRST] = sensors_values.state[STAIRWAY_SENS_UP_FIRST];
        value->state[STAIRWAY_SENS_UP_SECOND] = sensors_values.state[STAIRWAY_SENS_UP_SECOND];
        value->dist[STAIRWAY_SENS_UP_FIRST] = sensors_values.dist[STAIRWAY_SENS_UP_FIRST];
        value->dist[STAIRWAY_SENS_UP_SECOND] = sensors_values.dist[STAIRWAY_SENS_UP_SECOND];
#else
        value->error[STAIRWAY_SENS_UP_FIRST] = sensors_values.error[STAIRWAY_SENS_UP_SECOND];
        value->error[STAIRWAY_SENS_UP_SECOND] = sensors_values.error[STAIRWAY_SENS_UP_FIRST];
        value->state[STAIRWAY_SENS_UP_FIRST] = sensors_values.state[STAIRWAY_SENS_UP_SECOND];
        value->state[STAIRWAY_SENS_UP_SECOND] = sensors_values.state[STAIRWAY_SENS_UP_FIRST];
        value->dist[STAIRWAY_SENS_UP_FIRST] = sensors_values.dist[STAIRWAY_SENS_UP_SECOND];
        value->dist[STAIRWAY_SENS_UP_SECOND] = sensors_values.dist[STAIRWAY_SENS_UP_FIRST];
#endif
    }

    if (dev_error[DEV_ID_1]) {
        value->error[STAIRWAY_SENS_DOWN_FIRST] = true;
        value->error[STAIRWAY_SENS_DOWN_SECOND] = true;
        value->state[STAIRWAY_SENS_DOWN_FIRST] = false;
        value->state[STAIRWAY_SENS_DOWN_SECOND] = false;
        value->dist[STAIRWAY_SENS_DOWN_FIRST] = 0;
        value->dist[STAIRWAY_SENS_DOWN_SECOND] = 0;
    } else {
#if !DEV_1_SWAP
        value->error[STAIRWAY_SENS_DOWN_FIRST] = sensors_values.error[STAIRWAY_SENS_DOWN_FIRST];
        value->error[STAIRWAY_SENS_DOWN_SECOND] = sensors_values.error[STAIRWAY_SENS_DOWN_SECOND];
        value->state[STAIRWAY_SENS_DOWN_FIRST] = sensors_values.state[STAIRWAY_SENS_DOWN_FIRST];
        value->state[STAIRWAY_SENS_DOWN_SECOND] = sensors_values.state[STAIRWAY_SENS_DOWN_SECOND];
        value->dist[STAIRWAY_SENS_DOWN_FIRST] = sensors_values.dist[STAIRWAY_SENS_DOWN_FIRST];
        value->dist[STAIRWAY_SENS_DOWN_SECOND] = sensors_values.dist[STAIRWAY_SENS_DOWN_SECOND];
#else
        value->error[STAIRWAY_SENS_DOWN_FIRST] = sensors_values.error[STAIRWAY_SENS_DOWN_SECOND];
        value->error[STAIRWAY_SENS_DOWN_SECOND] = sensors_values.error[STAIRWAY_SENS_DOWN_FIRST];
        value->state[STAIRWAY_SENS_DOWN_FIRST] = sensors_values.state[STAIRWAY_SENS_DOWN_SECOND];
        value->state[STAIRWAY_SENS_DOWN_SECOND] = sensors_values.state[STAIRWAY_SENS_DOWN_FIRST];
        value->dist[STAIRWAY_SENS_DOWN_FIRST] = sensors_values.dist[STAIRWAY_SENS_DOWN_SECOND];
        value->dist[STAIRWAY_SENS_DOWN_SECOND] = sensors_values.dist[STAIRWAY_SENS_DOWN_FIRST];
#endif
    }
    return err;
}
