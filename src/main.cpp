#include <Arduino.h>
#include "TouchScreen.h"
#include "bess_tft.h"
#include "rs485.h"
#include "amethyst.h"

// ============================================
// Pin Definitions for Touch Screen (Shared with Parallel TFT)
// ============================================
#define YP 4   // LCD_WR
#define XM 15  // LCD_RS (DC)
#define YM 14  // LCD_D7
#define XP 27  // LCD_D6

// ============================================
// Pin Definitions for Buttons
// ============================================
#define PIN_BUTTON_ENTER 18
#define PIN_BUTTON_BACK  5
#define PIN_BUTTON_UP    19
#define PIN_BUTTON_DOWN  23

// ============================================
// Objects Initialization
// ============================================
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
BessTFT myTft;
RS485 rs485;
AmethystClient amethyst;

void handleButtonPress(bool enter, bool back, bool up, bool down);

// ============================================
// Global Telemetry Variables
// ============================================
int batSOC = 100;
int batSOH = 100;
float batVPack = 52.8;
float batCurrent = 0.0;
int batVCell[25];
int batTemp[4];

// Arrays formatted for display
float cells[16];
int cell_indices[16];
float cell_temps[4];

// Status and Error Flags from BMS
bool errUnderVoltage = false;
bool errOverVoltage = false;
bool errOverTemperature = false;
bool errOverCurrentCharge = false;
bool errOverCurrentDischarge = false;
bool errShortCircuit = false;
bool errAngsuran = false;
bool errPrecharge = false;

// BMS online status and timeout tracking
bool bmsOnline = false;
unsigned long lastValidModbusTime = 0;

// ============================================
// TFT-RS485 Pin Share Control Helpers
// ============================================
void startTftWrite() {
  pinMode(YM, OUTPUT);
  pinMode(XP, OUTPUT);
  pinMode(33, OUTPUT);
  digitalWrite(33, LOW); // Select TFT CS (Active LOW)
}

void endTftWrite() {
  digitalWrite(33, HIGH); // Deselect TFT CS to protect against touch signals
  pinMode(YM, INPUT);
  pinMode(XP, INPUT);
}

// ============================================
// Timing & Control Variables
// ============================================
unsigned long lastDisplayUpdate = 0;
const unsigned long displayUpdateInterval = 250;  // 250ms for fast real-time update

// RS485 Modbus variables
uint8_t modbusRXframe[128];
uint8_t modbusMsg[128];
uint8_t modbusRXlen;
uint8_t modbusRequestIndex = 0;
unsigned long RS485LastRequestTime = 0;
const unsigned long RS485RequestInterval = 500;  // 500ms between requests
bool isNewModbusMsg = false;

// ============================================
// Page & Touch State
// ============================================
int currentPage = 0;      // 0: Dashboard (Gauges), 1: Cells Grid
bool showModal = false;   // State detail modal (only in Cells page)
int modalType = 0;        // 1: Cell, 2: Temp
int modalId = 0;          // ID 1-16 or 1-4

// --- LOGIKA SLEEP MODE ---
bool isSleeping = false;
unsigned long lastActivityTime = 0;
const unsigned long sleepTimeout = 60000;    // 10 seconds timeout

// --- LOGIKA ANIMASI JALUR SIRKUIT ---
unsigned long lastAnimUpdate = 0;
const unsigned long animInterval = 80;       // Update every 80ms
int animStep = 0;
int prev_x1 = 0, prev_y1 = 0;
int prev_x2 = 0, prev_y2 = 0;
int prev_x3 = 0, prev_y3 = 0;
int prev_x4 = 0, prev_y4 = 0;

// Touch screen debounce & polling
unsigned long lastTouchTime = 0;
const unsigned long touchDebounce = 300;     // 300ms debounce
unsigned long lastTouchPoll = 0;
const unsigned long touchPollInterval = 50;   // Poll touch every 50ms

// ============================================
// Functions Declarations
// ============================================
boolean Touch_getXY(uint16_t *x, uint16_t *y);
void goToSleep();
void wakeUp();
void updateTelemetryData();
void handleModbusCommunication();

