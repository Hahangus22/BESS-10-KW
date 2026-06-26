#ifndef AMETHYST_H
#define AMETHYST_H

#include <Arduino.h>

// ============================================
// MQTT Configuration Settings
// (Silakan sesuaikan dengan detail broker Anda)
// ============================================
#define MQTT_BROKER    "mqtt.elbisolo.id"   // Host / URL Broker MQTT
#define MQTT_PORT      1883                  // Port Broker MQTT (biasanya 1883)
#define MQTT_USER      "dev"       // Username MQTT (kosongkan "" jika tidak memakai auth)
#define MQTT_PASS      "administrator"       // Password MQTT (kosongkan "" jika tidak memakai auth)
#define MQTT_TOPIC_PUB "BESS-10KW/sensor-dev"      // Topik Publish untuk telemetri baterai

enum AmethystState {
    AMETHYST_STATE_INIT_DELAY,      // Initial 5s delay
    AMETHYST_STATE_SEND_AT,         // Send AT ping
    AMETHYST_STATE_WAIT_AT_OK,      // Wait for OK response to AT
    AMETHYST_STATE_SEND_CONFIG,     // Send AT+MQTTINTERVAL=15
    AMETHYST_STATE_WAIT_CONFIG_OK,  // Wait for OK response to config
    AMETHYST_STATE_READY            // Ready to process data queue
};

class AmethystClient {
public:
    unsigned long lastUserActivityTime = 0; // Pause auto commands when user types
    bool telemetryPaused = false;
    void pauseTelemetry(bool pause) { telemetryPaused = pause; }
    bool isTelemetryPaused() const { return telemetryPaused; }
    void init();
    void update(); // Must be called in the main loop()
    void sendTelemetry(int soc, int soh, float voltage, float current, float temps[4], float cells[16]);

private:
    AmethystState state;
    unsigned long stateTimer;
    
    // Command Queue using circular buffer
    String cmdQueue[32];
    int queueHead;
    int queueTail;
    int queueCount;
    
    void enqueueCommand(const String& cmd);
    bool dequeueCommand(String& cmd);
    
    // Non-blocking transmission control
    bool waitingForResponse;
    unsigned long commandSentTime;
    String rxBuffer;
    
    unsigned long lastTxTime;
    const unsigned long interCommandDelay = 50;  // 50ms delay between commands
    const unsigned long maxResponseTimeout = 300; // 300ms max timeout for AT commands
};

extern HardwareSerial AmethystSerial;

#endif // AMETHYST_H
