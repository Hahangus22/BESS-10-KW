#include "bess_tft.h"

// Instantiate the TFT_eSPI object locally in this compilation unit
static TFT_eSPI tft = TFT_eSPI();

// Extern declarations for BMS error/status flags
extern bool errUnderVoltage;
extern bool errOverVoltage;
extern bool errOverTemperature;
extern bool errOverCurrentCharge;
extern bool errOverCurrentDischarge;
extern bool errShortCircuit;
extern bool errAngsuran;
extern bool errPrecharge;
extern int cell_indices[16];

void BessTFT::init() {
    tft.begin();
    tft.setRotation(1); // Landscape mode (480x320)
    tft.fillScreen(COLOR_BG);
}

void BessTFT::clearScreen() {
    tft.fillScreen(TFT_BLACK);
}

// Helper Fungsi untuk Menggambar Arc yang Halus & Tebal
void BessTFT::drawArc(int x, int y, int r_in, int r_out, int start_angle, int end_angle, uint16_t color) {
  if (start_angle > end_angle) {
    int temp = start_angle;
    start_angle = end_angle;
    end_angle = temp;
  }
  for (int a = start_angle; a < end_angle; a += 3) {
    float rad1 = (a - 90) * 0.0174532925;
    float rad2 = (min(a + 3, end_angle) - 90) * 0.0174532925;
    
    float cos1 = cos(rad1);
    float sin1 = sin(rad1);
    float cos2 = cos(rad2);
    float sin2 = sin(rad2);
    
    int x1_in = x + r_in * cos1;
    int y1_in = y + r_in * sin1;
    int x1_out = x + r_out * cos1;
    int y1_out = y + r_out * sin1;
    
    int x2_in = x + r_in * cos2;
    int y2_in = y + r_in * sin2;
    int x2_out = x + r_out * cos2;
    int y2_out = y + r_out * sin2;
    
    tft.fillTriangle(x1_in, y1_in, x1_out, y1_out, x2_in, y2_in, color);
    tft.fillTriangle(x1_out, y1_out, x2_out, y2_out, x2_in, y2_in, color);
  }
}

// Gambar Aksen Dekorasi Jalur Sirkuit Elektronik
void BessTFT::drawCircuitLines() {
  // Jalur Kiri Atas ke Tengah (Tegangan ke SOC)
  tft.drawLine(143, 115, 180, 115, COLOR_CIRCUIT);
  tft.drawLine(180, 115, 210, 145, COLOR_CIRCUIT);
  tft.fillCircle(180, 115, 3, COLOR_CIRCUIT);
  
  // Jalur Kanan Atas ke Tengah (Arus ke SOC)
  tft.drawLine(337, 115, 300, 115, COLOR_CIRCUIT);
  tft.drawLine(300, 115, 270, 145, COLOR_CIRCUIT);
  tft.fillCircle(300, 115, 3, COLOR_CIRCUIT);
  
  // Jalur Kiri Bawah ke Tengah (Suhu ke SOC)
  tft.drawLine(143, 255, 180, 255, COLOR_CIRCUIT);
  tft.drawLine(180, 255, 210, 225, COLOR_CIRCUIT);
  tft.fillCircle(180, 255, 3, COLOR_CIRCUIT);
  
  // Jalur Kanan Bawah ke Tengah (Daya ke SOC)
  tft.drawLine(337, 255, 300, 255, COLOR_CIRCUIT);
  tft.drawLine(300, 255, 270, 225, COLOR_CIRCUIT);
}