void setup() {
  Serial.begin(115200);
  
  // Set ADC resolution to 10-bit (0-1023) for analog TouchScreen compatibility
  analogReadResolution(10);
  
  Serial.println("Initializing BESS Integrated System (Wi-Fi Removed)...");

  // 1. Initialize Display
  myTft.init();
  Serial.println("TFT Display initialized.");
  
  // Set YM and XP to INPUT to cut off touchscreen idle current
  pinMode(YM, INPUT);
  pinMode(XP, INPUT);
  
  // Initialize physical buttons
  pinMode(PIN_BUTTON_ENTER, INPUT_PULLUP);
  pinMode(PIN_BUTTON_BACK, INPUT_PULLUP);
  pinMode(PIN_BUTTON_UP, INPUT_PULLUP);
  pinMode(PIN_BUTTON_DOWN, INPUT_PULLUP);
  

  // 3. Initialize RS485
  rs485.init();
  Serial.println("RS485 Modbus initialized.");

  // 4. Initialize Amethyst Network Client
  amethyst.init();

  // Initialize display data placeholders
  for (int i = 0; i < 24; i++) {
    batVCell[i] = 3300; // 3.3V default placeholder
  }
  for (int i = 0; i < 16; i++) {
    cells[i] = 3.300;
  }
  batTemp[0] = 28; // 28C default placeholder
  for (int i = 0; i < 4; i++) {
    cell_temps[i] = 28.0;
  }

  // Render initial static dashboard once
  startTftWrite();
  myTft.drawStaticDashboard();
  float power = (batVPack * batCurrent) / 1000.0;
  myTft.updateDashboard(batVPack, batCurrent, batSOC, batSOH, cell_temps[0], power, bmsOnline, true);
  endTftWrite();

  lastActivityTime = millis();
  Serial.println("Setup Completed. Running Loop...");
}

