
#ifndef INC_OS_CORE_H_
#define INC_OS_CORE_H_

#include <stdint.h>

#define OS_MAX_TASKS 8

typedef enum
{
    TASK_READY    = 0,
    TASK_RUNNING  = 1,
    TASK_SLEEPING = 2,
    TASK_BLOCKED  = 3
} os_task_state_t;

typedef struct
{
    uint8_t          id;
    const char      *name;
    os_task_state_t  state;
    uint32_t         wake_tick;
    void           (*task_fn)(void);
    uint32_t         run_count;
} os_tcb_t;

void os_init(void);

os_tcb_t *os_task_create(const char *name,
                          void (*task_fn)(void),
                          os_task_state_t initial_state);

void os_start(void);

void os_yield(void);

void os_sleep_ms(uint32_t ms);

void os_scheduler_tick(void);

os_tcb_t *os_current_task(void);

const os_tcb_t *os_get_task_table(void);

uint8_t os_get_task_count(void);

void os_reset_stats(void);

#endif /* INC_OS_CORE_H_ */
