# STM32 Mini OS

> OVERVIEW:

This project implements a lightweight cooperative operating system for STM32.

The OS provides:

Cooperative round-robin scheduler
SysTick based millisecond timebase
Task state management
Binary semaphore
Fixed-size message queue
UART command interface
Interrupt-driven event handling

The implementation uses static allocation and is designed to be simple, inspectable, and easy to understand. The scheduler and kernel primitives are implemented from scratch without using FreeRTOS, CMSIS-RTOS, Zephyr, ThreadX, or any other RTOS.

> HARDWARE:

Board: STM32 NUCLEO-G070RB Development board
MCU: STM32G070RBT6
UART: USART2, 115200 baud, 8N1, no flow control (PA2 = TX, PA3 = RX)
LED: LED_GREEN_Pin on GPIOA (PA5), toggled by the heartbeat task
Button / sensor input: GPIO_PIN_13 on GPIOC, EXTI falling edge

> TOOLCHAIN:

STM32CubeIDE with STM32CubeMX-generated peripheral init using STM32 HAL drivers.

> BUILD AND FLASH:

1. Open the project in STM32CubeIDE (or import the .ioc if regenerating).
2. Build the project (Project > Build All).
3. Connect the board over ST-Link USB.
4. Flash with Run > Debug.
5. Open a serial terminal (TeraTerm used here) and configure it at 115200 baud, 8 data bits, no parity and 1 stop bits.

> OS ARCHITECTURE:
The OS is built around a cooperative scheduler.

The kernel has three layers:

1. os_core — the scheduler core: a static task table (OS_MAX_TASKS = 8), task control blocks (os_tcb_t), and the dispatch loop in os_start().

2. Kernel primitives — os_sem (binary semaphore) and os_mq (fixed-depth message queue), built directly on top of the task table with no allocation.

3. os_tick — a 1 ms timebase driven by SysTick, exposing os_now_ms() and feeding os_scheduler_tick() every interrupt to wake sleeping tasks.

Three application tasks sit on top: LED_Task (heartbeat), UART_Task (command console), and Sensor_Task (button-triggered event producer). HAL interrupt callbacks (HAL_GPIO_EXTI_Falling_Callback, HAL_UART_RxCpltCallback) are the only places interrupt context touches kernel state, and they only ever signal a semaphore or enqueue a message — they never call a task function directly.

> SCHEDULER MODEL:

This is a cooperative, single-stack, round-robin scheduler. There is no preemption and no context switch in the traditional sense.

os_start() runs an infinite loop. On each pass it scans the task table starting from next_task_idx, finds the first task in the READY state, marks it RUNNING, and calls its task function as a plain C function call on the same stack the scheduler itself is running on.

A task "yields" simply by returning from its function. There is no saved program counter, no saved stack pointer, and no per-task stack at all. Every task function must run to completion (or to a deliberate early return) on every dispatch. When the function returns, if its state is still RUNNING the scheduler puts it back to READY and advances next_task_idx, so the next pass starts after it (round robin).

If no task is READY, the core executes __WFI() and waits for the next interrupt (most commonly the 1 ms SysTick).

> TASK STATES:

| State    | Description                              |
| -------- | ---------------------------------------- |
| READY    | Eligible to run                          |
| RUNNING  | Currently executing                      |
| SLEEPING | Waiting for timeout from `os_sleep_ms()` |
| BLOCKED  | Waiting on semaphore or message queue    |

For this implementation specifically:

LED_Task transitions RUNNING → SLEEPING → READY using os_sleep_ms(498).
Sensor_Task transitions RUNNING → BLOCKED → READY while waiting on sensor_sem.
UART_Task transitions RUNNING → BLOCKED → READY while waiting for messages in uart_queue.
SysTick wakes sleeping tasks and moves them back to READY

> KERNEL PRIMITIVES:

1. Binary semaphore (os_sem)

void os_sem_init(os_sem_t *sem, uint8_t value);
void os_sem_wait(os_sem_t *sem);
void os_sem_signal(os_sem_t *sem);

Two independent instances are used in the demo, for two different purposes:

sensor_sem — initialized to 0. Signaled only by the EXTI falling-edge callback (button press), waited on only by Sensor_Task. This is a one-directional ISR-to-task signal, not a shared resource.

uart_mutex — initialized to 1. Waited on and signaled by both LED_Task and UART_Task to serialize their calls to HAL_UART_Transmit() on the shared USART2 peripheral. This one has no connection to any interrupt handler.

2. Message queue (os_mq)

void os_mq_init(os_mq_t *mq);
int8_t os_mq_send(os_mq_t *mq, const void *msg);
void os_mq_recv(os_mq_t *mq, void *msg_out);

A single instance, uart_queue, is used — a depth-8 ring buffer of fixed-size os_msg_t messages ({ uint8_t type; char payload[32]; }). It has two producers and one consumer:

