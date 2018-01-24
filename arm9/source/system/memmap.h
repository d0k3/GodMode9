# pragma once

// not complete! (!!!)

#define __RAMDRV_ADDR   0x22800000
#define __RAMDRV_END    0x28000000

#define __STACK_ADDR    (__RAMDRV_ADDR - 0x800000)
#define __STACK_END     __RAMDRV_ADDR

#define __HEAP_ADDR     0x20700000
#define __HEAP_END      __STACK_ADDR
