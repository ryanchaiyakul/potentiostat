#include <ADS1256.h>
#include <Arduino.h>
#include <SPI.h>

ADS1256::ADS1256(uint8_t cs, uint8_t rdy)
{
  this->cs = cs;
  this->rdy = rdy;
}

/**
 * Sets pins and configures adc
 *
 * TODO: Make this configurable instead of default
 */
void ADS1256::init()
{
  // Set pins
  pinMode(rdy, INPUT);
  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH); // Active low
  SPI.begin();

  adcSendCMD(RESET); // RESET Command
  delay(100);        // let the system power up and stabilize (datasheet pg 24)

  // ADS Settings
  adc_WR8(0x01, 0x01);    // AIN0-AIN1
  adc_WR8(0x00, 0x32);    // Enable Buffer
  adc_WR8(0x02, 0x02);    // PGA=4
  adc_WR8(0x03, 0xF0);    // DRATE=30,000SPS
  delayMicroseconds(250); // settling time: 30,000SPS needs 0.21ms
  callibrate();
}

/**
 * Self callibrates adc and refreshes ofc and fsc
 */
void ADS1256::callibrate()
{
  adcSendCMD(CALLIBRATE); // send the calibration command
  delayMicroseconds(800); // roughly 692 us for 30,000SPS PGA=4

  waitforDRDY(); // Wait until DRDY is LOW => calibration complete
  uint8_t off0 = adc_RD8(0x05);
  uint8_t off1 = adc_RD8(0x06);
  uint8_t off2 = adc_RD8(0x07);
  uint8_t fs0 = adc_RD8(0x08);
  uint8_t fs1 = adc_RD8(0x09);
  uint8_t fs2 = adc_RD8(0x0A);

  ofc = (off2 << 16 & 0x00FF0000) | off1 << 8 | off0; // already sign-extends from 24 bits
  fsc = fs2;                                          // fsc = (fs2 << 16 & 0x00FF0000) | fs1 << 8 | fs0; idk why it breaks?? but it does
  fsc = (fsc << 8) + fs1;
  fsc = (fsc << 8) + fs0;
}

/**
 * Prints callibration values
 */
void ADS1256::printCallibration()
{
  Serial.print(ofc);
  Serial.print(",");
  Serial.println(fsc);
}

/**
 * Blocks until DRDY is low
 */
void ADS1256::waitforDRDY()
{
  while (digitalRead(rdy))
  {
    continue;
  }
}

/**
 * Read continuously for "count" iterations and stores the values in an output buffer
 *
 */
void ADS1256::readMulti(uint8_t count)
{
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(cs, LOW); // Pull SS Low to Enable Communications with ADS1247
  waitforDRDY();         // Wait until DRDY does low, then issue the command
  SPI.transfer(RDATAC);
  delayMicroseconds(7); // Wait t6 time (~6.51 us) REF: P34, FIG:30.

  uint8_t i = 0;
  while (i < count)
  {
    waitforDRDY();
    outputBuffer[i][0] = SPI.transfer(0);
    outputBuffer[i][1] = SPI.transfer(0);
    outputBuffer[i][2] = SPI.transfer(0); // Last byte is noisey
    i++;
  }
  SPI.transfer(SDATAC);
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
}

int32_t ADS1256::readSingle()
{
  int32_t adc_val;
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(cs, LOW); // Pull SS Low to Enable Communications with ADS1247
  waitforDRDY();         // Wait until DRDY is LOW
  SPI.transfer(RDATA);   // Issue read data command RDATA
  delayMicroseconds(7);

  // assemble 3 bytes into one 32 bit word
  adc_val = ((uint32_t)SPI.transfer(0) << 16) & 0x00FF0000;
  adc_val |= ((uint32_t)SPI.transfer(0) << 8);
  adc_val |= SPI.transfer(0);

  digitalWrite(cs, HIGH);
  SPI.endTransaction();

  //  Extend a signed number
  if (adc_val & 0x800000)
  {
    adc_val |= 0xFF000000;
  }
  return adc_val;
}

/**
 * Write a single byte to the addressed register
 */
void ADS1256::adc_WR8(byte addr, uint8_t data)
{
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(cs, LOW);
  delayMicroseconds(5);
  SPI.transfer(0x50 | addr);
  SPI.transfer(0x00);
  SPI.transfer(data);
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
}

/**
 * Read a single byte from the addressed register
 */
uint8_t ADS1256::adc_RD8(byte addr)
{
  uint8_t bufr;
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(cs, LOW);
  SPI.transfer(0x10 | addr); // send 1st command byte, address of the register
  SPI.transfer(0x00);        // send 2nd command byte, read only one register
  delayMicroseconds(5);
  bufr = SPI.transfer(0); // read data of the register
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
  return bufr;
}

/**
 * Send a single byte command
 */
void ADS1256::adcSendCMD(uint8_t cmd)
{
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(cs, LOW);
  SPI.transfer(cmd);
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
}

/**
  void adcSendCSV(int32_t result) {
  Serial.println(micros() - start_time);
  Serial.println(",");
  Serial.println(result);
  }
*/

/**
  void printSettings() {
  Serial.println("Printing ADC Registers");
  Serial.println(adc_RD8(0x01));
  Serial.println(adc_RD8(0x02));
  Serial.println(adc_RD8(0x03));
  }
*/

/**
  void validateADS() {
  Serial.println("Starting ADS Register Test");
  uint8_t prev_value = adc_RD8(0x02);
  Serial.print("Initial value: ");
  Serial.println(prev_value, HEX);
  adc_WR8(0x02, 0x08);  //PGA=16'
  Serial.print("Updated value (Should be 0x08): ");
  Serial.println(adc_RD8(0x02));
  adc_WR8(0x02, prev_value);
  Serial.print("Should be initial value: ");
  Serial.println(adc_RD8(0x02), HEX);
  }
*/