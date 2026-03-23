local i2c = {}

i2c.read = _i2c.read
i2c.write = _i2c.write

-- List of devices on the I2C bus
i2c.dev = {
    POWER     = 0, 	-- Unconfirmed
    CAMERA    = 1, 	-- Unconfirmed
    CAMERA2   = 2, 	-- Unconfirmed
    MCU       = 3,
    CAMERA3   = 4,
    LCD1      = 5,
    LCD2      = 6,
    GYRO3     = 9,
    GYRO      = 10,
    GYRO2     = 11,
    DEBUG_PAD = 12,
    IR        = 13,
    EEPROM    = 14, -- Unconfirmed
    NFC       = 15,
    QTM       = 16,
    N3DS_HID  = 17
}

-- Sub-tables for specific devices, containing register addresses and some bitmasks
i2c.mcu = {
    -- Bitmasks for the 4-byte interupt registers.
    interrupt_1 = {
        POWER_BUTTON_HELD           = 1 << 1,  -- The power button was held.
        HOME_BUTTON_PRESS           = 1 << 2,  -- The HOME button was pressed.
        HOME_BUTTON_RELEASE         = 1 << 3,  -- The HOME button was released.
        WLAN_SWITCH_TRIGGER         = 1 << 4,  -- The WiFi switch was triggered.
        SHELL_CLOSE                 = 1 << 5,  -- The shell was closed (or sleep switch turned off).
        SHELL_OPEN                  = 1 << 6,  -- The shell was opened (or sleep switch turned on).
        FATAL_HW_ERROR              = 1 << 7,  -- MCU watchdog reset occurred.
    },

    interrupt_2 = {
        AC_ADAPTER_REMOVED          = 1 << 0,  -- The AC adapter was disconnected.
        AC_ADAPTER_PLUGGED_IN       = 1 << 1,  -- The AC adapter was connected.
        RTC_ALARM                   = 1 << 2, -- The RTC alarm time has been reached.
        ACCELEROMETER_I2C_MANUAL_IO = 1 << 3, -- The manual accelerometer I²C read/write operation has completed.
        ACCELEROMETER_NEW_SAMPLE    = 1 << 4, -- A new accelerometer sample is available.
        CRITICAL_BATTERY            = 1 << 5, -- The battery is critically low.
        CHARGING_STOP               = 1 << 6, -- The battery stopped charging.
        CHARGING_START              = 1 << 7, -- The battery started charging.
    },

    interrupt_3 = {
        VOL_SLIDER                  = 1 << 6, -- The position of the volume slider changed.
    },

    interrupt_4 = {
        LCD_OFF                     = 1 << 0, -- The LCDs turned off.
        LCD_ON                      = 1 << 1, -- The LCDs turned on.
        BOT_BACKLIGHT_OFF           = 1 << 2, -- The bottom screen backlight turned off.
        BOT_BACKLIGHT_ON            = 1 << 3, -- The bottom screen backlight turned on.
        TOP_BACKLIGHT_OFF           = 1 << 4, -- The top screen backlight turned off.
        TOP_BACKLIGHT_ON            = 1 << 5, -- The top screen backlight turned on.
    },

    -- Register addresses
    reg = {
        VERSION_HIGH                 = 0x0,  -- Major version of the MCU firmware.
        VERSION_LOW                  = 0x1,  -- Minor version of the MCU firmware.

        RESET_EVENTS                 = 0x2,  -- @ref mcu.reset_event_flags

        VCOM_TOP                     = 0x3,  -- Flicker/VCOM value for the top screen.
        VCOM_BOTTOM                  = 0x4,  -- Flicker/VCOM value for the bottom screen.

        FIRMWARE_UPLOAD_0            = 0x5,  -- Firmware upload register. 
        FIRMWARE_UPLOAD_1            = 0x6,  -- Firmware upload register.
        FIRMWARE_UPLOAD_2            = 0x7,  -- Firmware upload register.

        RAW_3D_SLIDER_POSITION       = 0x8,  -- Position of the 3D slider.
        VOLUME_SLIDER_POSITION       = 0x9,  -- Position of the volume slider.

        BATTERY_PCB_TEMPERATURE      = 0xA,  -- Temperature of the battery, measured on a sensor on the PCB.
        BATTERY_PERCENTAGE_INT       = 0xB,  -- Integer part of the battery percentage.
        BATTERY_PERCENTAGE_FRAC      = 0xC,  -- Fractional part of the battery percentage.
        BATTERY_VOLTAGE              = 0xD,  -- Voltage of the battery, in units of 20 mV.

        HW_STATUS                    = 0xE,  -- Hardware status bits
        POWER_STATUS                 = 0xF,  -- @ref mcu.power_status_flags

        LEGACY_VERSION_HIGH          = 0xF,  -- (Old MCU_FIRM only) Major firmware version.
        LEGACY_VERSION_LOW           = 0x10, -- (Old MCU_FIRM only) Minor firmware version.
        LEGACY_FIRM_UPLOAD           = 0x3B, -- (Old MCU_FIRM only) Firmware upload register.

        RECEIVED_IRQS_1              = 0x10, -- Bitmask of received IRQs. @ref mcu.interrupt_1
        RECEIVED_IRQS_2              = 0x11, -- Bitmask of received IRQs. @ref mcu.interrupt_2
        RECEIVED_IRQS_3              = 0x12, -- Bitmask of received IRQs. @ref mcu.interrupt_3
        RECEIVED_IRQS_4              = 0x13, -- Bitmask of received IRQs. @ref mcu.interrupt_4
        UNUSED_RO_14                 = 0x14, -- Unused read-only register.
        USERDATA_RAM_15              = 0x15, -- Unused register, supports writing
        USERDATA_RAM_16              = 0x16, -- Unused register, supports writing
        USERDATA_RAM_17              = 0x17, -- Unused register, supports writing
        IRQ_MASK_1                   = 0x18, -- Bitmask of enabled IRQs.  @ref mcu.interrupt_1
        IRQ_MASK_2                   = 0x19, -- Bitmask of enabled IRQs.  @ref mcu.interrupt_2
        IRQ_MASK_3                   = 0x1A, -- Bitmask of enabled IRQs.  @ref mcu.interrupt_3
        IRQ_MASK_4                   = 0x1B, -- Bitmask of enabled IRQs.  @ref mcu.interrupt_4
        USERDATA_RAM_1C              = 0x1C, -- Unused register, supports writing
        USERDATA_RAM_1D              = 0x1D, -- Unused register, supports writing
        USERDATA_RAM_1E              = 0x1E, -- Unused register, supports writing
        USERDATA_RAM_1F              = 0x1F, -- Unused register, supports writing

        PWR_CTL                      = 0x20, -- @ref mcu.power_trigger
        LCD_PWR_CTL                  = 0x22, -- LCD Power control.
        MCU_RESET_CTL                = 0x23, -- Writing 'r' to this register resets the MCU. Stubbed on retail.
        FORCE_SHUTDOWN_DELAY         = 0x24, -- The amount of time, in units of 0.125s, for which the power button needs to be held to trigger a hard shutdown.

        VOLUME_UNK_25                = 0x25, -- Unknown register. Used in mcu::SND
        UNK_26                       = 0x26, -- Unknown register. Used in mcu::CDC

        VOLUME_SLIDER_RAW            = 0x27, -- Raw value of the volume slider, in the range of 0-255
        LED_BRIGHTNESS_STATE         = 0x28, -- Brightness of the status LEDs.
        POWER_LED_STATE              = 0x29, -- @ref mcu.power_led_state
        WLAN_LED_STATE               = 0x2A, -- Controls the WiFi LED.
        CAMERA_LED_STATE             = 0x2B, -- Controls the camera LED (on models that have one).
        LED_3D_STATE                 = 0x2C, -- Controls the 3D LED (on models that have one).
        NOTIFICATION_LED_STATE       = 0x2D, -- Controls the info (notification) LED.
        NOTIFICATION_LED_CYCLE_STATE = 0x2E, -- Bit 0 is set if the info (notification) LED has started a new cycle of its pattern.

        RTC_TIME_SECOND              = 0x30, -- RTC second.
        RTC_TIME_MINUTE              = 0x31, -- RTC minute.
        RTC_TIME_HOUR                = 0x32, -- RTC hour.
        RTC_TIME_WEEKDAY             = 0x33, -- RTC day of the week.
        RTC_TIME_DAY                 = 0x34, -- RTC day of the month.
        RTC_TIME_MONTH               = 0x35, -- RTC month.
        RTC_TIME_YEAR                = 0x36, -- RTC year.
        RTC_TIME_CORRECTION          = 0x37, -- RTC subsecond (RSUBC) correction value.

        RTC_ALARM_MINUTE             = 0x38, -- RTC alarm minute.
        RTC_ALARM_HOUR               = 0x39, -- RTC alarm hour.
        RTC_ALARM_DAY                = 0x3A, -- RTC alarm day.
        RTC_ALARM_MONTH              = 0x3B, -- RTC alarm month.
        RTC_ALARM_YEAR               = 0x3C, -- RTC alarm year.

        TICK_COUNTER_LSB             = 0x3D, -- MCU tick counter value (low byte).
        TICK_COUNTER_MSB             = 0x3E, -- MCU tick counter value (high byte).

        UNK_3F                       = 0x3F, -- Unknown register

        SENSOR_CONFIG                = 0x40, -- @ref mcu.sensor_config

        ACCELEROMETER_MANUAL_REGID_R = 0x41, -- Hardware register ID for use in manual accelerometer I²C reads.
        ACCELEROMETER_MANUAL_REGID_W = 0x43, -- Hardware reigster ID for use in manual accelerometer I²C writes.
        ACCELEROMETER_MANUAL_IO      = 0x44, -- Data register for manual accelerometer reads/writes.
        ACCELEROMETER_OUTPUT_X_LSB   = 0x45, -- Accelerometer X coordinate (low byte).
        ACCELEROMETER_OUTPUT_X_MSB   = 0x46, -- Accelerometer X coordinate (high byte).
        ACCELEROMETER_OUTPUT_Y_LSB   = 0x47, -- Accelerometer Y coordinate (low byte).
        ACCELEROMETER_OUTPUT_Y_MSB   = 0x48, -- Accelerometer Y coordinate (high byte).
        ACCELEROMETER_OUTPUT_Z_LSB   = 0x49, -- Accelerometer Z coordinate (low byte).
        ACCELEROMETER_OUTPUT_Z_MSB   = 0x4A, -- Accelerometer Z coordinate (high byte).

        PEDOMETER_STEPS_LOWBYTE      = 0x4B, -- Pedometer step count (low byte).
        PEDOMETER_STEPS_MIDDLEBYTE   = 0x4C, -- Pedometer step count (middle byte).
        PEDOMETER_STEPS_HIGHBYTE     = 0x4D, -- Pedometer step count (high byte).
        PEDOMETER_CNT                = 0x4E, -- @ref mcu.pedometer_control
        PEDOMETER_STEP_DATA          = 0x4F, -- Step history and time of last update.
        PEDOMETER_WRAP_MINUTE        = 0x50, -- The minute within each RTC hour at which the step history should roll into the next hour.
        PEDOMETER_WRAP_SECOND        = 0x51, -- The second within each RTC hour at which the step history should roll into the next hour.

        VOLUME_CALIBRATION_MIN       = 0x58, -- Lower bound of sound volume.
        VOLUME_CALIBRATION_MAX       = 0x59, -- Upper bound of sound volume.

        STORAGE_AREA_OFFSET          = 0x60, -- Offset within the storage area to read/write to.
        STORAGE_AREA                 = 0x61, -- Input/output byte for write/read operations in the storage area.

        INFO                         = 0x7F, -- System information register.
    },

    -- Bitmasks for the reset event flags
    reset_event_flags = {
        RTC_TIME_LOST  = 1 << 0,  -- RTC time was lost (as is the case when the battery is removed).
        WATCHDOG_RESET = 1 << 1,  -- MCU Watchdog reset occurred.
    },

    -- Bitmasks for the power status
    power_status_flags = {
        SHELL_OPEN        = 1 << 1, -- Set if the shell is open.
        ADAPTER_CONNECTED = 1 << 3, -- Set if the AC adapter is connected.
        CHARGING          = 1 << 4, -- Set if the battery is charging.
        BOTTOM_BL_ON      = 1 << 5, -- Set if the bottom backlight is on.
        TOP_BL_ON         = 1 << 6, -- Set if the top backlight is on.
        LCD_ON            = 1 << 7  -- Set if the LCDs are on.
    },

    -- Bitmasks for the pedometer control
    pedometer_control = {
        CLEAR      = 1 << 0, -- Clear the step history.
        STEPS_FULL = 1 << 4  -- Set when the step history is full.
    },

    -- Bitmasks for the power signal triggers
    power_trigger = {
        SHUTDOWN     = 1 << 0, -- Turn off the system.
        RESET        = 1 << 1, -- Reset the MCU.
        REBOOT       = 1 << 2, -- Reboot the system.
        LGY_SHUTDOWN = 1 << 3, -- Turn off the system. (Used by LgyBg)
        SLEEP        = 1 << 4, -- Signal to enter sleep mode.
        OLDMCU_BL_OFF    = 1 << 4, -- (Old MCU_FIRM only) turn the backlights off.
        OLDMCU_BL_ON     = 1 << 5, -- (Old MCU_FIRM only) turn the backlights on.
        OLDMCU_LCD_OFF   = 1 << 6, -- (Old MCU_FIRM only) turn the LCDs off.
        OLDMCU_LCD_ON    = 1 << 7, -- (Old MCU_FIRM only) turn the LCDs on.
    },

    power_led_state = {
        NORMAL     = 0, -- Fade power LED to blue, while checking for battery percentage.
        FADE_BLUE  = 1, -- Fade power LED to blue.
        SLEEP_MODE = 2, -- The power LED pulses blue slowly, as it does in sleep mode.
        OFF        = 3, -- Power LED fades off.
        RED        = 4, -- Power LED instantaneously turns red.
        BLUE       = 5, -- Power LED instantaneously turns blue.
        BLINK_RED  = 6, -- Power LED and info (notification) LED blink red, as they do when the battery is critically low.
    },

    -- WiFi mode register values
    wifi_mode = {
        CTR = 0, -- 3DS WiFi mode.
        MP  = 1, -- DS[i] WiFi mode ("MP").
    },

    -- Sensor configuration register values
    sensor_config = {
        ACCELEROMETER_ENABLE = 1 << 0, -- If set, the accelerometer is enabled.
        PEDOMETER_ENABLE     = 1 << 1, -- If set, the pedometer is enabled.
    },

    -- Accelerometer scale settings
    accelerometer_scale = {
        SCALE_2G = 0x0, -- -2G to 2G
        SCALE_4G = 0x1, -- -4G to 4G
        SCALE_8G = 0x3, -- -8G to 8G
    }
}

return i2c