void BessTFT::updateCircuitAnimation(int &animStep, int &prev_x1, int &prev_y1, int &prev_x2, int &prev_y2, int &prev_x3, int &prev_y3, int &prev_x4, int &prev_y4) {
  // 1. Hapus dot lama dengan menggambar lingkaran putih (COLOR_BG) di posisi sebelumnya
  if (prev_x1 > 0) {
    tft.fillCircle(prev_x1, prev_y1, 2, COLOR_BG);
    tft.fillCircle(prev_x2, prev_y2, 2, COLOR_BG);
    tft.fillCircle(prev_x3, prev_y3, 2, COLOR_BG);
    tft.fillCircle(prev_x4, prev_y4, 2, COLOR_BG);
  }

  // 2. Gambar ulang garis sirkuit abu-abu untuk memulihkan piksel garis yang terhapus
  drawCircuitLines();
  
  // 3. Hitung koordinat dot berjalan (Langkah 0 sampai 9)
  // Kiri Atas (Tegangan -> SOC)
  int x1 = 0, y1 = 0;
  if (animStep < 5) {
    x1 = 143 + animStep * 7;
    y1 = 115;
  } else {
    int ds = animStep - 5;
    x1 = 180 + ds * 6;
    y1 = 115 + ds * 6;
  }
  
  // Kanan Atas (Arus -> SOC)
  int x2 = 0, y2 = 0;
  if (animStep < 5) {
    x2 = 337 - animStep * 7;
    y2 = 115;
  } else {
    int ds = animStep - 5;
    x2 = 300 - ds * 6;
    y2 = 115 + ds * 6;
  }
  
  // Kiri Bawah (Suhu -> SOC)
  int x3 = 0, y3 = 0;
  if (animStep < 5) {
    x3 = 143 + animStep * 7;
    y3 = 255;
  } else {
    int ds = animStep - 5;
    x3 = 180 + ds * 6;
    y3 = 255 - ds * 6;
  }
  
  // Kanan Bawah (Daya -> SOC)
  int x4 = 0, y4 = 0;
  if (animStep < 5) {
    x4 = 337 - animStep * 7;
    y4 = 255;
  } else {
    int ds = animStep - 5;
    x4 = 300 - ds * 6;
    y4 = 255 - ds * 6;
  }
  
  // Only draw and update prev coordinates if in safe range (1 to 5) to prevent overlapping gauge borders
  if (animStep >= 1 && animStep <= 5) {
    tft.fillCircle(x1, y1, 2, COLOR_CYAN);
    tft.fillCircle(x2, y2, 2, COLOR_ORANGE);
    tft.fillCircle(x3, y3, 2, COLOR_CYAN);
    tft.fillCircle(x4, y4, 2, COLOR_PURPLE);
    
    prev_x1 = x1; prev_y1 = y1;
    prev_x2 = x2; prev_y2 = y2;
    prev_x3 = x3; prev_y3 = y3;
    prev_x4 = x4; prev_y4 = y4;
  } else {
    prev_x1 = 0; prev_x2 = 0; prev_x3 = 0; prev_x4 = 0;
  }

  // Maju ke step berikutnya (0 s/d 9)
  animStep = (animStep + 1) % 10;
}

void BessTFT::drawEcoLogo() {
  tft.drawCircle(30, 32, 16, COLOR_GREEN);
  tft.drawCircle(30, 32, 15, COLOR_GREEN);
  tft.fillTriangle(30, 22, 24, 32, 29, 32, COLOR_ORANGE);
  tft.fillTriangle(29, 32, 35, 32, 30, 42, COLOR_ORANGE);
}

void BessTFT::drawStatusLogo() {
  tft.drawCircle(450, 32, 16, COLOR_CYAN);
  tft.drawCircle(450, 32, 15, COLOR_CYAN);
  tft.fillRect(444, 28, 12, 8, COLOR_TEXT);
  tft.fillRect(448, 25, 2, 3, COLOR_TEXT);
  tft.fillRect(452, 25, 2, 3, COLOR_TEXT);
  tft.drawLine(450, 36, 450, 42, COLOR_TEXT);
}

// Menggambar Seluruh Frame Statis Dashboard Utama
void BessTFT::drawStaticDashboard() {
  tft.fillScreen(COLOR_BG);
  
  drawCircuitLines();
  drawEcoLogo();
  drawStatusLogo();
  
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextFont(4);
  tft.drawString("ELBI BESS 10kW", 240, 32);
  
  tft.setTextFont(2);
  tft.setTextColor(COLOR_CYAN);
  tft.drawString("Tegangan", 95, 52);
  
  tft.setTextColor(COLOR_ORANGE);
  tft.drawString("Arus", 385, 52);
  
  tft.setTextColor(COLOR_BLUE);
  tft.drawString("State of Charge", 240, 75);
  
  tft.setTextColor(COLOR_CYAN);
  tft.drawString("Suhu", 95, 192);
  
  tft.setTextColor(COLOR_PURPLE);
  tft.drawString("Daya", 385, 192);

  // Draw full static rings once
  drawArc(95, 115, 38, 48, 130, 410, COLOR_CYAN);
  drawArc(385, 115, 38, 48, 310, 590, COLOR_ORANGE);
  drawArc(240, 160, 60, 70, 0, 360, COLOR_BLUE);
  drawArc(95, 255, 38, 48, 130, 410, COLOR_CYAN);
  drawArc(385, 255, 38, 48, 310, 590, COLOR_PURPLE);
}

