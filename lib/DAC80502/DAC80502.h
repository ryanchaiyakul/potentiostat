#ifndef DAC80502_H
#define DAC80502_H
#include <Arduino.h>
// SPI Hz
#define DAC80502_SPEED 100000

// DAC Registers
#define DAC80502_CONFIG 0x03
#define DAC80502_GAIN 0x04
#define DAC80502_TRIGGER 0x05
#define DAC80502_BRDCAST 0x06
#define DAC80502_STATUS 0x07
#define DAC80502_A 0x08
#define DAC80502_B 0x09

/**
 * Handles setup and control for DAC8552
*/
class DAC80502 {
    public:
        DAC80502(uint8_t, uint16_t);
        void init();
        void setA(uint16_t);
        void setB(uint16_t);
        void setAandB(uint16_t, uint16_t);
    private:
        void DAC_WR(byte, uint16_t);
        void syncDAC(bool);
        uint16_t Voltage_Convert(uint16_t);
        bool sync;
        uint16_t refV;
        uint8_t cs;
        uint8_t gain;
};

#endif