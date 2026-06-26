// ============================================================
// TFT_eSPI User_Setup untuk ILI9488 3.5" + ESP32
// ============================================================
// CARA PAKAI:
//   1. Buka folder library TFT_eSPI di Documents/Arduino/libraries/TFT_eSPI/
//   2. Salin isi file ini ke "User_Setup.h" (TIMPA file aslinya)
//   3. ATAU: letakkan file ini di folder yang sama, lalu edit
//      "User_Setup_Select.h" agar mengarah ke file ini
// ============================================================

//─── Driver & Mode ───────────────────────────────────────────
#define ILI9488_DRIVER
#define ESP32_PARALLEL
#define TFT_PARALLEL_8_BIT

//─── Pinout Parallel (Sesuai Gambar Referensi) ───────────────
#define TFT_CS   33  // LCD_CS
#define TFT_DC   15  // LCD_RS (Data/Command)
#define TFT_RST  32  // LCD_RST
#define TFT_WR    4  // LCD_WR
// #define TFT_RD    2  // LCD_RD

#define TFT_D0   12
#define TFT_D1   13
#define TFT_D2   26
#define TFT_D3   25
#define TFT_D4   17
#define TFT_D5   16
#define TFT_D6   27
#define TFT_D7   14

// Touch CS (Jangan diaktifkan di sini karena bentrok dengan mode Parallel)
// #define TOUCH_CS  21 

//─── Font ─────────────────────────────────────────────────────
#define LOAD_GLCD    // Font 1: GLCD 5x7
#define LOAD_FONT2   // Font 2: small 14px
#define LOAD_FONT4   // Font 4: medium 26px
#define LOAD_FONT6   // Font 6: large 48px digits
#define LOAD_FONT7   // Font 7: 7-segment 48px
#define LOAD_FONT8   // Font 8: large 75px
#define LOAD_GFXFF   // GFX Free Fonts

#define SMOOTH_FONT

//─── SPI Frequency ────────────────────────────────────────────
#define SPI_FREQUENCY       27000000   // 27 MHz (aman untuk ILI9488)
#define SPI_READ_FREQUENCY  20000000   // Read lebih lambat
#define SPI_TOUCH_FREQUENCY  2500000   // Touch lebih lambat
    