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

  for (int i = 0; i < count; i++)
  {
    waitforDRDY();
    outputBuffer[i][0] = SPI.transfer(0); // MSB
    outputBuffer[i][1] = SPI.transfer(0);
    outputBuffer[i][2] = SPI.transfer(0); // Noisy at 30k SPS and PGA=4 but for completeness
  }
  waitforDRDY();
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

void ADS1256::readDifferentialSingle() //"High-speed" version of the cycleDifferential() function
{
  // Relevant viodeo: https://youtu.be/GBWJdyjRIdM

  int cycle = 1;
  int32_t registerData = 0;
  String ConversionResults;

  SPI.beginTransaction(SPISettings(1920000, MSBFIRST, SPI_MODE1)); // We start this SPI.beginTransaction once.

  // Setting up the input channel
  digitalWrite(cs, LOW);  // CS must stay LOW during the entire sequence [Ref: P34, T24]
  SPI.transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
  SPI.transfer(0x00);
  SPI.transfer(B00000001); // AIN0+AIN1
  digitalWrite(cs, HIGH);

  SPI.endTransaction();

  for (int package = 0; package < 10; package++) // We will collect 10 "packages"
  {
    for (cycle = 1; cycle < 5; cycle++)
    {
      // we cycle through all the 4 differential channels with the RDATA

      // RDATA = B00000001
      // SYNC = B11111100
      // WAKEUP = B11111111

      // Steps are on Page21
      // Step 1. - Updating MUX

      waitforDRDY();

      switch (cycle)
      {
      case 1:                   // Channel 2
        digitalWrite(cs, LOW);  // CS must stay LOW during the entire sequence [Ref: P34, T24]
        SPI.transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
        SPI.transfer(0x00);
        SPI.transfer(B00100011); // AIN2+AIN3
        break;

      case 2:                   // Channel 3
        digitalWrite(cs, LOW);  // CS must stay LOW during the entire sequence [Ref: P34, T24]
        SPI.transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
        SPI.transfer(0x00);
        SPI.transfer(B01000101); // AIN4+AIN5
        break;

      case 3:                   // Channel 4
        digitalWrite(cs, LOW);  // CS must stay LOW during the entire sequence [Ref: P34, T24]
        SPI.transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
        SPI.transfer(0x00);
        SPI.transfer(B01100111); // AIN6+AIN7
        break;

      case 4:                   // Channel 1
        digitalWrite(cs, LOW);  // CS must stay LOW during the entire sequence [Ref: P34, T24]
        SPI.transfer(0x50 | 1); // 0x50 = WREG //1 = MUX
        SPI.transfer(0x00);
        SPI.transfer(B00000001); // AIN0+AIN1
        break;
      }

      /**
       SPI.transfer(B11111100); // SYNC

      delayMicroseconds(4); // t11 delay 24*tau = 3.125 us //delay should be larger, so we delay by 4 us

      SPI.transfer(B11111111); // WAKEUP
      */
      
      // Step 3.
      // Issue RDATA (0000 0001) command
      SPI.transfer(B00000001);

      // Wait t6 time (~6.51 us) REF: P34, FIG:30.
      delayMicroseconds(5);

      // step out the data: MSB | mid-byte | LSB,

      // registerData is ZERO
      registerData |= SPI.transfer(0x0F); // MSB comes in, first 8 bit is updated // '|=' compound bitwise OR operator
      registerData <<= 8;                 // MSB gets shifted LEFT by 8 bits
      registerData |= SPI.transfer(0x0F); // MSB | Mid-byte
      registerData <<= 8;                 // MSB | Mid-byte gets shifted LEFT by 8 bits
      registerData |= SPI.transfer(0x0F); //(MSB | Mid-byte) | LSB - final result
      // After this, DRDY should go HIGH automatically

      // Constructing an output
      ConversionResults = ConversionResults + registerData;
      if (cycle < 4)
      {
        ConversionResults = ConversionResults + "\t";
      }
      else
      {
        ConversionResults = ConversionResults;
      }
      //---------------------
      registerData = 0;
      digitalWrite(cs, HIGH); // We finished the command sequence, so we switch it back to HIGH

      // Expected output when using a resistor ladder of 1k resistors and the ~+5V output of the Arduino:
      // Formatting  Channel 1 Channel 2 Channel 3 Channel 4
      /*
      16:14:23.066 -> 4.79074764  4.16625738  3.55839943  2.96235866
      16:14:23.136 -> 4.79277801  4.16681241  3.55990862  2.96264190
      16:14:23.238 -> 4.79327344  4.16698741  3.55968427  2.96277694        */
    }
    ConversionResults = ConversionResults + '\n'; // Add a linebreak after a line of data (4 columns)
  }
  Serial.print(ConversionResults); // print everything after 10 packages
  ConversionResults = "";
  SPI.endTransaction();
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