void BessTFT::updateDashboard(float voltage, float current, float soc, float soh, float temp, float power, bool online, bool forceRedraw) {
  tft.setTextDatum(MC_DATUM);
  
  static float prevVoltage = -1.0;
  static float prevCurrent = -999.0;
  static float prevSOC = -1.0;
  static float prevTemp = -1.0;
  static float prevPower = -1.0;
  static bool prevOnline = false;
  static String prevStatusStr = "";
  static uint16_t prevStatusColor = 0;
  
  if (forceRedraw) {
    prevVoltage = -1.0;
    prevCurrent = -999.0;
    prevSOC = -1.0;
    prevTemp = -1.0;
    prevPower = -1.0;
    prevOnline = false;
    prevStatusStr = "";
    prevStatusColor = 0;
  }
  
  if (!online) {
    if (prevOnline != online || forceRedraw) {
      prevOnline = online;
      prevSOC = -1.0;
      prevVoltage = -1.0;
      prevCurrent = -999.0;
      prevTemp = -1.0;
      prevPower = -1.0;
      prevStatusStr = "";
      prevStatusColor = 0;
      
      // 1. GAUGE TEGANGAN (Center: 95, 115)
      tft.fillRect(67, 93, 56, 42, COLOR_BG);
      tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
      tft.setTextFont(4);
      tft.drawString("---", 95, 115);
      
      // 2. GAUGE ARUS (Center: 385, 115)
      tft.fillRect(357, 93, 56, 42, COLOR_BG);
      tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
      tft.setTextFont(4);
      tft.drawString("---", 385, 115);
      
      // 3. GAUGE SOC (Center: 240, 160)
      tft.fillRect(200, 135, 80, 48, COLOR_BG);
      tft.setTextColor(TFT_RED, COLOR_BG);
      tft.setTextFont(4);
      tft.drawString("OFFLINE", 240, 160);
      
      // Clear status/capacity area when offline
      tft.fillRect(140, 235, 200, 48, COLOR_BG);
      tft.setTextFont(2);
      tft.setTextColor(TFT_RED, COLOR_BG);
      tft.drawString("BMS DISCONNECTED", 240, 256);
      
      // 4. GAUGE SUHU (Center: 95, 255)
      tft.fillRect(67, 233, 56, 42, COLOR_BG);
      tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
      tft.setTextFont(4);
      tft.drawString("---", 95, 255);
      
      // 5. GAUGE DAYA (Center: 385, 255)
      tft.fillRect(357, 233, 56, 42, COLOR_BG);
      tft.setTextColor(COLOR_TEXT_DIM, COLOR_BG);
      tft.setTextFont(4);
      tft.drawString("---", 385, 255);
    }
    return;
  }

  prevOnline = online;

  // 1. GAUGE TEGANGAN (Center: 95, 115)
  if (abs(voltage - prevVoltage) >= 0.05 || forceRedraw) {
    prevVoltage = voltage;
    tft.fillRect(67, 93, 56, 42, COLOR_BG);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextFont(4);
    tft.drawString(String(voltage, 1), 95, 107);
    tft.setTextFont(2);
    tft.drawString("V", 95, 126);
  }
  
  // 2. GAUGE ARUS (Center: 385, 115)
  if (abs(current - prevCurrent) >= 0.05 || forceRedraw) {
    prevCurrent = current;
    tft.fillRect(357, 93, 56, 42, COLOR_BG);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextFont(4);
    
    // Format current string with explicit '+' or '-'
    String currentStr = "";
    if (current > 0.01) {
      currentStr = "+" + String(current, 1);
    } else if (current < -0.01) {
      currentStr = String(current, 1); // Negative sign included automatically
    } else {
      currentStr = "0.0";
    }
    tft.drawString(currentStr, 385, 107);
    tft.setTextFont(2);
    tft.drawString("A", 385, 126);
  }
  
  // 3. GAUGE SOC & CAPACITY (Center: 240, 160)
  if ((int)soc != (int)prevSOC || forceRedraw) {
    prevSOC = soc;
    
    // Clear only the text region inside the circle (never touches the arc)
    tft.fillRect(200, 135, 80, 48, COLOR_BG);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextFont(7);
    tft.drawString(String((int)soc), 230, 158);
    tft.setTextFont(4);
    tft.drawString("%", 268, 164);
    
    // Clear stats & capacity area below the circle (never touches the arc)
    tft.fillRect(140, 235, 200, 48, COLOR_BG);
    
    // Draw remaining Capacity instead of SOH, formatted as "176 | 200 Ah"
    tft.setTextFont(2);
    tft.setTextColor(COLOR_CYAN, COLOR_BG);
    tft.drawString(String((soc / 100.0) * 200.0, 0) + " | 200 Ah", 240, 246);
  }

  // 4. GAUGE SUHU (Center: 95, 255)
  if ((int)temp != (int)prevTemp || forceRedraw) {
    prevTemp = temp;
    tft.fillRect(67, 233, 56, 42, COLOR_BG);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextFont(4);
    tft.drawString(String((int)temp), 95, 247);
    tft.setTextFont(2);
    tft.drawString("C", 95, 266);
  }

  // 5. GAUGE DAYA (Center: 385, 255)
  if (abs(power - prevPower) >= 0.05 || forceRedraw) {
    prevPower = power;
    tft.fillRect(357, 233, 56, 42, COLOR_BG);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextFont(4);
    tft.drawString(String(abs(power), 2), 385, 247);
    tft.setTextFont(2);
    tft.drawString("kW", 385, 266);
  }

  // 6. STATUS / ERROR DISPLAY
  String statusStr = "";
  uint16_t statusColor = COLOR_GREEN;
  
  if (errShortCircuit) {
    if (statusStr != "") statusStr += " | ";
    statusStr += "SHORT CIRCUIT";
    statusColor = COLOR_ALERT_RED;
  }
  if (errOverTemperature) {
    if (statusStr != "") statusStr += " | ";
    statusStr += "OVER TEMP";
    statusColor = COLOR_ALERT_RED;
  }
  if (errOverVoltage) {
    if (statusStr != "") statusStr += " | ";
    statusStr += "OVER VOLTAGE";
    statusColor = COLOR_ALERT_RED;
  }
  if (errUnderVoltage) {
    if (statusStr != "") statusStr += " | ";
    statusStr += "UNDER VOLTAGE";
    statusColor = COLOR_ALERT_RED;
  }
  if (errOverCurrentDischarge) {
    if (statusStr != "") statusStr += " | ";
    statusStr += "OVER CURR DSG";
    statusColor = COLOR_ALERT_RED;
  }
  if (errOverCurrentCharge) {
    if (statusStr != "") statusStr += " | ";
    statusStr += "OVER CURR CHG";
    statusColor = COLOR_ALERT_RED;
  }
  if (errAngsuran) {
    if (statusStr != "") statusStr += " | ";
    statusStr += "ANGSURAN ALERT";
    if (statusColor != COLOR_ALERT_RED) {
      statusColor = COLOR_ORANGE;
    }
  }
  if (errPrecharge) {
    if (statusStr != "") statusStr += " | ";
    statusStr += "PRECHARGING";
    if (statusColor != COLOR_ALERT_RED && statusColor != COLOR_ORANGE) {
      statusColor = COLOR_CYAN;
    }
  }
  
  if (statusStr == "") {
    statusStr = "SYSTEM NORMAL";
    statusColor = COLOR_GREEN;
  }
  
  if (statusStr != prevStatusStr || statusColor != prevStatusColor || forceRedraw) {
    prevStatusStr = statusStr;
    prevStatusColor = statusColor;
    
    tft.fillRect(100, 256, 280, 20, COLOR_BG);
    tft.setTextFont(2);
    tft.setTextColor(statusColor, COLOR_BG);
    tft.drawString(statusStr, 240, 266);
  }
}

