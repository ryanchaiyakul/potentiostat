#include <SPI.h>

// SPI Speeds
#define ADC_SPEED 1000000
#define DAC_SPEED 16000000

// Baud Rate
#define BAUD_RATE 115200  // 115200 or 230400 for DPV

// DAC Commands
#define LOAD_BUFF_A 0x00
#define LOAD_BUFF_B 0x04
#define WR_LOAD_A 0x10
#define WR_LOAD_B 0x24

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

// DAC Constants
#define RefDACMV 3301        // 5v
#define AmpOffset 0          // 0.103v
#define sampleWidthUS 10000  // 10ms granularity

// DPV Setting Sources (And Electrochemical Workstation)
// https://pineresearch.com/shop/kb/software/methods-and-techniques/voltammetric-methods/differential-pulse-voltammetry-dpv/

// DPV Settings

// All in mV
int startMV = -100;  // 100mv
int endMV = -450;    // 600mv
int incrE = -5;
int amptitude = 50;

// 10ms granularity (us)
unsigned long pulseWidth = 100000;
unsigned long pulsePeriod = 500000;  // Includes pulse
unsigned long quietTime = 2000000;   // Hold reference at startMV for x us

int cycles = 1;  // number of cycles (cycles separated by "~\n")

// DPV Variables
int pulseStart = (pulsePeriod - pulseWidth) / sampleWidthUS;
int pulseLength = pulsePeriod / sampleWidthUS;

int trueStartMV = startMV - amptitude;  // Offset as measuring by peaks
int trueEndMV = endMV - amptitude;

// ADC Callibration
int32_t ofc;
uint32_t fsc;

// ISR Variables
unsigned long ini_time;  // start of experiment
int idx;                 // the # of 10ms intervals since start
uint8_t k = 0;           // counter to sample every 3 ISR calls

// Outputs
int outputMV;  // set to WE-RE when setV_f is switched to true

// Trigger Flags
bool setV_f = false;
bool sampleV_f = false;
bool end_f = false;

// State Machine
enum states {
  IDLE,
  QUIET,
  ACTIVE,
} state = IDLE;

void (*funcISR)(void);

// DPV Variables
int count;

void setup() {
  Serial.begin(BAUD_RATE);

  // pin setup
  pinMode(SCLK, OUTPUT);  // CLK
  pinMode(DOUT, INPUT);   // MISO
  pinMode(DIN, OUTPUT);   // MOSI

  pinMode(RDY, INPUT);

  pinMode(CS_dac, OUTPUT);
  digitalWrite(CS_dac, HIGH);  // Active low
  pinMode(CS_adc, OUTPUT);
  digitalWrite(CS_adc, HIGH);  // Active low

  SPI.begin();

  setVoltage_B(2.0);  //Set voltage (channel B) == 2.0V
  initADS();

  // Get initial time
  timer_init();  // Init Timer to 3000Hz

  // Default DPV
  funcISR = updateDPV;
}

/**
   DO NOT MODIFY

   Change DPV Settings above (or in Serial)
*/
void loop() {
  switch (state) {
    case IDLE:
      if (Serial.available()) {
        String serialValue = Serial.readString();
        if (serialValue.length() == 2) {
          switch (serialValue.charAt(0)) {
              case 's':
              case 'S':
                state = QUIET;
                count = cycles;

                // Print callibration before experiment
                Serial.print(ofc);
                Serial.print(",");
                Serial.println(fsc);
                reset();
                break;
            }
        } else {

        }
      }
      break;
    case QUIET:
      if (micros() - ini_time > quietTime) {
        state = ACTIVE;
        ini_time += quietTime;  // offset ini_time for ISR
      }
      break;
    case ACTIVE:
      // IF setV_f is true, set voltage (channel A) so WE-RE=outputMV
      if (setV_f == true) {
        setVoltage_A((2000.0 - outputMV) / 1000.0 - AmpOffset);
        setV_f = false;
      }

      // IF sampleV_f is true, read from ADC and send on Serial.print()
      if (sampleV_f == true) {
        int32_t results = (read_Value() >> 8) << 8;
        Serial.println(results);  // Clear last 8 bits b/c noisy
        sampleV_f = false;
      }
      if (end_f == true) {
        reset();
        --count <= 0 ? state = IDLE : Serial.println("~");  // Continue to QUIET if still cycles remaining
      }
      break;
  }
}

void reset() {
  state = QUIET;
  setV_f = false;
  sampleV_f = false;
  end_f = false;
  setVoltage_A((2000.0 - startMV) / 1000.0 - AmpOffset);
  ini_time = micros();
}

