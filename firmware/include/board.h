#pragma once

// ZECTRIX NOTE4C PCB 1.0 — pin map from wiki.zectrix.com

// Buttons (active LOW)
#define PIN_BTN_BOOT   0
#define PIN_BTN_DOWN   18
#define PIN_BTN_UP     39

// Power / status
#define PIN_STDBY      1   // STDBY_H — charge full / standby
#define PIN_CHRG       2   // CHRG_L  — charging (active LOW typical)
#define PIN_LED        3
// NOTE4C 状态灯：低电平点亮（与官方 board_power_bsp 一致：ON=0 / OFF=1）
#define LED_ON_LEVEL   LOW
#define LED_OFF_LEVEL  HIGH
#define PIN_BAT_ADC    4
#define PIN_PWR_HOLD   17

// RTC / NFC sense
#define PIN_RTC_INT    5
#define PIN_NFC_FIELD  7
#define PIN_NFC_PWR    21

// E-paper SSD2683
#define PIN_EPD_PWR    6
#define PIN_EPD_BUSY   8
#define PIN_EPD_RST    9
#define PIN_EPD_DC     10
#define PIN_EPD_CS     11
#define PIN_EPD_SCK    12
#define PIN_EPD_MOSI   13

// Audio ES8311 / I2S
#define PIN_I2S_MCLK   14
#define PIN_I2S_BCLK   15
#define PIN_I2S_DOUT   16  // codec -> ESP
#define PIN_I2S_LRCK   38
#define PIN_AUDIO_PWR  42
#define PIN_I2S_DIN    45  // ESP -> codec
#define PIN_SPK_PA     46

// I2C
#define PIN_I2C_SDA    47
#define PIN_I2C_SCL    48
#define I2C_ADDR_RTC   0x51  // PCF8563
#define I2C_ADDR_NFC   0x55
#define I2C_ADDR_ES8311 0x18 // common default; scan will confirm

// Display geometry (for later)
#define EPD_WIDTH      400
#define EPD_HEIGHT     300
