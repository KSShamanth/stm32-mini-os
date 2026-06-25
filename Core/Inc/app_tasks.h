
#ifndef INC_APP_TASKS_H_
#define INC_APP_TASKS_H_

#include "os_sem.h"
#include "os_mq.h"

#define MAX_CMD_LEN  32

extern os_sem_t sensor_sem;
extern os_sem_t uart_mutex;
extern os_mq_t  uart_queue;

void App_TasksInit(void);

void LED_Task(void);

void UART_Task(void);

void Sensor_Task(void);

#endif /* INC_APP_TASKS_H_ */
