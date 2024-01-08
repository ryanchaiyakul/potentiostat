#ifndef ADS1256_H
#define ADS1256_H
#include <Arduino.h>
// SPI Hz
#define ADC_SPEED 2000000

#define BUFFER_CAPACITY 64 // Each buffer object is 16 bytes (NOISE FREE)

// ADC Commands
#define RESET 0xFE
#define CALLIBRATE 0xF0
#define RDATA 0x01
#define RDATAC 0x03
#define SDATAC 0x0F

class ADS1256
{
public:
  ADS1256(uint8_t, uint8_t);
  void init();
  void callibrate();
  void printCallibration();
  int32_t readSingle();
  void readMulti(uint8_t);
  void readDifferentialSingle();
  uint8_t outputBuffer[BUFFER_CAPACITY][3];

private:
  uint8_t adc_RD8(byte);
  void adc_WR8(byte, uint8_t);
  void adcSendCMD(uint8_t);
  void waitforDRDY();
  int32_t ofc;
  uint32_t fsc;
  uint8_t cs;
  uint8_t rdy;
};

#endif