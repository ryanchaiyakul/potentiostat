#include <SPI.h>
#include <ArduinoJson.h>

// SPI Speeds
#define ADC_SPEED 2000000
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

// DAC Constants
#define RefDACMV 3314        // 5v
#define AmpOffset 0          // 0.103v
#define sampleWidthUS 1000   // 1ms granularity

// JSON Constants
#define CAPACITY 96  // https://arduinojson.org/v6/assistant/

// State Machine

// Potentiometer States
#define IDLE 0
#define QUIET 1
#define ACTIVE 2
#define RELAX 3

// Modes
#define DPV 0
#define SWV 1

// DPV Setting Sources (And Electrochemical Workstation)
// https://pineresearch.com/shop/kb/software/methods-and-techniques/voltammetric-methods/differential-pulse-voltammetry-dpv/

// DPV Settings

// All in mV
int dpvStartMV = -100;
int dpvEndMV = -450;
int dpvIncrE = 5;       // Sign calculated later
int dpvAmplitude = 50;

// 10ms granularity (us)
unsigned long dpvPulseWidth = 100000;
unsigned long dpvPulsePeriod = 500000;  // Includes pulse
unsigned long dpvQuietTime = 2000000;   // Hold reference at dpvTrueStartMV for x us
unsigned long dpvRelaxTime = 2000000;   // Hold reference at dpvTrueEndMV for x us

// DPV Variables
int dpvPulseStart;
int dpvPulseLength;
int trueDpvStartMV;
int trueDpvEndMV;
int trueDpvIncrE;

bool dpvIsForward;

// SWV Setting Sources
// https://pineresearch.com/shop/kb/software/methods-and-techniques/voltammetric-methods/square-wave-voltammetry/

// SWV Settings

// All in mV
int swvStartMV = 1000;
int swvVerticesMVs[4] = { 0 }; // up to 4 vertices
int swvEndMV = 1000;
int swvIncrE = 2;
int swvAmplitude = 25;

int swvVertices = 1;

// 10ms granularity (us)
unsigned long swvFrequency = 50;      // Hz <= 500 Hz
unsigned long swvQuietTime = 2000000;  // Hold reference at swvTrueStartMV for x us
unsigned long swvRelaxTime = 2000000;  // Hold reference at swvTrueEndMV for x us

// SWV Variables
unsigned long swvPulsePeriod;
int swvPulseCenter;
int swvPulseLength;
int trueSwvStartMV;
int trueSwvEndMV;
int trueSwvIncrE;

bool swvIsForward;
bool swvFinishedVertices;
int swvCount;
int swvBreakMV;
int swvNewStartMV;
int trueSwvVerticesMVs[4];

// ADC Callibration
int32_t ofc;
uint32_t fsc;
uint8_t outputBuffer[3];

// ISR Variables
uint8_t k = 0;                    // counter to sample at 4000 Hz (30,000 SPS)
bool volatile sampleV_f = false;  // tells loop() to initiate ADS read
bool volatile updateV_f = false;  // tells loop() to check if idx has changed

// Loop variables
int8_t mode;
unsigned long ini_time;  // start of experiment
unsigned long prev_idx;            // Compared with idx to call onUpdate
int8_t state = IDLE;
char serialValue[126];

unsigned long quietTime;    // set to quietTime for respective mode
unsigned long relaxTime;    // set to relaxTime for respective mode
int startMV;                // quietTime mV
int endMV;                  // relaxTime mV
void (*customReset)(void);  // set to reset method for respective mode
void (*onUpdate)(void);     // function called when idx changes

// onUpdate variables

unsigned long idx;              // the # of 1ms intervals since start (ONLY READ IN onUpdate)
int baseMV;           // static variable to avoid creating a variable every cycle
int outputMV;         // set to WE-RE when setV_f is switched to true
bool setV_f = false;  // set to true when outputMV changes
bool end_f = false;   // set to true when experiment ends

// Debug
bool debug_f = false;
unsigned long start_time;

// JSON
StaticJsonDocument<CAPACITY> settings;
JsonArray verticesV;

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
  selectDPV();
}

