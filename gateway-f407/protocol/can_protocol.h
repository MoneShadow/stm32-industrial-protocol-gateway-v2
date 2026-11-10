#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>

typedef struct {
    uint16_t temperature;
    uint16_t light;
    uint16_t fan_rpm;
    uint16_t led_state;
} device_state_t;

typedef struct {
    uint8_t node_id;
    uint8_t heart_seq;
} device_heart_t;

typedef struct {
    uint8_t commandcode;
    uint8_t result;
    uint8_t seq;
} device_ack_t;

#endif
