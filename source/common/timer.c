#include "timer.h"

u64 timer_start( void ) {
    static bool timer_init = true;
    // timer is initialized at least once (right at the beginning)
    // this makes sure it is reinitialized in case of inconsistencies
    if (!(*TIMER_CNT0 & *TIMER_CNT1 & *TIMER_CNT2 & *TIMER_CNT3 & TIMER_ACTIVE) ||
        !(*TIMER_CNT1 & *TIMER_CNT2 & *TIMER_CNT3 & TIMER_COUNT_UP))
        timer_init = true;
    
    if (timer_init) {
        // deactivate, then reset timers
        *TIMER_CNT0 = 0;
        *TIMER_CNT1 = *TIMER_CNT2 = *TIMER_CNT3 = TIMER_COUNT_UP;
        *TIMER_VAL0 = *TIMER_VAL1 = *TIMER_VAL2 = *TIMER_VAL3 = 0;

        // start timers
        *TIMER_CNT0 = TIMER_ACTIVE;
        *TIMER_CNT1 = *TIMER_CNT2 = *TIMER_CNT3 = TIMER_ACTIVE | TIMER_COUNT_UP;
        
        // timer initialized
        timer_init = false;
    }
    return timer_ticks( 0 );
}

/*void timer_stop( void ) {
    *TIMER_CNT0 &= ~TIMER_ACTIVE;
    *TIMER_CNT1 &= ~TIMER_ACTIVE;
    *TIMER_CNT2 &= ~TIMER_ACTIVE;
    *TIMER_CNT3 &= ~TIMER_ACTIVE;
}*/

u64 timer_ticks( u64 start_time ) {
    u64 ticks = 0;
    ticks |= (u64) *TIMER_VAL0 <<  0;
    ticks |= (u64) *TIMER_VAL1 << 16;
    ticks |= (u64) *TIMER_VAL2 << 32;
    ticks |= (u64) *TIMER_VAL3 << 48;
    return ticks - start_time;
}

u64 timer_msec( u64 start_time ) {
    return timer_ticks( start_time ) / (TICKS_PER_SEC/1000);
}

u64 timer_sec( u64 start_time ) {
    return timer_ticks( start_time ) / TICKS_PER_SEC;
}

void wait_msec( u64 msec ) {
    u64 timer = timer_start();
    while (timer_msec( timer ) < msec );
}