// Menggambar Seluruh Frame Statis Halaman Cells (hanya menggambar sel yang aktif)
void BessTFT::drawStaticCells(float cells[16]) {
  tft.fillScreen(COLOR_BG);
  
  // 1. Tombol BACK (Pojok Kiri Atas) - shifted up to y = 5
  tft.fillRoundRect(10, 5, 70, 30, 4, COLOR_CARD);
  tft.drawRoundRect(10, 5, 70, 30, 4, COLOR_BORDER);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.drawString("BACK", 45, 20);
  
  // 2. Judul Utama
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextFont(4);
  tft.drawString("CELL STATUS", 95, 25);
  
  // 3. Sub-judul Kiri (Sistem BMS)
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(COLOR_CYAN);
  tft.drawString("SISTEM BMS (Voltase)", 15, 48);
  
  // 4. Sub-judul Kanan (Sensor Thermal)
  tft.setTextDatum(TR_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(COLOR_ORANGE);
  tft.drawString("THERMAL (Suhu)", 465, 48);
  
  // 5. Box Sel Voltase (16 Cells: 4x4 Grid)
  int w = 80;
  int h = 46;         
  int col_space = 6;
  int row_space = 8;
  
  for (int r = 0; r < 4; r++) {
    int y = 65 + r * (h + row_space);
    for (int c = 0; c < 4; c++) {
      int x = 10 + c * (w + col_space);
      int cell_num = c * 4 + r + 1;
      
      if (cell_num <= 16) {
        tft.fillRoundRect(x, y, w, h, 4, COLOR_CARD);
        tft.drawRoundRect(x, y, w, h, 4, COLOR_BORDER);
        tft.fillRect(x + 2, y + h - 3, w - 4, 2, COLOR_CYAN);
        
        tft.setTextColor(COLOR_TEXT_DIM);
        tft.setTextFont(1); // Small Font 1 for "C1" to avoid crowding
        tft.setTextDatum(TL_DATUM);
        int idx = cell_indices[cell_num - 1];
        if (idx > 0) {
          tft.drawString("C" + String(idx), x + 4, y + 4);
        } else {
          tft.drawString("C" + String(cell_num), x + 4, y + 4);
        }
      }
    }
  }
  
  // 6. Box Sensor Suhu (4 Sensors)
  int tx = 360;
  int tw = 110;
  int th = 46;
  int t_space = 8;
  
  for (int i = 0; i < 4; i++) {
    int ty = 65 + i * (th + t_space);
    
    tft.drawRoundRect(tx, ty, tw, th, 4, COLOR_BORDER);
    tft.fillRoundRect(tx + 1, ty + 1, tw - 2, th - 2, 4, COLOR_ORANGE_BG);
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(1); // Small Font 1 for label
    tft.setTextColor(COLOR_ORANGE);
    tft.drawString("SUHU " + String(i + 1), tx + tw/2, ty + 10);
  }

  // 7. Frame Status Bar di Bawah
  tft.fillRoundRect(10, 285, 460, 28, 4, COLOR_CARD);
  tft.drawRoundRect(10, 285, 460, 28, 4, COLOR_BORDER);
  tft.drawFastVLine(120, 286, 26, COLOR_BORDER);
  tft.drawFastVLine(360, 286, 26, COLOR_BORDER);
}

void BessTFT::updateCells(float cells[16], float cell_temps[4]) {
  int w = 80;
  int h = 46;
  int col_space = 6;
  int row_space = 8;
  
  tft.setTextFont(4); // Use Font 4 for cell voltages (larger text)
  tft.setTextDatum(MC_DATUM);
  
  for (int r = 0; r < 4; r++) {
    int y = 65 + r * (h + row_space);
    for (int c = 0; c < 4; c++) {
      int x = 10 + c * (w + col_space);
      int cell_num = c * 4 + r;
      if (cell_num < 16) {
        // Redraw cell index dynamically based on cell_indices
        tft.setTextFont(1);
        tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD);
        tft.setTextDatum(TL_DATUM);
        int idx = cell_indices[cell_num];
        if (idx > 0) {
          tft.drawString("C" + String(idx) + "  ", x + 4, y + 4);
        } else {
          tft.drawString("C" + String(cell_num + 1) + "  ", x + 4, y + 4);
        }
        
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        if (cells[cell_num] >= 3.0) {
          tft.setTextColor(COLOR_TEXT, COLOR_CARD);
          tft.drawString(String(cells[cell_num], 2) + "V", x + w/2, y + 28);
        } else {
          tft.setTextColor(COLOR_TEXT_DIM, COLOR_CARD);
          tft.drawString("     ", x + w/2, y + 28); // Empty when no value
        }
      }
    }
  }
  
  int tx = 360;
  int tw = 110;
  int th = 46;
  int t_space = 8;
  
  tft.setTextFont(4); // Use Font 4 for temperatures (larger text)
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_TEXT, COLOR_ORANGE_BG);
  
  for (int i = 0; i < 4; i++) {
    int ty = 65 + i * (th + t_space);
    tft.drawString(String(cell_temps[i], 1) + " C", tx + tw/2, ty + 28);
  }
  
  // Hitung Voltase Min/Max/Avg Temp (loop 16 kali, abaikan sel kosong < 3.0V)
  float min_v = 99.0;
  float max_v = -99.0;
  int min_idx = -1;
  int max_idx = -1;
  float sum_v = 0.0;
  int valid_cell_count = 0;
  
  for (int i = 0; i < 16; i++) {
    if (cells[i] >= 3.0) {
      sum_v += cells[i];
      valid_cell_count++;
      if (cells[i] < min_v) { min_v = cells[i]; min_idx = i; }
      if (cells[i] > max_v) { max_v = cells[i]; max_idx = i; }
    }
  }
  float sum_t = 0.0;
  for (int i = 0; i < 4; i++) sum_t += cell_temps[i];
  float avg_temp = sum_t / 4.0;
  
  tft.setTextFont(2); // Use Font 2 (larger)
  tft.setTextColor(COLOR_TEXT, COLOR_CARD);
  
  // Clear previous stats text area inside the card and redraw dividers
  tft.fillRect(12, 287, 456, 24, COLOR_CARD);
  tft.drawFastVLine(120, 286, 26, COLOR_BORDER);
  tft.drawFastVLine(360, 286, 26, COLOR_BORDER);
  
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Volt: " + String(sum_v, 2) + "V", 65, 299);
  
  String stats;
  if (valid_cell_count > 0) {
    int maxPhysical = (cell_indices[max_idx] > 0) ? cell_indices[max_idx] : (max_idx + 1);
    int minPhysical = (cell_indices[min_idx] > 0) ? cell_indices[min_idx] : (min_idx + 1);
    stats = "Max: C" + String(maxPhysical) + " (" + String(max_v, 2) + "V) | Min: C" + String(minPhysical) + " (" + String(min_v, 2) + "V)";
  } else {
    stats = "No valid cells";
  }
  tft.drawString(stats, 240, 299);
  
  tft.drawString("Temp: " + String(avg_temp, 1) + " C", 415, 299);
}