void loop() {
  unsigned long now = millis();


  // Pass-through & Manual Command Parser:
  // Forward PC Serial input to Amethyst module directly.
  static String serialInputBuffer = "";
  static unsigned long lastCharTime = 0;
  while (Serial.available() > 0) {
    char c = Serial.read();
    now = millis();
    lastCharTime = now;
    
    // Echo the character back to the Serial Monitor so the user knows they are typing
    Serial.write(c);
    
    if (c == '\n' || c == '\r') {
      if (serialInputBuffer.length() > 0) {
        serialInputBuffer.trim();
        Serial.printf("\n[PC -> Amethyst] Sending: %s\n", serialInputBuffer.c_str());
        
        // Forward the command to the Amethyst serial port directly
        AmethystSerial.println(serialInputBuffer);
        serialInputBuffer = "";
      }
    } else {
      if (c >= 32 && c <= 126) { // Only append printable ASCII characters
        if (serialInputBuffer.length() < 128) {
          serialInputBuffer += c;
        }
      }
    }
  }

  // Timeout flush: if there is text in the buffer and no new characters for 1000ms,
  // process it as a command (handles "No line ending" in serial monitor)
  if (serialInputBuffer.length() > 0 && (now - lastCharTime > 1000)) {
    serialInputBuffer.trim();
    if (serialInputBuffer.length() > 0) {
      Serial.printf("\n[PC -> Amethyst - Timeout Flush] Sending: %s\n", serialInputBuffer.c_str());
      // Forward the command to the Amethyst serial port directly
      AmethystSerial.println(serialInputBuffer);
    }
    serialInputBuffer = "";
  }

  // Update non-blocking Amethyst state machine
  amethyst.update();

  // Handle RS485 Modbus requests and responses
  handleModbusCommunication();

  // Update telemetry and SD logging periodically (every 1 second)
  if (now - lastDisplayUpdate >= displayUpdateInterval) {
    lastDisplayUpdate = now;
    updateTelemetryData();
    
    // SD Card logging removed
    // Update display only if awake
    if (!isSleeping) {
      startTftWrite();
      if (currentPage == 0) {
        float power = (batVPack * batCurrent) / 1000.0;
        myTft.updateDashboard(batVPack, batCurrent, batSOC, batSOH, cell_temps[0], power, bmsOnline, false);
      } else {
        if (!showModal) {
          myTft.updateCells(cells, cell_temps);
        } else {
          myTft.updateModalContent(modalType, modalId, cells, cell_temps);
        }
      }
      endTftWrite();
    }
  }

  // Circuit path animation enabled (caching makes parallel bus overhead very low now)
  if (!isSleeping && currentPage == 0 && (now - lastAnimUpdate >= animInterval)) {
    lastAnimUpdate = now;
    startTftWrite();
    myTft.updateCircuitAnimation(animStep, prev_x1, prev_y1, prev_x2, prev_y2, prev_x3, prev_y3, prev_x4, prev_y4);
    endTftWrite();
  }

  // Check inactivity timeout for Sleep Mode
  if (!isSleeping && (now - lastActivityTime >= sleepTimeout)) {
    goToSleep();
  }

  // Check touch input every 50ms (Unconditional polling since Wi-Fi is removed)
  if (now - lastTouchPoll >= touchPollInterval) {
    lastTouchPoll = now;
    
    uint16_t tx, ty;
    if (Touch_getXY(&tx, &ty)) {
      if (isSleeping) {
        wakeUp();
        lastTouchTime = now;
      } else {
        if (now - lastTouchTime > touchDebounce) {
          lastTouchTime = now;
          lastActivityTime = now; // Reset timer
          Serial.printf("Touch detected: X=%d, Y=%d\n", tx, ty);
          
          if (currentPage == 0) {
            // Check if center SOC circle is pressed (x: 170-310, y: 90-230)
            if (tx >= 170 && tx <= 310 && ty >= 90 && ty <= 230) {
              currentPage = 1;
              showModal = false;
              prev_x1 = 0; // Reset animation coordinates
              modalType = 0; // Reset selected cell to none at start
              
              startTftWrite();
              myTft.drawStaticCells(cells);
              myTft.updateCells(cells, cell_temps);
              endTftWrite();
              
              Serial.println("Navigated to Cells Page.");
            }
          } else if (currentPage == 1) {
            if (showModal) {
              // Check if TUTUP button is pressed (widened & shifted up: x: 150-330, y: 180-260)
              if (tx >= 150 && tx <= 330 && ty >= 180 && ty <= 260) {
                showModal = false;
                
                startTftWrite();
                myTft.drawStaticCells(cells);
                if (modalType != 0) {
                  myTft.highlightCell(modalType, modalId); // Keep highlighting the pressed cell
                }
                myTft.updateCells(cells, cell_temps);
                endTftWrite();
                
                Serial.println("Closed modal.");
              }
            } else {
              // Check if BACK button is pressed (x: 0-90, y: 0-42, allowing wrapped negative values for top-left edge)
              if ((tx <= 90 || tx >= 65000) && (ty <= 42 || ty >= 65000)) {
                currentPage = 0;
                prev_x1 = 0; // Reset animation
                
                startTftWrite();
                myTft.drawStaticDashboard();
                float power = (batVPack * batCurrent) / 1000.0;
                myTft.updateDashboard(batVPack, batCurrent, batSOC, batSOH, cell_temps[0], power, bmsOnline, true);
                endTftWrite();
                
                Serial.println("Navigated back to Dashboard.");
              } else {
                // Check if one of the 16 cell boxes is pressed
                int cellW = 80;
                int cellH = 46;
                int colSpace = 6;
                int rowSpace = 8;
                bool touchedElement = false;
                
                for (int r = 0; r < 4; r++) {
                  int boxY = 65 + r * (cellH + rowSpace);
                  for (int c = 0; c < 4; c++) {
                    int minX, maxX;
                    if (c == 0) { minX = 10; maxX = 92; }
                    else if (c == 1) { minX = 93; maxX = 178; }
                    else if (c == 2) { minX = 179; maxX = 264; }
                    else { minX = 265; maxX = 350; } // Column 3
                    
                    if (tx >= minX && tx <= maxX && ty >= (boxY - 3) && ty <= (boxY + cellH + 3)) {
                      int pressedId = c * 4 + r + 1;
                      if (pressedId <= 16 && cells[pressedId - 1] >= 3.0) {
                        modalType = 1;
                        modalId = pressedId;
                        showModal = true;
                        
                        startTftWrite();
                        myTft.highlightCell(modalType, modalId);
                        delay(150); // Visual feedback delay
                        myTft.drawModal(modalType, modalId, cells, cell_temps);
                        endTftWrite();
                        
                        touchedElement = true;
                        Serial.printf("Cell C%d detail popup opened.\n", modalId);
                      }
                      break;
                    }
                  }
                  if (touchedElement) break;
                }
                
                // Check if one of the 4 temp sensor boxes is pressed
                if (!touchedElement) {
                  int tempH = 46;
                  int tempSpace = 8;
                  for (int i = 0; i < 4; i++) {
                    int boxY = 65 + i * (tempH + tempSpace);
                    if (tx >= 358 && ty >= (boxY - 3) && ty <= (boxY + tempH + 3)) {
                      modalType = 2;
                      modalId = i + 1;
                      showModal = true;
                      
                      startTftWrite();
                      myTft.highlightCell(modalType, modalId);
                      delay(150); // Visual feedback delay
                      myTft.drawModal(modalType, modalId, cells, cell_temps);
                      endTftWrite();
                      
                      Serial.printf("Temp sensor T%d detail popup opened.\n", modalId);
                      break;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  // Check physical buttons press
  static unsigned long lastBtnTime = 0;
  const unsigned long btnDebounce = 250; // 250ms debounce
  if (now - lastBtnTime >= btnDebounce) {
    bool enterPressed = (digitalRead(PIN_BUTTON_ENTER) == LOW);
    bool backPressed = (digitalRead(PIN_BUTTON_BACK) == LOW);
    bool upPressed = (digitalRead(PIN_BUTTON_UP) == LOW);
    bool downPressed = (digitalRead(PIN_BUTTON_DOWN) == LOW);
    
    if (enterPressed || backPressed || upPressed || downPressed) {
      lastBtnTime = now;
      lastActivityTime = now; // Reset sleep timer
      
      if (isSleeping) {
        wakeUp();
      } else {
        handleButtonPress(enterPressed, backPressed, upPressed, downPressed);
      }
    }
  }
}

// ============================================
// RS485 Modbus Master Implementation
// ============================================
void handleModbusCommunication() {
  // Check BMS online status timeout (5 seconds of no valid messages)
  if (millis() - lastValidModbusTime > 5000 && millis() > 5000) {
    if (bmsOnline) {
      bmsOnline = false;
      Serial.println("[System] BMS Communication lost! Battery OFFLINE.");
    }
  }

  // Read incoming modbus response frame
  if (rs485.readModbus(modbusRXframe, modbusRXlen)) {
    isNewModbusMsg = true;
    
    // Print received Modbus frame in Hex
    Serial.print("[Modbus RX] (Len=");
    Serial.print(modbusRXlen);
    Serial.print("): ");
    for (int i = 0; i < modbusRXlen; i++) {
      Serial.printf("%02X ", modbusRXframe[i]);
    }
    Serial.println();
  }

  // Parse received modbus message
  if (isNewModbusMsg) {
    isNewModbusMsg = false;
    memcpy(modbusMsg, modbusRXframe, sizeof(modbusRXframe));

    uint8_t msgAddress = modbusMsg[0];
    
    if (msgAddress == 0x99) { // BMS address
      bmsOnline = true;
      lastValidModbusTime = millis();
      uint8_t channel = modbusMsg[2];
      
      // 1. Pack Data Channel (SOC, SOH, Pack V, Current)
      if (channel == 0x00) {
        batSOC = modbusMsg[4];
        batSOH = modbusMsg[5];
        batVPack = ((modbusMsg[6] << 8) | modbusMsg[7]) / 100.0;
        
        uint16_t rawCurrent = (modbusMsg[8] << 8) | modbusMsg[9];
        if (modbusMsg[10] == 0x01) { // 1 = discharge (negative)
          batCurrent = rawCurrent / -100.0;
        } else { // 0 = charge (positive)
          batCurrent = rawCurrent / 100.0;
        }
        
        Serial.printf("  -> Parsed Pack Data: SOC=%d%%, SOH=%d%%, VPack=%.2fV, Current=%.2fA\n", batSOC, batSOH, batVPack, batCurrent);
      }
      // 2. Cell Voltages Channel
      else if (channel == 0x01) {
        uint8_t subChannel = modbusMsg[3];
        if (subChannel == 0x00) { // Cells 1 - 5
          batVCell[0] = (modbusMsg[4] << 8) | modbusMsg[5];
          batVCell[1] = (modbusMsg[6] << 8) | modbusMsg[7];
          batVCell[2] = (modbusMsg[8] << 8) | modbusMsg[9];
          batVCell[3] = (modbusMsg[10] << 8) | modbusMsg[11];
          batVCell[4] = (modbusMsg[12] << 8) | modbusMsg[13];

          Serial.printf("  -> Parsed Cells 1-5: C1=%dmV, C2=%dmV, C3=%dmV, C4=%dmV, C5=%dmV\n", batVCell[0], batVCell[1], batVCell[2], batVCell[3], batVCell[4]);
        }
        else if (subChannel == 0x01) { // Cells 6 - 10
          batVCell[5] = (modbusMsg[4] << 8) | modbusMsg[5];
          batVCell[6] = (modbusMsg[6] << 8) | modbusMsg[7];
          batVCell[7] = (modbusMsg[8] << 8) | modbusMsg[9];
          batVCell[8] = (modbusMsg[10] << 8) | modbusMsg[11];
          batVCell[9] = (modbusMsg[12] << 8) | modbusMsg[13];
          
          Serial.printf("  -> Parsed Cells 6-10: C6=%dmV, C7=%dmV, C8=%dmV, C9=%dmV, C10=%dmV\n", batVCell[5], batVCell[6], batVCell[7], batVCell[8], batVCell[9]);
        }
        else if (subChannel == 0x02) { // Cells 11 - 15
          batVCell[10] = (modbusMsg[4] << 8) | modbusMsg[5];
          batVCell[11] = (modbusMsg[6] << 8) | modbusMsg[7];
          batVCell[12] = (modbusMsg[8] << 8) | modbusMsg[9];
          batVCell[13] = (modbusMsg[10] << 8) | modbusMsg[11]; // Cell 14
          batVCell[14] = (modbusMsg[12] << 8) | modbusMsg[13]; // Cell 15
          
          Serial.printf("  -> Parsed Cells 11-15: C11=%dmV, C12=%dmV, C13=%dmV, C14=%dmV, C15=%dmV\n", batVCell[10], batVCell[11], batVCell[12], batVCell[13], batVCell[14]);
        }
        else if (subChannel == 0x03) { // Cells 16 - 20
          batVCell[15] = (modbusMsg[4] << 8) | modbusMsg[5];
          batVCell[16] = (modbusMsg[6] << 8) | modbusMsg[7];
          batVCell[17] = (modbusMsg[8] << 8) | modbusMsg[9];
          batVCell[18] = (modbusMsg[10] << 8) | modbusMsg[11];
          batVCell[19] = (modbusMsg[12] << 8) | modbusMsg[13];
          
          Serial.printf("  -> Parsed Cells 16-20: C16=%dmV, C17=%dmV, C18=%dmV, C19=%dmV, C20=%dmV\n", batVCell[15], batVCell[16], batVCell[17], batVCell[18], batVCell[19]);
        }
        else if (subChannel == 0x04) { // Cells 21 - 25
          batVCell[20] = (modbusMsg[4] << 8) | modbusMsg[5];
          batVCell[21] = (modbusMsg[6] << 8) | modbusMsg[7];
          batVCell[22] = (modbusMsg[8] << 8) | modbusMsg[9];
          batVCell[23] = (modbusMsg[10] << 8) | modbusMsg[11];
          batVCell[24] = (modbusMsg[12] << 8) | modbusMsg[13];
          
          Serial.printf("  -> Parsed Cells 21-25: C21=%dmV, C22=%dmV, C23=%dmV, C24=%dmV, C25=%dmV\n", batVCell[20], batVCell[21], batVCell[22], batVCell[23], batVCell[24]);
        }
      }
      // 3. Temperature Channel
      else if (channel == 0x02) {
        batTemp[0] = modbusMsg[4];
        batTemp[1] = modbusMsg[5];
        batTemp[2] = modbusMsg[6];
        batTemp[3] = modbusMsg[7];
        
        Serial.printf("  -> Parsed Temps: T1=%dC, T2=%dC, T3=%dC, T4=%dC\n", batTemp[0], batTemp[1], batTemp[2], batTemp[3]);
      }
      // 4. Status/Error Channel
      else if (channel == 0x03) {
        // Read 8-bit decimal error code from the first data byte of the Modbus message
        uint8_t errorCode = modbusMsg[4];
        
        errPrecharge             = ((errorCode & (1 << 7)) != 0); // Bit 7 (128)
        errAngsuran              = ((errorCode & (1 << 6)) != 0); // Bit 6 (64)
        errShortCircuit          = ((errorCode & (1 << 5)) != 0); // Bit 5 (32)
        errOverCurrentDischarge  = ((errorCode & (1 << 4)) != 0); // Bit 4 (16)
        errOverCurrentCharge     = ((errorCode & (1 << 3)) != 0); // Bit 3 (8)
        errOverTemperature       = ((errorCode & (1 << 2)) != 0); // Bit 2 (4)
        errOverVoltage           = ((errorCode & (1 << 1)) != 0); // Bit 1 (2)
        errUnderVoltage          = ((errorCode & (1 << 0)) != 0); // Bit 0 (1)
        
        Serial.printf("  -> Parsed Status Decimal=%u: UV=%d, OV=%d, OT=%d, OCC=%d, OCD=%d, SC=%d, ANG=%d, PRE=%d\n",
                      errorCode, errUnderVoltage, errOverVoltage, errOverTemperature, errOverCurrentCharge,
                      errOverCurrentDischarge, errShortCircuit, errAngsuran, errPrecharge);
      }
      
      // Instantly update the display/logging telemetry arrays
      updateTelemetryData();

      // Trigger telemetry transmission to Amethyst only when a complete Modbus polling cycle is finished (at Temperature Channel 0x02 or Status Channel 0x03)
      if (channel == 0x02 || channel == 0x03) {
        amethyst.sendTelemetry(batSOC, batSOH, batVPack, batCurrent, cell_temps, cells);
      }
    }
  }

  // Periodic Modbus Requests (Sequential polling)
  if (millis() - RS485LastRequestTime >= RS485RequestInterval) {
    RS485LastRequestTime = millis();
    uint8_t modbusTXframe[8] = {0};
    
    modbusTXframe[0] = 0x99; // BMS address
    modbusTXframe[1] = 0x01; // Read function
 
    switch (modbusRequestIndex) {
      case 0:
        modbusTXframe[2] = 0x00; // Pack data
        modbusTXframe[3] = 0x00;
        break;
      case 1:
        modbusTXframe[2] = 0x01; // Cells 1 - 5
        modbusTXframe[3] = 0x00;
        break;
      case 2:
        modbusTXframe[2] = 0x01; // Cells 6 - 10
        modbusTXframe[3] = 0x01;
        break;
      case 3:
        modbusTXframe[2] = 0x01; // Cells 11 - 15
        modbusTXframe[3] = 0x02;
        break;
      case 4:
        modbusTXframe[2] = 0x01; // Cells 16 - 20
        modbusTXframe[3] = 0x03;
        break;
      case 5:
        modbusTXframe[2] = 0x01; // Cells 21 - 24
        modbusTXframe[3] = 0x04;
        break;
      case 6:
        modbusTXframe[2] = 0x02; // Temperature
        modbusTXframe[3] = 0x00;
        break;
      case 7:
        modbusTXframe[2] = 0x03; // Status/Error Channel
        modbusTXframe[3] = 0x00;
        break;
    }

    modbusRequestIndex = (modbusRequestIndex + 1) % 8;

    // Calculate CRC
    uint16_t crc = rs485.modbusCRC(modbusTXframe, 6);
    modbusTXframe[6] = crc & 0xFF;
    modbusTXframe[7] = (crc >> 8) & 0xFF;

    rs485.modbusSend(modbusTXframe, 8);
    
    // Print sent Modbus request in Hex
    Serial.print("[Modbus TX]: ");
    for (int i = 0; i < 8; i++) {
      Serial.printf("%02X ", modbusTXframe[i]);
    }
    Serial.println();
  }
}

// ============================================
// Touch Screen Handlerb & Mapping
// ============================================
#define MINPRESSURE 100
#define MAXPRESSURE 1000
const int coords[] = {960, 110, 905, 140}; 

boolean Touch_getXY(uint16_t *x, uint16_t *y) {
  // Deselect TFT CS (Pin 33) to prevent touch signals from corrupting TFT state
  pinMode(33, OUTPUT);
  digitalWrite(33, HIGH);

  TSPoint p = ts.getPoint();
  
  // Restore LCD control pin modes (TFT_WR = 4, TFT_DC = 15) immediately after analog touch reading
  pinMode(4, OUTPUT);      
  pinMode(15, OUTPUT);
  digitalWrite(4, HIGH);   
  digitalWrite(15, HIGH);
  
  // Restore shared data lines (YM/TFT_D7 and XP/TFT_D6) to INPUT to avoid bus contention and leakage
  pinMode(YM, INPUT);
  pinMode(XP, INPUT);
  
  // Leave TFT CS in its default HIGH (deselected) state
  digitalWrite(33, HIGH);
  
  bool pressed = (p.z > MINPRESSURE && p.z < MAXPRESSURE);
  if (pressed) {
    // Map landscape rotation
    *x = map(p.y, coords[0], coords[1], 0, 480);
    *y = map(p.x, coords[3], coords[2], 0, 320);
    Serial.printf("Touch raw: px=%d, py=%d | mapped: X=%d, Y=%d\n", p.x, p.y, *x, *y);
  }
  return pressed;
}

// ============================================
// Sleep Mode Transitions
// ============================================
void goToSleep() {
  isSleeping = true;
  
  startTftWrite();
  myTft.clearScreen();
  endTftWrite();
  
  prev_x1 = 0; // Reset animation coordinates
  Serial.println("LCD entered Sleep Mode.");
}

void wakeUp() {
  startTftWrite();
  delay(80); // Settling delay for shared analog touch pins and LCD controller
  prev_x1 = 0;
  
  if (currentPage == 0) {
    myTft.drawStaticDashboard();
    float power = (batVPack * batCurrent) / 1000.0;
    myTft.updateDashboard(batVPack, batCurrent, batSOC, batSOH, cell_temps[0], power, bmsOnline, true); // Force redraw when waking up
  } else {
    myTft.drawStaticCells(cells);
    if (modalType != 0) {
      myTft.highlightCell(modalType, modalId); // Restore the highlight for the active selection
    }
    myTft.updateCells(cells, cell_temps);
    if (showModal) {
      myTft.drawModal(modalType, modalId, cells, cell_temps);
    }
  }
  
  endTftWrite();
  
  isSleeping = false;
  lastActivityTime = millis();
  Serial.println("LCD woken up.");
}

// ============================================
// Data Operations & Calculations
// ============================================
void updateTelemetryData() {
  if (bmsOnline) {
    // Collect active cells (>= 3.0V) from the 25 possible slots
    int activeIdx = 0;
    for (int i = 0; i < 25; i++) {
      float v = batVCell[i] / 1000.0;
      if (v >= 3.0) {
        if (activeIdx < 16) {
          cells[activeIdx] = v;
          cell_indices[activeIdx] = i + 1; // 1-based original index from BMS
          activeIdx++;
        }
      }
    }
    // Zero out any remaining slots in the 16 cells array
    for (int i = activeIdx; i < 16; i++) {
      cells[i] = 0.0;
      cell_indices[i] = 0;
    }
    
    // Set temperatures
    cell_temps[0] = batTemp[0];
    cell_temps[1] = batTemp[1];
    cell_temps[2] = batTemp[2];
    cell_temps[3] = batTemp[3];
  } else {
    // Clear telemetries when offline
    for (int i = 0; i < 16; i++) {
      cells[i] = 0.0;
      cell_indices[i] = 0;
    }
    for (int i = 0; i < 4; i++) {
      cell_temps[i] = 0.0;
    }
  }
}

// ============================================
// Physical Button Operations Handler
// ============================================
void handleButtonPress(bool enter, bool back, bool up, bool down) {
  if (currentPage == 0) {
    if (enter) {
      currentPage = 1;
      showModal = false;
      prev_x1 = 0;
      if (modalType == 0) {
        modalType = 1;
        modalId = 1;
      }
      
      startTftWrite();
      myTft.drawStaticCells(cells);
      myTft.highlightCell(modalType, modalId);
      myTft.updateCells(cells, cell_temps);
      endTftWrite();
      
      Serial.println("Buttons: Navigated to Cells Page.");
    }
  } else if (currentPage == 1) {
    if (showModal) {
      if (enter || back) {
        showModal = false;
        
        startTftWrite();
        myTft.drawStaticCells(cells);
        if (modalType != 0) {
          myTft.highlightCell(modalType, modalId);
        }
        myTft.updateCells(cells, cell_temps);
        endTftWrite();
        
        Serial.println("Buttons: Closed modal.");
      }
    } else {
      if (back) {
        currentPage = 0;
        prev_x1 = 0;
        
        startTftWrite();
        myTft.drawStaticDashboard();
        float power = (batVPack * batCurrent) / 1000.0;
        myTft.updateDashboard(batVPack, batCurrent, batSOC, batSOH, cell_temps[0], power, bmsOnline, true);
        endTftWrite();
        
        Serial.println("Buttons: Navigated back to Dashboard.");
      } else if (enter) {
        bool canOpen = false;
        if (modalType == 1) {
          if (cells[modalId - 1] >= 3.0) {
            canOpen = true;
          }
        } else if (modalType == 2) {
          canOpen = true;
        }
        
        if (canOpen) {
          showModal = true;
          startTftWrite();
          myTft.highlightCell(modalType, modalId);
          delay(150);
          myTft.drawModal(modalType, modalId, cells, cell_temps);
          endTftWrite();
          Serial.printf("Buttons: Opened modal for Type=%d, Id=%d\n", modalType, modalId);
        }
      } else if (up || down) {
        startTftWrite();
        if (modalType != 0) {
          myTft.clearHighlightCell(modalType, modalId);
        }
        
        int currentIdx = 0;
        if (modalType == 1) {
          currentIdx = modalId - 1; // 0 to 15
        } else if (modalType == 2) {
          currentIdx = 15 + modalId; // 16 to 19
        }
        
        if (down) {
          currentIdx = (currentIdx + 1) % 20;
        } else if (up) {
          currentIdx = (currentIdx - 1 + 20) % 20;
        }
        
        if (currentIdx < 16) {
          modalType = 1;
          modalId = currentIdx + 1;
        } else {
          modalType = 2;
          modalId = currentIdx - 15;
        }
        
        myTft.highlightCell(modalType, modalId);
        myTft.updateCells(cells, cell_temps);
        endTftWrite();
        
        Serial.printf("Buttons: Moved selection to Type=%d, Id=%d\n", modalType, modalId);
      }
    }
  }
}