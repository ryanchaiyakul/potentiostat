#include  <SPI.h>

#define DAC_SPEED 16000000
#define LOAD_BUFF_A         0x00
#define LOAD_BUFF_B         0x04
#define WR_LOAD_A           0x10
#define WR_LOAD_B           0x24

const int DIN = 11;
const int CS_dac = 10;
const int SCLK = 13;
const int DOUT = 12;

float RefDACMV = 3314;    // 5v

bool toggle = false;
unsigned long ini_time;

void setup() {
  delay(50);
  Serial.begin(115200);
  // For USB Serial
  while (!Serial) {
    ;
  }

  pinMode(DIN, OUTPUT);    // MOSI
  pinMode(CS_dac, OUTPUT);   // CS
  digitalWrite(CS_dac, HIGH);
  pinMode(SCLK, OUTPUT);   // CLK
  pinMode(DOUT, INPUT); // MISO

  SPI.begin();
  setVoltage_B(2.0);
  ini_time = micros();
}

void loop() {
  if (toggle) {
    setVoltage_A(1.0);
  } else {
    setVoltage_A(2.0);
  }
  toggle = !toggle;
  delay(500);
}

void Monitor_Voltage(float voltage) {
  Serial.print(micros() - ini_time);
  Serial.print(",");
  Serial.println(voltage);
}

void setVoltage_A(float voltage){
  uint16_t data = Voltage_Convert(voltage);
  DAC_WR(WR_LOAD_A, data);
  Monitor_Voltage(voltage);
}

void setVoltage_B(float voltage){
  uint16_t data = Voltage_Convert(voltage);
  DAC_WR(WR_LOAD_B, data);
}

void DAC_Readable_Debug(byte command, uint16_t data) {
  Serial.print("command: ");
  for (int i = 0; i < 8; i++) {
    Serial.print(bitRead(command, 7-i));
  }
  Serial.print(", data: ");
  Serial.print(data);
  Serial.print(", voltage: ");
  Serial.println(((((float) data) / 65536)  * RefDACMV) / 1000);
}
  
void DAC_WR(byte command, uint16_t data){
  byte dataHigh = data >> 8; //high 8 bits
  byte dataLow  = data & 0xff; //low 8 bits
  SPI.beginTransaction(SPISettings(DAC_SPEED, MSBFIRST, SPI_MODE1));
  digitalWrite(CS_dac, LOW);
  SPI.transfer(command);
  SPI.transfer(dataHigh);
  SPI.transfer(dataLow);
  digitalWrite(CS_dac, HIGH);
  SPI.endTransaction();
  }

/**
 * Voltage is in millivolts
*/
uint16_t Voltage_Convert(float voltage) {
  if((voltage*1000) > RefDACMV) return 0;
  return (unsigned int)(65536 * (voltage*1000 / (RefDACMV)));
}
