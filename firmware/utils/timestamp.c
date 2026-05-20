#include "timestamp.h"
#include "hardware/timer.h"

void timestamp_init(void) {
    
}

uint32_t timestamp_get_us(void) {
    
    return timer_hw->timerawl;
}