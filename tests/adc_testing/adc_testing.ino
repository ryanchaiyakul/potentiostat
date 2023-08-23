#include <SPI.h>
#define ADC_SPEED 2000000  // 8000000 / 4 (f_clk / 4)

// ADC Commands
#define RESET 0xFE
#define CALLIBRATE 0xF0
#define RDATA 0x01
#define RDATAC 0x03
#define SDATAC 0x0F

// Pinout
#define RDY 3
#define CS_adc 8
#define CS_dac 10
#define DIN 11   // MISO
#define DOUT 12  // MOSI
#define SCLK 13  // SCLK

int32_t ofc;
uint32_t fsc;

unsigned long ini_time;
unsigned long total_time;
uint32_t sample_counter = 150000;

uint8_t outputBuffer[3];

bool volatile dataReady = false;
int count = 0;

void setup() {
  delay(50);
  // put your setup code here, to run once:
  Serial.begin(230400);
  while (!Serial) {
    ;
  }
  SPI.begin();
  pinMode(DIN, OUTPUT);   // MOSI
  pinMode(SCLK, OUTPUT);  // CLK
  pinMode(CS_adc, OUTPUT);
  digitalWrite(CS_adc, HIGH);
  pinMode(RDY, INPUT);
  pinMode(DOUT, INPUT);  // MISO

  attachInterrupt(digitalPinToInterrupt(RDY), RDY_ISR, FALLING);

  initADS();
  printCallibration();
  ini_time = micros();
}

void loop() {
  read_multi(sample_counter);
  total_time = micros() - ini_time;
  Serial.print("Total Duration (us): ");
  Serial.println(total_time);
  Serial.print("# of Samples: ");
  Serial.println(sample_counter);
  double period = double(total_time) / sample_counter;
  double frequency = (1 / period) * 1000000;
  Serial.print("Average Period (us): ");
  Serial.println(period, 6);
  Serial.print("Frequency (hz): ");
  Serial.println(frequency, 6);
  Serial.println();
  ini_time = micros();
}

// ADC

void initADS() {
  adcSendCMD(RESET);  //RESET Command
  delay(100);         //let the system power up and stabilize (datasheet pg 24)

  // ADS Settings
  adc_WR8(0x01, 0x01);  // AIN0-AIN1
  adc_WR8(0x00, 0x32);  // Enable Buffer
  adc_WR8(0x02, 0x02);  // PGA=4
  adc_WR8(0x03, 0xF0);  // DRATE=30,000SPS
  Serial.println(adc_RD8(0x01));
  Serial.println(adc_RD8(0x02));
  Serial.println(adc_RD8(0x03));
  delayMicroseconds(250);  // settling time: 30,000SPS needs 0.21ms
  callibrateADS();
}

void callibrateADS() {
  adcSendCMD(CALLIBRATE);  //send the calibration command
  delayMicroseconds(800);  // roughly 692 us for 30,000SPS PGA=4

  waitforDRDY();  // Wait until DRDY is LOW => calibration complete
  uint8_t off0 = adc_RD8(0x05);
  uint8_t off1 = adc_RD8(0x06);
  uint8_t off2 = adc_RD8(0x07);
  uint8_t fs0 = adc_RD8(0x08);
  uint8_t fs1 = adc_RD8(0x09);
  uint8_t fs2 = adc_RD8(0x0A);

  ofc = (off2 << 16 & 0x00FF0000) | off1 << 8 | off0;  // already sign-extends from 24 bits
  fsc = fs2;                                           // fsc = (fs2 << 16 & 0x00FF0000) | fs1 << 8 | fs0; idk why it breaks?? but it does
  fsc = (fsc << 8) + fs1;
  fsc = (fsc << 8) + fs0;
}

void printCallibration() {
  Serial.print(ofc);
  Serial.print(",");
  Serial.println(fsc);
}

/**
   Blocks until DRDY falls
*/
void waitforDRDY() {
  dataReady = false;
  while (!dataReady) {
    continue;
  }
}

void RDY_ISR () {
  dataReady = true;
}

void read_multi(uint32_t count) {
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_adc, LOW);  //Pull SS Low to Enable Communications with ADS1247
  while (dataReady == false) {} //Wait until DRDY does low, then issue the command
  SPI.transfer(RDATAC);
  delayMicroseconds(7); //Wait t6 time (~6.51 us) REF: P34, FIG:30.

  uint32_t i = 0;
  while (i < count) {
    waitforDRDY();
    outputBuffer[0] = SPI.transfer(0);
    outputBuffer[1] = SPI.transfer(0);
    outputBuffer[2] = SPI.transfer(0);
    //Serial.write(outputBuffer, 3);
    //Serial.write('\n');
    i++;
  }
  SPI.transfer(SDATAC);
  digitalWrite(CS_adc, HIGH);
  SPI.endTransaction();
}

int32_t read_single() {
  int32_t adc_val;
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_adc, LOW);  //Pull SS Low to Enable Communications with ADS1247
  waitforDRDY();  // Wait until DRDY is LOW
  SPI.transfer(RDATA);         // Issue read data command RDATA
  delayMicroseconds(7);

  // assemble 3 bytes into one 32 bit word
  adc_val = ((uint32_t)SPI.transfer(0) << 16) & 0x00FF0000;
  adc_val |= ((uint32_t)SPI.transfer(0) << 8);
  adc_val |= SPI.transfer(0);

  digitalWrite(CS_adc, HIGH);
  SPI.endTransaction();

  //  Extend a signed number
  if (adc_val & 0x800000) {
    adc_val |= 0xFF000000;
  }
  return adc_val;
}

void adc_WR8(byte addr, uint8_t data) {
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_adc, LOW);
  delayMicroseconds(5);
  SPI.transfer(0x50 | addr);
  SPI.transfer(0x00);
  SPI.transfer(data);
  digitalWrite(CS_adc, HIGH);
  SPI.endTransaction();
}

uint8_t adc_RD8(byte addr) {
  uint8_t bufr;
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_adc, LOW);
  SPI.transfer(0x10 | addr);  // send 1st command byte, address of the register
  SPI.transfer(0x00);         // send 2nd command byte, read only one register
  delayMicroseconds(5);
  bufr = SPI.transfer(0);  // read data of the register
  digitalWrite(CS_adc, HIGH);
  SPI.endTransaction();
  return bufr;
}

void adcSendCMD(uint8_t cmd) {
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_adc, LOW);
  SPI.transfer(cmd);
  digitalWrite(CS_adc, HIGH);
  SPI.endTransaction();
}
