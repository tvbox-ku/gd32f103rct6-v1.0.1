// ============================================================
// Custom User_Setup.h for GD32F103RCT6 + 16-bit Parallel TFT
// ============================================================
// Pin mapping:
//   PB0~PB7   → TFT D0~D7  (8-bit data bus, lower 8 bits)
//   PB8~PB15  → TFT D8~D15 (held LOW in 8-bit mode)
//   PA2       → TFT_DC  (Data/Command)
//   PA4       → TFT_CS  (Chip Select)
//   PA5       → TFT_WR  (Write strobe)
//   PA3       → TFT_RD  (Read strobe)
//   PA1       → TFT_RST (Reset)
// ============================================================

#define USER_SETUP_INFO "GD32F103RCT6 TFT Parallel"

// Define STM32 to invoke optimised processor support (GD32 is STM32-compatible)
#define STM32

// ============================================================
// Section 1. Display driver and interface mode
// ============================================================

// Use 8-bit parallel interface (16-bit parallel is RP2040 only)
#define TFT_PARALLEL_8_BIT

// Use Port B pins 0-7 as data bus for optimised writes
// This enables single-cycle BSRR writes: GPIOB->BSRR = (0x00FF0000 | data)
#define STM_PORTB_DATA_BUS

// Display driver: change this if your LCD uses a different controller
// 3.5寸 TFT 常用驱动: ILI9488 (最常见), ILI9486, ILI9481
#define ILI9488_DRIVER
//#define ILI9341_DRIVER
//#define ILI9486_DRIVER
//#define ILI9481_DRIVER
//#define ST7796_DRIVER

// ============================================================
// Section 2. Pin definitions
// ============================================================

// Control pins
#define TFT_DC   PA2   // Data/Command (Register Select)
#define TFT_CS   PA4   // Chip Select
#define TFT_WR   PA5   // Write strobe
#define TFT_RD   PA3   // Read strobe
#define TFT_RST  PA1   // Reset

// Data bus pins D0-D7 are implicitly PB0-PB7 via STM_PORTB_DATA_BUS

// ============================================================
// Section 3. Fonts
// ============================================================

#define LOAD_GLCD    // Font 1. Original Adafruit 8 pixel font (~1820 bytes)
#define LOAD_FONT2   // Font 2. Small 16 pixel high font (~3534 bytes)
#define LOAD_FONT4   // Font 4. Medium 26 pixel high font (~5848 bytes)
#define LOAD_FONT6   // Font 6. Large 48 pixel font (~2666 bytes)
#define LOAD_FONT7   // Font 7. 7 segment 48 pixel font (~2438 bytes)
#define LOAD_FONT8   // Font 8. Large 75 pixel font (~3256 bytes)
#define LOAD_GFXFF   // FreeFonts: 48 Adafruit_GFX free fonts FF1 to FF48

// Smooth font support (anti-aliased fonts from FLASH arrays)
#define SMOOTH_FONT

// ============================================================
// Section 4. Other options
// 开启颜色反转
#define TFT_RGB_ORDER TFT_RGB
// ============================================================

// SPI frequency (not used in parallel mode, but defined for completeness)
#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