/**
   DO NOT MODIFY

   Change DPV/SWV Settings above (or in Serial)
*/
void loop() {
  switch (state) {
    case IDLE:
      if (Serial.available()) {
        serialValue[Serial.readBytesUntil('\n', serialValue, 200)] = '\0';
        if (serialValue[1] == '\0') {
          // Single character corrresponds with a command
          switch (serialValue[0]) {
            // Begin experiment
            case 'b':
            case 'B':
              state = QUIET;
              start_time = micros();
              printCallibration();
              reset(startMV);
              break;
            // DPV mode
            case 'd':
            case 'D':
              Serial.println("DPV Selected");
              printDPV();
              selectDPV();
              break;
            // SWV mode
            case 's':
            case 'S':
              Serial.println("SWV Selected");
              printSWV();
              selectSWV();
              break;
            // CallibrateADS
            case 'c':
            case 'C':
              Serial.println("Callibrated ADS");
              callibrateADS();
              break;
            // Print JSON Format
            case 'h':
            case 'H':
              Serial.println("{ \"method\": \"d\", \"startV\": 0.0, \"endV\": 0.0, \"incrE\": 0.0, \"amplitude\": 0.0, \"pulseWidth\": 0.0, \"pulsePeriod\": 0.0 }");
              Serial.println("{ \"method\": \"s\", \"startV\": 0.0, \"verticesV\": [0.0, 0.0], \"endV\": 0.0, \"incrE\": 0.0, \"amplitude\": 0.0, \"frequency\": 0 }");
              break;
            default:
              break;
          }
        } else {
          /**
            Setting Formats:

            Newline sensitive
            Units are in volts, seconds, and Hz

            DPV:
            { "method": "d", "startV": 0.0, "endV": 0.0, "incrE": 0.0, "amplitude": 0.0, "pulseWidth": 0.0, "pulsePeriod": 0.0 }

            SWV:
            { "method": "s", "startV": 0.0, "verticesV": [0.0, 0.0], "endV": 0.0, "incrE": 0.0, "amplitude": 0.0, "frequency": 0 }

            Max 4 vertices
          */
          DeserializationError error = deserializeJson(settings, serialValue);
          if (error) {
            Serial.println(error.f_str());
            return;
          }
          switch (*settings["method"].as<const char*>()) {
            case 'd':
            case 'D':
              dpvStartMV = vToMV(settings["startV"].as<float>());
              dpvEndMV = vToMV(settings["endV"].as<float>());
              dpvIncrE = vToMV(settings["incrE"].as<float>());
              dpvAmplitude = vToMV(settings["amplitude"].as<float>());
              dpvPulseWidth = sToUS(settings["pulseWidth"].as<float>());
              dpvPulsePeriod = sToUS(settings["pulsePeriod"].as<float>());
              if (mode == DPV) {
                selectDPV();
              } else {
                loadDPV();
              }
              printDPV();
              break;
            case 's':
            case 'S':
              swvStartMV = vToMV(settings["startV"].as<float>());
              verticesV = settings["verticesV"].as<JsonArray>();
              swvVertices = 0;
              for (JsonVariant v : verticesV) {
                swvVerticesMVs[swvVertices++] = vToMV(v.as<float>());
              }
              swvEndMV = vToMV(settings["endV"].as<float>());
              swvIncrE = vToMV(settings["incrE"].as<float>());
              swvAmplitude = vToMV(settings["amplitude"].as<float>());
              swvFrequency = settings["frequency"].as<int>();
              if (mode == SWV) {
                selectSWV();
              } else {
                loadSWV();
              }
              printSWV();
              break;
            default:
              break;
          }
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
      // Only try to update at 4 kHz (to give time for other stuff)
      if (updateV_f == true) {
        idx = (micros() - ini_time) / sampleWidthUS;  // segments time into 1ms intervals
        if (prev_idx != idx) {
          onUpdate();
          prev_idx = idx;
        }
      }

      // IF setV_f is true, set voltage (channel A) so WE-RE=outputMV
      if (setV_f == true) {
        setVoltage_A((2000.0 - outputMV) / 1000.0 - AmpOffset);
        setV_f = false;
      }
      // IF sampleV_f is true, read from ADC and send on Serial.print()
      if (sampleV_f == true) {
        int32_t results = (read_single() >> 8) << 8;  // Clear last 9 bits b/c noisy
        Serial.println(results);
        sampleV_f = false;
      }

      // Experiments ends naturally or send anything to escape
      if (end_f == true || Serial.available()) {
        reset(endMV);
        state = RELAX;
      }
      break;
    case RELAX:
      if (micros() - ini_time > relaxTime) {
        state = IDLE;
      }
      break;
    default:
      break;
  }
}

// Conversions (for Serial JSON)
int vToMV(float voltage) {
  return round(voltage * 1000);
}

unsigned long sToUS(float seconds) {
  return round(seconds * 1000000);
}

// reset flags and set channel A to resting voltage
void reset(int voltage) {
  //Serial.println("~");  // Start/End of experiment
  // Reset flags
  state = QUIET;
  setV_f = false;
  sampleV_f = false;
  end_f = false;
  prev_idx = 0;
  idx = 0;
  customReset();
  // Initial conditions
  setVoltage_A((2000.0 - voltage) / 1000.0 - AmpOffset);
  ini_time = micros();
}

// DPV

void printDPV() {
  Serial.print("startMV: ");
  Serial.println(dpvStartMV);
  Serial.print("endMV: ");
  Serial.println(dpvEndMV);
  Serial.print("incrE: ");
  Serial.println(dpvIncrE);
  Serial.print("amplitude: ");
  Serial.println(dpvAmplitude);
  Serial.print("pulseWidth: ");
  Serial.println(dpvPulseWidth);
  Serial.print("pulsePeriod: ");
  Serial.println(dpvPulsePeriod);
}

void loadDPV() {
  dpvPulseStart = (dpvPulsePeriod - dpvPulseWidth) / sampleWidthUS;
  dpvPulseLength = dpvPulsePeriod / sampleWidthUS;

  trueDpvStartMV = dpvStartMV - dpvAmplitude;
  trueDpvEndMV = dpvEndMV - dpvAmplitude;

  trueDpvIncrE = dpvIncrE < 0 ? -dpvIncrE : dpvIncrE;
  if (trueDpvEndMV < trueDpvStartMV) {
    trueDpvIncrE *= -1;
    dpvIsForward = false;
  } else {
    dpvIsForward = true;
  }
}

void selectDPV() {
  loadDPV();

  mode = DPV;
  startMV = trueDpvStartMV;
  endMV = trueDpvEndMV;
  quietTime = dpvQuietTime;
  relaxTime = dpvRelaxTime;
  onUpdate = updateDPV;
  // DPV varaibles do not change so customReset is blank
  customReset = [] {};
}

// SWV

void printSWV() {
  Serial.print("startMV: ");
  Serial.println(swvStartMV);
  Serial.print("verticesMV: ");
  for (int i = 0; i < swvVertices - 1; i++) {
    Serial.print(swvVerticesMVs[i]);
    Serial.print(", ");
  }
  Serial.println(swvVerticesMVs[swvVertices - 1]);
  Serial.print("endMV: ");
  Serial.println(swvEndMV);
  Serial.print("incrE: ");
  Serial.println(swvIncrE);
  Serial.print("amplitude: ");
  Serial.println(swvAmplitude);
  Serial.print("frequency: ");
  Serial.println(swvFrequency);
}

void loadSWV() {
  swvPulsePeriod = (1.0 / swvFrequency) * 1000000;
  swvPulseCenter = (swvPulsePeriod / 2) / sampleWidthUS;
  swvPulseLength = swvPulsePeriod / sampleWidthUS;

  trueSwvStartMV = swvStartMV - swvAmplitude;
  for (int i = 0; i < swvVertices; i++) {
    trueSwvVerticesMVs[i] = swvVerticesMVs[i] - swvAmplitude;
  }
  trueSwvEndMV = swvEndMV - swvAmplitude;
  trueSwvIncrE = swvIncrE < 0 ? -swvIncrE : swvIncrE;
  resetSWV();
}

void selectSWV() {
  loadSWV();

  mode = SWV;
  startMV = trueSwvStartMV;
  endMV = trueSwvEndMV;
  quietTime = swvQuietTime;
  relaxTime = swvRelaxTime;
  onUpdate = updateSWV;
  customReset = resetSWV;
}

void resetSWV() {
  if (trueSwvStartMV < trueSwvVerticesMVs[0]) {
    swvIsForward = true;
  } else {
    swvIsForward = false;
  }
  swvFinishedVertices = false;
  swvBreakMV = trueSwvVerticesMVs[0];
  swvNewStartMV = trueSwvStartMV;
  swvCount = 0;
}

/**
   onUpdate

   Avaliable variables:

   idx = # of 1ms intervals since start

   Outputs:

   setV_f = update to outputMV
   endV_f = signifies end of experiment
   outputMV = WE-RE in mV
*/

void updateDPV() {
  baseMV = trueDpvStartMV + trueDpvIncrE * (idx / dpvPulseLength);  // (idx / dpvPulseLength) = current pulse #

  // "Low" from [0, dpvPulseStart); "High" from [dpvPulseStart, dpvPulseLength)
  int remainder = idx % dpvPulseLength;
  if (remainder == dpvPulseStart) {
    outputMV = baseMV + dpvAmplitude;
    setV_f = true;
  } else if (remainder == 0) {
    outputMV = baseMV;
    setV_f = true;
  }

  if ((dpvIsForward && baseMV > trueDpvEndMV) || (!dpvIsForward && baseMV < trueDpvEndMV)) {
    outputMV = trueDpvEndMV;
    end_f = true;
  }
}

void updateSWV() {
  baseMV = swvNewStartMV;
  // ini_time is reset once the vertex is reached so no need to account for that
  if (swvIsForward) {
    baseMV += trueSwvIncrE * (idx / swvPulseLength);
  } else {
    baseMV -= trueSwvIncrE * (idx / swvPulseLength);
  }

  // 50% duty cycle
  int remainder = idx % swvPulseLength;
  if (remainder == swvPulseCenter) {
    outputMV = baseMV - swvAmplitude;
    setV_f = true;
  } else if (remainder == 0) {
    outputMV = baseMV + swvAmplitude;
    setV_f = true;
  }

  // Special case for last stretch (do the last peak)
  if (swvFinishedVertices) {
    if ((swvIsForward && baseMV > trueSwvEndMV) || (!swvIsForward && baseMV < trueSwvEndMV)) {
      outputMV = trueSwvEndMV;
      end_f = true;
    }
  } else {
    // Only do one peak at the end (>= instead of >)
    if ((swvIsForward && baseMV >= swvBreakMV) || (!swvIsForward && baseMV <= swvBreakMV)) {
      swvIsForward = !swvIsForward;
      swvNewStartMV = swvBreakMV;
      if (++swvCount == swvVertices) {
        swvFinishedVertices = true;
      } else {
        swvBreakMV = trueSwvVerticesMVs[swvCount];
      }
      ini_time = micros();
    }
  }
}

// ISR

void timer_init() {
  // TIMER 1 for interrupt frequency 4 kHz:
  cli();       // stop interrupts
  TCCR1A = 0;  // set entire TCCR1A register to 0
  TCCR1B = 0;  // same for TCCR1B
  TCNT1 = 0;   // initialize counter value to 0
  // set compare match register for 4 kHz ments
  OCR1A = 3999;  // = 16000000 / (1 * 4000) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS12, CS11 and CS10 bits for 1 prescaler
  TCCR1B |= (0 << CS12) | (0 << CS11) | (1 << CS10);
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei();  // allow interrupts
}

ISR(TIMER1_COMPA_vect) {  // interrupt service routine at 4 kHz
  updateV_f = true;
  if (k == 0) {
    sampleV_f = true;
  }
  k = (k + 1) % 2;
}

// DAC

void setVoltage_A(float voltage) {
  uint16_t data = Voltage_Convert(voltage);
  DAC_WR(WR_LOAD_A, data);
  //sendDACCSV(voltage);
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
  adcSendCMD(RESET);  //RESET Command
  delay(100);         //let the system power up and stabilize (datasheet pg 24)

  // ADS Settings
  adc_WR8(0x01, 0x01);  // AIN0-AIN1
  adc_WR8(0x00, 0x32);  // Enable Buffer
  adc_WR8(0x02, 0x02);  // PGA=4
  adc_WR8(0x03, 0xF0);  // DRATE=30,000SPS
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
  while(digitalRead(RDY)) {
    continue;
  }
}

void read_multi(uint32_t count) {
  SPI.beginTransaction(SPISettings(ADC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_adc, LOW);  //Pull SS Low to Enable Communications with ADS1247
  waitforDRDY(); //Wait until DRDY does low, then issue the command
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


// DEBUGGING


void sendDACCSV(float voltage) {
  Serial.print(micros() - start_time);
  Serial.print(",");
  Serial.println(voltage, 4);
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
