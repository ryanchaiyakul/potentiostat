#ifndef DAC8852_H
#define DAC8852_H
#include <Arduino.h>
// SPI Hz
#define DAC_SPEED 16000000

// DAC Commands
#define LOAD_BUFF_A 0x00
#define LOAD_BUFF_B 0x04
#define WR_LOAD_A 0x10
#define WR_LOAD_B 0x24

/**
 * Handles setup and control for DAC8552
*/
class DAC8552 {
    public:
        DAC8552(uint8_t, uint16_t);
        void init();
        void setA(float);
        void setB(float);
    private:
        void DAC_WR(byte, uint16_t);
        uint16_t Voltage_Convert(float);
        uint16_t refV;
        uint8_t cs;
};

#endif