#include "timer.h"

void timer_start( void ) {
    // reset / deactivate timers
    *TIMER_CNT0 = 0;
    *TIMER_CNT1 = *TIMER_CNT2 = *TIMER_CNT3 = TIMER_COUNT_UP;
    *TIMER_VAL0 = *TIMER_VAL1 = *TIMER_VAL2 = *TIMER_VAL3 = 0;

    // start timers
    *TIMER_CNT0 = TIMER_ACTIVE;
    *TIMER_CNT1 = *TIMER_CNT2 = *TIMER_CNT3 = TIMER_ACTIVE | TIMER_COUNT_UP;
}

void timer_stop( void ) {
    *TIMER_CNT0 &= ~TIMER_ACTIVE;
    *TIMER_CNT1 &= ~TIMER_ACTIVE;
    *TIMER_CNT2 &= ~TIMER_ACTIVE;
    *TIMER_CNT3 &= ~TIMER_ACTIVE;
}

u64 timer_ticks( void ) {
    u64 ticks = 0;
    ticks |= (u64) *TIMER_VAL0 <<  0;
    ticks |= (u64) *TIMER_VAL1 << 16;
    ticks |= (u64) *TIMER_VAL2 << 32;
    ticks |= (u64) *TIMER_VAL3 << 48;
    return ticks;
}

u64 timer_msec( void ) {
    return (timer_ticks() * 1000) / TICKS_PER_SEC;
}

u64 timer_sec( void ) {
    return timer_ticks() / TICKS_PER_SEC;
}
