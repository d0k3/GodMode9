#pragma once

#include "common.h"

// see: https://www.3dbrew.org/wiki/TIMER_Registers
#define TIMER_VAL0  ((vu16*)0x10003000)
#define TIMER_VAL1  ((vu16*)0x10003004)
#define TIMER_VAL2  ((vu16*)0x10003008)
#define TIMER_VAL3  ((vu16*)0x1000300C)
#define TIMER_CNT0  ((vu16*)0x10003002)
#define TIMER_CNT1  ((vu16*)0x10003006)
#define TIMER_CNT2  ((vu16*)0x1000300A)
#define TIMER_CNT3  ((vu16*)0x1000300E)

#define TIMER_COUNT_UP  0x0004
#define TIMER_ACTIVE    0x0080
#define TICKS_PER_SEC   67027964ULL

void timer_start( void );
void timer_stop( void );
u64 timer_ticks( void );
u64 timer_msec( void );
u64 timer_sec( void );
