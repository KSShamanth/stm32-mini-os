
#ifndef INC_OS_SEM_H_
#define INC_OS_SEM_H_

#include <stdint.h>
#include "os_core.h"

typedef struct
{
    volatile uint8_t  count; //state of semaphore (0 or 1)
    os_tcb_t         *waiting_task;
} os_sem_t;

void os_sem_init(os_sem_t *sem, uint8_t value);

void os_sem_wait(os_sem_t *sem);


void os_sem_signal(os_sem_t *sem);

#endif /* INC_OS_SEM_H_ */
