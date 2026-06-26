#ifndef BESS_TFT_H
#define BESS_TFT_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// Definisikan Palet Warna RGB565 Premium - Tema Gelap (Dark Mode)
#define COLOR_BG        0x0821  // Background Space Navy sangat gelap
#define COLOR_CARD      0x1063  // Background Kartu (Biru-abu gelap elegan)
#define COLOR_BORDER    0x2125  // Garis tepi kartu
#define COLOR_CIRCUIT   0x18E4  // Jalur sirkuit redup
#define COLOR_TEXT      0xFFFF  // Teks utama Putih bersih
#define COLOR_TEXT_DIM  0x9D13  // Teks redup (Ice Grey)
#define COLOR_CYAN      0x07FF  // Sian Terang Kontras
#define COLOR_CYAN_BG   0x018C  // Sian redup/sangat gelap background
#define COLOR_ORANGE    0xFD20  // Oranye Terang Kontras
#define COLOR_ORANGE_BG 0x30E0  // Oranye sangat redup/gelap background
#define COLOR_BLUE      0x3DFF  // Biru Terang untuk SOC
#define COLOR_BLUE_BG   0x01C8  // Biru redup background
#define COLOR_PURPLE    0xCCFF  // Ungu Terang Kontras
#define COLOR_PURPLE_BG 0x280C  // Ungu redup background
#define COLOR_GREEN     0x3FE0  // Hijau Neon Terang Kontras Premium (Futuristic Green)
#define COLOR_WHITE     0xFFFF
#define COLOR_ALERT_RED 0xFA48  // Neon Vermilion Red (High Contrast against Space Navy)

class BessTFT {
public:
    void init();
    void drawEcoLogo();
    void drawStatusLogo();
    void drawStaticDashboard();
    void updateDashboard(float voltage, float current, float soc, float soh, float temp, float power, bool online = true, bool forceRedraw = false);
    void drawStaticCells(float cells[16]);
    void updateCells(float cells[16], float cell_temps[4]);
    void drawModal(int type, int id, float cells[16], float cell_temps[4]);
    void updateModalContent(int type, int id, float cells[16], float cell_temps[4]);
    void highlightCell(int type, int id);
    void clearHighlightCell(int type, int id);
    void drawCircuitLines();
    void updateCircuitAnimation(int &animStep, int &prev_x1, int &prev_y1, int &prev_x2, int &prev_y2, int &prev_x3, int &prev_y3, int &prev_x4, int &prev_y4);
    void drawArc(int x, int y, int r_in, int r_out, int start_angle, int end_angle, uint16_t color);
    void clearScreen();
};

#endif // BESS_TFT_H
