#include <stdint.h>

static const uint8_t TIMER_PERSIST = 0x01;

void timer_init();
void timer_sched(uint32_t ms, void (*tick)(void *), void *userdata, uint8_t flags);
