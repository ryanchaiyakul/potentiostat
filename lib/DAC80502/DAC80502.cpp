#include <DAC80502.h>
#include <ADS1256.h>
#include <SPI.h>

/**
 * Pass CS pin and reference voltage in mV
 */
DAC80502::DAC80502(uint8_t cs, uint16_t refV)
{
  this->refV = refV;
  this->cs = cs;
  gain = 1;
  sync = false;
}

/**
 * Set CS and other SPI pins
 */
void DAC80502::init()
{
  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH); // Active low
  SPI.begin();
  DAC_WR(DAC80502_GAIN, 0x0103);
  //DAC_WR(DAC80502_GAIN, 0x0003);
}

/**
 * Set Channel A to requested voltage
 * If outside of reference range, sets to 0V
 */
void DAC80502::setA(uint16_t voltage)
{
  uint16_t data = Voltage_Convert(voltage);
  DAC_WR(DAC80502_A, data);
}

/**
 * Set Channel B to requested voltage
 * If outside of reference range, sets to 0V
 */
void DAC80502::setB(uint16_t voltage)
{
  uint16_t data = Voltage_Convert(voltage);
  DAC_WR(DAC80502_B, data);
}

/**
 * Synchronizes and sets A and B then unsynchronizes
*/
void DAC80502::setAandB(uint16_t a_voltage, uint16_t b_voltage)
{
  syncDAC(true);
  setA(a_voltage);
  setB(b_voltage);
  DAC_WR(DAC80502_TRIGGER, 0x08);
  syncDAC(false);
}
/**
 * Switch between broadcast and synchronized mode
*/
void DAC80502::syncDAC(bool isSync)
{
  sync = isSync;
  if (isSync) {
    DAC_WR(DAC80502_CONFIG, 0x22);
  } else {
    DAC_WR(DAC80502_CONFIG, 0x20);
  }
}

void DAC80502::DAC_WR(byte command, uint16_t data)
{
  byte dataHigh = data >> 8;  // high 8 bits
  byte dataLow = data & 0xff; // low 8 bits
  SPI.beginTransaction(SPISettings(DAC80502_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(cs, LOW);
  SPI.transfer(command);
  SPI.transfer(dataHigh);
  SPI.transfer(dataLow);
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
}

uint16_t DAC80502::Voltage_Convert(uint16_t voltage)
{
  if (voltage > refV * gain)
    return 0;
  // DAC_DATA = V_OUT * 2**(16) / (VREFIO * GAIN)
  return (unsigned int)(65536 * voltage / (refV * gain));
}
