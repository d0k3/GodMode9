# pragma once


// general memory areas

#define __FCRAM0_ADDR   0x20000000
#define __FCRAM0_END    0x28000000

#define __FCRAM1_ADDR   0x28000000
#define __FCRAM1_END    0x30000000


// stuff in FCRAM

#define __FIRMTMP_ADDR  (__FCRAM0_END - 0x0800000)
#define __FIRMTMP_END   (__FIRMTMP_ADDR + 0x0400000)

#define __RAMDRV_ADDR   (__FCRAM0_ADDR + 0x2800000)
#define __RAMDRV_END    __FCRAM0_END // can be bigger on N3DS

#define __STACK_TOP     __RAMDRV_ADDR
#define __STACK_SIZE    0x7F0000 

#define __STACKABT_TOP  (__STACK_TOP - __STACK_SIZE)
#define __STACKABT_SIZE 0x10000

#define __HEAP_ADDR     (__FCRAM0_ADDR + 0x0200000)
#define __HEAP_END      (__STACKABT_TOP - __STACKABT_SIZE)
