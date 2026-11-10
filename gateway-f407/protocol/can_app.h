#ifndef CAN_APP_H
#define CAN_APP_H

#include "can_frame.h"
#include "can_protocol.h"
#include <stdint.h>

typedef enum {
    CAN_APP_OK = 0,
    CAN_APP_INVALID_ARG,
    CAN_APP_BAD_ID,
    CAN_APP_BAD_DLC,
    CAN_APP_BAD_FRAME
} can_app_result_t;

can_app_result_t Can_App_Fan_Code(uint16_t rpm, uint8_t seq, can_frame_t *frame);
can_app_result_t Can_App_LED_Code(uint8_t state, uint8_t seq, can_frame_t *frame);
can_app_result_t Can_App_States_Deco(const can_frame_t *frame, device_state_t *status);
can_app_result_t Can_App_Heart_Deco(const can_frame_t *frame, device_heart_t *Heart);
can_app_result_t Can_App_ACK_Deco(const can_frame_t *frame, device_ack_t *Ack);

#endif
