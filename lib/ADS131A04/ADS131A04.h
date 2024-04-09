#ifndef ADS131A04_H
#define ADS131A04_H
#include <Arduino.h>
// SPI Hz
#define ADS131A04_HZ 2000000

// System Commands
#define ADS131A04_CMD_NULL      0x0000
#define ADS131A04_CMD_RESET     0x0011
#define ADS131A04_CMD_STANDBY   0x0022
#define ADS131A04_CMD_WAKEUP    0x0033
#define ADS131A04_CMD_LOCK      0x0555
#define ADS131A04_CMD_UNLOCK    0x0655

/**
 * Reg Commands
 * 
 * Address: a aaaa
 * Data: dddd dddd
 * N registers: nnnn nnnn + 1
*/
#define ADS131A04_CMD_RREG  0x2000      // 001a aaaa 0000 0000
#define ADS131A04_CMD_RREGS 0x2000      // 001a aaaa nnnn nnnn
#define ADS131A04_CMD_WREG  0x4000      // 010a aaaa dddd dddd
#define ADS131A04_CMD_WREGS 0x6000      // 011a aaaa nnnn nnnn + dddd dddd eeee eeee

// Register Map

// Status
#define ADS131A04_STAT_1    0x02
#define ADS131A04_STAT_P    0x03
#define ADS131A04_STATS_N   0x04
#define ADS131A04_STATS_S   0x05
#define ADS131A04_EEROR_CNT 0x06
#define ADS131A04_STAT_M2   0x07

// Configurable
#define ADS131A04_A_SYS_CFG 0x0B
#define ADS131A04_D_SYS_CFG 0x0C
#define ADS131A04_CLK1      0x0D
#define ADS131A04_CLK2      0x0E
#define ADS131A04_ADC_ENA   0x0F
#define ADS131A04_ADC1      0x11
#define ADS131A04_ADC2      0x12
#define ADS131A04_ADC3      0x13
#define ADS131A04_ADC4      0x14

// GAIN
#define ADS131A04_GAIN1      0x00
#define ADS131A04_GAIN2      0x01
#define ADS131A04_GAIN4      0x02
#define ADS131A04_GAIN8      0x03
#define ADS131A04_GAIN16     0x04

class ADS131A04
{
public:
    ADS131A04(uint8_t);
    uint16_t readReg(uint16_t);
    uint16_t writeReg(uint16_t, uint16_t);
    void enable();
    void disable();
    void setGain(uint8_t, uint16_t);
    uint16_t* readChannels();
private:
    uint16_t callCMD(uint16_t);
    uint16_t writeCMD(uint16_t);
    uint8_t cs;
};

#endif