void BessTFT::updateModalContent(int type, int id, float cells[16], float cell_temps[4]) {
  tft.fillRect(95, 90, 290, 112, COLOR_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_TEXT);
  
  if (type == 1) {
    float v = cells[id - 1];
    if (v >= 3.0) {
      tft.setTextFont(4);
      tft.drawString(String(v, 3) + " V", 240, 110);
      
      tft.setTextFont(2);
      String status = "NORMAL";
      uint16_t statusColor = COLOR_GREEN;
      if (v > 3.4) {
        status = "OVERVOLT";
        statusColor = COLOR_ALERT_RED;
      }
      
      tft.drawString("Status: ", 200, 150);
      tft.setTextColor(statusColor);
      tft.drawString(status, 270, 150);
      
      tft.setTextColor(COLOR_TEXT);
      tft.drawString("Kapasitas: 200 Ah", 240, 175);
    } else {
      tft.setTextFont(4);
      tft.drawString("--- V", 240, 110);
      
      tft.setTextFont(2);
      tft.drawString("Status: ", 200, 150);
      tft.setTextColor(COLOR_ALERT_RED);
      tft.drawString("DISCONNECTED", 270, 150);
      
      tft.setTextColor(COLOR_TEXT);
      tft.drawString("Kapasitas: -- Ah", 240, 175);
    }
  } else {
    tft.setTextFont(4);
    tft.drawString(String(cell_temps[id - 1], 1) + " C", 240, 110);
    
    tft.setTextFont(2);
    float t = cell_temps[id - 1];
    String status = "NORMAL";
    uint16_t statusColor = COLOR_GREEN;
    if (t > 47.0) {
      status = "OVER TEMP";
      statusColor = COLOR_ALERT_RED;
    }
    
    tft.drawString("Status: ", 200, 148);
    tft.setTextColor(statusColor);
    tft.drawString(status, 270, 148);
    
    tft.setTextColor(COLOR_TEXT);
    tft.drawString("Kondisi: STABIL", 240, 168);
    tft.drawString("Posisi: MODUL " + String(id), 240, 188);
  }
}