Sensor_Task sends a MSG_TYPE_SENSOR_EVENT message after waking from sensor_sem. HAL_UART_RxCpltCallback (USART2 RX interrupt) sends a MSG_TYPE_UART_COMMAND message directly from interrupt context whenever a complete line (\r or \n) has been received. UART_Task is the sole consumer, dispatching on msg.type to either print the sensor event or parse it as a console command.

os_mq_send() returns OS_MQ_FULL if the queue has 8 messages already buffered and does not block the producer; the return value is currently unchecked at both call sites, which means a full queue silently drops the message rather than retrying.

> PUBLIC APIs

| Function                          | Description                                           | 
|-----------------------------------|-------------------------------------------------------|
| `os_init()`                       | Initialise the scheduler and task table               |
| `os_task_create(name, fn, state)` | Register a task, returns its TCB pointer              |
| `os_start()`                      | Start the scheduler — does not return                 |
| `os_sleep_ms(ms)`                 | Sleep the current task for at least `ms` milliseconds |
| `os_yield()`                      | Voluntarily yield CPU back to the scheduler           |
| `os_now_ms()`                     | Return current uptime in milliseconds                 |
| `os_sem_init(sem, value)`         | Initialise a binary semaphore                         |
| `os_sem_wait(sem)`                | Wait on a semaphore, blocks if unavailable            |
| `os_sem_signal(sem)`              | Signal a semaphore, wakes a waiting task              |
| `os_mq_init(mq)`                  | Initialise a message queue                            |
| `os_mq_send(mq, msg)`             | Enqueue a message, returns error if full              |
| `os_mq_recv(mq, msg)`             | Receive a message, blocks if empty                    |

> DEMO APPLICATION

Three tasks, created in App_TasksInit():

1. LED_Task — the status/heartbeat task. Every ~498 ms it prints [HEARTBEAT] tick=<ms> over UART (guarded by uart_mutex) and toggles the green LED, then calls os_sleep_ms(498).
2. UART_Task — the command console. Blocks on os_mq_recv(&uart_queue, ...),and on receiving a message either prints a sensor event or parses a typed command.
3. Sensor_Task — blocks on sensor_sem; when the button on PC13 is pressed, itbuilds a MSG_TYPE_SENSOR_EVENT message and enqueues it into uart_queue for UART_Task to print.

UART commands

Implemented commands (typed over the serial terminal, terminated with \r or \n):

| Command  | Behavior |
| `HELP`   | Display the list of supported UART commands. |
| `STATUS` | Print OS uptime, task counts by state (READY, BLOCKED, SLEEPING), and general system status information. |
| `TASKS`  | Display each task's ID, name, current state, and execution count (`run_count`). |
| `LATEST` | Display the most recently received sensor event message. |
| `RESET`  | Reset all task execution counters and clear the stored sensor event message. |

Commands are case-insensitive — incoming text is uppercased in UART_Task before matching.

> HOW TO OBSERVE THE DEMO

1. Flash firmware to STM32 board.
2. Open serial terminal (TeraTerm) at 115200 baud.
3. Observe heartbeat messages.
4. Press USER button (PC13).
5. Observe sensor event message.
6. Enter UART commands:
    HELP
    STATUS
    TASKS
    LATEST
    RESET

> DEMO EVIDENCE

The submission includes:

- UART console screenshots
- Task state screenshots
- Sensor event demonstration screenshots
- Short demonstration video showing scheduler operation, UART commands, and interrupt-driven events


> KNOWN LIMITATIONS AND ASSUMPTIONS

- This is a cooperative scheduler, not preemptive — no preemption, single shared stack, fixed task table size, no mid-function resume on block.
- This implementation supports only a single waiting tasks for the semaphores as well as message queue. It supports only one blocked task per semaphore, no timeout mechanism, no priority handling, and no priority inheritance.
- os_mq_send()'s OS_MQ_FULL return value is not checked by either producer; a full queue drops the newest message rather than blocking or retrying. With one button-driven producer and one ISR-driven producer at typical UART typing speed, the 8-slot queue was not observed to fill in testing.
- os_yield() is present for API completeness. In this cooperative design, tasks naturally return to the scheduler after completing one dispatch cycle, so explicit yielding is not required by the demo application.
- The clock source used here is HSI with frequency of 16MHz.
- UART command traffic is assumed to be low enough that the fixed-size queue does not overflow during normal operation.
- The demo application is intended to demonstrate OS behavior and kernel primitives rather than provide production-grade fault tolerance.
- UART_Task holds the CPU while printing — HAL_UART_Transmit is blocking. During long outputs, LED_Task wakeup deadlines accumulate and it fires several times in quick succession when control returns to the scheduler. This is expected behavior in a cooperative single-stack design.
