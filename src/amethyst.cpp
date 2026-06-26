#include "amethyst.h"

// Amethyst is connected via Serial1 (UART1) using RX=34, TX=22 (swapped pins)
HardwareSerial AmethystSerial(1);

// Extern declarations for BMS error/status flags
extern bool errUnderVoltage;
extern bool errOverVoltage;
extern bool errOverTemperature;
extern bool errOverCurrentCharge;
extern bool errOverCurrentDischarge;
extern bool errShortCircuit;
extern bool errAngsuran;
extern bool errPrecharge;

void AmethystClient::init() {
    AmethystSerial.begin(9600, SERIAL_8N1, 34, 22);
    Serial.println("Amethyst Client: UART1 initialized (9600 baud, RX=34, TX=22).");
        
    state = AMETHYST_STATE_INIT_DELAY;
    stateTimer = millis();
    
    queueHead = 0;
    queueTail = 0;
    queueCount = 0;
    
    waitingForResponse = false;
    rxBuffer = "";
    lastTxTime = 0;
    telemetryPaused = false;
}

void AmethystClient::enqueueCommand(const String& cmd) {
    if (queueCount >= 32) {
        // Queue full, drop oldest command (dequeue) to make space
        String dummy;
        dequeueCommand(dummy);
        Serial.println("Amethyst Client Queue: Full, oldest command discarded.");
    }
    cmdQueue[queueTail] = cmd;
    queueTail = (queueTail + 1) % 32;
    queueCount++;
}

bool AmethystClient::dequeueCommand(String& cmd) {
    if (queueCount == 0) return false;
    cmd = cmdQueue[queueHead];
    queueHead = (queueHead + 1) % 32;
    queueCount--;
    return true;
}

void AmethystClient::sendTelemetry(int soc, int soh, float voltage, float current, float temps[4], float cells[16]) {
    unsigned long now = millis();
    // Rate limit to prevent duplicate uploads (minimum 2 seconds between uploads)
    static unsigned long lastUploadTime = 0;
    if (now - lastUploadTime < 2000) {
        return;
    }
    lastUploadTime = now;

    // Only queue data if the module configuration sequence is completed and ready
    if (state != AMETHYST_STATE_READY) {
        Serial.println("Amethyst Client: Telemetry upload skipped (module not ready/configuring).");
        return;
    }

    // Check if at least one cell has a valid voltage (>= 3.0V)
    bool hasValidCell = false;
    for (int i = 0; i < 16; i++) {
        if (cells[i] >= 3.0) {
            hasValidCell = true;
            break;
        }
    }
    if (!hasValidCell) {
        Serial.println("Amethyst Client: Telemetry upload skipped (no valid battery cells detected / BMS offline).");
        return;
    }

    // If the queue has too much stale data, flush it first
    if (queueCount + 8 > 32) {
        Serial.println("Amethyst Client Queue: Congestion detected, clearing queue.");
        queueHead = 0;
        queueTail = 0;
        queueCount = 0;
    }

    // Reconstruct decimal error code from binary flags to upload via MQTT (8-bit)
    int errorCode = 0;
    if (errPrecharge)             errorCode |= (1 << 7); // Bit 7 (128)
    if (errAngsuran)              errorCode |= (1 << 6); // Bit 6 (64)
    if (errShortCircuit)          errorCode |= (1 << 5); // Bit 5 (32)
    if (errOverCurrentDischarge)  errorCode |= (1 << 4); // Bit 4 (16)
    if (errOverCurrentCharge)     errorCode |= (1 << 3); // Bit 3 (8)
    if (errOverTemperature)       errorCode |= (1 << 2); // Bit 2 (4)
    if (errOverVoltage)           errorCode |= (1 << 1); // Bit 1 (2)
    if (errUnderVoltage)          errorCode |= (1 << 0); // Bit 0 (1)

    enqueueCommand("AT+INSERTDATA=bms/[0]/id,1"); //diganti jika BMS gant
    enqueueCommand("AT+INSERTDATA=bms/[0]/error_code," + String(errorCode));
    enqueueCommand("AT+INSERTDATA=bms/[0]/soc," + String(soc));
    enqueueCommand("AT+INSERTDATA=bms/[0]/soh," + String(soh));
    enqueueCommand("AT+INSERTDATA=bms/[0]/vpack," + String(voltage, 2));
    enqueueCommand("AT+INSERTDATA=bms/[0]/ipack," + String(current, 2));

    // Construct cell array data (mV): bms/[0]/cell_voltage,c1,c2,...
    String cellCmd = "AT+INSERTDATA=bms/[0]/cell_voltage";
    for (int i = 0; i < 16; i++) {
        int cellVal = (int)round(cells[i] * 1000.0);
        if (cellVal >= 3000) {
            cellCmd += ",";
            cellCmd += String(cellVal);
        }
    }
    enqueueCommand(cellCmd);

    // Construct temps array data: bms/[0]/temp,t1,t2,t3,t4
    String tempsCmd = "AT+INSERTDATA=bms/[0]/temp";
    for (int i = 0; i < 4; i++) {
        tempsCmd += ",";
        tempsCmd += String((int)round(temps[i]));
    }
    enqueueCommand(tempsCmd);
}