void BessTFT::drawModal(int type, int id, float cells[16], float cell_temps[4]) {
  tft.drawRoundRect(89, 49, 302, 222, 9, COLOR_BORDER);
  tft.fillRoundRect(90, 50, 300, 220, 8, COLOR_BG);
  tft.drawRoundRect(90, 50, 300, 220, 8, type == 1 ? COLOR_CYAN : COLOR_ORANGE);
  tft.fillRoundRect(92, 52, 296, 35, 6, type == 1 ? COLOR_CYAN : COLOR_ORANGE);
  
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COLOR_WHITE);
  tft.setTextFont(2);
  if (type == 1) {
    tft.drawString("DETAIL CELL C" + String(id), 240, 69);
  } else {
    tft.drawString("DETAIL SENSOR SUHU " + String(id), 240, 69);
  }
  
  updateModalContent(type, id, cells, cell_temps);
  
  tft.fillRoundRect(180, 205, 120, 35, 6, COLOR_TEXT);
  tft.setTextColor(COLOR_BG);
  tft.setTextFont(2);
  tft.drawString("TUTUP", 240, 222);
}

void BessTFT::highlightCell(int type, int id) {
  if (type == 1) { // Cell
    int w = 80;
    int h = 46;
    int col_space = 6;
    int row_space = 8;
    int c = (id - 1) / 4;
    int r = (id - 1) % 4;
    int x = 10 + c * (w + col_space);
    int y = 65 + r * (h + row_space);
    
    // Draw a thick cyan highlight border around the pressed cell box
    tft.drawRoundRect(x, y, w, h, 4, COLOR_CYAN);
    tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 4, COLOR_CYAN);
  } else if (type == 2) { // Temp
    int tx = 360;
    int tw = 110;
    int th = 46;
    int t_space = 8;
    int ty = 65 + (id - 1) * (th + t_space);
    
    // Draw a thick orange highlight border around the pressed temp box
    tft.drawRoundRect(tx, ty, tw, th, 4, COLOR_ORANGE);
    tft.drawRoundRect(tx + 1, ty + 1, tw - 2, th - 2, 4, COLOR_ORANGE);
  }
}

void BessTFT::clearHighlightCell(int type, int id) {
  if (type == 1) { // Cell
    int w = 80;
    int h = 46;
    int col_space = 6;
    int row_space = 8;
    int c = (id - 1) / 4;
    int r = (id - 1) % 4;
    int x = 10 + c * (w + col_space);
    int y = 65 + r * (h + row_space);
    
    // Restore original borders
    tft.drawRoundRect(x, y, w, h, 4, COLOR_BORDER);
    tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 4, COLOR_CARD);
  } else if (type == 2) { // Temp
    int tx = 360;
    int tw = 110;
    int th = 46;
    int t_space = 8;
    int ty = 65 + (id - 1) * (th + t_space);
    
    // Restore original borders
    tft.drawRoundRect(tx, ty, tw, th, 4, COLOR_BORDER);
    tft.drawRoundRect(tx + 1, ty + 1, tw - 2, th - 2, 4, COLOR_ORANGE_BG);
  }
}
