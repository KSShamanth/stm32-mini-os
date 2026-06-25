
#include <stdio.h>
#include "os_core.h"
#include "os_tick.h"
#include "cmsis_compiler.h"

static os_tcb_t  task_table[OS_MAX_TASKS]; //task table for storing all the task informations.

static uint8_t   task_count = 0;

static uint8_t   current_task_idx = 0;

static uint8_t   next_task_idx = 0;

void os_init(void)
{
    task_count       = 0;
    current_task_idx = 0;
    next_task_idx    = 0;

    for (uint8_t i = 0; i < OS_MAX_TASKS; i++)
    {
        task_table[i].id         = i;
        task_table[i].name       = "unused";
        task_table[i].state      = TASK_BLOCKED;
        task_table[i].wake_tick  = 0U;
        task_table[i].task_fn    = NULL;
        task_table[i].run_count  = 0U;
    }
}

os_tcb_t *os_task_create(const char     *name,
                          void          (*task_fn)(void),
                          os_task_state_t initial_state)
{
    if (task_count >= OS_MAX_TASKS || task_fn == NULL)
    {
        return NULL;
    }

    os_tcb_t *tcb = &task_table[task_count];  //to get free slot in the task table

    tcb->id         = task_count;
    tcb->name       = name;
    tcb->state      = initial_state;
    tcb->wake_tick  = 0U;
    tcb->task_fn    = task_fn;
    tcb->run_count  = 0U;

    task_count++;

    return tcb;
}

void os_start(void)  //round robin scheduler
{

    while (1)
    {
        uint8_t found    = 0U;
        uint8_t searched = 0U;

        while (searched < task_count)
        {
            uint8_t idx = (next_task_idx + searched) % task_count;

            if (task_table[idx].state == TASK_READY)
            {
            	current_task_idx = idx;
                found = 1U;

                next_task_idx = (idx + 1U) % task_count;

                task_table[idx].state = TASK_RUNNING;
                task_table[idx].run_count++;


                task_table[idx].task_fn();

                if (task_table[idx].state == TASK_RUNNING) //If task neither slept nor blocked, make it runnable again
                {
                    task_table[idx].state = TASK_READY;
                }

                break;
            }

            searched++;
        }

        if (!found)
        {

            __WFI();
        }
    }

}

void os_yield(void)
{
	return;
}

void os_sleep_ms(uint32_t ms)
{
    if (ms == 0U)
    {
        return;
    }

    os_tcb_t *tcb = &task_table[current_task_idx];

    tcb->wake_tick = os_now_ms() + ms;
    tcb->state     = TASK_SLEEPING;

    os_yield();
}

void os_scheduler_tick(void)
{
    uint32_t now = os_now_ms();

    for (uint8_t i = 0; i < task_count; i++)
    {
        if (task_table[i].state == TASK_SLEEPING && //if task is both sleeping and its wake tick has expired, make it ready.
            now >= task_table[i].wake_tick)
        {
            task_table[i].state = TASK_READY;

        }
    }
}

os_tcb_t *os_current_task(void)
{
    return &task_table[current_task_idx];
}

const os_tcb_t *os_get_task_table(void)
{
    return task_table;
}

uint8_t os_get_task_count(void)
{
    return task_count;
}

void os_reset_stats(void)
{
    for (uint8_t i = 0; i < task_count; i++)
        task_table[i].run_count = 0;
}