void AmethystClient::update() {
    unsigned long now = millis();

    // Read any incoming data non-blockingly
    while (AmethystSerial.available()) {
        char c = AmethystSerial.read();
        Serial.write(c); // Print raw Amethyst output directly to PC Serial Monitor for debugging
        if (rxBuffer.length() < 128) {
            rxBuffer += c;
        }
    }

    switch (state) {
        case AMETHYST_STATE_INIT_DELAY:
            if (now - stateTimer >= 5000) {
                Serial.println("Amethyst: 5-second startup delay finished. Starting AT check.");
                state = AMETHYST_STATE_SEND_AT;
            }
            break;

        case AMETHYST_STATE_SEND_AT:
            // Clear RX buffer
            rxBuffer = "";
            while (AmethystSerial.available()) AmethystSerial.read();

            Serial.println("Amethyst Send: AT");
            AmethystSerial.println("AT");
            
            waitingForResponse = true;
            commandSentTime = now;
            state = AMETHYST_STATE_WAIT_AT_OK;
            break;

        case AMETHYST_STATE_WAIT_AT_OK:
            if (rxBuffer.indexOf("OK") != -1 || rxBuffer.indexOf("*ATREADY: 1") != -1 || rxBuffer.indexOf("*INTERNETREADY: 1") != -1) {
                Serial.println("Amethyst Response: OK / ATREADY / INTERNETREADY. AT interface is active.");
                waitingForResponse = false;
                rxBuffer = "";
                state = AMETHYST_STATE_SEND_CONFIG;
            } 
            else if (rxBuffer.indexOf("ERROR") != -1 || (now - commandSentTime >= 1000)) {
                if (now - commandSentTime >= 1000) {
                    Serial.print("Amethyst Response: Timeout waiting for AT response. rxBuffer content: '");
                    Serial.print(rxBuffer);
                    Serial.println("'");
                } else {
                    Serial.println("Amethyst Response: ERROR to AT.");
                }
                waitingForResponse = false;
                rxBuffer = "";
                
                // Retry AT command in 2 seconds (using stateTimer to delay)
                state = AMETHYST_STATE_INIT_DELAY;
                stateTimer = now - 3000; // 5000 - 3000 = 2000ms remaining delay
            }
            break;

        case AMETHYST_STATE_SEND_CONFIG:
            // Clear RX buffer
            rxBuffer = "";
            while (AmethystSerial.available()) AmethystSerial.read();

            Serial.println("Amethyst Send: AT+MQTTINTERVAL=15");
            AmethystSerial.println("AT+MQTTINTERVAL=15");
            
            waitingForResponse = true;
            commandSentTime = now;
            state = AMETHYST_STATE_WAIT_CONFIG_OK;
            break;

        case AMETHYST_STATE_WAIT_CONFIG_OK:
            if (rxBuffer.indexOf("OK") != -1) {
                Serial.println("Amethyst Response: OK. MQTT config succeeded.");
                waitingForResponse = false;
                rxBuffer = "";
                
                // Enqueue MQTT Configuration commands
                Serial.println("Amethyst: Enqueuing MQTT broker, authentication and topic settings...");
            #if defined(MQTT_BROKER) && defined(MQTT_PORT)
                enqueueCommand("AT+MQTTBROKER=" + String(MQTT_BROKER) + "," + String(MQTT_PORT));
            #endif
                
                // Only send MQTTAUTH if username or password is provided
            #if defined(MQTT_USER) && defined(MQTT_PASS)
                if (String(MQTT_USER) != "" || String(MQTT_PASS) != "") {
                    enqueueCommand("AT+MQTTAUTH=" + String(MQTT_USER) + "," + String(MQTT_PASS));
                }
            #endif
                
            #if defined(MQTT_TOPIC_PUB)
                enqueueCommand("AT+MQTTTOPICPUB=" + String(MQTT_TOPIC_PUB));
            #endif

            #if defined(MQTT_TOPIC_SUB)
                enqueueCommand("AT+MQTTTOPICSUB=" + String(MQTT_TOPIC_SUB));
            #endif
                
                state = AMETHYST_STATE_READY;
                Serial.println("Amethyst Client: Ready to transmit telemetry.");
            } 
            else if (rxBuffer.indexOf("ERROR") != -1 || (now - commandSentTime >= 1000)) {
                if (now - commandSentTime >= 1000) {
                    Serial.print("Amethyst Response: Timeout waiting for MQTT config. rxBuffer content: '");
                    Serial.print(rxBuffer);
                    Serial.println("'");
                } else {
                    Serial.println("Amethyst Response: ERROR to MQTT config.");
                }
                waitingForResponse = false;
                rxBuffer = "";
                
                // Retry config
                state = AMETHYST_STATE_SEND_CONFIG;
            }
            break;

        case AMETHYST_STATE_READY:
            // If waiting for a response from a queued command
            if (waitingForResponse) {
                if (rxBuffer.indexOf("OK") != -1) {
                    Serial.println("Amethyst Command Result: OK");
                    waitingForResponse = false;
                    rxBuffer = "";
                    lastTxTime = now;
                } 
                else if (rxBuffer.indexOf("ERROR") != -1 || (now - commandSentTime >= maxResponseTimeout)) {
                    if (now - commandSentTime >= maxResponseTimeout) {
                        Serial.println("Amethyst Command Result: TIMEOUT");
                    } else {
                        Serial.println("Amethyst Command Result: ERROR");
                    }
                    waitingForResponse = false;
                    rxBuffer = "";
                    lastTxTime = now;
                }
            } 
            // If not waiting, check if we can send the next queued command
            else {
                if (queueCount > 0 && (now - lastTxTime >= interCommandDelay)) {
                    String cmd;
                    if (dequeueCommand(cmd)) {
                        // Clear RX buffer
                        rxBuffer = "";
                        while (AmethystSerial.available()) AmethystSerial.read();

                        Serial.print("Amethyst Send Queued: ");
                        Serial.println(cmd);
                        
                        AmethystSerial.println(cmd);
                        
                        waitingForResponse = true;
                        commandSentTime = now;
                    }
                }
            }
            break;
    }
}
