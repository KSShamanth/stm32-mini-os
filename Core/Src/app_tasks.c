#include "main.h"
#include "app_tasks.h"
#include "os_sem.h"
#include "os_mq.h"
#include "os_tick.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

//shared os resources
os_sem_t sensor_sem;
os_sem_t uart_mutex;
os_mq_t  uart_queue;

extern UART_HandleTypeDef huart2; //Allows tasks to access UART peripheral

static char last_sensor_msg[32] = "No event yet";

static void convertToUpper(char *str);
static const char* state_str(os_task_state_t s);

void App_TasksInit(void){
	os_sem_init(&sensor_sem, 0); //Sensor task starts waiting.
	os_sem_init(&uart_mutex, 1); //Mutex available immediately.
	os_mq_init(&uart_queue);

	os_task_create("LED",    LED_Task,    TASK_READY);
	os_task_create("UART",   UART_Task,   TASK_READY);
	os_task_create("Sensor", Sensor_Task, TASK_READY);
}

void LED_Task(void){
		char txBuffer[64];

		os_sem_wait(&uart_mutex);

		sprintf(txBuffer, "[HEARTBEAT] tick=%lu ms\r\n", os_now_ms());

		HAL_UART_Transmit(&huart2,
				(uint8_t*)txBuffer,
				strlen(txBuffer),
				HAL_MAX_DELAY);

		os_sem_signal(&uart_mutex);

		HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port,LED_GREEN_Pin);

		os_sleep_ms(498);

}

void UART_Task(void)
{
	char txBuffer[256];
    os_msg_t msg;

    os_mq_recv(&uart_queue, &msg); //Block until a message is available


    if(os_current_task()->state == TASK_BLOCKED)
    {
        return;
    }

    if (msg.type == MSG_TYPE_UART_COMMAND)
    {
        convertToUpper(msg.payload);
    }

    os_sem_wait(&uart_mutex);

    switch(msg.type)
    {
        case MSG_TYPE_SENSOR_EVENT:

            sprintf(txBuffer,
                        "\r\n[SENSOR EVENT] %s\r\n"
            			"\r\n",
                        msg.payload);

            HAL_UART_Transmit(&huart2,
                                  (uint8_t*)txBuffer,
                                  strlen(txBuffer),
                                  HAL_MAX_DELAY);

            strncpy(last_sensor_msg, msg.payload, sizeof(last_sensor_msg) - 1);

            break;

        case MSG_TYPE_UART_COMMAND:

            if(strncmp(msg.payload, "HELP",4) == 0)
            {
                 sprintf(txBuffer,"\r\n"
                		 "-----HELP-----\r\n"
                        "\r\nCommands:\r\n"
                        "HELP\r\n"
                        "STATUS\r\n"
                        "TASKS\r\n"
                		"LATEST\r\n"
                        "RESET\r\n"
                		 "\r\n");
            }

            else if (strncmp(msg.payload, "LATEST",6) == 0){
            	sprintf(txBuffer,"\r\n"
               		 "-----LATEST-----\r\n"
            			"\r\nLast Event: %s\r\n"
            			"\r\n",last_sensor_msg);
            }

            else if(strncmp(msg.payload, "STATUS", 6) == 0)
            {
                uint8_t ready    = 0;
                uint8_t running  = 0;
                uint8_t blocked  = 0;
                uint8_t sleeping = 0;

                const os_tcb_t *tasks = os_get_task_table();

                for(uint8_t i = 0; i < os_get_task_count(); i++)
                {
                    switch(tasks[i].state)
                    {
                        case TASK_READY:
                            ready++;
                            break;

                        case TASK_RUNNING:
                            running++;
                            break;

                        case TASK_BLOCKED:
                            blocked++;
                            break;

                        case TASK_SLEEPING:
                            sleeping++;
                            break;

                        default:
                            break;
                    }
                }

                sprintf(txBuffer,"\r\n"
               		 "-----STATUS-----\r\n""\r\n"
                        "Uptime      : %lu ms\r\n"
                        "Tasks Total : %u\r\n"
                        "Ready       : %u\r\n"
                        "Running     : %u\r\n"
                        "Blocked     : %u\r\n"
                        "Sleeping    : %u\r\n"
                        "Last Event  : %s\r\n"
                		"\r\n",
                        os_now_ms(),
                        os_get_task_count(),
                        ready,
                        running,
                        blocked,
                        sleeping,
                        last_sensor_msg);
            }

             else if(strncmp(msg.payload, "RESET",5) == 0){
            	 os_reset_stats();
            	 strncpy(last_sensor_msg, "No event yet", sizeof(last_sensor_msg) - 1);
            	 sprintf(txBuffer, "\r\n"
                		 "-----RESET-----\r\n"
            			 "\r\nReset done\r\n"
            			 "\r\n");
             }

             else if (strncmp(msg.payload, "TASKS", 5) == 0)
             {
                 const os_tcb_t *tasks = os_get_task_table();
                 uint8_t count = os_get_task_count();

                 HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, HAL_MAX_DELAY);

                 char txBuffer[64];
              	sprintf(txBuffer,"\r\n"
                 		 "-----TASKS-----\r\n");
              	HAL_UART_Transmit(&huart2,
              	                  (uint8_t*)txBuffer,
              	                  strlen(txBuffer),
              	                  HAL_MAX_DELAY);

                 for (uint8_t i = 0; i < count; i++)
                 {

                     char line[64];
                     sprintf(line,"\r\n"
                             "ID=%u  %-8s  %-8s  runs=%lu\r\n"
                    		 "\r\n",
                             tasks[i].id,
                             tasks[i].name,
                             state_str(tasks[i].state),
                             tasks[i].run_count);

                     HAL_UART_Transmit(&huart2, (uint8_t*)line, strlen(line), HAL_MAX_DELAY);
                 }

                 os_sem_signal(&uart_mutex);
                 return;
             }
            else
            {
                sprintf(txBuffer,
                        "\r\nUnknown Command\r\n"
                        "\r\n");
            }

            HAL_UART_Transmit(&huart2,
                              (uint8_t*)txBuffer,
                              strlen(txBuffer),
                              HAL_MAX_DELAY);

            break;

        default:
            break;
    }

    os_sem_signal(&uart_mutex);
    return;
}

void Sensor_Task(void)
{
    os_msg_t msg;

    os_sem_wait(&sensor_sem); //wait for button

    if(os_current_task()->state == TASK_BLOCKED)
    {
        return;
    }

    msg.type = MSG_TYPE_SENSOR_EVENT;

    strcpy(msg.payload,
           "Caution!! Button Pressed");

    os_mq_send(&uart_queue, &msg);

}

void convertToUpper(char *str)
{
	int i = 0;
	while (str[i] != '\0')
	{
		str[i] = toupper((unsigned char)str[i]);
		i++;
	}
}

static const char* state_str(os_task_state_t s)
{
    switch(s)
    {
        case TASK_READY:    return "READY";
        case TASK_RUNNING:  return "RUNNING";
        case TASK_SLEEPING: return "SLEEPING";
        case TASK_BLOCKED:  return "BLOCKED";
        default:            return "UNKNOWN";
    }
}









