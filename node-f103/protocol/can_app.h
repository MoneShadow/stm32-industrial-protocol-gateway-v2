#ifndef CAN_APP_H
#define CAN_APP_H

#include "can_frame.h"
#include "device.h"

#include <stdint.h>

typedef struct {
    uint8_t commandcode;
    uint8_t cmd_result;
    uint8_t seq;
} can_protocol_ack_t;

typedef struct {
    uint8_t commandcode;
    uint8_t seq;
    union {
        uint16_t fan_rpm;
        uint8_t led_state;
    };
} can_protocol_msg_t;

int8_t Can_App_Deco(can_frame_t* frame, can_protocol_msg_t *deco_msg);
int8_t Can_App_Ack_Code(const can_protocol_msg_t *deco_msg, can_protocol_ack_t *ack, uint8_t result);
int8_t Can_App_Ack_Frame_Code(const can_protocol_msg_t *deco_msg, uint8_t result, can_frame_t *frame);
int8_t Can_App_Heart_Code(uint8_t node_id, uint8_t seq, can_frame_t *frame);
int8_t Can_App_State_Code(const device_model_t *device_model, can_frame_t *frame);

#endif
