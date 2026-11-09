#ifndef CAN_PORT_H
#define CAN_PORT_H

#include <stdint.h>
#include "can_frame.h"

typedef enum {
    CAN_PORT_OK = 0,
    CAN_PORT_BUSY,
    CAN_PORT_INVALID_ARG,
    CAN_PORT_TIMEOUT,
    CAN_PORT_ERROR
} can_port_result_t;

can_port_result_t Can_Port_Init(void);
can_port_result_t Can_Port_send(const can_frame_t *frame);

#endif