/**
 * funcISR
 * 
 * Called at 3000 Hz
 *
 * Avaliable variables:
 *
 * idx = # of 10ms intervals since start
 *
 * Outputs:
 *
 * setV_f = update to outputMV
 * endV_f = signifies end of experiment
 * outputMV = WE-RE in mV
 */

void updateDPV() {
  int baseMV = trueStartMV + incrE * (idx / pulseLength);  // (idx / pulseLength) = current pulse #

  // "Low" from [0, pulseStart); "High" from [pulseStart, pulseLength)
  if (idx % pulseLength == pulseStart) {
    outputMV = baseMV + amptitude;
    setV_f = true;
  } else if (idx % pulseLength == 0) {
    outputMV = baseMV;
    setV_f = true;
  }

  if (baseMV <= trueEndMV) {
    outputMV = endMV;
    end_f = true;
  }
}

// ISR

void timer_init() {
  // TIMER 1 for interrupt frequency 3000.1875117194822 Hz:
  cli();       // stop interrupts
  TCCR1A = 0;  // set entire TCCR1A register to 0
  TCCR1B = 0;  // same for TCCR1B
  TCNT1 = 0;   // initialize counter value to 0
  // set compare match register for 3000.1875117194822 Hz ments
  OCR1A = 5332;  // = 16000000 / (1 * 3000.1875117194822) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12, CS11 and CS10 bits for 1 prescaler
  TCCR1B |= (0 << CS12) | (0 << CS11) | (1 << CS10);
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei();  // allow interrupts
}

ISR(TIMER1_COMPA_vect) {  // interrupt service routine
  unsigned long get_time = micros() - ini_time;
  idx = get_time / sampleWidthUS;  // segments time into 10ms intervals
  if (state == ACTIVE) {
    funcISR();
    // Sample Every 3rd Call (1000.0625039065 Hz)
    if (k == 0) {
      sampleV_f = true;
    }
    k = (k + 1) % 3;
  }
}

// DAC

void setVoltage_A(float voltage) {
  uint16_t data = Voltage_Convert(voltage);
  DAC_WR(WR_LOAD_A, data);
}

void setVoltage_B(float voltage) {
  uint16_t data = Voltage_Convert(voltage);
  DAC_WR(WR_LOAD_B, data);
}

void DAC_WR(byte command, uint16_t data) {
  byte dataHigh = data >> 8;   //high 8 bits
  byte dataLow = data & 0xff;  //low 8 bits
  SPI.beginTransaction(SPISettings(DAC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_dac, LOW);
  SPI.transfer(command);
  SPI.transfer(dataHigh);
  SPI.transfer(dataLow);
  digitalWrite(CS_dac, HIGH);
  SPI.endTransaction();
}

uint16_t Voltage_Convert(float voltage) {
  if ((voltage * 1000) > RefDACMV) return 0;
  return (unsigned int)(65536 * (voltage * 1000 / (RefDACMV)));
}

// ADC

void initADS() {
  delayMicroseconds(10);
  adcSendCMD(RESET);  //RESET Command
  delay(100);         //let the system power up and stabilize (datasheet pg 24)

  // ADS Settings
  adc_WR8(0x01, 0x01);  // AIN0-AIN1
  adc_WR8(0x00, 0x32);  // Enable Buffer
  adc_WR8(0x02, 0x04);  // PGA=16
  adc_WR8(0x03, 0xC0);  // DRATE=3,750SPS
  delay(100);           // settling time: 3750SPS needs 0.44ms

  adcSendCMD(CALLIBRATE);  //send the calibration command
  delay(10);

  waitforDRDY();  // Wait until DRDY is LOW => calibration complete
  uint8_t off0 = adc_RD8(0x05);
  uint8_t off1 = adc_RD8(0x06);
  uint8_t off2 = adc_RD8(0x07);
  uint8_t fs0 = adc_RD8(0x08);
  uint8_t fs1 = adc_RD8(0x09);
  uint8_t fs2 = adc_RD8(0x0A);

  int32_t ofc = (off2 << 16 & 0x00FF0000) | off1 << 8 | off0;  // already sign-extends from 24 bits
  uint32_t fsc = fs2;                                          // fsc = (fs2 << 16 & 0x00FF0000) | fs1 << 8 | fs0; idk why it breaks?? but it does
  fsc = (fsc << 8) + fs1;
  fsc = (fsc << 8) + fs0;
}

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

int32_t read_Value() {
  int32_t adc_val;
  adcSendCMD(READ);
  waitforDRDY();  // Wait until DRDY is LOW
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



// DEBUGGING

/**
  void sendDACCSV(float voltage) {
  Serial.print(micros() - start_time);
  Serial.print(",");
  Serial.println(voltage, 4);
  }
*/

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
