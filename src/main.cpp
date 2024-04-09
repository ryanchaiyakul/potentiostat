#include <Arduino.h>
#include <SPI.h>

#include <ADS1256.h>
#include <DAC8552.h>
#include <DAC80502.h>
#include <DPV.h>
#include <SWV.h>

// Baud Rate
#define BAUD_RATE 230400 // 115200 or 230400 for DPV

// Pinout
#define RDY 3
#define CS_adc 8
#define CS_dac 10
#define DIN 11  // MISO
#define DOUT 12 // MOSI
#define SCLK 13 // SCLK

#define REFDACMV 2498
#define AMPOFFSET 0

#define MINMV 2000 - REFDACMV // WE-RE lowest voltage
#define MAXMV 2000            // WE-RE highest voltage
#define WEMV 1000

#define KERNELSIZE 8 // Samples during sample period

// Modes
#define SERIAL_MODE 0
#define EXP_MODE 1
#define DEB_MODE 2

// DPV Setting Sources (And Electrochemical Workstation)
// https://pineresearch.com/shop/kb/software/methods-and-techniques/voltammetric-methods/differential-pulse-voltammetry-dpv/

// DPV Settings

// All in mV
int dpvStartMV = 100;
int dpvEndMV = -250;
int dpvIncrE = 5; // Sign calculated later
int dpvAmplitude = 25;

// 10ms granularity (us)
unsigned long dpvPulseWidth = 100000;
unsigned long dpvPulsePeriod = 500000; // Includes pulse
unsigned long dpvQuietTime = 2000000;  // Hold reference at dpvTrueStartMV for x us
unsigned long dpvRelaxTime = 2000000;  // Hold reference at dpvTrueEndMV for x us

unsigned long dpvSamplingOffsetUS = 1670;

// SWV Setting Sources
// https://pineresearch.com/shop/kb/software/methods-and-techniques/voltammetric-methods/square-wave-voltammetry/

// SWV Settings

// All in mV
int swvStartMV = -100;
int swvVerticesMVs[4] = {0}; // up to 4 vertices
int swvEndMV = -450;
int swvIncrE = 1;
int swvAmplitude = 25;

int swvVertices = 0;

// 10ms granularity (us)
unsigned long swvFrequency = 60;     // Hz <= 200 Hz
unsigned long swvQuietTime = 2000000; // Hold reference at swvTrueStartMV for x us
unsigned long swvRelaxTime = 2000000; // Hold reference at swvTrueEndMV for x us
unsigned long swvSamplingOffsetUS = 1000;

// Loop variables
int8_t state = 0;
Voltammetry *method;
UpdateStatus status;
int i; // tracks which sample has been sent

// Debug
bool debug_f = false;
unsigned long start_time;
unsigned count;

ADS1256 adc(CS_adc, RDY);
//DAC80502 dac(CS_dac, REFDACMV);

DPV dpv(dpvStartMV,
        dpvEndMV,
        dpvIncrE,
        dpvAmplitude,
        dpvPulseWidth,
        dpvPulsePeriod,
        dpvSamplingOffsetUS,
        dpvQuietTime,
        dpvRelaxTime,
        MINMV,
        MAXMV);

SWV swv(swvStartMV,
        swvEndMV,
        swvVerticesMVs,
        swvVertices,
        swvIncrE,
        swvAmplitude,
        swvFrequency,
        swvSamplingOffsetUS,
        swvQuietTime,
        swvRelaxTime,
        MINMV,
        MAXMV);

void selectDPV()
{
  method = &dpv;
}

void selectSWV()
{
  method = &swv;
}

void serialCMD(byte cmd)
{
  // Single character corrresponds with a command
  switch (cmd)
  {
  // Begin experiment
  case 'b':
  case 'B':
    if (debug_f)
    {
      state = DEB_MODE;
      start_time = micros();
    }
    else
    {
      state = EXP_MODE;
      adc.printCallibration();
    }
    i = 0;
    method->reset();
    break;
  // DPV mode
  case 'd':
  case 'D':
    selectDPV();
    break;
  // SWV mode
  case 's':
  case 'S':
    selectSWV();
    break;
  // CallibrateADS
  case 'c':
  case 'C':
    adc.callibrate();
    break;
  case 'v':
  case 'V':
    debug_f = !debug_f;
    break;
  default:
    break;
  }
}

void setup()
{
  Serial.begin(BAUD_RATE);
  pinMode(CS_dac, OUTPUT);
  //dac.init();
  // TODO: Add synchronized setB/setA to prevent the cell from excessive voltage at startup
  //dac.setB(WEMV / 1000.0); // Bias point
  //dac.setA(WEMV / 1000.0);
  adc.init();

  SPI.begin();

  // Default DPV
  selectDPV();
}

void potentiostatMain() {
  switch (state)
  {
  case SERIAL_MODE:
    if (Serial.available())
    {
      serialCMD(Serial.read());
    }
    break;
  case EXP_MODE:
    status = method->update();
    switch (status)
    {
    case NONE:
      // TODO: Implement working burst mode
      break;
    case SHIFTV:
      //Serial.println((WEMV - method->getVoltage()) / 1000.0 - AMPOFFSET);
      //dac.setA((WEMV - method->getVoltage()) / 1000.0 - AMPOFFSET);
      break;
    case SAMPLE:
      while (i++ < KERNELSIZE) {
        Serial.println(adc.readSingle() & ((int32_t) 0xFFFFFF00));
      }
      i = 0;
      break;
    case DONE:
      // Send last sample if there was not time to send it
      state = SERIAL_MODE;
      break;
    }
    break;
  case DEB_MODE:
  {
    // Does not actuate DAC or ADC, just reports what should happen when
    status = method->update();
    switch (status)
    {
    case NONE:
      break;
    case SHIFTV:
      Serial.print(micros() - start_time);
      Serial.print(',');
      Serial.println(method->getVoltage());
      break;
    case SAMPLE:
      // Count # of sent samples and timing
      Serial.print(micros() - start_time);
      Serial.print(',');
      Serial.println("Sample");
      count++;
      break;
    case DONE:
      state = SERIAL_MODE;
      break;
    }
  }
  break;
  }
}

void test() {
  Serial.println("1.0");
  //dac.setA(1.0);
  //dac.setB(1.4);
  delay(1000);
  Serial.println("1.4");
  //dac.setA(1.4);
  //dac.setB(1.0);
  delay(1000);
}

void test_manual(uint8_t value) {
  SPI.beginTransaction(SPISettings(200000, MSBFIRST, SPI_MODE2));
  digitalWrite(CS_dac, LOW);
  delayMicroseconds(5);
  SPI.transfer(0x08);
  SPI.transfer(value); //0x7FFF
  SPI.transfer(0xFF);
  digitalWrite(CS_dac, HIGH);
  SPI.endTransaction();
}

/**
   DO NOT MODIFY

   Change DPV/SWV Settings above (or in Serial)
*/
void loop()
{
  test_manual(0x7F);
  //test();
  //dac.setA(1.0);
  //dac.setB(1.0);
  //potentiostatMain();
}

