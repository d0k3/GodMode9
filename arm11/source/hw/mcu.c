#include <hid_map.h>
#include <types.h>
#include <arm.h>

#include "hw/gpio.h"
#include "hw/gpulcd.h"
#include "hw/i2c.h"
#include "hw/mcu.h"

enum {
    PWR_BTN = 0,
    PWR_HOLD = 1,
    HOME_BTN = 2,
    HOME_HOLD = 3,
    WIFI_SWITCH = 4,
    SHELL_CLOSE = 5,
    SHELL_OPEN = 6,
    VOL_SLIDER = 22,
};

static u8 cached_volume_slider = 0;
static u32 spec_hid = 0;

static void MCU_UpdateVolumeSlider(void)
{
    cached_volume_slider = MCU_ReadReg(0x09);
}

void MCU_HandleInterrupts(u32 __attribute__((unused)) irqn)
{
    u32 ints;

    // Reading the pending mask automagically acknowledges
    // all the interrupts, so we must be sure to process all
    // of them in one go, possibly keep reading from the
    // register until it returns all zeroes
    MCU_ReadRegBuf(0x10, (u8*)&ints, sizeof(ints));

    while(ints != 0) {
        u32 mcu_int_id = 31 - __builtin_clz(ints);

        switch(mcu_int_id) {
            case PWR_BTN:
            case PWR_HOLD:
                spec_hid |= BUTTON_POWER;
                break;

            case HOME_BTN:
            case HOME_HOLD:
                spec_hid |= BUTTON_HOME;
                break;

            case WIFI_SWITCH:
                spec_hid |= BUTTON_WIFI;
                break;

            case VOL_SLIDER:
                MCU_UpdateVolumeSlider();
                break;

            default:
                break;
        }

        ints &= ~BIT(mcu_int_id);
    }
}

void MCU_Init(void)
{
    u32 mask = 0xFFBF0000 | BIT(11);
    I2C_writeRegBuf(3, 0x18, (const u8*)&mask, sizeof(mask));
    MCU_UpdateVolumeSlider();
    GPIO_setBit(19, 9); // enables MCU interrupts?
}

u8 MCU_GetVolumeSlider(void)
{
    return cached_volume_slider;
}

u32 MCU_GetSpecialHID(void)
{
    u32 ret = spec_hid;
    spec_hid = 0;
    return ret;
}
