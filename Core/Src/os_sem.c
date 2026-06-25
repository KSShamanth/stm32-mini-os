#include "os_sem.h"
#include "os_core.h"
#include "cmsis_compiler.h"
#include <stdio.h>

void os_sem_init(os_sem_t *sem, uint8_t value)
{
    sem->count        = (value > 1U) ? 1U : value;
    sem->waiting_task = NULL;
}

void os_sem_wait(os_sem_t *sem) //Take semaphore
{

    __disable_irq(); //Protect semaphore update from interrupt access

    if (sem->count > 0U)
    {
        sem->count--;
        __enable_irq();
        return;
    }

    //no semaphore available
    os_tcb_t *me = os_current_task();
    sem->waiting_task = me;
    me->state = TASK_BLOCKED;

    __enable_irq();

    return;

}


void os_sem_signal(os_sem_t *sem)  //Give semaphore
{

    __disable_irq();

    if (sem->waiting_task != NULL) //Wake waiting task if one exists
    {
    	sem->count = 1;
        sem->waiting_task->state = TASK_READY;
        sem->waiting_task        = NULL;
    }
    else
    {
        if (sem->count < 1U)
        {
            sem->count = 1U;
        }
    }

    __enable_irq();

}
