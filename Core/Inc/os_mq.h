
#ifndef INC_OS_MQ_H_
#define INC_OS_MQ_H_

#include <stdint.h>
#include "os_core.h"

//to identify message contents
#define MSG_TYPE_SENSOR_EVENT  0x01
#define MSG_TYPE_UART_COMMAND  0x02

#define DEPTH 8
#define MSG_SIZE sizeof(os_msg_t)

#define OS_MQ_OK    ( 0)
#define OS_MQ_FULL  (-1)

typedef struct
{
    uint8_t  type;
    char     payload[32];
} os_msg_t;

typedef struct {

	uint8_t buffer[DEPTH * MSG_SIZE];
	volatile uint8_t   head; //write position
	volatile uint8_t   tail; //read position
	volatile uint8_t  count;
	os_tcb_t  *waiting_task;
} os_mq_t;

void os_mq_init(os_mq_t *mq);

int8_t os_mq_send(os_mq_t *mq, const void *msg);

void os_mq_recv(os_mq_t *mq, void *msg_out);

#endif /* INC_OS_MQ_H_ */
