
#ifndef INC_OS_TICK_H_
#define INC_OS_TICK_H_

#include <stdint.h>

void os_tickInit(void);

void tick_Handler(void);

uint32_t os_now_ms(void);


#endif /* INC_OS_TICK_H_ */
