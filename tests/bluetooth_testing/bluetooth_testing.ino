#include <SoftwareSerial.h>

#define CMD_DATA_REQ 0x04
#define CMD_DATA_CNF 0x44
#define CMD_DATA_IND 0x84
#define CMD_TXCOMPMLETE_RSP 0xC4

#define RESET 7

const byte rxPin = 5;
const byte txPin = 6;

unsigned long last_time;

char raw[126];
char output[121];

// Set up a new SoftwareSerial object
SoftwareSerial mySerial(rxPin, txPin);

void setup() {
  // put your setup code here, to run once:
  mySerial.begin(115200);
  Serial.begin(115200);
  pinMode(RESET, OUTPUT);

  // Enable bluetooth
  digitalWrite(RESET, LOW);
  delay(5);  // At least 1 ms
  digitalWrite(RESET, HIGH);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (Serial.available()) {
    raw[Serial.readBytes(raw, 126) - 1] = '\0';
    sendMessage(raw);
  }

  if (mySerial.available()) {
    uint16_t len = mySerial.readBytes(raw, 126) - 1;
    raw[len] = '\0';
    switch (recieveBT(raw, len)) {
      case 0:
        Serial.println(output);
        break;
      case 1:
        Serial.println("Unknown start byte");
      case 2:
        Serial.println("Corrupted message");
        Serial.println(output);
        break;
      case 3:
        Serial.println("Unknown command");
      default:
        break;
    }
  }

  /**
  if (millis() - last_time > 1000) {
    //sendABCD();
    sendBT(CMD_DATA_REQ, 4, "ABCD");
    last_time = millis();
  }
  */
}

/**
 Return codes:
 0: validated message recieved (and updates output)
 1: unknown start byte
 2: cs failed
 3: unknown message
 */
uint8_t recieveBT(char* data, uint16_t len) {
  if (data[0] != 0x02) {
    return 1;
  }
  byte cs = 0x02;
  uint8_t cmd = data[1];
  cs ^= cmd;

  for (uint16_t i = 2; i < len - 1; i++) {
    cs ^= data[i];
  }
  if (cs != data[len - 1]) {
    return 2;
  }

  switch (cmd) {
    case CMD_DATA_IND:
      Serial.println("recieved message");
      uint16_t msg_len = (data[3] << 8) | data[2];  // LSB
      for (uint16_t i = 0; i < msg_len; i++) {
        output[i] = data[i + 11];
      }
      output[len + 4] = '\0';
      return 0;
      break;
    default:
      return 3;
  }
}

/**
 Hard coded send ABCD
*/
void sendABCD() {
  byte data[] = { 0x02, 0x04, 0x04, 0x00, 0x41, 0x42, 0x43, 0x44, 0x06 };
  mySerial.write(data, 9);
}

/**
Send a c-string over BT
*/
void sendMessage(char* data) {
  int i = 0;
  while (data[i]) { i++ };
  sendBT(CMD_DATA_REQ, data, i);
}

/**
Send command to bluetooth
*/
void sendBT(byte cmd, char* data, uint16_t len) {
  mySerial.write(0x02);  // from datasheet
  mySerial.write(cmd);
  byte len_low = len & 0xFF;
  byte len_high = (len >> 8) & 0xFF;
  mySerial.write(len_low);  // LSB
  mySerial.write(len_high);

  byte cs = 0x02 ^ cmd ^ len_low ^ len_high;
  for (uint16_t i = 0; i < len; i++) {
    mySerial.write(data[i]);
    cs ^= data[i];
  }
  mySerial.write(cs);
}
