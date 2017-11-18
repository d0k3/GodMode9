#pragma once

#include "common.h"

#define BCDVALID(b) (((b)<=0x99)&&(((b)&0xF)<=0x9)&&((((b)>>4)&0xF)<=0x9))
#define BCD2NUM(b)  (BCDVALID(b) ? (((b)&0xF)+((((b)>>4)&0xF)*10)) : 0xFF)
#define NUM2BCD(n)  ((n<99) ? (((n/10)*0x10)|(n%10)) : 0x99)
#define DSTIMEGET(bcd,n) (BCD2NUM((bcd)->n))

// see: http://3dbrew.org/wiki/I2C_Registers#Device_3 (register 30)
typedef struct {
    u8 bcd_s;
    u8 bcd_m;
    u8 bcd_h;
    u8 weekday;
    u8 bcd_D;
    u8 bcd_M;
    u8 bcd_Y;
	u8 leap_count;
} __attribute__((packed)) DsTime;

bool is_valid_dstime(DsTime* dstime);
bool get_dstime(DsTime* dstime);
bool set_dstime(DsTime* dstime);
