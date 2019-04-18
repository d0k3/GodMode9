#include <common.h>
#include <types.h>
#include <hid_map.h>

#include "hw/codec.h"
#include "hw/hid.h"
#include "hw/mcu.h"

#define REG_HID	(~(*(vu16*)(0x10146000)) & BUTTON_ANY)

static u32 HID_ConvertCPAD(s16 cpad_x, s16 cpad_y)
{
	u32 ret = 0;

	switch(int_sign(cpad_x)) {
		default:
			break;
		case 1:
			ret |= BUTTON_RIGHT;
			break;
		case -1:
			ret |= BUTTON_LEFT;
	}

	switch(int_sign(cpad_y)) {
		default:
			break;
		case 1:
			ret |= BUTTON_UP;
			break;
		case -1:
			ret |= BUTTON_DOWN;
	}

	return ret;
}

u64 HID_GetState(void)
{
	CODEC_Input codec;
	u64 ret = 0;

	CODEC_Get(&codec);

	ret = REG_HID | MCU_GetSpecialHID();
	if (!(ret & BUTTON_ARROW))
		ret |= HID_ConvertCPAD(codec.cpad_x, codec.cpad_y);

	if (codec.ts_x <= 0xFFF)
		ret |= BUTTON_TOUCH;

	ret |= (((u64)codec.ts_x << 16) | (u64)codec.ts_y) << 32;

	return ret;
}
