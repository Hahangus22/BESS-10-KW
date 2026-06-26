#ifndef RS485_H
#define RS485_H

#include <Arduino.h>

// ============================================
// Define RS485 GPIO Pin
// ============================================
#define RS485_RE_DE_PIN 2
#define RS485_RX_PIN    35
#define RS485_TX_PIN    21

extern HardwareSerial _RS485Serial;

class RS485 {
  public:
    void init();
    void sendString(String &input);
    bool readLine(String &out, uint32_t timeoutMs = 50);
    void modbusSend(uint8_t *frame, uint8_t len);
    uint16_t modbusCRC(const uint8_t *buf, uint16_t len);
    bool readModbus(uint8_t *outBuf, uint8_t &outLen, uint32_t frameTimeoutMs = 5);
};

#endif
