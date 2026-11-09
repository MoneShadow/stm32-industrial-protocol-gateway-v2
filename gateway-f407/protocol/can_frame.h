/* 纯 can_frame_t */
#ifndef CAN_FRAME_H
#define CAN_FRAME_H

#include <stdint.h>

#define CAN_FRAME_DATA_MAX 8u

/* flags */
#define CAN_FRAME_FLAG_IDE 0x01u  /* 0=标准帧，1=扩展帧 */
#define CAN_FRAME_FLAG_RTR 0x02u  /* 0=数据帧，1=远程帧 */

typedef struct {
    uint32_t id;       /* 标准 11 位或扩展 29 位，右对齐 */
    uint8_t  dlc;      /* 0..8 */
    uint8_t  flags;    /* IDE/RTR */
    uint8_t  data[CAN_FRAME_DATA_MAX];
} can_frame_t;

#endif