#include <Arduino.h>
#include <SPI.h>

#include <ADS1256.h>
#include <DAC8552.h>
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

#define REFDACMV 3310
#define AMPOFFSET 0

#define MINMV 2000 - REFDACMV // WE-RE lowest voltage
#define MAXMV 2000            // WE-RE highest voltage

#define SAMPLECOUNT 2 // Samples during sample period

// Modes
#define SERIAL_MODE 0
#define EXP_MODE 1
#define DEB_MODE 2

// DPV Setting Sources (And Electrochemical Workstation)
// https://pineresearch.com/shop/kb/software/methods-and-techniques/voltammetric-methods/differential-pulse-voltammetry-dpv/

// DPV Settings

// All in mV
int dpvStartMV = 0;
int dpvEndMV = -350;
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
int swvStartMV = 0;
int swvVerticesMVs[4] = {0}; // up to 4 vertices
int swvEndMV = -350;
int swvIncrE = 1;
int swvAmplitude = 25;

int swvVertices = 0;

// 10ms granularity (us)
unsigned long swvFrequency = 200;     // Hz <= 200 Hz
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
int count, missed_samples;
bool sent;

ADS1256 adc(CS_adc, RDY);
DAC8552 dac(CS_dac, REFDACMV);

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
      count = missed_samples = 0;
    }
    else
    {
      state = EXP_MODE;
    }
    i = SAMPLECOUNT; // Prevents sending a blank sample
    adc.printCallibration();
    method->reset();
    break;
  // DPV mode
  case 'd':
  case 'D':
    selectDPV();
    if (debug_f)
    {
      method->printSettings();
    }
    break;
  // SWV mode
  case 's':
  case 'S':
    selectSWV();
    if (debug_f)
    {
      method->printSettings();
    }
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

  dac.init();
  dac.setB(2.0); // Bias point
  adc.init();

  // Default DPV
  selectDPV();
}

/**
   DO NOT MODIFY

   Change DPV/SWV Settings above (or in Serial)
*/
void loop()
{
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
      if (i < SAMPLECOUNT)
      {
        // TODO: Correctly output voltage
        // assemble 3 bytes into one 32 bit word
        int32_t adc_val = ((uint32_t)adc.outputBuffer[i][0] << 16) & 0x00FF0000;
        adc_val |= ((uint32_t)adc.outputBuffer[i][1] << 8);
        adc_val |= adc.outputBuffer[i][2];
        if (adc_val & 0x800000)
        {
          adc_val |= 0xFF000000;
        }
        Serial.println((adc_val >> 8) << 8);
        // Serial.write(adc.outputBuffer[i][0]);
        // Serial.write(adc.outputBuffer[i][1]);
        i++;
      }
      break;
    case SHIFTV:
      dac.setA((2000.0 - method->getVoltage()) / 1000.0 - AMPOFFSET);
      break;
    case SAMPLE:
      adc.readMulti(SAMPLECOUNT);
      i = 0;
      break;
    case DONE:
      // Send last sample if there was not time to send it
      if (i < SAMPLECOUNT)
      {
        Serial.println((((int16_t)adc.outputBuffer[i][0]) << 8) + adc.outputBuffer[i][1]);
        i++;
      }
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
      if (i < SAMPLECOUNT)
      {
        // TODO: satisfactory delay to printing without actually cluttering debug
        i++;
      }
      if (i == SAMPLECOUNT)
      {
        sent = true;
      }
      break;
      break;
    case SHIFTV:
      Serial.print(micros() - start_time);
      Serial.print(',');
      Serial.println(method->getVoltage());
      break;
    case SAMPLE:
      i = 0;
      // Count # of sent samples and timing
      Serial.print(micros() - start_time);
      Serial.print(',');
      Serial.println("Sample");
      count++;

      // Count # of missed samples because output buffer overridden
      if (!sent)
      {
        missed_samples++;
      }
      sent = false;
      break;
    case DONE:
      state = SERIAL_MODE;
      // Debug outputs
      Serial.println();
      Serial.print("# of samples: ");
      Serial.println(count);
      Serial.print("# of missed samples: ");
      Serial.println(missed_samples);
      break;
    }
  }
  break;
  }
}