#define FILE_ID "SW02"

#include "global.h"
#include "api/stairway_sensors.h"
#include "api/modbus_master_api.h"
#include "api/settings_api.h"
#include <string.h>

#define MAX_RANGE        4000
#define SIGNAL_THRESHOLD 15

#define SENSOR_PAYLOAD_LEN 12 /* 6 registers */

#define POLL_INTERVAL_MS 40

typedef enum { DEV_ID_MIN = 0, DEV_ID_UP = DEV_ID_MIN, DEV_ID_DOWN, DEV_ID_MAX } DevId;

// Raw values indexed by physical sensor (before swap)
static StairwaySensorsGet sensors_values = {0};
static bool dev_error[DEV_ID_MAX] = {false};
static Settings *settings = NULL;

static const StairwaySensorType dev_first_sensor[DEV_ID_MAX] = {
    [DEV_ID_UP] = STAIRWAY_SENS_UP_FIRST,
    [DEV_ID_DOWN] = STAIRWAY_SENS_DOWN_FIRST,
};

#if !EMULATE_SENSORS
static ErrCode sensor_parse(DevId dev, ModbusError status, uint8_t *raw_answer, uint8_t *payload, uint8_t payload_len) {
    ErrCode err = ERR_SUCCESS;
    if (status != MODBUS_ERR_SUCCESS) {
        dev_error[dev] = true;
        RETURN_IF_ERROR(ERR_FAIL);
    }

    switch (raw_answer[1]) {
        case 3:
        case 4: {
            RETURN_IF_COND(payload_len < SENSOR_PAYLOAD_LEN, ERR_FAIL);
            dev_error[dev] = false;
            uint32_t first = dev_first_sensor[dev];
            uint32_t second = first + 1;
            uint16_t error = (payload[10] << 8) | payload[11];
            sensors_values.dist[first] = (payload[2] << 8) | payload[3];
            sensors_values.dist[second] = (payload[4] << 8) | payload[5];
            sensors_values.signal[first] = (payload[6] << 8) | payload[7];
            sensors_values.signal[second] = (payload[8] << 8) | payload[9];
            sensors_values.error[first] = (error & 0x01) == 0x01;
            sensors_values.error[second] = (error & 0x02) == 0x02;
        } break;
        default: {
        } break;
    }

    return err;
}

static ErrCode sensor_up_cb(ModbusError status, uint8_t *raw_answer, uint16_t raw_len, uint8_t *payload,
                            uint8_t payload_len) {
    return sensor_parse(DEV_ID_UP, status, raw_answer, payload, payload_len);
}

static ErrCode sensor_down_cb(ModbusError status, uint8_t *raw_answer, uint16_t raw_len, uint8_t *payload,
                              uint8_t payload_len) {
    return sensor_parse(DEV_ID_DOWN, status, raw_answer, payload, payload_len);
}

static const ModbusMasterRequest request[DEV_ID_MAX] = {
    [DEV_ID_UP] =
        {
            .id = 2,
            .func = 3,
            .reg_addr = 0,
            .reg_length = 6,
            .timeout = 100,
            .cb = sensor_up_cb,
        },
    [DEV_ID_DOWN] =
        {
            .id = 1,
            .func = 3,
            .reg_addr = 0,
            .reg_length = 6,
            .timeout = 100,
            .cb = sensor_down_cb,
        },
};
#endif

ErrCode stairway_sensors_init(void) {
    ErrCode err = ERR_SUCCESS;

    settings = settings_get();
#if !EMULATE_SENSORS
    RETURN_IF_ERROR(modbus_master_init());
#endif

    return err;
}

ErrCode stairway_sensors_routine(void) {
    ErrCode err = ERR_SUCCESS;

#if !EMULATE_SENSORS
    static uint64_t request_ts = 0;
    static DevId next_dev = DEV_ID_MIN;

    modbus_master_routine();
    uint64_t ts = get_time_ms();
    if ((!modbus_master_get_busy()) && ((ts - request_ts) >= POLL_INTERVAL_MS)) {
        request_ts = ts;
        modbus_master_send(&request[next_dev]);
        next_dev = (next_dev + 1) % DEV_ID_MAX;
    }
#endif

    return err;
}

// Fills logical sensors (after swap) of one device
static void fill_device_values(StairwaySensorsGet *value, DevId dev, bool swap) {
    uint32_t first = dev_first_sensor[dev];
    bool error = dev_error[dev];
    for (uint32_t i = 0; i < 2; i++) {
        uint32_t dst = first + i;
        uint32_t src = swap ? (first + 1 - i) : dst;
        if (error) {
            value->error[dst] = true;
            value->state[dst] = false;
            value->dist[dst] = 0;
            value->signal[dst] = 0;
            continue;
        }
        value->error[dst] = sensors_values.error[src];
        value->signal[dst] = sensors_values.signal[src];
        // Weak signal means the measurement is unreliable: treat it as "nothing in range"
        value->dist[dst] = (sensors_values.signal[src] >= SIGNAL_THRESHOLD) ? sensors_values.dist[src] : MAX_RANGE;
        // Trigger distance is configured per logical sensor (after swap)
        value->state[dst] = value->dist[dst] < settings->dist_trigger[dst];
    }
}

ErrCode stairway_sensors_get(StairwaySensorsGet *value) {
    ErrCode err = ERR_SUCCESS;
    RETURN_IF_COND(value == NULL, ERR_PARAM_IS_NULL);

    fill_device_values(value, DEV_ID_UP, settings->sensor_up_swap);
    fill_device_values(value, DEV_ID_DOWN, settings->sensor_down_swap);

    return err;
}
