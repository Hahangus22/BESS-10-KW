#include "rs485.h"

HardwareSerial _RS485Serial(2);

void RS485::init() {
  pinMode(RS485_RE_DE_PIN, OUTPUT);
  digitalWrite(RS485_RE_DE_PIN, LOW);

  _RS485Serial.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
}

void RS485::sendString(String &input) {
  digitalWrite(RS485_RE_DE_PIN, HIGH);
  delayMicroseconds(10);

  _RS485Serial.println(input);
  _RS485Serial.flush();  // wait until sent

  delayMicroseconds(10);
  digitalWrite(RS485_RE_DE_PIN, LOW);
}

bool RS485::readLine(String &out, uint32_t timeoutMs) {
  static String buffer;
  unsigned long start = millis();

  digitalWrite(RS485_RE_DE_PIN, LOW);

  while (millis() - start < timeoutMs) {
    while (_RS485Serial.available()) {
      char c = _RS485Serial.read();

      if (c == '\n') {
        out = buffer;
        buffer = "";
        return true;
      }

      if (c != '\r') {
        buffer += c;
      }

      // safety limit
      if (buffer.length() > 128) {
        buffer = "";
        return false;
      }
    }
  }
  return false;
}


void RS485::modbusSend(uint8_t *frame, uint8_t len) {
  while (_RS485Serial.available()) _RS485Serial.read(); 

  digitalWrite(RS485_RE_DE_PIN, HIGH);
  delayMicroseconds(20);

  _RS485Serial.write(frame, len);
  _RS485Serial.flush();

  delayMicroseconds(20);
  digitalWrite(RS485_RE_DE_PIN, LOW);
}


bool RS485::readModbus(uint8_t *outBuf, uint8_t &outLen, uint32_t frameTimeoutMs) {
  digitalWrite(RS485_RE_DE_PIN, LOW); // RX mode

  static uint8_t buf[128];
  static uint8_t idx = 0;
  static unsigned long lastByteTime = 0;


  // Read incoming bytes (non-blocking)
  while (_RS485Serial.available()) {
    uint8_t b = _RS485Serial.read();
    //Serial.print(b, HEX);

    
    if (idx < sizeof(buf)) {
      buf[idx++] = b;
      lastByteTime = millis();
    }
    
    else {
      // overflow protection
      idx = 0;
      return false;
    }
    
    
  }

  // No data yet
  if (idx == 0) return false;

  // End of frame detected (silent gap)
  if (millis() - lastByteTime > frameTimeoutMs) {
    if (idx >= 5) { // minimum Modbus RTU frame
      memcpy(outBuf, buf, idx);
      outLen = idx;
      idx = 0;
      return true;
    }

    // invalid frame
    idx = 0;
  }

  return false;
}


uint16_t RS485::modbusCRC(const uint8_t *buf, uint16_t len) {
  uint16_t crc = 0xFFFF;

  for (uint16_t pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];

    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}
