
#include <stdint.h>
#include <string.h>
#include "os_mq.h"
#include "os_core.h"
#include "cmsis_compiler.h"

void os_mq_init(os_mq_t *mq){
	mq->head = 0;
	mq->tail = 0;
	mq->count = 0;
	mq->waiting_task= NULL;
}

int8_t os_mq_send(os_mq_t *mq, const void *msg){ //Producer pushes message into queue.
	__disable_irq();

	if (mq->count == DEPTH){
		__enable_irq();
		return OS_MQ_FULL;
	}

	memcpy(&mq->buffer[mq->head * sizeof(os_msg_t)], msg, sizeof(os_msg_t)); //copy message from msg to the buffer of size=sizeof(os_msg_t)
	mq->head = (mq->head + 1) % DEPTH; //ring buffer behavior
	mq->count++;

	if (mq->waiting_task != NULL){ //Wake waiting receiver
		mq->waiting_task->state = TASK_READY;
		mq->waiting_task = NULL;
	}

	__enable_irq();

	return OS_MQ_OK;
}

void os_mq_recv(os_mq_t *mq, void *msg_out){  //Consumer receives message
	__disable_irq();

	while (mq->count == 0){ //no message available
	    os_tcb_t *me = os_current_task();
	    mq->waiting_task = me;
	    me->state = TASK_BLOCKED;
	    __enable_irq();
	    return;
	}

	memcpy(msg_out, &mq->buffer[mq->tail * sizeof(os_msg_t)], sizeof(os_msg_t)); //copy message from buffer to msg_out
	mq->tail = (mq->tail + 1) % DEPTH;
	mq->count--;

	__enable_irq();
}
