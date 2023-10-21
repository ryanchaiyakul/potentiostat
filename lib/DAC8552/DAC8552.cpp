#include <DAC8552.h>
#include <ADS1256.h>
#include <SPI.h>

/**
 * Pass CS pin and reference voltage in mV
 */
DAC8552::DAC8552(uint8_t cs, uint16_t refV)
{
  this->refV = refV;
  this->cs = cs;
}

/**
 * Set CS and other SPI pins
 */
void DAC8552::init()
{
  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH); // Active low
  SPI.begin();
}

/**
 * Set Channel A to requested voltage
 * If outside of reference range, sets to 0V
 */
void DAC8552::setA(float voltage)
{
  uint16_t data = Voltage_Convert(voltage);
  DAC_WR(WR_LOAD_A, data);
}

/**
 * Set Channel B to requested voltage
 * If outside of reference range, sets to 0V
 */
void DAC8552::setB(float voltage)
{
  uint16_t data = Voltage_Convert(voltage);
  DAC_WR(WR_LOAD_B, data);
}

void DAC8552::DAC_WR(byte command, uint16_t data)
{
  byte dataHigh = data >> 8;  // high 8 bits
  byte dataLow = data & 0xff; // low 8 bits
  SPI.beginTransaction(SPISettings(DAC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(cs, LOW);
  SPI.transfer(command);
  SPI.transfer(dataHigh);
  SPI.transfer(dataLow);
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
}

uint16_t DAC8552::Voltage_Convert(float voltage)
{
  if (abs(voltage * 1000) > refV)
    return 0;
  return (unsigned int)(65536 * ((voltage * 1000) / (refV)));
}
