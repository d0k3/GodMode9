local i2c = {}

i2c.read = _i2c.read
i2c.write = _i2c.write

-- List of devices on the I2C bus
i2c.dev = {
    POWER     = 0, 	-- Unconfirmed
    CAMERA    = 1, 	-- Unconfirmed
    CAMERA2   = 2, 	-- Unconfirmed
    MCU       = 3,
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

return i2c
