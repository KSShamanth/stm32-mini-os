#include "main.h"
#include "os_core.h"
#include "os_tick.h"
#include "cmsis_compiler.h"

static volatile uint32_t os_tick_ms =0;

void os_tickInit(void){

	if (HAL_SYSTICK_Config(SystemCoreClock / 1000) !=0){ //divided by 1000 to make systick interrupt every 1ms
		Error_Handler();
	}

}

void tick_Handler(void){
	os_tick_ms++;
	os_scheduler_tick();
}

uint32_t os_now_ms(void){ //how much time has passed since startup

	uint32_t ticks;

	__disable_irq();
	ticks = os_tick_ms;
	__enable_irq();

	return ticks;

}
