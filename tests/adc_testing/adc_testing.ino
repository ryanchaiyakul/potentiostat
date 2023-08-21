#include <SPI.h>
#define ADC_SPEED 1000000

// ADC Commands
#define RESET 0xFE
#define CALLIBRATE 0xF0
#define READ 0x10

// Pinout
#define RDY 3
#define CS_adc 8
#define CS_dac 10
#define DIN 11   // MISO
#define DOUT 12  // MOSI
#define SCLK 13  // SCLK

int32_t ofc;
uint32_t fsc;

bool sampleV_f;
bool read_f;

void setup() {
  delay(50);
  // put your setup code here, to run once:
  Serial.begin(115200);
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

  initADS();
  timer_init();
  printCallibration();
}

void loop() {
  if (sampleV_f) {
    adcSendCMD(READ);
    sampleV_f = false;
    if (read_f) {
      Serial.println("f");
    }
    read_f = true;
  }

  if (read_f && !digitalRead(RDY)) {
    int16_t result = read_Value() >> 8 << 8;
    Serial.println(result);
    read_f = false;
  }
}

void timer_init() {
  // TIMER 1 for interrupt frequency 29962.5468164794 kHz:
  cli();       // stop interrupts
  TCCR1A = 0;  // set entire TCCR1A register to 0
  TCCR1B = 0;  // same for TCCR1B
  TCNT1 = 0;   // initialize counter value to 0
  // set compare match register for 29962.5468164794 kHz ments
  OCR1A = 533;  // = 16000000 / (1 * 29962.5468164794) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12, CS11 and CS10 bits for 1 prescaler
  TCCR1B |= (0 << CS12) | (0 << CS11) | (1 << CS10);
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei();  // allow interrupts
}

ISR(TIMER1_COMPA_vect) {  // interrupt service routine at 29962.5468164794 kHz
  sampleV_f = true;
}

// ADC

void initADS() {
  delayMicroseconds(10);
  adcSendCMD(RESET);  //RESET Command
  delay(100);         //let the system power up and stabilize (datasheet pg 24)

  // ADS Settings
  adc_WR8(0x01, 0x01);  // AIN0-AIN1
  adc_WR8(0x00, 0x32);  // Enable Buffer
  adc_WR8(0x02, 0x03);  // PGA=8
  //adc_WR8(0x02, 0x04);  // PGA=16
  adc_WR8(0x03, 0xC0);  // DRATE=3,750SPS
  delay(100);           // settling time: 3750SPS needs 0.44ms

  callibrateADS();
}

void callibrateADS() {
  adcSendCMD(CALLIBRATE);  //send the calibration command
  delay(10);

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

// BLOCKING, ONLY USE FOR CALLIBRATION
void waitforDRDY() {
  while (digitalRead(RDY)) {
    continue;
  }
}

void adc_WR8(byte addr, uint8_t data) {
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_adc, LOW);
  SPI.transfer(addr + 0x50);
  SPI.transfer(0x00);
  SPI.transfer(data);
  digitalWrite(CS_adc, HIGH);
  SPI.endTransaction();
}

// Must send READ cmd earlier and wait for DRDY
int32_t read_Value() {
  int32_t adc_val;
  //adcSendCMD(READ);
  //waitforDRDY();  // Wait until DRDY is LOW
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_adc, LOW);  //Pull SS Low to Enable Communications with ADS1247
  SPI.transfer(0x01);         // Issue read data command RDATA
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

uint8_t adc_RD8(byte addr) {
  uint8_t bufr;
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_adc, LOW);
  SPI.transfer(0x10 + addr);  // send 1st command byte, address of the register
  SPI.transfer(0x00);         // send 2nd command byte, read only one register
  delayMicroseconds(10);
  bufr = SPI.transfer(0);  // read data of the register
  delayMicroseconds(2);
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
