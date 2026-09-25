/*
  Portable Backgammon Match Clock and Scoreboard
  Target: Elecrow CrowPanel ESP32 HMI 5.0-inch display (DIS07050H)

  Display library:
    This sketch uses the ESP32-S3 RGB LCD peripheral for the built-in 800x480
    RGB-parallel panel.

  Clock model:
    - At the start of each game, no clock is running.
    - Pressing Player A's large button ends A's turn and starts B's clock.
    - Pressing Player B's large button ends B's turn and starts A's clock.
    - Optional delay/increment is supported. Delay is Bronstein/simple delay:
      main time does not tick down until the active turn exceeds delaySeconds.
    - Increment, if enabled, is added to the player whose turn was ended.

  Crawford model:
    - When either player reaches match_target - 1 and Crawford has not been used,
      the following game is marked Crawford and the cube is disabled.
    - After that game ends, all later games are post-Crawford and cube is enabled.
*/

#include <Arduino.h>
#include <Wire.h>
#include <stdarg.h>
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include "splash_image.h"

#if __has_include(<gfxfont.h>)
#include <gfxfont.h>
#else
typedef struct {
  uint16_t bitmapOffset;
  uint8_t width;
  uint8_t height;
  uint8_t xAdvance;
  int8_t xOffset;
  int8_t yOffset;
} GFXglyph;

typedef struct {
  uint8_t *bitmap;
  GFXglyph *glyph;
  uint16_t first;
  uint16_t last;
  uint16_t yAdvance;
} GFXfont;
#endif

#if __has_include("7segment48pt7b_numbers.h")
#include "7segment48pt7b_numbers.h"
#define BGMC_HAS_CLOCK_FONT 1
#define BGMC_CLOCK_FONT SevenSegment48pt7bNumbers
#elif __has_include("Seven_Segment48pt7b_numbers.h")
#include "Seven_Segment48pt7b_numbers.h"
#define BGMC_HAS_CLOCK_FONT 1
#define BGMC_CLOCK_FONT Seven_Segment48pt7bNumbers
#elif __has_include("Seven_Segment48pt7b.h")
#include "Seven_Segment48pt7b.h"
#define BGMC_HAS_CLOCK_FONT 1
#define BGMC_CLOCK_FONT Seven_Segment48pt7b
#elif __has_include("Seven_Segment24pt7b.h")
#include "Seven_Segment24pt7b.h"
#define BGMC_HAS_CLOCK_FONT 1
#define BGMC_CLOCK_FONT Seven_Segment24pt7b
#elif __has_include("FreeMonoBold24pt7b.h")
#include "FreeMonoBold24pt7b.h"
#define BGMC_HAS_CLOCK_FONT 1
#define BGMC_CLOCK_FONT FreeMonoBold24pt7b
#elif __has_include(<Fonts/FreeMonoBold24pt7b.h>)
#include <Fonts/FreeMonoBold24pt7b.h>
#define BGMC_HAS_CLOCK_FONT 1
#define BGMC_CLOCK_FONT FreeMonoBold24pt7b
#else
#define BGMC_HAS_CLOCK_FONT 0
#endif

#if __has_include("FreeSans18pt7b.h")
#include "FreeSans18pt7b.h"
#define BGMC_HAS_UI_FONT 1
#elif __has_include(<Fonts/FreeSans18pt7b.h>)
#include <Fonts/FreeSans18pt7b.h>
#define BGMC_HAS_UI_FONT 1
#else
#define BGMC_HAS_UI_FONT 0
#endif

#if __has_include("FreeSans12pt7b.h")
#include "FreeSans12pt7b.h"
#define BGMC_HAS_SMALL_UI_FONT 1
#elif __has_include(<Fonts/FreeSans12pt7b.h>)
#include <Fonts/FreeSans12pt7b.h>
#define BGMC_HAS_SMALL_UI_FONT 1
#else
#define BGMC_HAS_SMALL_UI_FONT 0
#endif

#if __has_include("FreeSansBold96pt7b.h")
#include "FreeSansBold96pt7b.h"
#define BGMC_HAS_SCORE_96_FONT 1
#elif __has_include(<Fonts/FreeSansBold96pt7b.h>)
#include <Fonts/FreeSansBold96pt7b.h>
#define BGMC_HAS_SCORE_96_FONT 1
#else
#define BGMC_HAS_SCORE_96_FONT 0
#endif

#if __has_include("collegeb96pt7b_numbers.h")
#include "collegeb96pt7b_numbers.h"
#define BGMC_HAS_COLLEGE_SCORE_FONT 1
#define BGMC_COLLEGE_SCORE_FONT collegeb96pt7bNumbers
#elif __has_include("college96pt7b_numbers.h")
#include "college96pt7b_numbers.h"
#define BGMC_HAS_COLLEGE_SCORE_FONT 1
#define BGMC_COLLEGE_SCORE_FONT college96pt7bNumbers
#elif __has_include("collegeb96pt7b.h")
#include "collegeb96pt7b.h"
#define BGMC_HAS_COLLEGE_SCORE_FONT 1
#define BGMC_COLLEGE_SCORE_FONT collegeb96pt7b
#else
#define BGMC_HAS_COLLEGE_SCORE_FONT 0
#endif

#if __has_include("FreeSansBold48pt7b_numbers.h")
#include "FreeSansBold48pt7b_numbers.h"
#define BGMC_HAS_MATCH_TARGET_48_FONT 1
#define BGMC_MATCH_TARGET_48_FONT FreeSansBold48pt7bNumbers
#elif __has_include("FreeSansBold48pt7b.h")
#include "FreeSansBold48pt7b.h"
#define BGMC_HAS_MATCH_TARGET_48_FONT 1
#define BGMC_MATCH_TARGET_48_FONT FreeSansBold48pt7b
#elif __has_include(<Fonts/FreeSansBold48pt7b.h>)
#include <Fonts/FreeSansBold48pt7b.h>
#define BGMC_HAS_MATCH_TARGET_48_FONT 1
#define BGMC_MATCH_TARGET_48_FONT FreeSansBold48pt7b
#else
#define BGMC_HAS_MATCH_TARGET_48_FONT 0
#endif

#if __has_include("FreeSansBold24pt7b.h")
#include "FreeSansBold24pt7b.h"
#define BGMC_HAS_SCORE_FONT 1
#elif __has_include(<Fonts/FreeSansBold24pt7b.h>)
#include <Fonts/FreeSansBold24pt7b.h>
#define BGMC_HAS_SCORE_FONT 1
#else
#define BGMC_HAS_SCORE_FONT 0
#endif

// ---------------------------------------------------------------------------
// DIS07050H display-first pin map. External buttons can be added later on
// available expansion pins; avoid the LCD RGB bus pins below.
// ---------------------------------------------------------------------------
static const uint8_t PIN_PLAYER_A_BUTTON = 38; // GPIO_D IO38: one-button clock toggle to GND.
static const uint8_t PIN_PLAYER_B_BUTTON = 255;
static const uint8_t PIN_SCORE_A_PLUS = 255;
static const uint8_t PIN_SCORE_A_MINUS = 255;
static const uint8_t PIN_SCORE_B_PLUS = 255;
static const uint8_t PIN_SCORE_B_MINUS = 255;
static const uint8_t PIN_MENU_SELECT = 255;
static const uint8_t PIN_MENU_UP = 255;
static const uint8_t PIN_MENU_DOWN = 255;
static const uint8_t PIN_BUZZER = 255;
static const uint8_t PIN_TFT_BACKLIGHT = 2;
static const uint8_t PIN_BATTERY_ADC = 255;
static const uint8_t PIN_CHARGE_DETECT = 255; // Optional charger STAT pin, active-low when wired.

static const uint16_t PANEL_WIDTH = 800;
static const uint16_t PANEL_HEIGHT = 480;
static const uint8_t PIN_LCD_D0 = 8;
static const uint8_t PIN_LCD_D1 = 3;
static const uint8_t PIN_LCD_D2 = 46;
static const uint8_t PIN_LCD_D3 = 9;
static const uint8_t PIN_LCD_D4 = 1;
static const uint8_t PIN_LCD_D5 = 5;
static const uint8_t PIN_LCD_D6 = 6;
static const uint8_t PIN_LCD_D7 = 7;
static const uint8_t PIN_LCD_D8 = 15;
static const uint8_t PIN_LCD_D9 = 16;
static const uint8_t PIN_LCD_D10 = 4;
static const uint8_t PIN_LCD_D11 = 45;
static const uint8_t PIN_LCD_D12 = 48;
static const uint8_t PIN_LCD_D13 = 47;
static const uint8_t PIN_LCD_D14 = 21;
static const uint8_t PIN_LCD_D15 = 14;
static const uint8_t PIN_LCD_DE = 40;
static const uint8_t PIN_LCD_VSYNC = 41;
static const uint8_t PIN_LCD_HSYNC = 39;
static const uint8_t PIN_LCD_PCLK = 0;
static const uint8_t PIN_I2C_SDA = 19;
static const uint8_t PIN_I2C_SCL = 20;
static const uint8_t PCF8575_PLAY_PAUSE_PIN = 0; // PCF8575 P00, button to GND.
static const uint8_t PCF8575_PLAYER_A_PIN = 1; // PCF8575 P01, button to GND.
static const uint8_t PCF8575_PLAYER_B_PIN = 7; // PCF8575 P07, button to GND.
static const uint8_t MAX17048_I2C_ADDRESS = 0x36;
static const uint8_t DS3231_I2C_ADDRESS = 0x68;

// Rotary encoder placeholders. This sketch uses up/down buttons by default.
static const uint8_t PIN_ENCODER_A = 255;
static const uint8_t PIN_ENCODER_B = 255;

// Battery divider calibration placeholders.
static const float BATTERY_ADC_REF_VOLTS = 3.30f;
static const float BATTERY_DIVIDER_RATIO = 2.00f; // Vbat = Vadc * ratio.
static const float BATTERY_EMPTY_VOLTS = 3.30f;
static const float BATTERY_FULL_VOLTS = 4.20f;

// Display/backlight defaults.
static const uint8_t DEFAULT_BRIGHTNESS = 210; // 0..255 PWM duty.
static const bool TFT_BACKLIGHT_ACTIVE_HIGH = true;
static const uint16_t ILI9341_BLACK = 0x0000;
static const uint16_t ILI9341_WHITE = 0xFFFF;
static const uint16_t ILI9341_RED = 0xF800;
static const uint16_t ILI9341_GREEN = 0x07E0;
static const uint16_t ILI9341_BLUE = 0x001F;
static const uint16_t ILI9341_CYAN = 0x07FF;
static const uint16_t ILI9341_YELLOW = 0xFFE0;
static const uint16_t TFT_BACKGROUND = ILI9341_BLACK;
static const uint16_t TFT_PANEL = 0x2104;
static const uint16_t TFT_PANEL_ALT = 0x3186;
static const uint16_t TFT_BORDER = 0x7BEF;
static const uint16_t TFT_BUTTON_BORDER = 0xD6BA;
static const uint16_t TFT_TEXT = ILI9341_WHITE;
static const uint16_t TFT_TEXT_DARK = ILI9341_BLACK;
static const uint16_t TFT_ACTIVE_TEXT = 0x4208;
static const uint16_t TFT_MUTED = 0xBDF7;
static const uint16_t TFT_ACCENT_A = ILI9341_CYAN;
static const uint16_t TFT_ACCENT_B = 0xFD20;
static const uint16_t TFT_MAGENTA = 0xF81F;
static const uint16_t TFT_WARN = ILI9341_YELLOW;
static const uint16_t TFT_DANGER = ILI9341_RED;
static const uint16_t TFT_OK = ILI9341_GREEN;
static const uint16_t TFT_TIMER_WARNING_BG = 0xFBCF; // Light red for the final 90 seconds.
static const uint16_t PLAYER_COLOR_RED = 0x7800;    // #7C0A02 approximated in RGB565.
static const uint16_t PLAYER_COLOR_BLUE = 0x194A;   // #1C2951 approximated in RGB565.
static const uint16_t PLAYER_COLOR_GREEN = 0x19A5;  // #1C352D approximated in RGB565.
static const uint16_t PLAYER_COLOR_YELLOW = 0xFD20; // #FFA700 approximated in RGB565.
static const uint16_t PLAYER_COLOR_GRAY = 0x4208;
static const uint16_t PLAYER_COLOR_WHITE = ILI9341_WHITE;

// Time behavior.
static const uint32_t DEFAULT_INITIAL_TIME_SECONDS = 15UL * 60UL;
static const uint8_t DEFAULT_DELAY_SECONDS = 12;
static const uint8_t DEFAULT_INCREMENT_SECONDS = 0;
static const uint32_t LOW_TIME_WARNING_SECONDS = 30;
static const uint32_t CRITICAL_TIME_WARNING_SECONDS = 10;
static const uint32_t SLEEP_AFTER_IDLE_MS = 10UL * 60UL * 1000UL;
static const uint32_t LCD_PIXEL_CLOCK_HZ = 20000000;
static const uint16_t LCD_BOUNCE_BUFFER_LINES = 10;
static const uint8_t PLAYER_NAME_MAX_LEN = 10;

// Debounce intervals. Large buttons stay snappy; score/menu buttons are slower.
static const uint16_t PLAYER_BUTTON_DEBOUNCE_MS = 25;
static const uint16_t SMALL_BUTTON_DEBOUNCE_MS = 90;
static const uint16_t DISPLAY_REFRESH_MS = 1000;
static const uint16_t SETTINGS_SAVE_DELAY_MS = 1500;
static const char *FIRMWARE_VERSION = "lcd-refresh-2026-06-07";
static const uint8_t MATCH_LOG_MAX_ENTRIES = 8;

void logLine(const char *message);
void logPrintf(const char *format, ...);

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
enum Player : uint8_t {
  PLAYER_A = 0,
  PLAYER_B = 1,
  PLAYER_NONE = 255
};

enum TimingMode : uint8_t {
  SIMPLE_DELAY,
  BRONSTEIN_DELAY,
  FISCHER_INCREMENT
};

enum TimerScope : uint8_t {
  TIMER_BY_GAME,
  TIMER_BY_MATCH
};

enum DisplayTheme : uint8_t {
  THEME_NIGHT,
  THEME_DAY
};

enum ScoreDisplayMode : uint8_t {
  SCORE_STANDARD,
  SCORE_POINTS_TO_WIN
};

enum DeviceMode : uint8_t {
  MODE_MENU,
  MODE_MATCH,
  MODE_MATCH_OVER,
  MODE_TIMEOUT,
  MODE_NAME_EDIT,
  MODE_LOG,
  MODE_CLOCK_SET,
  MODE_APPEARANCE,
  MODE_CONFIRM_END_MATCH
};

enum MenuItem : uint8_t {
  MENU_MATCH_TARGET,
  MENU_INITIAL_TIME,
  MENU_DELAY,
  MENU_INCREMENT,
  MENU_BRIGHTNESS,
  MENU_START_MATCH,
  MENU_RESET_MATCH,
  MENU_COUNT
};

struct DebouncedButton {
  uint8_t pin;
  bool activeLow;
  bool stablePressed;
  bool lastRawPressed;
  bool pressedEvent;
  uint32_t lastChangeMs;
  uint16_t debounceMs;

  void begin(uint8_t buttonPin, uint16_t debounce, bool useActiveLow = true) {
    pin = buttonPin;
    activeLow = useActiveLow;
    stablePressed = false;
    lastRawPressed = false;
    pressedEvent = false;
    lastChangeMs = 0;
    debounceMs = debounce;

    if (pin != 255) {
      pinMode(pin, activeLow ? INPUT_PULLUP : INPUT);
    }
  }

  void update(uint32_t nowMs) {
    pressedEvent = false;
    if (pin == 255) {
      return;
    }

    bool rawPressed = activeLow ? (digitalRead(pin) == LOW) : (digitalRead(pin) == HIGH);
    if (rawPressed != lastRawPressed) {
      lastRawPressed = rawPressed;
      lastChangeMs = nowMs;
    }

    if ((nowMs - lastChangeMs) >= debounceMs && rawPressed != stablePressed) {
      stablePressed = rawPressed;
      if (stablePressed) {
        pressedEvent = true;
      }
    }
  }

  bool wasPressed() const {
    return pressedEvent;
  }
};

struct ExpanderButton {
  uint8_t bit;
  bool stablePressed;
  bool lastRawPressed;
  bool pressedEvent;
  uint32_t lastChangeMs;
  uint16_t debounceMs;

  void begin(uint8_t expanderBit, uint16_t debounce) {
    bit = expanderBit;
    stablePressed = false;
    lastRawPressed = false;
    pressedEvent = false;
    lastChangeMs = 0;
    debounceMs = debounce;
  }

  void update(uint32_t nowMs, uint16_t portState) {
    pressedEvent = false;
    if (bit >= 16) {
      return;
    }

    bool rawPressed = (portState & (1U << bit)) == 0;
    if (rawPressed != lastRawPressed) {
      lastRawPressed = rawPressed;
      lastChangeMs = nowMs;
    }

    if ((nowMs - lastChangeMs) >= debounceMs && rawPressed != stablePressed) {
      stablePressed = rawPressed;
      if (stablePressed) {
        pressedEvent = true;
      }
    }
  }

  bool wasPressed() const {
    return pressedEvent;
  }
};

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;

  bool contains(int16_t px, int16_t py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }

  bool containsPadded(int16_t px, int16_t py, int16_t padX, int16_t padY) const {
    return px >= x - padX && py >= y - padY && px < x + w + padX && py < y + h + padY;
  }
};

struct TouchPoint {
  bool touched;
  uint16_t x;
  uint16_t y;
};

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
class CrowPanelRGBDisplay {
public:
  CrowPanelRGBDisplay(uint8_t csPin, uint8_t dcPin, uint8_t rstPin)
    : cs(csPin), dc(dcPin), rst(rstPin) {}

  void begin() {
    (void)cs;
    (void)dc;
    (void)rst;

    esp_lcd_rgb_panel_config_t panelConfig = {};
    panelConfig.clk_src = LCD_CLK_SRC_DEFAULT;
    panelConfig.timings.pclk_hz = LCD_PIXEL_CLOCK_HZ;
    panelConfig.timings.h_res = PANEL_WIDTH;
    panelConfig.timings.v_res = PANEL_HEIGHT;
    panelConfig.timings.hsync_pulse_width = 4;
    panelConfig.timings.hsync_back_porch = 43;
    panelConfig.timings.hsync_front_porch = 8;
    panelConfig.timings.vsync_pulse_width = 4;
    panelConfig.timings.vsync_back_porch = 12;
    panelConfig.timings.vsync_front_porch = 8;
    panelConfig.timings.flags.hsync_idle_low = 0;
    panelConfig.timings.flags.vsync_idle_low = 0;
    panelConfig.timings.flags.de_idle_high = 0;
    panelConfig.timings.flags.pclk_active_neg = 1;
    panelConfig.timings.flags.pclk_idle_high = 0;
    panelConfig.data_width = 16;
    panelConfig.bits_per_pixel = 16;
    panelConfig.num_fbs = 1;
    panelConfig.bounce_buffer_size_px = PANEL_WIDTH * LCD_BOUNCE_BUFFER_LINES;
    panelConfig.dma_burst_size = 32;
    panelConfig.hsync_gpio_num = PIN_LCD_HSYNC;
    panelConfig.vsync_gpio_num = PIN_LCD_VSYNC;
    panelConfig.de_gpio_num = PIN_LCD_DE;
    panelConfig.pclk_gpio_num = PIN_LCD_PCLK;
    panelConfig.disp_gpio_num = -1;
    panelConfig.data_gpio_nums[0] = PIN_LCD_D0;
    panelConfig.data_gpio_nums[1] = PIN_LCD_D1;
    panelConfig.data_gpio_nums[2] = PIN_LCD_D2;
    panelConfig.data_gpio_nums[3] = PIN_LCD_D3;
    panelConfig.data_gpio_nums[4] = PIN_LCD_D4;
    panelConfig.data_gpio_nums[5] = PIN_LCD_D5;
    panelConfig.data_gpio_nums[6] = PIN_LCD_D6;
    panelConfig.data_gpio_nums[7] = PIN_LCD_D7;
    panelConfig.data_gpio_nums[8] = PIN_LCD_D8;
    panelConfig.data_gpio_nums[9] = PIN_LCD_D9;
    panelConfig.data_gpio_nums[10] = PIN_LCD_D10;
    panelConfig.data_gpio_nums[11] = PIN_LCD_D11;
    panelConfig.data_gpio_nums[12] = PIN_LCD_D12;
    panelConfig.data_gpio_nums[13] = PIN_LCD_D13;
    panelConfig.data_gpio_nums[14] = PIN_LCD_D14;
    panelConfig.data_gpio_nums[15] = PIN_LCD_D15;
    panelConfig.flags.fb_in_psram = 1;

    esp_err_t err = esp_lcd_new_rgb_panel(&panelConfig, &panelHandle);
    logPrintf("esp_lcd_new_rgb_panel: %d\n", err);
    if (err != ESP_OK) {
      panelHandle = nullptr;
      return;
    }

    err = esp_lcd_panel_reset(panelHandle);
    logPrintf("esp_lcd_panel_reset: %d\n", err);
    err = esp_lcd_panel_init(panelHandle);
    logPrintf("esp_lcd_panel_init: %d\n", err);

    lineBuffer = (uint16_t *)heap_caps_malloc(PANEL_WIDTH * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (lineBuffer == nullptr) {
      lineBuffer = (uint16_t *)heap_caps_malloc(PANEL_WIDTH * sizeof(uint16_t), MALLOC_CAP_8BIT);
    }
    logPrintf("LCD line buffer: %s\n", lineBuffer == nullptr ? "FAILED" : "OK");
  }

  void setRotation(uint8_t rotation) {
    (void)rotation;
    screenWidth = PANEL_WIDTH;
    screenHeight = PANEL_HEIGHT;
  }

  void fillScreen(uint16_t color) {
    fillRect(0, 0, screenWidth, screenHeight, color);
  }

  void setTextSize(uint8_t size) {
    textSize = (size == 0) ? 1 : size;
  }

  void setTextColor(uint16_t color, uint16_t background) {
    textColor = color;
    textBackground = background;
  }

  void setFont(const GFXfont *font = nullptr) {
    gfxFont = font;
  }

  void setCursor(int16_t x, int16_t y) {
    cursorX = x;
    cursorY = y;
  }

  void print(const String &text) {
    print(text.c_str());
  }

  void print(const char *text) {
    if (text == nullptr) {
      return;
    }
    while (*text) {
      drawChar(*text++);
    }
  }

  void print(int value) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d", value);
    print(buffer);
  }

  void print(unsigned int value) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%u", value);
    print(buffer);
  }

  void print(unsigned long value) {
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%lu", value);
    print(buffer);
  }

  void printf(const char *format, ...) {
    char buffer[80];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    print(buffer);
  }

  int16_t width() const { return screenWidth; }
  int16_t height() const { return screenHeight; }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0 || x >= screenWidth || y >= screenHeight) {
      return;
    }
    if (x < 0) {
      w += x;
      x = 0;
    }
    if (y < 0) {
      h += y;
      y = 0;
    }
    if (x + w > screenWidth) {
      w = screenWidth - x;
    }
    if (y + h > screenHeight) {
      h = screenHeight - y;
    }
    if (w <= 0 || h <= 0) {
      return;
    }

    if (panelHandle == nullptr || lineBuffer == nullptr) {
      return;
    }

    for (int16_t col = 0; col < w; col++) {
      lineBuffer[col] = color;
    }

    for (int16_t row = 0; row < h; row++) {
      esp_lcd_panel_draw_bitmap(panelHandle, x, y + row, x + w, y + row + 1, lineBuffer);
    }
  }

  void drawRGB565Image(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint16_t *pixels) {
    if (panelHandle == nullptr || lineBuffer == nullptr || pixels == nullptr) {
      return;
    }

    uint16_t drawWidth = w;
    uint16_t drawHeight = h;
    if (x < 0 || y < 0 || x >= screenWidth || y >= screenHeight) {
      return;
    }
    if (x + drawWidth > screenWidth) {
      drawWidth = screenWidth - x;
    }
    if (y + drawHeight > screenHeight) {
      drawHeight = screenHeight - y;
    }

    for (uint16_t row = 0; row < drawHeight; row++) {
      const uint32_t sourceOffset = (uint32_t)row * (uint32_t)w;
      for (uint16_t col = 0; col < drawWidth; col++) {
        lineBuffer[col] = pgm_read_word(&pixels[sourceOffset + col]);
      }
      esp_lcd_panel_draw_bitmap(panelHandle, x, y + row, x + drawWidth, y + row + 1, lineBuffer);
    }
  }

  void drawRGB565ImageScaled2x(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint16_t *pixels) {
    if (panelHandle == nullptr || lineBuffer == nullptr || pixels == nullptr) {
      return;
    }

    uint16_t drawWidth = w * 2;
    uint16_t drawHeight = h * 2;
    if (x < 0 || y < 0 || x >= screenWidth || y >= screenHeight) {
      return;
    }
    if (x + drawWidth > screenWidth) {
      drawWidth = screenWidth - x;
    }
    if (y + drawHeight > screenHeight) {
      drawHeight = screenHeight - y;
    }

    for (uint16_t row = 0; row < drawHeight; row++) {
      const uint32_t sourceOffset = (uint32_t)(row / 2) * (uint32_t)w;
      for (uint16_t col = 0; col < drawWidth; col++) {
        lineBuffer[col] = pgm_read_word(&pixels[sourceOffset + (col / 2)]);
      }
      esp_lcd_panel_draw_bitmap(panelHandle, x, y + row, x + drawWidth, y + row + 1, lineBuffer);
    }
  }

  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t radius, uint16_t color) {
    (void)radius;
    fillRect(x, y, w, 1, color);
    fillRect(x, y + h - 1, w, 1, color);
    fillRect(x, y, 1, h, color);
    fillRect(x + w - 1, y, 1, h, color);
  }

  void getTextBounds(const String &text, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
    if (gfxFont != nullptr) {
      getGfxTextBounds(text, x, y, x1, y1, w, h);
      return;
    }
    *x1 = 0;
    *y1 = 0;
    *w = text.length() * 6 * textSize;
    *h = 8 * textSize;
  }

private:
  void writeCommand(uint8_t command) {
    (void)command;
  }

  void writeData(uint8_t data) {
    (void)data;
  }

  void writeData16(uint16_t data) {
    (void)data;
  }

  void setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    writeCommand(0x2A);
    writeData16(x0);
    writeData16(x1);
    writeCommand(0x2B);
    writeData16(y0);
    writeData16(y1);
    writeCommand(0x2C);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || y < 0 || x >= screenWidth || y >= screenHeight) {
      return;
    }
    fillRect(x, y, 1, 1, color);
  }

  void getGfxTextBounds(const String &text, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
    int16_t minX = 32767;
    int16_t minY = 32767;
    int16_t maxX = -32768;
    int16_t maxY = -32768;
    int16_t cursor = x;
    uint16_t first = pgm_read_word(&gfxFont->first);
    uint16_t last = pgm_read_word(&gfxFont->last);
    const GFXglyph *glyphs = (const GFXglyph *)pgm_read_ptr(&gfxFont->glyph);

    for (uint16_t i = 0; i < text.length(); i++) {
      char c = text.charAt(i);
      if (c < first || c > last) {
        continue;
      }
      GFXglyph glyph;
      memcpy_P(&glyph, &glyphs[c - first], sizeof(GFXglyph));
      if (glyph.width > 0 && glyph.height > 0) {
        int16_t gx1 = cursor + (glyph.xOffset * textSize);
        int16_t gy1 = y + (glyph.yOffset * textSize);
        int16_t gx2 = gx1 + ((int16_t)glyph.width * textSize) - 1;
        int16_t gy2 = gy1 + ((int16_t)glyph.height * textSize) - 1;
        if (gx1 < minX) minX = gx1;
        if (gy1 < minY) minY = gy1;
        if (gx2 > maxX) maxX = gx2;
        if (gy2 > maxY) maxY = gy2;
      }
      cursor += (int16_t)glyph.xAdvance * textSize;
    }

    if (maxX < minX || maxY < minY) {
      *x1 = x;
      *y1 = y;
      *w = 0;
      *h = 0;
      return;
    }

    *x1 = minX;
    *y1 = minY;
    *w = maxX - minX + 1;
    *h = maxY - minY + 1;
  }

  const char *glyphRow(char c, uint8_t row) const {
    if (c >= 'a' && c <= 'z') {
      c -= 32;
    }
    static const char blank[7][6] = {"00000","00000","00000","00000","00000","00000","00000"};
    static const char unknown[7][6] = {"11111","10001","00010","00100","00100","00000","00100"};
    #define GLYPH(ch, a,b,c,d,e,f,g) case ch: { static const char p[7][6] = {a,b,c,d,e,f,g}; return p[row]; }
    switch (c) {
      GLYPH(' ', "00000","00000","00000","00000","00000","00000","00000")
      GLYPH('0', "01110","10001","10011","10101","11001","10001","01110")
      GLYPH('1', "00100","01100","00100","00100","00100","00100","01110")
      GLYPH('2', "01110","10001","00001","00010","00100","01000","11111")
      GLYPH('3', "11110","00001","00001","01110","00001","00001","11110")
      GLYPH('4', "00010","00110","01010","10010","11111","00010","00010")
      GLYPH('5', "11111","10000","10000","11110","00001","00001","11110")
      GLYPH('6', "01110","10000","10000","11110","10001","10001","01110")
      GLYPH('7', "11111","00001","00010","00100","01000","01000","01000")
      GLYPH('8', "01110","10001","10001","01110","10001","10001","01110")
      GLYPH('9', "01110","10001","10001","01111","00001","00001","01110")
      GLYPH('A', "01110","10001","10001","11111","10001","10001","10001")
      GLYPH('B', "11110","10001","10001","11110","10001","10001","11110")
      GLYPH('C', "01111","10000","10000","10000","10000","10000","01111")
      GLYPH('D', "11110","10001","10001","10001","10001","10001","11110")
      GLYPH('E', "11111","10000","10000","11110","10000","10000","11111")
      GLYPH('F', "11111","10000","10000","11110","10000","10000","10000")
      GLYPH('G', "01111","10000","10000","10011","10001","10001","01111")
      GLYPH('H', "10001","10001","10001","11111","10001","10001","10001")
      GLYPH('I', "01110","00100","00100","00100","00100","00100","01110")
      GLYPH('J', "00111","00010","00010","00010","00010","10010","01100")
      GLYPH('K', "10001","10010","10100","11000","10100","10010","10001")
      GLYPH('L', "10000","10000","10000","10000","10000","10000","11111")
      GLYPH('M', "10001","11011","10101","10101","10001","10001","10001")
      GLYPH('N', "10001","11001","10101","10011","10001","10001","10001")
      GLYPH('O', "01110","10001","10001","10001","10001","10001","01110")
      GLYPH('P', "11110","10001","10001","11110","10000","10000","10000")
      GLYPH('Q', "01110","10001","10001","10001","10101","10010","01101")
      GLYPH('R', "11110","10001","10001","11110","10100","10010","10001")
      GLYPH('S', "01111","10000","10000","01110","00001","00001","11110")
      GLYPH('T', "11111","00100","00100","00100","00100","00100","00100")
      GLYPH('U', "10001","10001","10001","10001","10001","10001","01110")
      GLYPH('V', "10001","10001","10001","10001","10001","01010","00100")
      GLYPH('W', "10001","10001","10001","10101","10101","10101","01010")
      GLYPH('X', "10001","10001","01010","00100","01010","10001","10001")
      GLYPH('Y', "10001","10001","01010","00100","00100","00100","00100")
      GLYPH('Z', "11111","00001","00010","00100","01000","10000","11111")
      GLYPH(':', "00000","00100","00100","00000","00100","00100","00000")
      GLYPH('.', "00000","00000","00000","00000","00000","01100","01100")
      GLYPH('-', "00000","00000","00000","11111","00000","00000","00000")
      GLYPH('/', "00001","00010","00010","00100","01000","01000","10000")
      GLYPH('+', "00000","00100","00100","11111","00100","00100","00000")
      GLYPH('%', "11001","11010","00010","00100","01000","01011","10011")
      default:
        return (c == '\0') ? blank[row] : unknown[row];
    }
    #undef GLYPH
  }

  void drawChar(char c) {
    if (c == '\n') {
      cursorX = 0;
      cursorY += gfxFont == nullptr ? (8 * textSize) : (pgm_read_word(&gfxFont->yAdvance) * textSize);
      return;
    }

    if (gfxFont != nullptr) {
      drawGfxChar(c);
      return;
    }

    for (uint8_t row = 0; row < 7; row++) {
      const char *bits = glyphRow(c, row);
      for (uint8_t col = 0; col < 5; col++) {
        uint16_t color = (bits[col] == '1') ? textColor : textBackground;
        fillRect(cursorX + col * textSize, cursorY + row * textSize, textSize, textSize, color);
      }
    }
    fillRect(cursorX + 5 * textSize, cursorY, textSize, 7 * textSize, textBackground);
    cursorX += 6 * textSize;
  }

  void drawGfxChar(char c) {
    uint16_t first = pgm_read_word(&gfxFont->first);
    uint16_t last = pgm_read_word(&gfxFont->last);
    if (c < first || c > last) {
      return;
    }

    const GFXglyph *glyphs = (const GFXglyph *)pgm_read_ptr(&gfxFont->glyph);
    const uint8_t *bitmap = (const uint8_t *)pgm_read_ptr(&gfxFont->bitmap);
    GFXglyph glyph;
    memcpy_P(&glyph, &glyphs[c - first], sizeof(GFXglyph));

    uint16_t bitIndex = 0;
    uint16_t bitmapOffset = glyph.bitmapOffset;
    uint8_t bits = 0;
    for (uint8_t yy = 0; yy < glyph.height; yy++) {
      for (uint8_t xx = 0; xx < glyph.width; xx++) {
        if ((bitIndex & 7) == 0) {
          bits = pgm_read_byte(&bitmap[bitmapOffset++]);
        }
        if (bits & 0x80) {
          fillRect(cursorX + (glyph.xOffset + xx) * textSize, cursorY + (glyph.yOffset + yy) * textSize,
                   textSize, textSize, textColor);
        }
        bits <<= 1;
        bitIndex++;
      }
    }
    cursorX += (int16_t)glyph.xAdvance * textSize;
  }

  uint8_t cs;
  uint8_t dc;
  uint8_t rst;
  int16_t screenWidth = PANEL_WIDTH;
  int16_t screenHeight = PANEL_HEIGHT;
  int16_t cursorX = 0;
  int16_t cursorY = 0;
  uint16_t textColor = ILI9341_WHITE;
  uint16_t textBackground = ILI9341_BLACK;
  uint8_t textSize = 1;
  const GFXfont *gfxFont = nullptr;
  esp_lcd_panel_handle_t panelHandle = nullptr;
  uint16_t *lineBuffer = nullptr;
};

CrowPanelRGBDisplay tft(255, 255, 255);
Preferences prefs;

DebouncedButton btnPlayerA;
DebouncedButton btnPlayerB;
DebouncedButton btnScoreAPlus;
DebouncedButton btnScoreAMinus;
DebouncedButton btnScoreBPlus;
DebouncedButton btnScoreBMinus;
DebouncedButton btnSelect;
DebouncedButton btnUp;
DebouncedButton btnDown;
ExpanderButton expanderBtnPlayerA;
ExpanderButton expanderBtnPlayerB;
ExpanderButton expanderBtnPlayPause;

int playerA_score = 0;
int playerB_score = 0;
int match_target = 9;
uint32_t playerA_time_remaining = DEFAULT_INITIAL_TIME_SECONDS * 1000UL;
uint32_t playerB_time_remaining = DEFAULT_INITIAL_TIME_SECONDS * 1000UL;
uint32_t match_elapsed_seconds = 0;
Player active_player = PLAYER_NONE;
bool clock_running = false;
bool crawford_used = false;
bool crawford_active = false;
bool post_crawford = false;
bool cube_enabled = true;
uint16_t game_number = 0;

uint32_t initial_time_seconds = DEFAULT_INITIAL_TIME_SECONDS;
uint8_t delay_seconds = DEFAULT_DELAY_SECONDS;
uint8_t increment_seconds = DEFAULT_INCREMENT_SECONDS;
uint8_t brightness = DEFAULT_BRIGHTNESS;
TimingMode timingMode = SIMPLE_DELAY;
TimerScope timerScope = TIMER_BY_MATCH;
DisplayTheme displayTheme = THEME_NIGHT;
ScoreDisplayMode scoreDisplayMode = SCORE_STANDARD;
String playerA_name = "PLAYER A";
String playerB_name = "PLAYER B";
uint16_t playerA_color = PLAYER_COLOR_BLUE;
uint16_t playerB_color = PLAYER_COLOR_RED;
String matchLog[MATCH_LOG_MAX_ENTRIES];
uint8_t matchLogCount = 0;

DeviceMode mode = MODE_MENU;
MenuItem currentMenuItem = MENU_MATCH_TARGET;
Player timeoutPlayer = PLAYER_NONE;
Player matchWinner = PLAYER_NONE;
Player editingNameFor = PLAYER_NONE;
String editingName;
uint16_t editingPlayerColor = PLAYER_COLOR_BLUE;

bool pending_crawford_next_game = false;
bool game_in_progress = false;
bool settingsDirty = false;
bool displayDirty = true;
bool displayModeKnown = false;
DeviceMode lastDrawMode = MODE_MENU;
uint32_t settingsDirtyMs = 0;
uint32_t lastTickMs = 0;
uint32_t turnStartMs = 0;
uint32_t pausedTurnElapsedMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastActivityMs = 0;
uint32_t lastLowTimeBeepMs = 0;
uint8_t gt911Address = 0;
bool touchWasDown = false;
uint16_t touchStartX = 0;
uint16_t touchStartY = 0;
uint16_t touchLastX = 0;
uint16_t touchLastY = 0;
uint16_t touchMinY = 0;
uint16_t touchMaxY = 0;
uint32_t touchStartMs = 0;
uint32_t touchLastSeenMs = 0;
uint32_t touchLastHandledMs = 0;
uint8_t pca9557Address = 0;
uint8_t pcf8575Address = 0;
bool max17048Found = false;
bool ds3231Found = false;
bool lastTouchSeen = false;
float lastChargeCheckVoltage = 0.0f;
float lastChargeCheckPercent = 0.0f;
bool chargeTrendRising = false;
uint32_t lastChargeCheckMs = 0;
uint32_t chargeIndicatorUntilMs = 0;
String cachedRtcStatus = "DATE UNSET";
uint32_t lastRtcStatusMs = 0;
uint16_t editingRtcYear = 2026;
uint8_t editingRtcMonth = 1;
uint8_t editingRtcDay = 1;
uint8_t editingRtcHour = 12;
uint8_t editingRtcMinute = 0;

const int MATCH_TARGETS[] = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21};
const uint8_t MATCH_TARGET_COUNT = sizeof(MATCH_TARGETS) / sizeof(MATCH_TARGETS[0]);

const Rect REGION_STATUS = {0, 0, 800, 34};
const Rect REGION_HEADER_A = {12, 42, 228, 32};
const Rect REGION_TIMER_A = {12, 82, 228, 108};
const Rect REGION_SCORE_A = {12, 210, 228, 246};
const Rect REGION_PLAY_PAUSE = {255, 42, 290, 56};
const Rect REGION_END_GAME = {255, 113, 130, 56};
const Rect REGION_END_MATCH = {415, 113, 130, 56};
const Rect REGION_MATCH_TARGET = {255, 184, 290, 211};
const Rect REGION_LOG = {255, 410, 140, 46};
const Rect REGION_SETTINGS = {405, 410, 140, 46};
const Rect REGION_HEADER_B = {560, 42, 228, 32};
const Rect REGION_TIMER_B = {560, 82, 228, 108};
const Rect REGION_SCORE_B = {560, 210, 228, 246};
const Rect HIT_PLAY_PAUSE = {245, 36, 310, 70};
const Rect HIT_END_GAME = {245, 106, 150, 70};
const Rect HIT_END_MATCH = {405, 106, 150, 70};
const Rect CONFIRM_NO = {172, 306, 190, 62};
const Rect CONFIRM_YES = {438, 306, 190, 62};

const Rect SETTINGS_MATCH_MINUS = {372, 86, 96, 48};
const Rect SETTINGS_MATCH_PLUS = {672, 86, 96, 48};
const Rect SETTINGS_TIME_MINUS = {372, 166, 96, 48};
const Rect SETTINGS_TIME_PLUS = {672, 166, 96, 48};
const Rect SETTINGS_DELAY_MINUS = {372, 246, 96, 48};
const Rect SETTINGS_DELAY_PLUS = {672, 246, 96, 48};
const Rect SETTINGS_MATCH_ROW_MINUS = {52, 82, 348, 58};
const Rect SETTINGS_MATCH_ROW_PLUS = {400, 82, 348, 58};
const Rect SETTINGS_TIME_ROW_MINUS = {52, 162, 348, 58};
const Rect SETTINGS_TIME_ROW_PLUS = {400, 162, 348, 58};
const Rect SETTINGS_DELAY_ROW_MINUS = {52, 242, 348, 58};
const Rect SETTINGS_DELAY_ROW_PLUS = {400, 242, 348, 58};
const Rect SETTINGS_SCOPE_GAME = {255, 306, 120, 36};
const Rect SETTINGS_SCOPE_MATCH = {392, 306, 120, 36};
const Rect SETTINGS_MODE_SIMPLE = {52, 352, 210, 38};
const Rect SETTINGS_MODE_BRONSTEIN = {295, 352, 210, 38};
const Rect SETTINGS_MODE_FISCHER = {538, 352, 210, 38};
const Rect SETTINGS_APPEARANCE = {52, 410, 170, 44};
const Rect SETTINGS_CLOCK = {240, 410, 130, 44};
const Rect SETTINGS_START = {388, 410, 170, 44};
const Rect SETTINGS_END = {576, 410, 170, 44};
const Rect SETTINGS_BACK = {24, 24, 108, 44};
const Rect LOG_BACK = {24, 24, 108, 44};
const Rect LOG_CLEAR = {650, 24, 108, 44};
const Rect CLOCK_BACK = {24, 24, 108, 44};
const Rect APPEARANCE_BACK = {24, 24, 108, 44};
const Rect APPEARANCE_NIGHT = {170, 132, 220, 64};
const Rect APPEARANCE_DAY = {410, 132, 220, 64};
const Rect APPEARANCE_SCORE = {170, 286, 220, 64};
const Rect APPEARANCE_POINTS = {410, 286, 220, 64};
const Rect CLOCK_SAVE = {560, 400, 168, 48};
const Rect CLOCK_YEAR_MINUS = {372, 84, 96, 44};
const Rect CLOCK_YEAR_PLUS = {672, 84, 96, 44};
const Rect CLOCK_MONTH_MINUS = {372, 142, 96, 44};
const Rect CLOCK_MONTH_PLUS = {672, 142, 96, 44};
const Rect CLOCK_DAY_MINUS = {372, 200, 96, 44};
const Rect CLOCK_DAY_PLUS = {672, 200, 96, 44};
const Rect CLOCK_HOUR_MINUS = {372, 258, 96, 44};
const Rect CLOCK_HOUR_PLUS = {672, 258, 96, 44};
const Rect CLOCK_MINUTE_MINUS = {372, 316, 96, 44};
const Rect CLOCK_MINUTE_PLUS = {672, 316, 96, 44};

// ---------------------------------------------------------------------------
// Forward declarations requested by the firmware structure.
// ---------------------------------------------------------------------------
void drawDisplay();
void drawTimerPanels();
void drawTimerPanel(Player player, const Rect &region);
void drawSplashImage();
void fillTriangleRows(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color);
void fillCircleRows(int16_t cx, int16_t cy, int16_t radius, uint16_t color);
void drawCenteredInRect(const String &text, const Rect &region, uint8_t textSize, uint16_t color, uint16_t background);
void drawScoreInRect(int score, const Rect &region, uint16_t color, uint16_t background);
void drawMatchTargetInRect(int target, const Rect &region, uint16_t color, uint16_t background);
void drawGfxTextScaledInRect(const GFXfont *font, const String &text, const Rect &region, uint16_t color, uint8_t scaleNum, uint8_t scaleDen);
void getGfxTextBoundsScaled(const GFXfont *font, const String &text, uint8_t scaleNum, uint8_t scaleDen, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h);
void drawGfxTextScaled(const GFXfont *font, const String &text, int16_t x, int16_t y, uint16_t color, uint8_t scaleNum, uint8_t scaleDen);
void drawMatchWinnerBanner();
void drawTimeoutBanner();
void drawNameEditor();
void drawMatchLog();
void drawClockSettings();
void drawAppearanceSettings();
void drawEndMatchConfirm();
void openClockSettings();
void handleClockSettingsTap(uint16_t x, uint16_t y);
void handleAppearanceSettingsTap(uint16_t x, uint16_t y);
void handleEndMatchConfirmTap(uint16_t x, uint16_t y);
void adjustRtcField(uint8_t field, int delta);
void openNameEditor(Player player);
void handleNameEditorTap(uint16_t x, uint16_t y);
uint16_t colorForPlayer(Player player);
uint16_t textColorForPlayerColor(uint16_t color);
uint16_t playerColorOption(uint8_t index);
uint8_t playerColorOptionCount();
void savePlayerNames();
void recordMatchLog();
void clearMatchLog();
String matchLogDate();
String matchLogPart(const String &entry, uint8_t part);
void handleButtons();
void handleTouch();
void handleTouchTap(uint16_t x, uint16_t y);
void handleTouchSwipe(uint16_t startX, uint16_t startY, uint16_t endX, uint16_t endY);
void updateClock();
void adjustScore(Player player, int delta);
void checkMatchState();
void startNewGame();
void endCurrentGame();
void markCrawfordUsed();
void forceEndMatch();
void resetMatch();
void enterMenu();
void handleMenu();

// Helper declarations.
void loadSettings();
void saveSettingsIfNeeded();
void markSettingsDirty();
void markDisplayDirty();
void setBrightness(uint8_t duty);
void wakeActivity();
void maybeSleep();
void beep(uint16_t frequency, uint16_t durationMs);
void playButtonBeep();
void playTimeoutBeep();
void playMatchOverBeep();
void pauseClock();
void resumeClock();
void togglePlayPause();
void switchClockTo(Player nextPlayer);
void addIncrementTo(Player player);
bool applyTurnElapsed(Player player, uint32_t nowMs);
uint32_t visibleReserveMs(Player player, uint32_t nowMs);
uint32_t *reserveFor(Player player);
uint32_t matchClockSeconds();
uint32_t startingClockMs();
String formatTimeMs(uint32_t milliseconds);
String timingModeLabel();
String playPauseLabel();
void drawCentered(const String &text, int16_t y, uint8_t textSize, uint16_t color);
void drawPlayerPanel(Player player, int16_t x, int16_t y, int16_t w, int16_t h);
void drawStatusBar();
bool dayThemeEnabled();
uint16_t uiBackgroundColor();
uint16_t uiPanelColor();
uint16_t uiTextColor();
uint16_t uiMutedColor();
void drawChargeBolt(int16_t x, int16_t y, uint16_t color, uint16_t background);
void drawStartupTest();
void initCrowPanelExpander();
void initButtonExpander();
void initFuelGauge();
void initRtcClock();
bool pca9557Write(uint8_t address, uint8_t reg, uint8_t value);
uint8_t pca9557Read(uint8_t address, uint8_t reg);
bool pcf8575Write(uint8_t address, uint16_t value);
uint16_t pcf8575Read(uint8_t address);
bool max17048Read16(uint8_t reg, uint16_t *value);
bool max17048Write16(uint8_t reg, uint16_t value);
bool max17048QuickStart();
float max17048CellVoltage();
float max17048StateOfCharge();
bool ds3231ReadTime(uint8_t *second, uint8_t *minute, uint8_t *hour, uint8_t *day, uint8_t *month, uint16_t *year);
bool ds3231SetTime(uint8_t second, uint8_t minute, uint8_t hour, uint8_t day, uint8_t month, uint16_t year);
bool rtcDateLooksSet(uint8_t day, uint8_t month, uint16_t year);
uint8_t daysInMonth(uint8_t month, uint16_t year);
uint8_t bcdToDec(uint8_t value);
uint8_t decToBcd(uint8_t value);
String rtcStatusText();
bool updateRtcStatusCache(bool force);
void scanI2C(const char *label);
void resetTouchController(uint8_t outputValue, uint8_t configValue);
void initTouch();
TouchPoint readTouch();
bool gt911Read(uint16_t reg, uint8_t *data, uint8_t len);
bool gt911Write(uint16_t reg, uint8_t value);
String formatTime(uint32_t seconds);
String matchStateLabel();
int batteryPercent();
bool chargingIndicatorOn();
void changeMenuValue(int delta);
void nextMenuItem();
void previousMenuItem();
uint8_t findCurrentTargetIndex();

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);
  delay(200);
  logLine("");
  logLine("Backgammon Match Clock booting");
  logPrintf("Firmware: %s\n", FIRMWARE_VERSION);
  logPrintf("PSRAM found: %s\n", psramFound() ? "yes" : "no");
  logPrintf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
  logPrintf("Total PSRAM: %u bytes\n", ESP.getPsramSize());
  if (!psramFound() || ESP.getPsramSize() < 800000) {
    logLine("ERROR: PSRAM is not enabled. Set Tools > PSRAM to OPI PSRAM and upload again.");
  }
  logLine("Buttons are optional for startup; the menu should draw with no buttons connected.");
  logPrintf("DIS07050H RGB panel: %ux%u, BL=%u, PCLK=%u HSYNC=%u VSYNC=%u DE=%u\n",
                PANEL_WIDTH, PANEL_HEIGHT, PIN_TFT_BACKLIGHT, PIN_LCD_PCLK,
                PIN_LCD_HSYNC, PIN_LCD_VSYNC, PIN_LCD_DE);

  btnPlayerA.begin(PIN_PLAYER_A_BUTTON, PLAYER_BUTTON_DEBOUNCE_MS);
  btnPlayerB.begin(PIN_PLAYER_B_BUTTON, PLAYER_BUTTON_DEBOUNCE_MS);
  btnScoreAPlus.begin(PIN_SCORE_A_PLUS, SMALL_BUTTON_DEBOUNCE_MS);
  btnScoreAMinus.begin(PIN_SCORE_A_MINUS, SMALL_BUTTON_DEBOUNCE_MS);
  btnScoreBPlus.begin(PIN_SCORE_B_PLUS, SMALL_BUTTON_DEBOUNCE_MS);
  btnScoreBMinus.begin(PIN_SCORE_B_MINUS, SMALL_BUTTON_DEBOUNCE_MS);
  btnSelect.begin(PIN_MENU_SELECT, SMALL_BUTTON_DEBOUNCE_MS);
  btnUp.begin(PIN_MENU_UP, SMALL_BUTTON_DEBOUNCE_MS);
  btnDown.begin(PIN_MENU_DOWN, SMALL_BUTTON_DEBOUNCE_MS);
  expanderBtnPlayerA.begin(PCF8575_PLAYER_A_PIN, PLAYER_BUTTON_DEBOUNCE_MS);
  expanderBtnPlayerB.begin(PCF8575_PLAYER_B_PIN, PLAYER_BUTTON_DEBOUNCE_MS);
  expanderBtnPlayPause.begin(PCF8575_PLAY_PAUSE_PIN, PLAYER_BUTTON_DEBOUNCE_MS);

  if (PIN_BUZZER != 255) {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
  }
  if (PIN_CHARGE_DETECT != 255) {
    pinMode(PIN_CHARGE_DETECT, INPUT_PULLUP);
  }

  pinMode(PIN_TFT_BACKLIGHT, OUTPUT);
  setBrightness(255);

  analogReadResolution(12);

  initCrowPanelExpander();
  initButtonExpander();
  initFuelGauge();
  initRtcClock();
  initTouch();
  tft.begin();
  tft.setRotation(1);
  drawStartupTest();

  loadSettings();
  setBrightness(brightness);
  resetMatch();
  mode = MODE_MATCH;
  markDisplayDirty();

  lastTickMs = millis();
  lastActivityMs = lastTickMs;
  updateRtcStatusCache(true);
  drawDisplay();
  displayDirty = false;
  lastDisplayMs = lastTickMs;
}

void loop() {
  handleTouch();
  handleButtons();

  if (mode == MODE_MATCH) {
    updateClock();
  }

  saveSettingsIfNeeded();
  maybeSleep();

  uint32_t nowMs = millis();
  if (!displayDirty && nowMs - lastRtcStatusMs >= 1000UL) {
    bool rtcChanged = updateRtcStatusCache(false);
    if (rtcChanged && (mode == MODE_MATCH || mode == MODE_MATCH_OVER || mode == MODE_TIMEOUT)) {
      drawStatusBar();
    }
  }

  bool timeDisplayDue = (clock_running && nowMs - lastDisplayMs >= DISPLAY_REFRESH_MS);
  if (displayDirty) {
    drawDisplay();
    lastDisplayMs = nowMs;
    displayDirty = false;
  } else if (timeDisplayDue) {
    drawTimerPanels();
    lastDisplayMs = nowMs;
  }
}

// ---------------------------------------------------------------------------
// Button handling
// ---------------------------------------------------------------------------
void handleButtons() {
  uint32_t nowMs = millis();

  btnPlayerA.update(nowMs);
  btnPlayerB.update(nowMs);
  btnScoreAPlus.update(nowMs);
  btnScoreAMinus.update(nowMs);
  btnScoreBPlus.update(nowMs);
  btnScoreBMinus.update(nowMs);
  btnSelect.update(nowMs);
  btnUp.update(nowMs);
  btnDown.update(nowMs);

  uint16_t expanderState = (pcf8575Address == 0) ? 0xFFFF : pcf8575Read(pcf8575Address);
  expanderBtnPlayerA.update(nowMs, expanderState);
  expanderBtnPlayerB.update(nowMs, expanderState);
  expanderBtnPlayPause.update(nowMs, expanderState);

  bool gpioPlayerAPressed = btnPlayerA.wasPressed();
  bool gpioPlayerBPressed = btnPlayerB.wasPressed();
  bool expanderPlayerAPressed = expanderBtnPlayerA.wasPressed();
  bool expanderPlayerBPressed = expanderBtnPlayerB.wasPressed();
  bool expanderPlayPausePressed = expanderBtnPlayPause.wasPressed();

  if (expanderPlayPausePressed) {
    logPrintf("PCF8575 P00 Play/Pause pressed: mode=%u running=%s game=%s\n",
              (unsigned int)mode,
              clock_running ? "yes" : "no",
              game_in_progress ? "yes" : "no");
  }

  bool anyPress = gpioPlayerAPressed || gpioPlayerBPressed ||
                  expanderPlayerAPressed || expanderPlayerBPressed ||
                  expanderPlayPausePressed ||
                  btnScoreAPlus.wasPressed() || btnScoreAMinus.wasPressed() ||
                  btnScoreBPlus.wasPressed() || btnScoreBMinus.wasPressed() ||
                  btnSelect.wasPressed() || btnUp.wasPressed() || btnDown.wasPressed();

  if (anyPress) {
    wakeActivity();
    playButtonBeep();
  }

  if (mode == MODE_MENU) {
    handleMenu();
    return;
  }

  if (mode == MODE_NAME_EDIT) {
    if (btnSelect.wasPressed()) {
      mode = MODE_MATCH;
      editingNameFor = PLAYER_NONE;
      markDisplayDirty();
    }
    return;
  }

  if (mode == MODE_LOG) {
    if (btnSelect.wasPressed()) {
      mode = MODE_MATCH;
      markDisplayDirty();
    }
    return;
  }

  if (mode == MODE_APPEARANCE) {
    if (btnSelect.wasPressed()) {
      mode = MODE_MENU;
      markDisplayDirty();
    }
    return;
  }

  if (mode == MODE_CONFIRM_END_MATCH) {
    if (btnSelect.wasPressed()) {
      mode = MODE_MATCH;
      markDisplayDirty();
    }
    return;
  }

  // The menu/select button pauses or resumes menu access during a match.
  if (btnSelect.wasPressed()) {
    enterMenu();
    return;
  }

  if (mode == MODE_MATCH) {
    if (expanderPlayPausePressed) {
      togglePlayPause();
      return;
    }

    if (gpioPlayerAPressed) {
      if (clock_running && active_player == PLAYER_A) {
        switchClockTo(PLAYER_B);
      } else {
        switchClockTo(PLAYER_A);
      }
    }
    if (gpioPlayerBPressed) {
      switchClockTo(PLAYER_A);
    }
    if (expanderPlayerAPressed) {
      switchClockTo(PLAYER_B);
    }
    if (expanderPlayerBPressed) {
      switchClockTo(PLAYER_A);
    }

    if (btnScoreAPlus.wasPressed()) {
      adjustScore(PLAYER_A, +1);
    }
    if (btnScoreAMinus.wasPressed()) {
      adjustScore(PLAYER_A, -1);
    }
    if (btnScoreBPlus.wasPressed()) {
      adjustScore(PLAYER_B, +1);
    }
    if (btnScoreBMinus.wasPressed()) {
      adjustScore(PLAYER_B, -1);
    }
  }

  // After timeout or match over, select returns to menu so the device can reset.
  if ((mode == MODE_TIMEOUT || mode == MODE_MATCH_OVER) && btnSelect.wasPressed()) {
    enterMenu();
  }
}

void handleTouch() {
  const uint32_t TOUCH_RELEASE_GRACE_MS = 140;
  const uint32_t TOUCH_REPEAT_GUARD_MS = 220;
  const int16_t TOUCH_SWIPE_THRESHOLD = 32;

  TouchPoint point = readTouch();
  uint32_t nowMs = millis();

  if (point.touched) {
    touchLastX = point.x;
    touchLastY = point.y;
    if (point.y < touchMinY) {
      touchMinY = point.y;
    }
    if (point.y > touchMaxY) {
      touchMaxY = point.y;
    }
    touchLastSeenMs = nowMs;
    lastTouchSeen = true;
    if (!touchWasDown) {
      touchWasDown = true;
      touchStartX = point.x;
      touchStartY = point.y;
      touchMinY = point.y;
      touchMaxY = point.y;
      touchStartMs = nowMs;
    }
    return;
  }

  if (!touchWasDown) {
    return;
  }

  // The GT911 can briefly report no touch while a finger is still moving.
  // Waiting a moment here keeps swipes from being split into tiny taps.
  if (nowMs - touchLastSeenMs < TOUCH_RELEASE_GRACE_MS) {
    return;
  }

  touchWasDown = false;
  wakeActivity();

  if (nowMs - touchLastHandledMs < TOUCH_REPEAT_GUARD_MS) {
    return;
  }
  touchLastHandledMs = nowMs;

  if (mode == MODE_MENU || mode == MODE_LOG || mode == MODE_NAME_EDIT ||
      mode == MODE_CLOCK_SET || mode == MODE_APPEARANCE) {
    handleTouchTap(touchLastX, touchLastY);
    return;
  }

  int16_t dx = (int16_t)touchLastX - (int16_t)touchStartX;
  int16_t upDistance = (int16_t)touchStartY - (int16_t)touchMinY;
  int16_t downDistance = (int16_t)touchMaxY - (int16_t)touchStartY;
  int16_t dy = (downDistance > upDistance) ? downDistance : -upDistance;
  if (abs(dy) >= TOUCH_SWIPE_THRESHOLD && abs(dy) > abs(dx)) {
    uint16_t gestureEndY = touchStartY;
    if (dy > 0) {
      uint32_t candidate = (uint32_t)touchStartY + (uint32_t)abs(dy);
      gestureEndY = (candidate >= PANEL_HEIGHT) ? PANEL_HEIGHT - 1 : (uint16_t)candidate;
    } else {
      gestureEndY = (touchStartY > abs(dy)) ? touchStartY - abs(dy) : 0;
    }
    handleTouchSwipe(touchStartX, touchStartY, touchLastX, gestureEndY);
  } else {
    handleTouchTap(touchLastX, touchLastY);
  }
}

void handleTouchTap(uint16_t x, uint16_t y) {
  if (mode == MODE_NAME_EDIT) {
    handleNameEditorTap(x, y);
    return;
  }

  if (mode == MODE_CLOCK_SET) {
    handleClockSettingsTap(x, y);
    return;
  }

  if (mode == MODE_APPEARANCE) {
    handleAppearanceSettingsTap(x, y);
    return;
  }

  if (mode == MODE_CONFIRM_END_MATCH) {
    handleEndMatchConfirmTap(x, y);
    return;
  }

  if (mode == MODE_LOG) {
    if (LOG_BACK.containsPadded(x, y, 18, 14)) {
      mode = MODE_MATCH;
      markDisplayDirty();
      return;
    }
    if (LOG_CLEAR.containsPadded(x, y, 18, 14)) {
      clearMatchLog();
      markDisplayDirty();
      return;
    }
    return;
  }

  if (mode == MODE_MENU) {
    if (SETTINGS_BACK.containsPadded(x, y, 18, 14)) {
      mode = MODE_MATCH;
      markDisplayDirty();
      return;
    }

    if (SETTINGS_MATCH_MINUS.containsPadded(x, y, 40, 18) ||
        SETTINGS_MATCH_ROW_MINUS.contains(x, y)) {
      currentMenuItem = MENU_MATCH_TARGET;
      changeMenuValue(-1);
      return;
    }
    if (SETTINGS_MATCH_PLUS.containsPadded(x, y, 40, 18) ||
        SETTINGS_MATCH_ROW_PLUS.contains(x, y)) {
      currentMenuItem = MENU_MATCH_TARGET;
      changeMenuValue(+1);
      return;
    }
    if (SETTINGS_TIME_MINUS.containsPadded(x, y, 40, 18) ||
        SETTINGS_TIME_ROW_MINUS.contains(x, y)) {
      currentMenuItem = MENU_INITIAL_TIME;
      changeMenuValue(-1);
      return;
    }
    if (SETTINGS_TIME_PLUS.containsPadded(x, y, 40, 18) ||
        SETTINGS_TIME_ROW_PLUS.contains(x, y)) {
      currentMenuItem = MENU_INITIAL_TIME;
      changeMenuValue(+1);
      return;
    }
    if (SETTINGS_DELAY_MINUS.containsPadded(x, y, 40, 18) ||
        SETTINGS_DELAY_ROW_MINUS.contains(x, y)) {
      currentMenuItem = MENU_DELAY;
      changeMenuValue(-1);
      return;
    }
    if (SETTINGS_DELAY_PLUS.containsPadded(x, y, 40, 18) ||
        SETTINGS_DELAY_ROW_PLUS.contains(x, y)) {
      currentMenuItem = MENU_DELAY;
      changeMenuValue(+1);
      return;
    }
    if (SETTINGS_SCOPE_GAME.containsPadded(x, y, 18, 10)) {
      timerScope = TIMER_BY_GAME;
      playerA_time_remaining = startingClockMs();
      playerB_time_remaining = startingClockMs();
      markSettingsDirty();
      markDisplayDirty();
      return;
    }
    if (SETTINGS_SCOPE_MATCH.containsPadded(x, y, 18, 10)) {
      timerScope = TIMER_BY_MATCH;
      playerA_time_remaining = startingClockMs();
      playerB_time_remaining = startingClockMs();
      markSettingsDirty();
      markDisplayDirty();
      return;
    }
    if (SETTINGS_MODE_SIMPLE.containsPadded(x, y, 12, 10)) {
      timingMode = SIMPLE_DELAY;
      markSettingsDirty();
      markDisplayDirty();
      return;
    }
    if (SETTINGS_MODE_BRONSTEIN.containsPadded(x, y, 12, 10)) {
      timingMode = BRONSTEIN_DELAY;
      markSettingsDirty();
      markDisplayDirty();
      return;
    }
    if (SETTINGS_MODE_FISCHER.containsPadded(x, y, 12, 10)) {
      timingMode = FISCHER_INCREMENT;
      markSettingsDirty();
      markDisplayDirty();
      return;
    }
    if (SETTINGS_APPEARANCE.containsPadded(x, y, 10, 10)) {
      mode = MODE_APPEARANCE;
      markDisplayDirty();
      return;
    }
    if (SETTINGS_CLOCK.containsPadded(x, y, 18, 14)) {
      openClockSettings();
      return;
    }
    if (SETTINGS_START.containsPadded(x, y, 18, 14)) {
      resetMatch();
      startNewGame();
      mode = MODE_MATCH;
      markDisplayDirty();
      return;
    }
    if (SETTINGS_END.containsPadded(x, y, 18, 14)) {
      resetMatch();
      mode = MODE_MATCH;
      markDisplayDirty();
      return;
    }
    return;
  }

  bool timerAHit = REGION_TIMER_A.containsPadded(x, y, 12, 14) ||
                   REGION_HEADER_A.containsPadded(x, y, 12, 10);
  bool timerBHit = REGION_TIMER_B.containsPadded(x, y, 12, 14) ||
                   REGION_HEADER_B.containsPadded(x, y, 12, 10);

  if (!clock_running && REGION_HEADER_A.containsPadded(x, y, 12, 10)) {
    openNameEditor(PLAYER_A);
    return;
  }

  if (!clock_running && REGION_HEADER_B.containsPadded(x, y, 12, 10)) {
    openNameEditor(PLAYER_B);
    return;
  }

  if (mode == MODE_MATCH && REGION_LOG.contains(x, y)) {
    mode = MODE_LOG;
    markDisplayDirty();
    return;
  }

  if (mode == MODE_MATCH && (REGION_SETTINGS.contains(x, y) || REGION_MATCH_TARGET.contains(x, y))) {
    enterMenu();
    return;
  }

  if (HIT_PLAY_PAUSE.contains(x, y)) {
    togglePlayPause();
    return;
  }

  if (HIT_END_GAME.contains(x, y)) {
    if (mode == MODE_TIMEOUT) {
      mode = MODE_MATCH;
      startNewGame();
      return;
    }
    if (game_in_progress || clock_running) {
      endCurrentGame();
    }
    return;
  }

  if (HIT_END_MATCH.contains(x, y)) {
    if (mode == MODE_MATCH_OVER) {
      resetMatch();
      startNewGame();
    } else {
      pauseClock();
      mode = MODE_CONFIRM_END_MATCH;
      markDisplayDirty();
    }
    return;
  }

  if (timerAHit) {
    switchClockTo(PLAYER_B);
    return;
  }

  if (timerBHit) {
    switchClockTo(PLAYER_A);
    return;
  }

  if (!clock_running && REGION_SCORE_A.contains(x, y)) {
    int delta = (y < REGION_SCORE_A.y + (REGION_SCORE_A.h / 2)) ? +1 : -1;
    adjustScore(PLAYER_A, delta);
    return;
  }

  if (!clock_running && REGION_SCORE_B.contains(x, y)) {
    int delta = (y < REGION_SCORE_B.y + (REGION_SCORE_B.h / 2)) ? +1 : -1;
    adjustScore(PLAYER_B, delta);
    return;
  }
}

void handleTouchSwipe(uint16_t startX, uint16_t startY, uint16_t endX, uint16_t endY) {
  if ((mode != MODE_MATCH && mode != MODE_TIMEOUT) || clock_running) {
    return;
  }

  int delta = (endY < startY) ? +1 : -1;
  bool scoreA = REGION_SCORE_A.contains(startX, startY);
  bool scoreB = REGION_SCORE_B.contains(startX, startY);

  if (!scoreA && !scoreB) {
    return;
  }

  adjustScore(scoreA ? PLAYER_A : PLAYER_B, delta);
  markDisplayDirty();
}

void pauseClock() {
  if (!clock_running || active_player == PLAYER_NONE) {
    return;
  }

  uint32_t nowMs = millis();
  pausedTurnElapsedMs = nowMs - turnStartMs;
  clock_running = false;
  lastTickMs = nowMs;
  markDisplayDirty();
}

void resumeClock() {
  if (clock_running || active_player == PLAYER_NONE || matchWinner != PLAYER_NONE || timeoutPlayer != PLAYER_NONE) {
    return;
  }

  uint32_t nowMs = millis();
  turnStartMs = nowMs - pausedTurnElapsedMs;
  lastTickMs = nowMs;
  clock_running = true;
  markDisplayDirty();
}

void togglePlayPause() {
  if (mode != MODE_MATCH) {
    return;
  }

  if (clock_running) {
    pauseClock();
  } else if (!game_in_progress) {
    startNewGame();
  } else {
    resumeClock();
  }
}

void switchClockTo(Player nextPlayer) {
  uint32_t nowMs = millis();

  if (mode != MODE_MATCH || matchWinner != PLAYER_NONE || timeoutPlayer != PLAYER_NONE) {
    return;
  }

  if (!game_in_progress) {
    startNewGame();
  }

  // Once the clock is running, only the active player's own large button
  // should end the turn. Presses on the inactive side are ignored so an
  // accidental bump cannot reset the current player's delay window.
  if (clock_running && active_player == nextPlayer) {
    return;
  }

  if (clock_running && active_player != PLAYER_NONE && active_player != nextPlayer) {
    if (!applyTurnElapsed(active_player, nowMs)) {
      return;
    }
  }

  active_player = nextPlayer;
  clock_running = true;
  turnStartMs = nowMs;
  pausedTurnElapsedMs = 0;
  lastTickMs = nowMs;
  markDisplayDirty();
}

// ---------------------------------------------------------------------------
// Clock behavior
// ---------------------------------------------------------------------------
void updateClock() {
  if (!clock_running || active_player == PLAYER_NONE) {
    lastTickMs = millis();
    return;
  }

  uint32_t nowMs = millis();
  uint32_t elapsedMs = nowMs - lastTickMs;
  if (elapsedMs < 1000) {
    return;
  }

  uint32_t wholeSeconds = elapsedMs / 1000;
  lastTickMs += wholeSeconds * 1000;
  match_elapsed_seconds += wholeSeconds;

  if (visibleReserveMs(active_player, nowMs) == 0) {
    uint32_t *remaining = reserveFor(active_player);
    if (remaining != nullptr) {
      *remaining = 0;
    }
    clock_running = false;
    timeoutPlayer = active_player;
    active_player = PLAYER_NONE;
    if (crawford_active) {
      markCrawfordUsed();
    }
    game_in_progress = false;
    mode = MODE_TIMEOUT;
    playTimeoutBeep();
    markDisplayDirty();
    return;
  }

  // Low-time warning. The critical zone beeps more often.
  uint32_t visibleSeconds = visibleReserveMs(active_player, nowMs) / 1000UL;
  if (visibleSeconds <= LOW_TIME_WARNING_SECONDS) {
    uint32_t interval = (visibleSeconds <= CRITICAL_TIME_WARNING_SECONDS) ? 1000UL : 5000UL;
    if (nowMs - lastLowTimeBeepMs >= interval) {
      beep(2200, 45);
      lastLowTimeBeepMs = nowMs;
    }
  }
}

void addIncrementTo(Player player) {
  if (increment_seconds == 0) {
    return;
  }

  if (player == PLAYER_A) {
    playerA_time_remaining += (uint32_t)increment_seconds * 1000UL;
  } else if (player == PLAYER_B) {
    playerB_time_remaining += (uint32_t)increment_seconds * 1000UL;
  }
  markDisplayDirty();
}

bool applyTurnElapsed(Player player, uint32_t nowMs) {
  uint32_t *reserve = reserveFor(player);
  if (reserve == nullptr) {
    return true;
  }

  uint32_t elapsedMs = nowMs - turnStartMs;
  uint32_t delayOrIncrementMs = (uint32_t)delay_seconds * 1000UL;
  uint32_t deductionMs = 0;

  if (timingMode == FISCHER_INCREMENT) {
    deductionMs = elapsedMs;
  } else {
    deductionMs = (elapsedMs > delayOrIncrementMs) ? (elapsedMs - delayOrIncrementMs) : 0;
  }

  if (deductionMs >= *reserve) {
    *reserve = 0;
    clock_running = false;
    timeoutPlayer = player;
    active_player = PLAYER_NONE;
    if (crawford_active) {
      markCrawfordUsed();
    }
    game_in_progress = false;
    mode = MODE_TIMEOUT;
    playTimeoutBeep();
    markDisplayDirty();
    return false;
  }

  *reserve -= deductionMs;
  if (timingMode == FISCHER_INCREMENT) {
    *reserve += delayOrIncrementMs;
  }
  return true;
}

uint32_t visibleReserveMs(Player player, uint32_t nowMs) {
  uint32_t *reserve = reserveFor(player);
  if (reserve == nullptr) {
    return 0;
  }
  if (active_player != player) {
    return *reserve;
  }

  uint32_t elapsedMs = 0;
  if (clock_running) {
    elapsedMs = nowMs - turnStartMs;
  } else if (pausedTurnElapsedMs > 0) {
    elapsedMs = pausedTurnElapsedMs;
  } else {
    return *reserve;
  }

  uint32_t delayOrIncrementMs = (uint32_t)delay_seconds * 1000UL;
  uint32_t deductionMs = 0;

  if (timingMode == SIMPLE_DELAY) {
    deductionMs = (elapsedMs > delayOrIncrementMs) ? (elapsedMs - delayOrIncrementMs) : 0;
  } else {
    deductionMs = elapsedMs;
  }

  return (deductionMs >= *reserve) ? 0 : (*reserve - deductionMs);
}

uint32_t *reserveFor(Player player) {
  if (player == PLAYER_A) {
    return &playerA_time_remaining;
  }
  if (player == PLAYER_B) {
    return &playerB_time_remaining;
  }
  return nullptr;
}

uint32_t matchClockSeconds() {
  return initial_time_seconds * (uint32_t)match_target;
}

uint32_t startingClockMs() {
  uint32_t seconds = (timerScope == TIMER_BY_MATCH) ? matchClockSeconds() : initial_time_seconds;
  return seconds * 1000UL;
}

// ---------------------------------------------------------------------------
// Score and match state
// ---------------------------------------------------------------------------
void adjustScore(Player player, int delta) {
  if ((mode != MODE_MATCH && mode != MODE_TIMEOUT) || clock_running) {
    return;
  }

  bool scorerIsOpponentOfCrawfordPlayer =
    (match_target > 1) &&
    ((player == PLAYER_A && playerB_score == match_target - 1) ||
     (player == PLAYER_B && playerA_score == match_target - 1));

  if (delta > 0 && (crawford_active || scorerIsOpponentOfCrawfordPlayer)) {
    // Any score recorded during the Crawford game ends Crawford immediately.
    // Also catch the real scoreboard pattern: the one-away player did not
    // score, so the opponent's score changed and the next game is post-Crawford.
    markCrawfordUsed();
  }

  if (delta > 0 && game_in_progress) {
    // A positive score adjustment is treated as the end of the current game.
    // Manual corrections with minus buttons do not advance Crawford state.
    endCurrentGame();
  }

  int *score = (player == PLAYER_A) ? &playerA_score : &playerB_score;
  *score += delta;
  if (*score < 0) {
    *score = 0;
  }
  if (*score > match_target) {
    *score = match_target;
  }

  checkMatchState();
  markDisplayDirty();

  if (matchWinner != PLAYER_NONE || mode == MODE_MATCH_OVER) {
    return;
  }

  // New games are counted only when startNewGame() is explicitly called.
  // Scoring corrections should not advance the game counter.
}

void checkMatchState() {
  if (playerA_score >= match_target || playerB_score >= match_target) {
    matchWinner = (playerA_score >= match_target) ? PLAYER_A : PLAYER_B;
    clock_running = false;
    active_player = PLAYER_NONE;
    game_in_progress = false;
    recordMatchLog();
    mode = MODE_MATCH_OVER;
    playMatchOverBeep();
    markDisplayDirty();
    return;
  }

  if (crawford_used) {
    pending_crawford_next_game = false;
    if (!crawford_active) {
      post_crawford = true;
      cube_enabled = true;
    }
    return;
  }

  // Crawford is only meaningful for match targets above 1.
  if (match_target > 1 && !crawford_used && !crawford_active) {
    bool someoneAtCrawfordScore =
      (playerA_score == match_target - 1) || (playerB_score == match_target - 1);

    pending_crawford_next_game = someoneAtCrawfordScore;
  }
}

void startNewGame() {
  if (mode == MODE_MATCH_OVER) {
    return;
  }

  game_number++;
  game_in_progress = true;
  clock_running = false;
  active_player = PLAYER_NONE;
  timeoutPlayer = PLAYER_NONE;
  if (timerScope == TIMER_BY_GAME) {
    playerA_time_remaining = startingClockMs();
    playerB_time_remaining = startingClockMs();
  }
  pausedTurnElapsedMs = 0;
  lastLowTimeBeepMs = 0;

  if (pending_crawford_next_game && !crawford_used) {
    crawford_active = true;
    post_crawford = false;
    cube_enabled = false;
    pending_crawford_next_game = false;
  } else if (crawford_used) {
    crawford_active = false;
    post_crawford = true;
    cube_enabled = true;
  } else {
    crawford_active = false;
    post_crawford = false;
    cube_enabled = true;
  }

  lastTickMs = millis();
  turnStartMs = lastTickMs;
  markDisplayDirty();
}

void endCurrentGame() {
  clock_running = false;
  active_player = PLAYER_NONE;
  pausedTurnElapsedMs = 0;

  if (crawford_active) {
    markCrawfordUsed();
  }

  game_in_progress = false;
  markDisplayDirty();
}

void markCrawfordUsed() {
  crawford_active = false;
  crawford_used = true;
  post_crawford = true;
  pending_crawford_next_game = false;
  cube_enabled = true;
}

void forceEndMatch() {
  clock_running = false;
  active_player = PLAYER_NONE;
  pausedTurnElapsedMs = 0;
  game_in_progress = false;

  matchWinner = (playerA_score == playerB_score) ? PLAYER_NONE :
                ((playerA_score > playerB_score) ? PLAYER_A : PLAYER_B);
  recordMatchLog();
  mode = MODE_MATCH_OVER;
  playMatchOverBeep();
  markDisplayDirty();
}

void resetMatch() {
  playerA_score = 0;
  playerB_score = 0;
  playerA_time_remaining = startingClockMs();
  playerB_time_remaining = startingClockMs();
  match_elapsed_seconds = 0;
  active_player = PLAYER_NONE;
  clock_running = false;
  pausedTurnElapsedMs = 0;
  crawford_used = false;
  crawford_active = false;
  post_crawford = false;
  cube_enabled = true;
  game_number = 0;
  timeoutPlayer = PLAYER_NONE;
  matchWinner = PLAYER_NONE;
  pending_crawford_next_game = false;
  game_in_progress = false;
  mode = MODE_MATCH;
  markDisplayDirty();
}

// ---------------------------------------------------------------------------
// Menu behavior
// ---------------------------------------------------------------------------
void enterMenu() {
  clock_running = false;
  active_player = PLAYER_NONE;
  mode = MODE_MENU;
  markDisplayDirty();
}

void handleMenu() {
  if (btnPlayerA.wasPressed() || btnUp.wasPressed()) {
    changeMenuValue(+1);
  }

  if (btnPlayerB.wasPressed() || btnDown.wasPressed()) {
    changeMenuValue(-1);
  }

  if (btnScoreAPlus.wasPressed() || btnScoreBPlus.wasPressed()) {
    nextMenuItem();
  }

  if (btnScoreAMinus.wasPressed() || btnScoreBMinus.wasPressed()) {
    previousMenuItem();
  }

  if (btnSelect.wasPressed()) {
    if (currentMenuItem == MENU_START_MATCH) {
      resetMatch();
      startNewGame();
    } else if (currentMenuItem == MENU_RESET_MATCH) {
      resetMatch();
      enterMenu();
    } else {
      nextMenuItem();
    }
  }
}

void changeMenuValue(int delta) {
  switch (currentMenuItem) {
    case MENU_MATCH_TARGET: {
      int index = findCurrentTargetIndex();
      index += delta;
      if (index < 0) {
        index = MATCH_TARGET_COUNT - 1;
      }
      if (index >= MATCH_TARGET_COUNT) {
        index = 0;
      }
      match_target = MATCH_TARGETS[index];
      playerA_time_remaining = startingClockMs();
      playerB_time_remaining = startingClockMs();
      markSettingsDirty();
      break;
    }

    case MENU_INITIAL_TIME: {
      int32_t next = (int32_t)initial_time_seconds + (delta * 30);
      if (next < 60) {
        next = 60;
      }
      if (next > 99L * 60L) {
        next = 99L * 60L;
      }
      initial_time_seconds = (uint32_t)next;
      playerA_time_remaining = startingClockMs();
      playerB_time_remaining = startingClockMs();
      markSettingsDirty();
      break;
    }

    case MENU_DELAY: {
      int next = (int)delay_seconds + delta;
      if (next < 0) {
        next = 0;
      }
      if (next > 60) {
        next = 60;
      }
      delay_seconds = (uint8_t)next;
      markSettingsDirty();
      break;
    }

    case MENU_INCREMENT: {
      int next = (int)increment_seconds + delta;
      if (next < 0) {
        next = 0;
      }
      if (next > 60) {
        next = 60;
      }
      increment_seconds = (uint8_t)next;
      markSettingsDirty();
      break;
    }

    case MENU_BRIGHTNESS: {
      int next = (int)brightness + (delta * 15);
      if (next < 15) {
        next = 15;
      }
      if (next > 255) {
        next = 255;
      }
      brightness = (uint8_t)next;
      setBrightness(brightness);
      markSettingsDirty();
      break;
    }

    default:
      break;
  }
  markDisplayDirty();
}

void nextMenuItem() {
  currentMenuItem = (MenuItem)(((uint8_t)currentMenuItem + 1) % MENU_COUNT);
  markDisplayDirty();
}

void previousMenuItem() {
  uint8_t item = (uint8_t)currentMenuItem;
  currentMenuItem = (MenuItem)((item == 0) ? (MENU_COUNT - 1) : (item - 1));
  markDisplayDirty();
}

uint8_t findCurrentTargetIndex() {
  for (uint8_t i = 0; i < MATCH_TARGET_COUNT; i++) {
    if (MATCH_TARGETS[i] == match_target) {
      return i;
    }
  }
  return 4; // Default to match to 9.
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
void drawDisplay() {
  bool modeChanged = !displayModeKnown || mode != lastDrawMode;
  if (modeChanged) {
    tft.fillScreen(uiBackgroundColor());
  }
  displayModeKnown = true;
  lastDrawMode = mode;

  if (mode == MODE_NAME_EDIT) {
    drawNameEditor();
    return;
  }

  if (mode == MODE_LOG) {
    drawMatchLog();
    return;
  }

  if (mode == MODE_CLOCK_SET) {
    drawClockSettings();
    return;
  }

  if (mode == MODE_APPEARANCE) {
    drawAppearanceSettings();
    return;
  }

  if (mode == MODE_MENU) {
    tft.setTextColor(TFT_TEXT, TFT_BACKGROUND);
    tft.setTextSize(3);
    tft.setCursor(248, 28);
    tft.print("SETTINGS");

    tft.fillRect(SETTINGS_BACK.x, SETTINGS_BACK.y, SETTINGS_BACK.w, SETTINGS_BACK.h, TFT_PANEL);
    tft.drawRoundRect(SETTINGS_BACK.x, SETTINGS_BACK.y, SETTINGS_BACK.w, SETTINGS_BACK.h, 6, TFT_BORDER);
    tft.setTextSize(2);
    tft.setCursor(44, 38);
    tft.print("BACK");

    tft.fillRect(52, 82, 696, 58, TFT_PANEL);
    tft.fillRect(52, 162, 696, 58, TFT_PANEL);
    tft.fillRect(52, 242, 696, 58, TFT_PANEL);
    tft.drawRoundRect(52, 82, 696, 58, 6, TFT_BORDER);
    tft.drawRoundRect(52, 162, 696, 58, 6, TFT_BORDER);
    tft.drawRoundRect(52, 242, 696, 58, 6, TFT_BORDER);

    tft.setTextColor(TFT_TEXT, TFT_PANEL);
    tft.setTextSize(2);
    tft.setCursor(76, 102);
    tft.print("MATCH TARGET");
    tft.setCursor(536, 102);
    tft.printf("%d", match_target);

    tft.setCursor(76, 182);
    tft.print("TIME / GAME");
    tft.setCursor(520, 182);
    tft.print(formatTime(initial_time_seconds));
    tft.setTextColor(TFT_MUTED, TFT_PANEL);
    tft.setTextSize(1);
    tft.setCursor(76, 207);
    tft.print("TOTAL ");
    tft.print(formatTime(matchClockSeconds()));
    tft.setTextColor(TFT_TEXT, TFT_PANEL);
    tft.setTextSize(2);

    tft.setCursor(76, 262);
    tft.print("BONUS / DELAY");
    tft.setCursor(536, 262);
    tft.printf("%u", delay_seconds);

    const Rect controls[] = {
      SETTINGS_MATCH_MINUS, SETTINGS_MATCH_PLUS,
      SETTINGS_TIME_MINUS, SETTINGS_TIME_PLUS,
      SETTINGS_DELAY_MINUS, SETTINGS_DELAY_PLUS
    };
    const char *symbols[] = {"-", "+", "-", "+", "-", "+"};
    for (uint8_t i = 0; i < 6; i++) {
      tft.fillRect(controls[i].x, controls[i].y, controls[i].w, controls[i].h, TFT_PANEL_ALT);
      tft.drawRoundRect(controls[i].x, controls[i].y, controls[i].w, controls[i].h, 6, TFT_BORDER);
      tft.setTextColor(TFT_TEXT, TFT_PANEL_ALT);
      tft.setTextSize(3);
      tft.setCursor(controls[i].x + 40, controls[i].y + 14);
      tft.print(symbols[i]);
    }

    tft.setTextColor(TFT_TEXT, TFT_BACKGROUND);
    tft.setTextSize(2);
    tft.setCursor(76, 315);
    tft.print("TIMER");
    tft.fillRect(SETTINGS_SCOPE_GAME.x, SETTINGS_SCOPE_GAME.y, SETTINGS_SCOPE_GAME.w, SETTINGS_SCOPE_GAME.h,
                 timerScope == TIMER_BY_GAME ? TFT_OK : TFT_PANEL_ALT);
    tft.drawRoundRect(SETTINGS_SCOPE_GAME.x, SETTINGS_SCOPE_GAME.y, SETTINGS_SCOPE_GAME.w, SETTINGS_SCOPE_GAME.h, 6, TFT_BORDER);
    drawCenteredInRect("GAME", SETTINGS_SCOPE_GAME, 2, timerScope == TIMER_BY_GAME ? ILI9341_BLACK : TFT_TEXT,
                       timerScope == TIMER_BY_GAME ? TFT_OK : TFT_PANEL_ALT);
    tft.fillRect(SETTINGS_SCOPE_MATCH.x, SETTINGS_SCOPE_MATCH.y, SETTINGS_SCOPE_MATCH.w, SETTINGS_SCOPE_MATCH.h,
                 timerScope == TIMER_BY_MATCH ? TFT_OK : TFT_PANEL_ALT);
    tft.drawRoundRect(SETTINGS_SCOPE_MATCH.x, SETTINGS_SCOPE_MATCH.y, SETTINGS_SCOPE_MATCH.w, SETTINGS_SCOPE_MATCH.h, 6, TFT_BORDER);
    drawCenteredInRect("MATCH", SETTINGS_SCOPE_MATCH, 2, timerScope == TIMER_BY_MATCH ? ILI9341_BLACK : TFT_TEXT,
                       timerScope == TIMER_BY_MATCH ? TFT_OK : TFT_PANEL_ALT);

    const Rect modeButtons[] = {SETTINGS_MODE_SIMPLE, SETTINGS_MODE_BRONSTEIN, SETTINGS_MODE_FISCHER};
    const char *modeLabels[] = {"SIMPLE", "BRONSTEIN", "FISCHER"};
    for (uint8_t i = 0; i < 3; i++) {
      bool selected = (uint8_t)timingMode == i;
      tft.fillRect(modeButtons[i].x, modeButtons[i].y, modeButtons[i].w, modeButtons[i].h,
                   selected ? TFT_OK : TFT_PANEL_ALT);
      tft.drawRoundRect(modeButtons[i].x, modeButtons[i].y, modeButtons[i].w, modeButtons[i].h, 6, TFT_BORDER);
      drawCenteredInRect(modeLabels[i], modeButtons[i], 2, selected ? ILI9341_BLACK : TFT_TEXT,
                         selected ? TFT_OK : TFT_PANEL_ALT);
    }

    tft.fillRect(SETTINGS_APPEARANCE.x, SETTINGS_APPEARANCE.y, SETTINGS_APPEARANCE.w, SETTINGS_APPEARANCE.h, TFT_PANEL_ALT);
    tft.drawRoundRect(SETTINGS_APPEARANCE.x, SETTINGS_APPEARANCE.y, SETTINGS_APPEARANCE.w, SETTINGS_APPEARANCE.h, 6, TFT_BORDER);
    drawCenteredInRect("APPEARANCE", SETTINGS_APPEARANCE, 2, TFT_TEXT, TFT_PANEL_ALT);

    tft.fillRect(SETTINGS_CLOCK.x, SETTINGS_CLOCK.y, SETTINGS_CLOCK.w, SETTINGS_CLOCK.h, TFT_PANEL_ALT);
    tft.drawRoundRect(SETTINGS_CLOCK.x, SETTINGS_CLOCK.y, SETTINGS_CLOCK.w, SETTINGS_CLOCK.h, 6, TFT_BORDER);
    drawCenteredInRect("CLOCK", SETTINGS_CLOCK, 2, TFT_TEXT, TFT_PANEL_ALT);

    tft.fillRect(SETTINGS_START.x, SETTINGS_START.y, SETTINGS_START.w, SETTINGS_START.h, TFT_OK);
    tft.fillRect(SETTINGS_END.x, SETTINGS_END.y, SETTINGS_END.w, SETTINGS_END.h, TFT_DANGER);
    tft.drawRoundRect(SETTINGS_START.x, SETTINGS_START.y, SETTINGS_START.w, SETTINGS_START.h, 6, TFT_BORDER);
    tft.drawRoundRect(SETTINGS_END.x, SETTINGS_END.y, SETTINGS_END.w, SETTINGS_END.h, 6, TFT_BORDER);
    drawCenteredInRect("START MATCH", SETTINGS_START, 2, ILI9341_BLACK, TFT_OK);
    drawCenteredInRect("END MATCH", SETTINGS_END, 2, TFT_TEXT, TFT_DANGER);

    return;
  }

  drawStatusBar();

  uint16_t background = uiBackgroundColor();
  uint16_t panel = uiPanelColor();
  uint16_t text = uiTextColor();

  uint16_t headerABg = playerA_color;
  uint16_t headerBBg = playerB_color;
  uint16_t headerAText = ILI9341_WHITE;
  uint16_t headerBText = ILI9341_WHITE;

  tft.fillRect(0, REGION_STATUS.h, PANEL_WIDTH, PANEL_HEIGHT - REGION_STATUS.h, background);

  tft.fillRect(REGION_HEADER_A.x, REGION_HEADER_A.y, REGION_HEADER_A.w, REGION_HEADER_A.h, headerABg);
  tft.fillRect(REGION_HEADER_B.x, REGION_HEADER_B.y, REGION_HEADER_B.w, REGION_HEADER_B.h, headerBBg);
  drawTimerPanel(PLAYER_A, REGION_TIMER_A);
  tft.fillRect(REGION_PLAY_PAUSE.x, REGION_PLAY_PAUSE.y, REGION_PLAY_PAUSE.w, REGION_PLAY_PAUSE.h, panel);
  tft.fillRect(REGION_END_GAME.x, REGION_END_GAME.y, REGION_END_GAME.w, REGION_END_GAME.h, panel);
  tft.fillRect(REGION_END_MATCH.x, REGION_END_MATCH.y, REGION_END_MATCH.w, REGION_END_MATCH.h,
               mode == MODE_MATCH_OVER ? TFT_DANGER : panel);
  tft.fillRect(REGION_SCORE_A.x, REGION_SCORE_A.y, REGION_SCORE_A.w, REGION_SCORE_A.h, playerA_color);
  tft.fillRect(REGION_MATCH_TARGET.x, REGION_MATCH_TARGET.y, REGION_MATCH_TARGET.w, REGION_MATCH_TARGET.h, panel);
  tft.fillRect(REGION_LOG.x, REGION_LOG.y, REGION_LOG.w, REGION_LOG.h, panel);
  tft.fillRect(REGION_SETTINGS.x, REGION_SETTINGS.y, REGION_SETTINGS.w, REGION_SETTINGS.h, panel);
  tft.fillRect(REGION_SCORE_B.x, REGION_SCORE_B.y, REGION_SCORE_B.w, REGION_SCORE_B.h, playerB_color);

  tft.drawRoundRect(REGION_HEADER_A.x, REGION_HEADER_A.y, REGION_HEADER_A.w, REGION_HEADER_A.h, 4, TFT_ACCENT_A);
  tft.drawRoundRect(REGION_HEADER_B.x, REGION_HEADER_B.y, REGION_HEADER_B.w, REGION_HEADER_B.h, 4, TFT_ACCENT_B);
  tft.drawRoundRect(REGION_PLAY_PAUSE.x, REGION_PLAY_PAUSE.y, REGION_PLAY_PAUSE.w, REGION_PLAY_PAUSE.h, 6, TFT_BUTTON_BORDER);
  tft.drawRoundRect(REGION_END_GAME.x, REGION_END_GAME.y, REGION_END_GAME.w, REGION_END_GAME.h, 6, TFT_BUTTON_BORDER);
  tft.drawRoundRect(REGION_END_MATCH.x, REGION_END_MATCH.y, REGION_END_MATCH.w, REGION_END_MATCH.h, 6, TFT_BUTTON_BORDER);
  drawTimerPanel(PLAYER_B, REGION_TIMER_B);
  tft.drawRoundRect(REGION_SCORE_A.x, REGION_SCORE_A.y, REGION_SCORE_A.w, REGION_SCORE_A.h, 6, TFT_ACCENT_A);
  tft.drawRoundRect(REGION_MATCH_TARGET.x, REGION_MATCH_TARGET.y, REGION_MATCH_TARGET.w, REGION_MATCH_TARGET.h, 6, TFT_BUTTON_BORDER);
  tft.drawRoundRect(REGION_LOG.x, REGION_LOG.y, REGION_LOG.w, REGION_LOG.h, 6, TFT_BUTTON_BORDER);
  tft.drawRoundRect(REGION_SETTINGS.x, REGION_SETTINGS.y, REGION_SETTINGS.w, REGION_SETTINGS.h, 6, TFT_BUTTON_BORDER);
  tft.drawRoundRect(REGION_SCORE_B.x, REGION_SCORE_B.y, REGION_SCORE_B.w, REGION_SCORE_B.h, 6, TFT_ACCENT_B);

  tft.setTextSize(2);
  tft.setTextColor(text, panel);
  drawCenteredInRect(playerA_name, REGION_HEADER_A, playerA_name.length() > 9 ? 1 : 2, headerAText, headerABg);
  drawCenteredInRect(playerB_name, REGION_HEADER_B, playerB_name.length() > 9 ? 1 : 2, headerBText, headerBBg);

  drawCenteredInRect(playPauseLabel(), REGION_PLAY_PAUSE, 2, text, panel);

  drawCenteredInRect(mode == MODE_TIMEOUT ? "New Game" : "End Game", REGION_END_GAME, 2, text, panel);
  drawCenteredInRect(mode == MODE_MATCH_OVER ? "Reset" : "End Match", REGION_END_MATCH, 2,
                     mode == MODE_MATCH_OVER ? TFT_TEXT : text, mode == MODE_MATCH_OVER ? TFT_DANGER : panel);
  tft.setTextColor(text, panel);

  int displayScoreA = scoreDisplayMode == SCORE_POINTS_TO_WIN ? max(0, match_target - playerA_score) : playerA_score;
  int displayScoreB = scoreDisplayMode == SCORE_POINTS_TO_WIN ? max(0, match_target - playerB_score) : playerB_score;
  Rect scoreNumberA = REGION_SCORE_A;
  Rect scoreNumberB = REGION_SCORE_B;
  if (scoreDisplayMode == SCORE_POINTS_TO_WIN) {
    scoreNumberA = {REGION_SCORE_A.x, REGION_SCORE_A.y + 4, REGION_SCORE_A.w, REGION_SCORE_A.h - 48};
    scoreNumberB = {REGION_SCORE_B.x, REGION_SCORE_B.y + 4, REGION_SCORE_B.w, REGION_SCORE_B.h - 48};
  }
  drawScoreInRect(displayScoreA, scoreNumberA, textColorForPlayerColor(playerA_color), playerA_color);
  drawScoreInRect(displayScoreB, scoreNumberB, textColorForPlayerColor(playerB_color), playerB_color);
  if (scoreDisplayMode == SCORE_POINTS_TO_WIN) {
    drawCenteredInRect("Points to Win", {REGION_SCORE_A.x, REGION_SCORE_A.y + REGION_SCORE_A.h - 46, REGION_SCORE_A.w, 34},
                       2, textColorForPlayerColor(playerA_color), playerA_color);
    drawCenteredInRect("Points to Win", {REGION_SCORE_B.x, REGION_SCORE_B.y + REGION_SCORE_B.h - 46, REGION_SCORE_B.w, 34},
                       2, textColorForPlayerColor(playerB_color), playerB_color);
  }

  drawCenteredInRect("Match to:", {REGION_MATCH_TARGET.x, REGION_MATCH_TARGET.y + 22, REGION_MATCH_TARGET.w, 26}, 2, text, panel);
  drawMatchTargetInRect(match_target, {REGION_MATCH_TARGET.x, REGION_MATCH_TARGET.y + 56, REGION_MATCH_TARGET.w, 74}, text, panel);
  drawCenteredInRect(String("Game ") + String(game_number), {REGION_MATCH_TARGET.x, REGION_MATCH_TARGET.y + 136, REGION_MATCH_TARGET.w, 28}, 2, text, panel);
  String stateLabel = "";
  uint16_t stateColor = TFT_MAGENTA;
  if (mode == MODE_MATCH_OVER) {
    stateLabel = "Match Ended";
    stateColor = TFT_WARN;
  } else if (crawford_active || pending_crawford_next_game) {
    stateLabel = "CRAWFORD";
  } else if (post_crawford || crawford_used) {
    stateLabel = "POST CRAWFORD";
  }
  drawCenteredInRect(stateLabel, {REGION_MATCH_TARGET.x, REGION_MATCH_TARGET.y + 166, REGION_MATCH_TARGET.w, 30}, 2, stateColor, panel);

  tft.setTextColor(text, panel);
  drawCenteredInRect("LOG", REGION_LOG, 2, text, panel);
  drawCenteredInRect("SETTINGS", REGION_SETTINGS, 2, text, panel);

  if (mode == MODE_TIMEOUT) {
    drawTimeoutBanner();
  }

  if (mode == MODE_CONFIRM_END_MATCH) {
    drawEndMatchConfirm();
  }

}

void drawMatchWinnerBanner() {
  const Rect banner = {6, 204, 788, 260};
  const Rect nameLine = {24, 270, 752, 76};
  const Rect winnerLine = {24, 348, 752, 76};
  String winnerName = (matchWinner == PLAYER_A) ? playerA_name : playerB_name;

  tft.fillRect(banner.x, banner.y, banner.w, banner.h, TFT_PANEL);
  tft.drawRoundRect(banner.x, banner.y, banner.w, banner.h, 6, TFT_OK);
  drawCenteredInRect(winnerName, nameLine, winnerName.length() > 8 ? 6 : 7, TFT_TEXT, TFT_PANEL);
  drawCenteredInRect("IS THE WINNER", winnerLine, 5, TFT_OK, TFT_PANEL);
}

void drawTimeoutBanner() {
  const Rect banner = {42, 224, 716, 174};
  Player loser = timeoutPlayer;
  String loserName = (loser == PLAYER_A) ? playerA_name : playerB_name;

  tft.fillRect(banner.x, banner.y, banner.w, banner.h, TFT_PANEL);
  tft.drawRoundRect(banner.x, banner.y, banner.w, banner.h, 6, TFT_DANGER);
  drawCenteredInRect(loserName, {banner.x + 16, banner.y + 24, banner.w - 32, 48}, loserName.length() > 8 ? 4 : 5, TFT_TEXT, TFT_PANEL);
  drawCenteredInRect("RAN OUT OF TIME", {banner.x + 16, banner.y + 80, banner.w - 32, 36}, 3, TFT_DANGER, TFT_PANEL);
  drawCenteredInRect("LOSES THE GAME", {banner.x + 16, banner.y + 122, banner.w - 32, 34}, 3, TFT_WARN, TFT_PANEL);
}

void drawEndMatchConfirm() {
  const Rect dialog = {118, 132, 564, 236};

  tft.fillRect(dialog.x, dialog.y, dialog.w, dialog.h, TFT_PANEL);
  tft.drawRoundRect(dialog.x, dialog.y, dialog.w, dialog.h, 8, TFT_BUTTON_BORDER);

  drawCenteredInRect("END MATCH?", {dialog.x, dialog.y + 28, dialog.w, 34}, 3, TFT_WARN, TFT_PANEL);
  drawCenteredInRect("This will finish the match", {dialog.x, dialog.y + 84, dialog.w, 24}, 2, TFT_TEXT, TFT_PANEL);
  drawCenteredInRect("and record the current leader.", {dialog.x, dialog.y + 112, dialog.w, 24}, 2, TFT_TEXT, TFT_PANEL);

  tft.fillRect(CONFIRM_NO.x, CONFIRM_NO.y, CONFIRM_NO.w, CONFIRM_NO.h, TFT_PANEL_ALT);
  tft.drawRoundRect(CONFIRM_NO.x, CONFIRM_NO.y, CONFIRM_NO.w, CONFIRM_NO.h, 6, TFT_BUTTON_BORDER);
  drawCenteredInRect("NO", CONFIRM_NO, 3, TFT_TEXT, TFT_PANEL_ALT);

  tft.fillRect(CONFIRM_YES.x, CONFIRM_YES.y, CONFIRM_YES.w, CONFIRM_YES.h, TFT_DANGER);
  tft.drawRoundRect(CONFIRM_YES.x, CONFIRM_YES.y, CONFIRM_YES.w, CONFIRM_YES.h, 6, TFT_BUTTON_BORDER);
  drawCenteredInRect("YES", CONFIRM_YES, 3, TFT_TEXT, TFT_DANGER);
}

void drawMatchLog() {
  tft.fillScreen(TFT_BACKGROUND);

  tft.setTextColor(TFT_TEXT, TFT_BACKGROUND);
  tft.setTextSize(3);
  tft.setCursor(286, 28);
  tft.print("MATCH LOG");

  tft.fillRect(LOG_BACK.x, LOG_BACK.y, LOG_BACK.w, LOG_BACK.h, TFT_PANEL);
  tft.drawRoundRect(LOG_BACK.x, LOG_BACK.y, LOG_BACK.w, LOG_BACK.h, 6, TFT_BORDER);
  drawCenteredInRect("BACK", LOG_BACK, 2, TFT_TEXT, TFT_PANEL);

  tft.fillRect(LOG_CLEAR.x, LOG_CLEAR.y, LOG_CLEAR.w, LOG_CLEAR.h, TFT_DANGER);
  tft.drawRoundRect(LOG_CLEAR.x, LOG_CLEAR.y, LOG_CLEAR.w, LOG_CLEAR.h, 6, TFT_BORDER);
  drawCenteredInRect("CLEAR", LOG_CLEAR, 2, TFT_TEXT, TFT_DANGER);

  if (matchLogCount == 0) {
    drawCenteredInRect("NO MATCHES YET", {120, 210, 560, 70}, 3, TFT_MUTED, TFT_BACKGROUND);
    return;
  }

  for (uint8_t i = 0; i < matchLogCount && i < MATCH_LOG_MAX_ENTRIES; i++) {
    const int16_t y = 88 + (i * 46);
    const Rect row = {34, y, 732, 40};
    String date = matchLogPart(matchLog[i], 0);
    String nameA = matchLogPart(matchLog[i], 1);
    String nameB = matchLogPart(matchLog[i], 2);
    String score = matchLogPart(matchLog[i], 3);
    String names = nameA + " VS " + nameB;
    String scoreLine = "SCORE " + score;

    tft.fillRect(row.x, row.y, row.w, row.h, (i % 2 == 0) ? TFT_PANEL : TFT_PANEL_ALT);
    tft.drawRoundRect(row.x, row.y, row.w, row.h, 4, TFT_BORDER);
    tft.setTextColor(TFT_MUTED, (i % 2 == 0) ? TFT_PANEL : TFT_PANEL_ALT);
    tft.setTextSize(1);
    tft.setCursor(row.x + 12, row.y + 8);
    tft.print(date);
    tft.setCursor(row.x + 152, row.y + 8);
    tft.print(names);
    tft.setTextColor(TFT_TEXT, (i % 2 == 0) ? TFT_PANEL : TFT_PANEL_ALT);
    tft.setTextSize(2);
    tft.setCursor(row.x + 560, row.y + 12);
    tft.print(scoreLine);
  }
}

void drawClockSettings() {
  tft.fillScreen(TFT_BACKGROUND);

  tft.setTextColor(TFT_TEXT, TFT_BACKGROUND);
  tft.setTextSize(3);
  tft.setCursor(248, 28);
  tft.print("CLOCK SET");

  tft.fillRect(CLOCK_BACK.x, CLOCK_BACK.y, CLOCK_BACK.w, CLOCK_BACK.h, TFT_PANEL);
  tft.drawRoundRect(CLOCK_BACK.x, CLOCK_BACK.y, CLOCK_BACK.w, CLOCK_BACK.h, 6, TFT_BORDER);
  drawCenteredInRect("BACK", CLOCK_BACK, 2, TFT_TEXT, TFT_PANEL);

  const Rect rows[] = {
    {52, 82, 696, 48},
    {52, 140, 696, 48},
    {52, 198, 696, 48},
    {52, 256, 696, 48},
    {52, 314, 696, 48}
  };
  const Rect minusButtons[] = {
    CLOCK_YEAR_MINUS,
    CLOCK_MONTH_MINUS,
    CLOCK_DAY_MINUS,
    CLOCK_HOUR_MINUS,
    CLOCK_MINUTE_MINUS
  };
  const Rect plusButtons[] = {
    CLOCK_YEAR_PLUS,
    CLOCK_MONTH_PLUS,
    CLOCK_DAY_PLUS,
    CLOCK_HOUR_PLUS,
    CLOCK_MINUTE_PLUS
  };
  const char *labels[] = {"YEAR", "MONTH", "DAY", "HOUR", "MINUTE"};
  uint16_t values[] = {editingRtcYear, editingRtcMonth, editingRtcDay, editingRtcHour, editingRtcMinute};

  for (uint8_t i = 0; i < 5; i++) {
    tft.fillRect(rows[i].x, rows[i].y, rows[i].w, rows[i].h, TFT_PANEL);
    tft.drawRoundRect(rows[i].x, rows[i].y, rows[i].w, rows[i].h, 6, TFT_BORDER);
    tft.setTextColor(TFT_TEXT, TFT_PANEL);
    tft.setTextSize(2);
    tft.setCursor(76, rows[i].y + 16);
    tft.print(labels[i]);
    tft.setCursor(i == 0 ? 514 : 536, rows[i].y + 16);
    if (i == 0) {
      tft.printf("%u", values[i]);
    } else {
      tft.printf("%02u", values[i]);
    }

    tft.fillRect(minusButtons[i].x, minusButtons[i].y, minusButtons[i].w, minusButtons[i].h, TFT_PANEL_ALT);
    tft.drawRoundRect(minusButtons[i].x, minusButtons[i].y, minusButtons[i].w, minusButtons[i].h, 6, TFT_BORDER);
    drawCenteredInRect("-", minusButtons[i], 3, TFT_TEXT, TFT_PANEL_ALT);
    tft.fillRect(plusButtons[i].x, plusButtons[i].y, plusButtons[i].w, plusButtons[i].h, TFT_PANEL_ALT);
    tft.drawRoundRect(plusButtons[i].x, plusButtons[i].y, plusButtons[i].w, plusButtons[i].h, 6, TFT_BORDER);
    drawCenteredInRect("+", plusButtons[i], 3, TFT_TEXT, TFT_PANEL_ALT);
  }

  tft.setTextColor(ds3231Found ? TFT_MUTED : TFT_DANGER, TFT_BACKGROUND);
  tft.setTextSize(1);
  tft.setCursor(72, 380);
  tft.print(ds3231Found ? "SAVE writes this date/time to the DS3231 RTC." : "DS3231 RTC not detected on I2C address 0x68.");

  tft.fillRect(CLOCK_SAVE.x, CLOCK_SAVE.y, CLOCK_SAVE.w, CLOCK_SAVE.h, ds3231Found ? TFT_OK : TFT_PANEL_ALT);
  tft.drawRoundRect(CLOCK_SAVE.x, CLOCK_SAVE.y, CLOCK_SAVE.w, CLOCK_SAVE.h, 6, TFT_BORDER);
  drawCenteredInRect("SAVE", CLOCK_SAVE, 2, ds3231Found ? ILI9341_BLACK : TFT_MUTED, ds3231Found ? TFT_OK : TFT_PANEL_ALT);
}

void drawAppearanceSettings() {
  uint16_t background = uiBackgroundColor();
  uint16_t panel = uiPanelColor();
  uint16_t text = uiTextColor();

  tft.fillScreen(background);
  drawStatusBar();
  drawCenteredInRect("APPEARANCE", {180, 42, 440, 46}, 3, text, background);

  tft.fillRect(APPEARANCE_BACK.x, APPEARANCE_BACK.y, APPEARANCE_BACK.w, APPEARANCE_BACK.h, panel);
  tft.drawRoundRect(APPEARANCE_BACK.x, APPEARANCE_BACK.y, APPEARANCE_BACK.w, APPEARANCE_BACK.h, 6, TFT_BORDER);
  drawCenteredInRect("BACK", APPEARANCE_BACK, 2, text, panel);

  drawCenteredInRect("COLOR THEME", {170, 92, 460, 30}, 2, text, background);
  drawCenteredInRect("SCORE DISPLAY", {170, 246, 460, 30}, 2, text, background);

  const Rect buttons[] = {APPEARANCE_NIGHT, APPEARANCE_DAY, APPEARANCE_SCORE, APPEARANCE_POINTS};
  const char *labels[] = {"NIGHT", "DAY", "SCORE", "POINTS TO WIN"};
  const bool selected[] = {
    displayTheme == THEME_NIGHT,
    displayTheme == THEME_DAY,
    scoreDisplayMode == SCORE_STANDARD,
    scoreDisplayMode == SCORE_POINTS_TO_WIN
  };

  for (uint8_t i = 0; i < 4; i++) {
    uint16_t buttonBg = selected[i] ? TFT_OK : panel;
    uint16_t buttonText = selected[i] ? ILI9341_BLACK : text;
    tft.fillRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, buttonBg);
    tft.drawRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 6, TFT_BORDER);
    drawCenteredInRect(labels[i], buttons[i], 2, buttonText, buttonBg);
  }
}

void drawNameEditor() {
  const Rect title = {70, 24, 660, 42};
  const Rect input = {92, 78, 616, 64};
  const Rect cancel = {72, 420, 168, 44};
  const Rect clearKey = {258, 420, 168, 44};
  const Rect ok = {560, 420, 168, 44};
  const char *rows[] = {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ"};
  const int16_t rowY[] = {148, 208, 268};
  const int16_t keyW = 66;
  const int16_t keyH = 44;
  const int16_t gap = 8;
  const int16_t swatchY = 326;
  const int16_t swatchSize = 36;
  const int16_t swatchGap = 14;

  tft.fillScreen(TFT_BACKGROUND);
  tft.fillRect(48, 14, 704, 452, TFT_PANEL);
  tft.drawRoundRect(48, 14, 704, 452, 8, TFT_BORDER);
  drawCenteredInRect(editingNameFor == PLAYER_A ? "PLAYER A NAME" : "PLAYER B NAME", title, 3, TFT_TEXT, TFT_PANEL);

  tft.fillRect(input.x, input.y, input.w, input.h, TFT_BACKGROUND);
  tft.drawRoundRect(input.x, input.y, input.w, input.h, 6, TFT_OK);
  drawCenteredInRect(editingName.length() == 0 ? " " : editingName, input, 4, TFT_TEXT, TFT_BACKGROUND);

  for (uint8_t r = 0; r < 3; r++) {
    uint8_t count = strlen(rows[r]);
    int16_t startX = (PANEL_WIDTH - ((count * keyW) + ((count - 1) * gap))) / 2;
    for (uint8_t i = 0; i < count; i++) {
      Rect key = {startX + (int16_t)i * (keyW + gap), rowY[r], keyW, keyH};
      tft.fillRect(key.x, key.y, key.w, key.h, TFT_PANEL_ALT);
      tft.drawRoundRect(key.x, key.y, key.w, key.h, 6, TFT_BORDER);
      String label;
      label += rows[r][i];
      drawCenteredInRect(label, key, 3, TFT_TEXT, TFT_PANEL_ALT);
    }
  }

  for (uint8_t i = 0; i < playerColorOptionCount(); i++) {
    int16_t startX = (PANEL_WIDTH - (playerColorOptionCount() * swatchSize + (playerColorOptionCount() - 1) * swatchGap)) / 2;
    Rect swatch = {startX + (int16_t)i * (swatchSize + swatchGap), swatchY, swatchSize, swatchSize};
    uint16_t color = playerColorOption(i);
    tft.fillRect(swatch.x, swatch.y, swatch.w, swatch.h, color);
    tft.drawRoundRect(swatch.x, swatch.y, swatch.w, swatch.h, 5, color == editingPlayerColor ? TFT_OK : TFT_BORDER);
    if (color == PLAYER_COLOR_WHITE) {
      tft.drawRoundRect(swatch.x + 3, swatch.y + 3, swatch.w - 6, swatch.h - 6, 4, TFT_MUTED);
    }
  }

  const Rect spaceKey = {166, 372, 244, 38};
  const Rect delKey = {430, 372, 204, 38};
  tft.fillRect(spaceKey.x, spaceKey.y, spaceKey.w, spaceKey.h, TFT_PANEL_ALT);
  tft.drawRoundRect(spaceKey.x, spaceKey.y, spaceKey.w, spaceKey.h, 6, TFT_BORDER);
  drawCenteredInRect("SPACE", spaceKey, 2, TFT_TEXT, TFT_PANEL_ALT);
  tft.fillRect(delKey.x, delKey.y, delKey.w, delKey.h, TFT_PANEL_ALT);
  tft.drawRoundRect(delKey.x, delKey.y, delKey.w, delKey.h, 6, TFT_BORDER);
  drawCenteredInRect("DEL", delKey, 2, TFT_TEXT, TFT_PANEL_ALT);

  tft.fillRect(cancel.x, cancel.y, cancel.w, cancel.h, TFT_DANGER);
  tft.drawRoundRect(cancel.x, cancel.y, cancel.w, cancel.h, 6, TFT_BORDER);
  drawCenteredInRect("CANCEL", cancel, 2, TFT_TEXT, TFT_DANGER);
  tft.fillRect(clearKey.x, clearKey.y, clearKey.w, clearKey.h, TFT_PANEL_ALT);
  tft.drawRoundRect(clearKey.x, clearKey.y, clearKey.w, clearKey.h, 6, TFT_BORDER);
  drawCenteredInRect("CLEAR", clearKey, 2, TFT_TEXT, TFT_PANEL_ALT);
  tft.fillRect(ok.x, ok.y, ok.w, ok.h, TFT_OK);
  tft.drawRoundRect(ok.x, ok.y, ok.w, ok.h, 6, TFT_BORDER);
  drawCenteredInRect("OK", ok, 2, ILI9341_BLACK, TFT_OK);
}

void openNameEditor(Player player) {
  if (player != PLAYER_A && player != PLAYER_B) {
    return;
  }

  clock_running = false;
  active_player = PLAYER_NONE;
  editingNameFor = player;
  editingName = (player == PLAYER_A) ? playerA_name : playerB_name;
  editingPlayerColor = colorForPlayer(player);
  mode = MODE_NAME_EDIT;
  markDisplayDirty();
}

void openClockSettings() {
  uint8_t second = 0;
  uint8_t minute = 0;
  uint8_t hour = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint16_t year = 0;
  if (ds3231ReadTime(&second, &minute, &hour, &day, &month, &year) && rtcDateLooksSet(day, month, year)) {
    editingRtcYear = year;
    editingRtcMonth = month;
    editingRtcDay = day;
    editingRtcHour = hour;
    editingRtcMinute = minute;
  } else {
    editingRtcYear = 2026;
    editingRtcMonth = 1;
    editingRtcDay = 1;
    editingRtcHour = 12;
    editingRtcMinute = 0;
  }
  mode = MODE_CLOCK_SET;
  markDisplayDirty();
}

void handleClockSettingsTap(uint16_t x, uint16_t y) {
  if (CLOCK_BACK.containsPadded(x, y, 18, 14)) {
    mode = MODE_MENU;
    markDisplayDirty();
    return;
  }

  const Rect minusButtons[] = {
    CLOCK_YEAR_MINUS,
    CLOCK_MONTH_MINUS,
    CLOCK_DAY_MINUS,
    CLOCK_HOUR_MINUS,
    CLOCK_MINUTE_MINUS
  };
  const Rect plusButtons[] = {
    CLOCK_YEAR_PLUS,
    CLOCK_MONTH_PLUS,
    CLOCK_DAY_PLUS,
    CLOCK_HOUR_PLUS,
    CLOCK_MINUTE_PLUS
  };

  for (uint8_t i = 0; i < 5; i++) {
    if (minusButtons[i].containsPadded(x, y, 36, 12)) {
      adjustRtcField(i, -1);
      return;
    }
    if (plusButtons[i].containsPadded(x, y, 36, 12)) {
      adjustRtcField(i, +1);
      return;
    }
  }

  if (CLOCK_SAVE.containsPadded(x, y, 18, 14) && ds3231Found) {
    if (ds3231SetTime(0, editingRtcMinute, editingRtcHour, editingRtcDay, editingRtcMonth, editingRtcYear)) {
      logPrintf("DS3231 RTC set to %04u-%02u-%02u %02u:%02u:00\n",
                editingRtcYear, editingRtcMonth, editingRtcDay, editingRtcHour, editingRtcMinute);
      updateRtcStatusCache(true);
      mode = MODE_MENU;
    } else {
      logLine("DS3231 RTC save failed.");
    }
    markDisplayDirty();
    return;
  }
}

void handleAppearanceSettingsTap(uint16_t x, uint16_t y) {
  if (APPEARANCE_BACK.containsPadded(x, y, 18, 14)) {
    mode = MODE_MENU;
    markDisplayDirty();
    return;
  }

  if (APPEARANCE_NIGHT.containsPadded(x, y, 12, 10)) {
    displayTheme = THEME_NIGHT;
  } else if (APPEARANCE_DAY.containsPadded(x, y, 12, 10)) {
    displayTheme = THEME_DAY;
  } else if (APPEARANCE_SCORE.containsPadded(x, y, 12, 10)) {
    scoreDisplayMode = SCORE_STANDARD;
  } else if (APPEARANCE_POINTS.containsPadded(x, y, 12, 10)) {
    scoreDisplayMode = SCORE_POINTS_TO_WIN;
  } else {
    return;
  }

  markSettingsDirty();
  markDisplayDirty();
}

void handleEndMatchConfirmTap(uint16_t x, uint16_t y) {
  if (CONFIRM_NO.containsPadded(x, y, 18, 14)) {
    mode = MODE_MATCH;
    markDisplayDirty();
    return;
  }

  if (CONFIRM_YES.containsPadded(x, y, 18, 14)) {
    forceEndMatch();
    return;
  }
}

void adjustRtcField(uint8_t field, int delta) {
  switch (field) {
    case 0:
      editingRtcYear = constrain((int)editingRtcYear + delta, 2024, 2099);
      break;
    case 1:
      editingRtcMonth = constrain((int)editingRtcMonth + delta, 1, 12);
      break;
    case 2:
      editingRtcDay = constrain((int)editingRtcDay + delta, 1, (int)daysInMonth(editingRtcMonth, editingRtcYear));
      break;
    case 3:
      editingRtcHour = (uint8_t)(((int)editingRtcHour + delta + 24) % 24);
      break;
    case 4:
      editingRtcMinute = (uint8_t)(((int)editingRtcMinute + delta + 60) % 60);
      break;
    default:
      break;
  }

  uint8_t maxDay = daysInMonth(editingRtcMonth, editingRtcYear);
  if (editingRtcDay > maxDay) {
    editingRtcDay = maxDay;
  }
  markDisplayDirty();
}

void handleNameEditorTap(uint16_t x, uint16_t y) {
  const Rect cancel = {72, 420, 168, 44};
  const Rect clearKey = {258, 420, 168, 44};
  const Rect ok = {560, 420, 168, 44};
  const Rect spaceKey = {166, 372, 244, 38};
  const Rect delKey = {430, 372, 204, 38};
  const char *rows[] = {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ"};
  const int16_t rowY[] = {148, 208, 268};
  const int16_t keyW = 66;
  const int16_t keyH = 44;
  const int16_t gap = 8;
  const int16_t swatchY = 326;
  const int16_t swatchSize = 36;
  const int16_t swatchGap = 14;

  if (cancel.contains(x, y)) {
    editingNameFor = PLAYER_NONE;
    mode = MODE_MATCH;
    markDisplayDirty();
    return;
  }

  if (ok.contains(x, y)) {
    if (editingName.length() == 0) {
      editingName = (editingNameFor == PLAYER_A) ? "PLAYER A" : "PLAYER B";
    }
    if (editingNameFor == PLAYER_A) {
      playerA_name = editingName;
      playerA_color = editingPlayerColor;
    } else if (editingNameFor == PLAYER_B) {
      playerB_name = editingName;
      playerB_color = editingPlayerColor;
    }
    savePlayerNames();
    editingNameFor = PLAYER_NONE;
    mode = MODE_MATCH;
    markDisplayDirty();
    return;
  }

  if (clearKey.contains(x, y)) {
    editingName = "";
    markDisplayDirty();
    return;
  }

  if (delKey.contains(x, y)) {
    if (editingName.length() > 0) {
      editingName.remove(editingName.length() - 1);
    }
    markDisplayDirty();
    return;
  }

  if (spaceKey.contains(x, y)) {
    if (editingName.length() < PLAYER_NAME_MAX_LEN && editingName.length() > 0) {
      editingName += ' ';
    }
    markDisplayDirty();
    return;
  }

  int16_t swatchStartX = (PANEL_WIDTH - (playerColorOptionCount() * swatchSize + (playerColorOptionCount() - 1) * swatchGap)) / 2;
  for (uint8_t i = 0; i < playerColorOptionCount(); i++) {
    Rect swatch = {swatchStartX + (int16_t)i * (swatchSize + swatchGap), swatchY, swatchSize, swatchSize};
    if (swatch.contains(x, y)) {
      editingPlayerColor = playerColorOption(i);
      markDisplayDirty();
      return;
    }
  }

  for (uint8_t r = 0; r < 3; r++) {
    uint8_t count = strlen(rows[r]);
    int16_t startX = (PANEL_WIDTH - ((count * keyW) + ((count - 1) * gap))) / 2;
    for (uint8_t i = 0; i < count; i++) {
      Rect key = {startX + (int16_t)i * (keyW + gap), rowY[r], keyW, keyH};
      if (key.contains(x, y) && editingName.length() < PLAYER_NAME_MAX_LEN) {
        editingName += rows[r][i];
        markDisplayDirty();
        return;
      }
    }
  }
}

uint16_t colorForPlayer(Player player) {
  if (player == PLAYER_A) {
    return playerA_color;
  }
  if (player == PLAYER_B) {
    return playerB_color;
  }
  return TFT_PANEL;
}

uint16_t textColorForPlayerColor(uint16_t color) {
  return (color == PLAYER_COLOR_WHITE || color == PLAYER_COLOR_YELLOW) ? TFT_TEXT_DARK : TFT_TEXT;
}

uint16_t playerColorOption(uint8_t index) {
  switch (index) {
    case 0:
      return PLAYER_COLOR_RED;
    case 1:
      return PLAYER_COLOR_BLUE;
    case 2:
      return PLAYER_COLOR_GREEN;
    case 3:
      return PLAYER_COLOR_YELLOW;
    case 4:
      return PLAYER_COLOR_GRAY;
    case 5:
      return PLAYER_COLOR_WHITE;
    default:
      return PLAYER_COLOR_BLUE;
  }
}

uint8_t playerColorOptionCount() {
  return 6;
}

bool dayThemeEnabled() {
  return displayTheme == THEME_DAY;
}

uint16_t uiBackgroundColor() {
  return dayThemeEnabled() ? 0x8410 : TFT_BACKGROUND;
}

uint16_t uiPanelColor() {
  return dayThemeEnabled() ? 0xEF5D : TFT_PANEL;
}

uint16_t uiTextColor() {
  return dayThemeEnabled() ? ILI9341_BLACK : TFT_TEXT;
}

uint16_t uiMutedColor() {
  return dayThemeEnabled() ? 0x630C : TFT_MUTED;
}

void drawTimerPanels() {
  if (mode != MODE_MATCH) {
    return;
  }

  drawStatusBar();
  drawTimerPanel(PLAYER_A, REGION_TIMER_A);
  drawTimerPanel(PLAYER_B, REGION_TIMER_B);
}

void drawTimerPanel(Player player, const Rect &region) {
  bool isA = (player == PLAYER_A);
  bool active = (active_player == player && clock_running);
  uint16_t accent = isA ? TFT_ACCENT_A : TFT_ACCENT_B;
  uint16_t panelColor = dayThemeEnabled() ? uiPanelColor() : (active ? ILI9341_WHITE : TFT_PANEL);
  uint32_t remaining = visibleReserveMs(player, millis());
  String value = formatTimeMs(remaining);
  uint16_t valueColor = dayThemeEnabled() ? ILI9341_BLACK : (active ? TFT_ACTIVE_TEXT : TFT_TEXT);
  bool showingDelay = false;

  if (remaining <= 90UL * 1000UL) {
    panelColor = TFT_TIMER_WARNING_BG;
    valueColor = ILI9341_BLACK;
  }

  if (active && timingMode == SIMPLE_DELAY && delay_seconds > 0) {
    uint32_t elapsedMs = millis() - turnStartMs;
    uint32_t delayMs = (uint32_t)delay_seconds * 1000UL;
    if (elapsedMs < delayMs) {
      uint32_t delayRemaining = (delayMs - elapsedMs + 999UL) / 1000UL;
      value = String(delayRemaining);
      valueColor = dayThemeEnabled() ? ILI9341_BLACK : (active ? TFT_ACTIVE_TEXT : TFT_WARN);
      showingDelay = true;
    }
  }

  tft.fillRect(region.x, region.y, region.w, region.h, panelColor);
  tft.drawRoundRect(region.x, region.y, region.w, region.h, 6, accent);

#if BGMC_HAS_CLOCK_FONT
  tft.setFont(&BGMC_CLOCK_FONT);
  tft.setTextSize(1);
  tft.setTextColor(valueColor, panelColor);
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  tft.getTextBounds(value, 0, 0, &x1, &y1, &w, &h);
  int16_t valueX = region.x + ((region.w - (int16_t)w) / 2) - x1;
  int16_t valueY = region.y + ((region.h - (int16_t)h) / 2) - y1;
  tft.setCursor(valueX, valueY);
  tft.print(value);
  tft.setFont();
#else
  tft.setTextSize(showingDelay ? 6 : 4);
  tft.setTextColor(valueColor, panelColor);
  int16_t valueX = showingDelay ? (region.x + 104) : (region.x + 48);
  if (!showingDelay && remaining >= 10UL * 60UL * 1000UL) {
    valueX = region.x + 36;
  }
  tft.setCursor(valueX, showingDelay ? (region.y + 30) : (region.y + 38));
  tft.print(value);
#endif
}

void drawPlayerPanel(Player player, int16_t x, int16_t y, int16_t w, int16_t h) {
  bool isA = (player == PLAYER_A);
  bool active = (active_player == player && clock_running);
  uint16_t accent = isA ? TFT_ACCENT_A : TFT_ACCENT_B;
  uint16_t border = active ? TFT_OK : accent;
  uint32_t remaining = isA ? playerA_time_remaining : playerB_time_remaining;
  int score = isA ? playerA_score : playerB_score;

  tft.fillRect(x, y, w, h, TFT_PANEL);
  tft.drawRoundRect(x, y, w, h, 6, border);
  tft.drawRoundRect(x + 3, y + 3, w - 6, h - 6, 6, TFT_PANEL_ALT);
  if (active) {
    tft.fillRect(x + 8, y + 8, w - 16, 10, TFT_OK);
  }

  tft.setTextColor(accent, TFT_PANEL);
  tft.setTextSize(3);
  tft.setCursor(x + 24, y + 34);
  tft.print(isA ? "PLAYER A" : "PLAYER B");

  tft.setTextColor((remaining <= LOW_TIME_WARNING_SECONDS) ? TFT_DANGER : TFT_TEXT, TFT_PANEL);
  tft.setTextSize(6);
  tft.setCursor(x + 24, y + 98);
  tft.print(formatTimeMs(remaining));

  tft.fillRect(x + 24, y + 188, 150, 34, active ? TFT_OK : TFT_PANEL_ALT);
  tft.setTextColor(active ? ILI9341_BLACK : TFT_TEXT, active ? TFT_OK : TFT_PANEL_ALT);
  tft.setTextSize(2);
  tft.setCursor(x + 40, y + 198);
  tft.printf("SCORE %d", score);

  tft.setTextColor(TFT_MUTED, TFT_PANEL);
  tft.setTextSize(1);
  tft.setCursor(x + 212, y + 204);
  tft.print(active ? "RUNNING" : "WAITING");
}

void drawStatusBar() {
  tft.fillRect(REGION_STATUS.x, REGION_STATUS.y, REGION_STATUS.w, REGION_STATUS.h, TFT_PANEL);
  tft.fillRect(0, REGION_STATUS.h - 1, tft.width(), 1, TFT_BORDER);
  tft.setTextColor(TFT_TEXT, TFT_PANEL);
  tft.setTextSize(2);
  tft.setCursor(18, 10);
  tft.print("BATTERY");
  tft.setCursor(114, 10);
  int pct = batteryPercent();
  if (pct < 0) {
    tft.print("--%");
  } else {
    tft.printf("%d%%", pct);
  }
  if (chargingIndicatorOn()) {
    drawChargeBolt(168, 6, TFT_WARN, TFT_PANEL);
  }

  tft.setTextColor(TFT_MUTED, TFT_PANEL);
  tft.setCursor(300, 10);
  tft.print("ELAPSED");
  tft.setTextColor(TFT_TEXT, TFT_PANEL);
  tft.setCursor(410, 10);
  tft.print(formatTime(match_elapsed_seconds));

  tft.setTextColor(TFT_TEXT, TFT_PANEL);
  tft.setTextSize(2);
  tft.setCursor(tft.width() - 168, 10);
  tft.print(cachedRtcStatus);
}

void drawChargeBolt(int16_t x, int16_t y, uint16_t color, uint16_t background) {
  tft.fillRect(x, y, 20, 24, background);
  fillTriangleRows(x + 11, y + 3, x + 5, y + 12, x + 11, y + 12, color);
  fillTriangleRows(x + 9, y + 10, x + 16, y + 10, x + 6, y + 21, color);
}

void drawStartupTest() {
  logLine("Drawing startup splash image");

  setBrightness(255);
  drawSplashImage();
  delay(4000);
}

void drawSplashImage() {
  if (SPLASH_IMAGE_SCALE == 2) {
    tft.drawRGB565ImageScaled2x(0, 0, SPLASH_IMAGE_WIDTH, SPLASH_IMAGE_HEIGHT, SPLASH_IMAGE_RGB565);
  } else {
    tft.drawRGB565Image(0, 0, SPLASH_IMAGE_WIDTH, SPLASH_IMAGE_HEIGHT, SPLASH_IMAGE_RGB565);
  }
}

void drawGeneratedSplashImage() {
  const uint16_t board = 0x0841;
  const uint16_t wood = 0x3922;
  const uint16_t woodLite = 0x7A84;
  const uint16_t gold = 0xDDA6;
  const uint16_t amber = 0xB3A0;
  const uint16_t cream = 0xDEB2;
  const uint16_t orange = 0x8A80;
  const uint16_t blackChecker = 0x10A2;
  const uint16_t whiteChecker = 0xF6D8;
  const uint16_t clockBg = 0x0000;
  const uint16_t silver = 0xD6BA;

  tft.fillScreen(board);
  tft.fillRect(0, 0, 58, PANEL_HEIGHT, wood);
  tft.fillRect(742, 0, 58, PANEL_HEIGHT, wood);
  for (int16_t x = 8; x < 58; x += 12) {
    tft.fillRect(x, 0, 2, PANEL_HEIGHT, woodLite);
    tft.fillRect(742 + x, 0, 2, PANEL_HEIGHT, woodLite);
  }

  const int16_t pointTop = 0;
  const int16_t pointBottom = PANEL_HEIGHT - 1;
  const int16_t pointH = 220;
  const int16_t leftPoints[] = {88, 140, 190};
  const int16_t rightPoints[] = {610, 662, 714};
  for (uint8_t i = 0; i < 3; i++) {
    uint16_t color = (i == 1) ? orange : cream;
    fillTriangleRows(leftPoints[i], pointTop, leftPoints[i] + 44, pointTop, leftPoints[i] + 22, pointTop + pointH, color);
    fillTriangleRows(rightPoints[i], pointTop, rightPoints[i] + 44, pointTop, rightPoints[i] + 22, pointTop + pointH, color);
    fillTriangleRows(leftPoints[i], pointBottom, leftPoints[i] + 44, pointBottom, leftPoints[i] + 22, pointBottom - pointH, color);
    fillTriangleRows(rightPoints[i], pointBottom, rightPoints[i] + 44, pointBottom, rightPoints[i] + 22, pointBottom - pointH, color);
  }

  fillCircleRows(372, 120, 32, whiteChecker);
  fillCircleRows(418, 128, 28, blackChecker);
  fillCircleRows(372, 120, 22, 0xE5B1);
  fillCircleRows(418, 128, 19, 0x2945);
  tft.fillRect(296, 138, 78, 2, gold);
  tft.fillRect(450, 138, 106, 2, gold);
  tft.fillRect(314, 122, 36, 2, gold);
  tft.fillRect(318, 110, 24, 2, gold);

  drawCenteredInRect("BACKGAMMON", {110, 166, 580, 60}, 6, TFT_TEXT, board);
  tft.fillRect(250, 236, 58, 2, gold);
  tft.fillRect(492, 236, 58, 2, gold);
  drawCenteredInRect("CLOCK", {298, 220, 204, 48}, 4, amber, board);

  tft.fillRect(224, 288, 352, 78, clockBg);
  tft.drawRoundRect(224, 288, 352, 78, 8, gold);
  drawCenteredInRect("15:00 : 15:00", {242, 300, 316, 52}, 4, silver, clockBg);
  tft.setTextColor(amber, board);
  tft.setTextSize(2);
  tft.setCursor(254, 390);
  tft.print("FOCUS. STRATEGY. EVERY MOVE COUNTS.");
  tft.setCursor(286, 434);
  tft.print("PLAY FAIR. PLAY STRONG.");
}

void fillTriangleRows(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
  if (y1 < y0) {
    int16_t tx = x0; x0 = x1; x1 = tx;
    int16_t ty = y0; y0 = y1; y1 = ty;
  }
  if (y2 < y0) {
    int16_t tx = x0; x0 = x2; x2 = tx;
    int16_t ty = y0; y0 = y2; y2 = ty;
  }
  if (y2 < y1) {
    int16_t tx = x1; x1 = x2; x2 = tx;
    int16_t ty = y1; y1 = y2; y2 = ty;
  }

  int16_t totalHeight = y2 - y0;
  if (totalHeight == 0) {
    return;
  }

  for (int16_t y = y0; y <= y2; y++) {
    bool secondHalf = y > y1 || y1 == y0;
    int16_t segmentHeight = secondHalf ? (y2 - y1) : (y1 - y0);
    if (segmentHeight == 0) {
      continue;
    }
    float alpha = (float)(y - y0) / (float)totalHeight;
    float beta = secondHalf ? (float)(y - y1) / (float)segmentHeight : (float)(y - y0) / (float)segmentHeight;
    int16_t ax = x0 + (int16_t)((x2 - x0) * alpha);
    int16_t bx = secondHalf ? x1 + (int16_t)((x2 - x1) * beta) : x0 + (int16_t)((x1 - x0) * beta);
    if (ax > bx) {
      int16_t tmp = ax;
      ax = bx;
      bx = tmp;
    }
    tft.fillRect(ax, y, bx - ax + 1, 1, color);
  }
}

void fillCircleRows(int16_t cx, int16_t cy, int16_t radius, uint16_t color) {
  int16_t r2 = radius * radius;
  for (int16_t y = -radius; y <= radius; y++) {
    int16_t x = 0;
    while ((x + 1) * (x + 1) + y * y <= r2) {
      x++;
    }
    tft.fillRect(cx - x, cy + y, (x * 2) + 1, 1, color);
  }
  tft.drawRoundRect(cx - radius, cy - radius, radius * 2, radius * 2, radius, TFT_BORDER);
}

void initCrowPanelExpander() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
  scanI2C("before PCA9557 init");

  uint8_t pcaAddress = 0;
  for (uint8_t address = 0x18; address <= 0x1F; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      pcaAddress = address;
      break;
    }
  }

  if (pcaAddress == 0) {
    logLine("PCA9557 not found on I2C; continuing without expander init");
    return;
  }

  logPrintf("PCA9557 found at 0x%02X\n", pcaAddress);
  pca9557Address = pcaAddress;

  // Elecrow V3 sequence: IO0/IO1 low, short delay, IO0 high, IO1 input.
  // Registers: 1=output port, 2=polarity, 3=configuration (1=input, 0=output).
  bool ok = true;
  ok &= pca9557Write(pcaAddress, 0x02, 0x00);
  ok &= pca9557Write(pcaAddress, 0x03, 0x00);
  ok &= pca9557Write(pcaAddress, 0x01, 0x00);
  delay(20);
  ok &= pca9557Write(pcaAddress, 0x01, 0x01);
  delay(100);
  ok &= pca9557Write(pcaAddress, 0x03, 0x02);

  logPrintf("PCA9557 init: %s\n", ok ? "OK" : "FAILED");
  scanI2C("after PCA9557 init");
}

bool pca9557Write(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

uint8_t pca9557Read(uint8_t address, uint8_t reg) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return 0xFF;
  }
  if (Wire.requestFrom((int)address, 1) != 1) {
    return 0xFF;
  }
  return Wire.read();
}

void initButtonExpander() {
  uint8_t foundAddress = 0;
  for (uint8_t address = 0x20; address <= 0x27; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      foundAddress = address;
      break;
    }
  }

  if (foundAddress == 0) {
    logLine("PCF8575 button expander not found; physical I2C buttons disabled.");
    return;
  }

  pcf8575Address = foundAddress;
  bool ok = pcf8575Write(pcf8575Address, 0xFFFF);
  logPrintf("PCF8575 button expander found at 0x%02X: %s\n", pcf8575Address, ok ? "OK" : "FAILED");
  logPrintf("PCF8575 buttons: Play/Pause=P%02u Player A=P%02u Player B=P%02u\n",
            PCF8575_PLAY_PAUSE_PIN, PCF8575_PLAYER_A_PIN, PCF8575_PLAYER_B_PIN);
  scanI2C("after PCF8575 init");
}

bool pcf8575Write(uint8_t address, uint16_t value) {
  Wire.beginTransmission(address);
  Wire.write(value & 0xFF);
  Wire.write((value >> 8) & 0xFF);
  return Wire.endTransmission() == 0;
}

uint16_t pcf8575Read(uint8_t address) {
  if (Wire.requestFrom((int)address, 2) != 2) {
    return 0xFFFF;
  }
  uint8_t low = Wire.read();
  uint8_t high = Wire.read();
  return (uint16_t)low | ((uint16_t)high << 8);
}

void initFuelGauge() {
  Wire.beginTransmission(MAX17048_I2C_ADDRESS);
  if (Wire.endTransmission() != 0) {
    max17048Found = false;
    logLine("MAX17048 fuel gauge not found; battery display will use fallback.");
    return;
  }

  uint16_t version = 0;
  max17048Found = true;
  max17048Read16(0x08, &version);
  logPrintf("MAX17048 fuel gauge found at 0x%02X version=0x%04X\n", MAX17048_I2C_ADDRESS, version);
  logPrintf("MAX17048 quick-start reset: %s\n", max17048QuickStart() ? "OK" : "FAILED");
  delay(250);
  logPrintf("MAX17048 battery: %.2fV %.1f%%\n", max17048CellVoltage(), max17048StateOfCharge());
  scanI2C("after MAX17048 init");
}

bool max17048Read16(uint8_t reg, uint16_t *value) {
  if (value == nullptr) {
    return false;
  }

  Wire.beginTransmission(MAX17048_I2C_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom((int)MAX17048_I2C_ADDRESS, 2) != 2) {
    return false;
  }

  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  *value = ((uint16_t)msb << 8) | lsb;
  return true;
}

bool max17048Write16(uint8_t reg, uint16_t value) {
  if (!max17048Found) {
    return false;
  }

  Wire.beginTransmission(MAX17048_I2C_ADDRESS);
  Wire.write(reg);
  Wire.write((value >> 8) & 0xFF);
  Wire.write(value & 0xFF);
  return Wire.endTransmission() == 0;
}

bool max17048QuickStart() {
  // MAX17048 MODE register quick-start command. This resets/restarts the
  // fuel-gauge algorithm without clearing the whole chip.
  return max17048Write16(0x06, 0x4000);
}

float max17048CellVoltage() {
  uint16_t raw = 0;
  if (!max17048Found || !max17048Read16(0x02, &raw)) {
    return 0.0f;
  }
  return (float)(raw >> 4) * 0.00125f;
}

float max17048StateOfCharge() {
  uint16_t raw = 0;
  if (!max17048Found || !max17048Read16(0x04, &raw)) {
    return 0.0f;
  }
  float pct = (float)(raw >> 8) + ((float)(raw & 0xFF) / 256.0f);
  if (pct < 0.0f) {
    pct = 0.0f;
  }
  if (pct > 100.0f) {
    pct = 100.0f;
  }
  return pct;
}

void initRtcClock() {
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  if (Wire.endTransmission() != 0) {
    ds3231Found = false;
    logLine("DS3231 RTC not found; match log dates will be DATE UNSET.");
    return;
  }

  ds3231Found = true;
  uint8_t second = 0;
  uint8_t minute = 0;
  uint8_t hour = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint16_t year = 0;
  if (ds3231ReadTime(&second, &minute, &hour, &day, &month, &year)) {
    if (rtcDateLooksSet(day, month, year)) {
      logPrintf("DS3231 RTC found at 0x%02X date=%04u-%02u-%02u time=%02u:%02u:%02u\n",
                DS3231_I2C_ADDRESS, year, month, day, hour, minute, second);
    } else {
      logPrintf("DS3231 RTC found at 0x%02X but date looks unset: %04u-%02u-%02u\n",
                DS3231_I2C_ADDRESS, year, month, day);
    }
  } else {
    logPrintf("DS3231 RTC found at 0x%02X but time read failed.\n", DS3231_I2C_ADDRESS);
  }
  scanI2C("after DS3231 init");
}

bool ds3231ReadTime(uint8_t *second, uint8_t *minute, uint8_t *hour, uint8_t *day, uint8_t *month, uint16_t *year) {
  if (second == nullptr || minute == nullptr || hour == nullptr || day == nullptr || month == nullptr || year == nullptr) {
    return false;
  }

  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom((int)DS3231_I2C_ADDRESS, 7) != 7) {
    return false;
  }

  uint8_t rawSecond = Wire.read();
  uint8_t rawMinute = Wire.read();
  uint8_t rawHour = Wire.read();
  Wire.read(); // Day of week, unused for match logs.
  uint8_t rawDay = Wire.read();
  uint8_t rawMonth = Wire.read();
  uint8_t rawYear = Wire.read();

  *second = bcdToDec(rawSecond & 0x7F);
  *minute = bcdToDec(rawMinute & 0x7F);
  if ((rawHour & 0x40) != 0) {
    uint8_t hour12 = bcdToDec(rawHour & 0x1F);
    bool pm = (rawHour & 0x20) != 0;
    *hour = (hour12 % 12) + (pm ? 12 : 0);
  } else {
    *hour = bcdToDec(rawHour & 0x3F);
  }
  *day = bcdToDec(rawDay & 0x3F);
  *month = bcdToDec(rawMonth & 0x1F);
  *year = 2000 + bcdToDec(rawYear);
  return true;
}

bool ds3231SetTime(uint8_t second, uint8_t minute, uint8_t hour, uint8_t day, uint8_t month, uint16_t year) {
  if (!ds3231Found || !rtcDateLooksSet(day, month, year) || hour > 23 || minute > 59 || second > 59) {
    return false;
  }

  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0x00);
  Wire.write(decToBcd(second));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour)); // 24-hour mode.
  Wire.write(decToBcd(1));    // Day of week is not used by this firmware.
  Wire.write(decToBcd(day));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd((uint8_t)(year - 2000)));
  return Wire.endTransmission() == 0;
}

bool rtcDateLooksSet(uint8_t day, uint8_t month, uint16_t year) {
  return year >= 2024 && year <= 2099 && month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

uint8_t daysInMonth(uint8_t month, uint16_t year) {
  switch (month) {
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
    case 2: {
      bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
      return leap ? 29 : 28;
    }
    default:
      return 31;
  }
}

uint8_t bcdToDec(uint8_t value) {
  return ((value >> 4) * 10) + (value & 0x0F);
}

uint8_t decToBcd(uint8_t value) {
  return ((value / 10) << 4) | (value % 10);
}

String rtcStatusText() {
  uint8_t second = 0;
  uint8_t minute = 0;
  uint8_t hour = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint16_t year = 0;
  if (!ds3231ReadTime(&second, &minute, &hour, &day, &month, &year) || !rtcDateLooksSet(day, month, year)) {
    return "DATE UNSET";
  }

  char buffer[18];
  snprintf(buffer, sizeof(buffer), "%02u/%02u %02u:%02u", month, day, hour, minute);
  return String(buffer);
}

bool updateRtcStatusCache(bool force) {
  uint32_t nowMs = millis();
  if (!force && nowMs - lastRtcStatusMs < 1000UL) {
    return false;
  }

  String next = rtcStatusText();
  bool changed = force || next != cachedRtcStatus;
  cachedRtcStatus = next;
  lastRtcStatusMs = nowMs;
  return changed;
}

void scanI2C(const char *label) {
  logPrintf("I2C scan %s:", label);
  bool any = false;
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      logPrintf(" 0x%02X", address);
      any = true;
    }
  }
  logLine(any ? "" : " none");
}

void resetTouchController(uint8_t outputValue, uint8_t configValue) {
  if (pca9557Address == 0) {
    return;
  }
  pca9557Write(pca9557Address, 0x03, 0x00);
  pca9557Write(pca9557Address, 0x01, outputValue);
  delay(20);
  pca9557Write(pca9557Address, 0x03, configValue);
  delay(80);
}

void initTouch() {
  scanI2C("before GT911 init");

  const uint8_t addresses[] = {0x5D, 0x14};
  for (uint8_t i = 0; i < sizeof(addresses); i++) {
    Wire.beginTransmission(addresses[i]);
    if (Wire.endTransmission() == 0) {
      gt911Address = addresses[i];
      break;
    }
  }

  if (gt911Address == 0) {
    const uint8_t resetOutputs[] = {0x01, 0x00, 0x03, 0x02};
    const uint8_t resetConfigs[] = {0x02, 0x02, 0x00, 0x00};
    for (uint8_t attempt = 0; attempt < 4 && gt911Address == 0; attempt++) {
      resetTouchController(resetOutputs[attempt], resetConfigs[attempt]);
      scanI2C("after touch reset attempt");
      for (uint8_t i = 0; i < sizeof(addresses); i++) {
        Wire.beginTransmission(addresses[i]);
        if (Wire.endTransmission() == 0) {
          gt911Address = addresses[i];
          break;
        }
      }
    }
  }

  if (gt911Address == 0) {
    logLine("GT911 touch controller not found. Touch disabled.");
    return;
  }

  uint8_t productId[4] = {};
  gt911Read(0x8140, productId, sizeof(productId));
  logPrintf("GT911 found at 0x%02X id=%c%c%c%c\n", gt911Address,
            productId[0], productId[1], productId[2], productId[3]);
  gt911Write(0x814E, 0x00);
}

TouchPoint readTouch() {
  TouchPoint point = {false, 0, 0};
  if (gt911Address == 0) {
    return point;
  }

  uint8_t status = 0;
  if (!gt911Read(0x814E, &status, 1)) {
    return point;
  }

  uint8_t count = status & 0x0F;
  if (count == 0) {
    if (status & 0x80) {
      gt911Write(0x814E, 0x00);
    }
    return point;
  }
  if (count > 5) {
    gt911Write(0x814E, 0x00);
    return point;
  }

  uint8_t data[7] = {};
  if (gt911Read(0x8150, data, sizeof(data))) {
    uint16_t xLowHigh = ((uint16_t)data[1] << 8) | data[0];
    uint16_t yLowHigh = ((uint16_t)data[3] << 8) | data[2];
    uint16_t xHighLow = ((uint16_t)data[0] << 8) | data[1];
    uint16_t yHighLow = ((uint16_t)data[2] << 8) | data[3];
    bool lowHighValid = xLowHigh < PANEL_WIDTH && yLowHigh < PANEL_HEIGHT;
    bool highLowValid = xHighLow < PANEL_WIDTH && yHighLow < PANEL_HEIGHT;

    if (lowHighValid || highLowValid) {
      point.touched = true;
      point.x = lowHighValid ? xLowHigh : xHighLow;
      point.y = lowHighValid ? yLowHigh : yHighLow;
    } else {
      uint16_t shiftedXLowHigh = ((uint16_t)data[2] << 8) | data[1];
      uint16_t shiftedYLowHigh = ((uint16_t)data[4] << 8) | data[3];
      uint16_t shiftedXHighLow = ((uint16_t)data[1] << 8) | data[2];
      uint16_t shiftedYHighLow = ((uint16_t)data[3] << 8) | data[4];
      bool shiftedLowHighValid = shiftedXLowHigh < PANEL_WIDTH && shiftedYLowHigh < PANEL_HEIGHT;
      bool shiftedHighLowValid = shiftedXHighLow < PANEL_WIDTH && shiftedYHighLow < PANEL_HEIGHT;

      if (shiftedLowHighValid || shiftedHighLowValid) {
        point.touched = true;
        point.x = shiftedLowHighValid ? shiftedXLowHigh : shiftedXHighLow;
        point.y = shiftedLowHighValid ? shiftedYLowHigh : shiftedYHighLow;
      }
    }
  }

  gt911Write(0x814E, 0x00);
  return point;
}

bool gt911Read(uint16_t reg, uint8_t *data, uint8_t len) {
  Wire.beginTransmission(gt911Address);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  uint8_t received = Wire.requestFrom((int)gt911Address, (int)len);
  if (received != len) {
    return false;
  }

  for (uint8_t i = 0; i < len; i++) {
    data[i] = Wire.read();
  }
  return true;
}

bool gt911Write(uint16_t reg, uint8_t value) {
  Wire.beginTransmission(gt911Address);
  Wire.write(reg >> 8);
  Wire.write(reg & 0xFF);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void logLine(const char *message) {
  Serial.println(message);
  Serial0.println(message);
}

void logPrintf(const char *format, ...) {
  char buffer[160];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  Serial.print(buffer);
  Serial0.print(buffer);
}

void drawCentered(const String &text, int16_t y, uint8_t textSize, uint16_t color) {
  tft.setTextSize(textSize);
  tft.setTextColor(color, TFT_BACKGROUND);
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, y);
  tft.print(text);
}

void drawCenteredInRect(const String &text, const Rect &region, uint8_t textSize, uint16_t color, uint16_t background) {
#if BGMC_HAS_SMALL_UI_FONT
  bool useUiFont = (textSize >= 2 && textSize <= 3);
  if (useUiFont) {
    tft.setFont(&FreeSans12pt7b);
    tft.setTextSize(1);
  } else {
    tft.setFont();
    tft.setTextSize(textSize);
  }
#elif BGMC_HAS_UI_FONT
  bool useUiFont = (textSize >= 2 && textSize <= 3);
  if (useUiFont) {
    tft.setFont(&FreeSans18pt7b);
    tft.setTextSize(1);
  } else {
    tft.setFont();
    tft.setTextSize(textSize);
  }
#else
  tft.setFont();
  tft.setTextSize(textSize);
#endif
  tft.setTextColor(color, background);
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int16_t x = region.x + ((region.w - (int16_t)w) / 2) - x1;
  int16_t y = region.y + ((region.h - (int16_t)h) / 2) - y1;
  tft.setCursor(x, y);
  tft.print(text);
  tft.setFont();
}

void drawScoreInRect(int score, const Rect &region, uint16_t color, uint16_t background) {
  String text = String(score);
#if BGMC_HAS_COLLEGE_SCORE_FONT
  tft.setFont(&BGMC_COLLEGE_SCORE_FONT);
  tft.setTextSize(1);
#elif BGMC_HAS_SCORE_96_FONT
  tft.setFont(&FreeSansBold96pt7b);
  tft.setTextSize(1);
#elif BGMC_HAS_SCORE_FONT
  tft.setFont(&FreeSansBold24pt7b);
  tft.setTextSize(4);
#elif BGMC_HAS_UI_FONT
  tft.setFont(&FreeSans18pt7b);
  tft.setTextSize(3);
#else
  tft.setFont();
  tft.setTextSize(10);
#endif
  tft.setTextColor(color, background);
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
#if BGMC_HAS_COLLEGE_SCORE_FONT || BGMC_HAS_SCORE_96_FONT
  if (w > region.w - 28 || h > region.h - 28) {
#if BGMC_HAS_SCORE_FONT
    tft.setFont(&FreeSansBold24pt7b);
    tft.setTextSize(4);
#else
    tft.setFont();
    tft.setTextSize(10);
#endif
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  }
#endif
#if BGMC_HAS_SCORE_FONT
  uint8_t scoreSize = 4;
  while (scoreSize > 1 && (w > region.w - 28 || h > region.h - 28)) {
    scoreSize--;
    tft.setTextSize(scoreSize);
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  }
#endif
  int16_t x = region.x + ((region.w - (int16_t)w) / 2) - x1;
  int16_t y = region.y + ((region.h - (int16_t)h) / 2) - y1;
  tft.setCursor(x, y);
  tft.print(text);
  tft.setFont();
}

void drawMatchTargetInRect(int target, const Rect &region, uint16_t color, uint16_t background) {
  String text = String(target);
#if BGMC_HAS_MATCH_TARGET_48_FONT
  {
    tft.setFont(&BGMC_MATCH_TARGET_48_FONT);
    tft.setTextSize(1);
    tft.setTextColor(color, background);
    int16_t x1;
    int16_t y1;
    uint16_t w;
    uint16_t h;
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int16_t x = region.x + ((region.w - (int16_t)w) / 2) - x1;
    int16_t y = region.y + ((region.h - (int16_t)h) / 2) - y1;
    tft.setCursor(x, y);
    tft.print(text);
    tft.setFont();
  }
  return;
#elif BGMC_HAS_SCORE_FONT
  (void)background;
  drawGfxTextScaledInRect(&FreeSansBold24pt7b, text, region, color, 3, 2);
  return;
#elif BGMC_HAS_SMALL_UI_FONT
  tft.setFont(&FreeSans12pt7b);
  tft.setTextSize(3);
#elif BGMC_HAS_UI_FONT
  tft.setFont(&FreeSans18pt7b);
  tft.setTextSize(2);
#else
  tft.setFont();
  tft.setTextSize(7);
#endif
  tft.setTextColor(color, background);
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
#if BGMC_HAS_SMALL_UI_FONT
  if (w > region.w - 24 || h > region.h - 8) {
    tft.setTextSize(2);
    tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  }
#endif
  int16_t x = region.x + ((region.w - (int16_t)w) / 2) - x1;
  int16_t y = region.y + ((region.h - (int16_t)h) / 2) - y1;
  tft.setCursor(x, y);
  tft.print(text);
  tft.setFont();
}

void drawGfxTextScaledInRect(const GFXfont *font, const String &text, const Rect &region, uint16_t color,
                             uint8_t scaleNum, uint8_t scaleDen) {
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  getGfxTextBoundsScaled(font, text, scaleNum, scaleDen, &x1, &y1, &w, &h);

  if (w > region.w - 24 || h > region.h - 8) {
    scaleNum = 1;
    scaleDen = 1;
    getGfxTextBoundsScaled(font, text, scaleNum, scaleDen, &x1, &y1, &w, &h);
  }

  int16_t x = region.x + ((region.w - (int16_t)w) / 2) - x1;
  int16_t y = region.y + ((region.h - (int16_t)h) / 2) - y1;
  drawGfxTextScaled(font, text, x, y, color, scaleNum, scaleDen);
}

void getGfxTextBoundsScaled(const GFXfont *font, const String &text, uint8_t scaleNum, uint8_t scaleDen,
                            int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
  int16_t minX = 32767;
  int16_t minY = 32767;
  int16_t maxX = -32768;
  int16_t maxY = -32768;
  int16_t cursor = 0;
  uint16_t first = pgm_read_word(&font->first);
  uint16_t last = pgm_read_word(&font->last);
  const GFXglyph *glyphs = (const GFXglyph *)pgm_read_ptr(&font->glyph);

  for (uint16_t i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    if (c < first || c > last) {
      continue;
    }
    GFXglyph glyph;
    memcpy_P(&glyph, &glyphs[c - first], sizeof(GFXglyph));
    if (glyph.width > 0 && glyph.height > 0) {
      int16_t gx1 = cursor + ((int32_t)glyph.xOffset * scaleNum) / scaleDen;
      int16_t gy1 = ((int32_t)glyph.yOffset * scaleNum) / scaleDen;
      int16_t gx2 = cursor + ((int32_t)(glyph.xOffset + glyph.width) * scaleNum + scaleDen - 1) / scaleDen - 1;
      int16_t gy2 = ((int32_t)(glyph.yOffset + glyph.height) * scaleNum + scaleDen - 1) / scaleDen - 1;
      if (gx1 < minX) minX = gx1;
      if (gy1 < minY) minY = gy1;
      if (gx2 > maxX) maxX = gx2;
      if (gy2 > maxY) maxY = gy2;
    }
    cursor += ((int32_t)glyph.xAdvance * scaleNum + scaleDen - 1) / scaleDen;
  }

  if (maxX < minX || maxY < minY) {
    *x1 = 0;
    *y1 = 0;
    *w = 0;
    *h = 0;
    return;
  }
  *x1 = minX;
  *y1 = minY;
  *w = maxX - minX + 1;
  *h = maxY - minY + 1;
}

void drawGfxTextScaled(const GFXfont *font, const String &text, int16_t x, int16_t y, uint16_t color,
                       uint8_t scaleNum, uint8_t scaleDen) {
  int16_t cursor = x;
  uint16_t first = pgm_read_word(&font->first);
  uint16_t last = pgm_read_word(&font->last);
  const GFXglyph *glyphs = (const GFXglyph *)pgm_read_ptr(&font->glyph);
  const uint8_t *bitmap = (const uint8_t *)pgm_read_ptr(&font->bitmap);

  for (uint16_t i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    if (c < first || c > last) {
      continue;
    }

    GFXglyph glyph;
    memcpy_P(&glyph, &glyphs[c - first], sizeof(GFXglyph));
    uint16_t bitIndex = 0;
    uint16_t bitmapOffset = glyph.bitmapOffset;
    uint8_t bits = 0;

    for (uint8_t yy = 0; yy < glyph.height; yy++) {
      int16_t rowTop = y + ((int32_t)(glyph.yOffset + yy) * scaleNum) / scaleDen;
      int16_t rowBottom = y + ((int32_t)(glyph.yOffset + yy + 1) * scaleNum + scaleDen - 1) / scaleDen;
      int16_t rowH = max((int16_t)1, (int16_t)(rowBottom - rowTop));

      for (uint8_t xx = 0; xx < glyph.width; xx++) {
        if ((bitIndex & 7) == 0) {
          bits = pgm_read_byte(&bitmap[bitmapOffset++]);
        }
        if (bits & 0x80) {
          int16_t colLeft = cursor + ((int32_t)(glyph.xOffset + xx) * scaleNum) / scaleDen;
          int16_t colRight = cursor + ((int32_t)(glyph.xOffset + xx + 1) * scaleNum + scaleDen - 1) / scaleDen;
          int16_t colW = max((int16_t)1, (int16_t)(colRight - colLeft));
          tft.fillRect(colLeft, rowTop, colW, rowH, color);
        }
        bits <<= 1;
        bitIndex++;
      }
    }
    cursor += ((int32_t)glyph.xAdvance * scaleNum + scaleDen - 1) / scaleDen;
  }
}

String formatTime(uint32_t seconds) {
  uint32_t minutes = seconds / 60;
  uint32_t secs = seconds % 60;
  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu", (unsigned long)minutes, (unsigned long)secs);
  return String(buffer);
}

String formatTimeMs(uint32_t milliseconds) {
  uint32_t seconds = (milliseconds + 999UL) / 1000UL;
  return formatTime(seconds);
}

String timingModeLabel() {
  switch (timingMode) {
    case BRONSTEIN_DELAY:
      return "Bronstein";
    case FISCHER_INCREMENT:
      return "Fischer";
    case SIMPLE_DELAY:
    default:
      return "Simple";
  }
}

String playPauseLabel() {
  if (clock_running) {
    return "Pause Game";
  }
  if (game_number == 0 && !game_in_progress) {
    return "Start Match";
  }
  return "Play Game";
}

String matchStateLabel() {
  if (crawford_active) {
    return "CRAWFORD GAME - CUBE OFF";
  }
  if (post_crawford) {
    return "POST-CRAWFORD - CUBE ON";
  }
  return "NORMAL";
}

int batteryPercent() {
  if (max17048Found) {
    return (int)(max17048StateOfCharge() + 0.5f);
  }

  if (PIN_BATTERY_ADC == 255) {
    return -1;
  }

  int raw = analogRead(PIN_BATTERY_ADC);
  float adcVolts = ((float)raw / 4095.0f) * BATTERY_ADC_REF_VOLTS;
  float batteryVolts = adcVolts * BATTERY_DIVIDER_RATIO;
  float pct = (batteryVolts - BATTERY_EMPTY_VOLTS) * 100.0f /
              (BATTERY_FULL_VOLTS - BATTERY_EMPTY_VOLTS);
  if (pct < 0.0f) {
    pct = 0.0f;
  }
  if (pct > 100.0f) {
    pct = 100.0f;
  }
  return (int)(pct + 0.5f);
}

bool chargingIndicatorOn() {
  if (PIN_CHARGE_DETECT == 255) {
    if (!max17048Found) {
      return false;
    }

    uint32_t nowMs = millis();
    if (lastChargeCheckMs == 0 || nowMs - lastChargeCheckMs >= 30000UL) {
      float voltage = max17048CellVoltage();
      float percent = max17048StateOfCharge();
      if (lastChargeCheckMs != 0) {
        chargeTrendRising = (voltage > lastChargeCheckVoltage + 0.006f) ||
                            (percent > lastChargeCheckPercent + 0.08f);
        if (chargeTrendRising) {
          chargeIndicatorUntilMs = nowMs + 120000UL;
        }
      }
      lastChargeCheckVoltage = voltage;
      lastChargeCheckPercent = percent;
      lastChargeCheckMs = nowMs;
    }

    return chargeTrendRising || nowMs < chargeIndicatorUntilMs;
  }
  return digitalRead(PIN_CHARGE_DETECT) == LOW;
}

void recordMatchLog() {
  String entry = matchLogDate();
  entry += "|";
  entry += playerA_name;
  entry += "|";
  entry += playerB_name;
  entry += "|";
  entry += String(playerA_score);
  entry += "-";
  entry += String(playerB_score);

  for (int8_t i = MATCH_LOG_MAX_ENTRIES - 1; i > 0; i--) {
    matchLog[i] = matchLog[i - 1];
  }
  matchLog[0] = entry;
  if (matchLogCount < MATCH_LOG_MAX_ENTRIES) {
    matchLogCount++;
  }

  prefs.putUChar("logCount", matchLogCount);
  for (uint8_t i = 0; i < MATCH_LOG_MAX_ENTRIES; i++) {
    char key[8];
    snprintf(key, sizeof(key), "log%u", i);
    prefs.putString(key, matchLog[i]);
  }
}

void clearMatchLog() {
  matchLogCount = 0;
  prefs.putUChar("logCount", matchLogCount);
  for (uint8_t i = 0; i < MATCH_LOG_MAX_ENTRIES; i++) {
    matchLog[i] = "";
    char key[8];
    snprintf(key, sizeof(key), "log%u", i);
    prefs.putString(key, "");
  }
}

String matchLogDate() {
  uint8_t second = 0;
  uint8_t minute = 0;
  uint8_t hour = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint16_t year = 0;
  if (!ds3231ReadTime(&second, &minute, &hour, &day, &month, &year) || !rtcDateLooksSet(day, month, year)) {
    return "DATE UNSET";
  }

  char buffer[12];
  snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u", year, month, day);
  return String(buffer);
}

String matchLogPart(const String &entry, uint8_t part) {
  int start = 0;
  for (uint8_t i = 0; i < part; i++) {
    int separator = entry.indexOf('|', start);
    if (separator < 0) {
      return "";
    }
    start = separator + 1;
  }

  int end = entry.indexOf('|', start);
  if (end < 0) {
    return entry.substring(start);
  }
  return entry.substring(start, end);
}

// ---------------------------------------------------------------------------
// Settings and power management
// ---------------------------------------------------------------------------
void loadSettings() {
  prefs.begin("bgclock", false);
  match_target = prefs.getInt("target", 9);
  initial_time_seconds = prefs.getULong("time", DEFAULT_INITIAL_TIME_SECONDS);
  delay_seconds = prefs.getUChar("delay", DEFAULT_DELAY_SECONDS);
  if (delay_seconds == 7) {
    delay_seconds = DEFAULT_DELAY_SECONDS;
    prefs.putUChar("delay", delay_seconds);
  }
  increment_seconds = prefs.getUChar("inc", DEFAULT_INCREMENT_SECONDS);
  brightness = prefs.getUChar("bright", DEFAULT_BRIGHTNESS);
  timingMode = (TimingMode)prefs.getUChar("timing", (uint8_t)SIMPLE_DELAY);
  if (timingMode > FISCHER_INCREMENT) {
    timingMode = SIMPLE_DELAY;
  }
  timerScope = (TimerScope)prefs.getUChar("scope", (uint8_t)TIMER_BY_MATCH);
  if (timerScope > TIMER_BY_MATCH) {
    timerScope = TIMER_BY_MATCH;
  }
  displayTheme = (DisplayTheme)prefs.getUChar("theme", (uint8_t)THEME_NIGHT);
  if (displayTheme > THEME_DAY) {
    displayTheme = THEME_NIGHT;
  }
  scoreDisplayMode = (ScoreDisplayMode)prefs.getUChar("scoreView", (uint8_t)SCORE_STANDARD);
  if (scoreDisplayMode > SCORE_POINTS_TO_WIN) {
    scoreDisplayMode = SCORE_STANDARD;
  }
  playerA_name = prefs.getString("nameA", "PLAYER A");
  playerB_name = prefs.getString("nameB", "PLAYER B");
  playerA_color = prefs.getUShort("colorA", PLAYER_COLOR_BLUE);
  playerB_color = prefs.getUShort("colorB", PLAYER_COLOR_RED);
  if (playerA_name.length() == 0 || playerA_name.length() > PLAYER_NAME_MAX_LEN) {
    playerA_name = "PLAYER A";
  }
  if (playerB_name.length() == 0 || playerB_name.length() > PLAYER_NAME_MAX_LEN) {
    playerB_name = "PLAYER B";
  }

  matchLogCount = prefs.getUChar("logCount", 0);
  if (matchLogCount > MATCH_LOG_MAX_ENTRIES) {
    matchLogCount = MATCH_LOG_MAX_ENTRIES;
  }
  for (uint8_t i = 0; i < MATCH_LOG_MAX_ENTRIES; i++) {
    char key[8];
    snprintf(key, sizeof(key), "log%u", i);
    matchLog[i] = prefs.getString(key, "");
  }

  bool validTarget = false;
  for (uint8_t i = 0; i < MATCH_TARGET_COUNT; i++) {
    if (MATCH_TARGETS[i] == match_target) {
      validTarget = true;
    }
  }
  if (!validTarget) {
    match_target = 9;
  }
}

void saveSettingsIfNeeded() {
  if (!settingsDirty) {
    return;
  }

  uint32_t nowMs = millis();
  if (nowMs - settingsDirtyMs < SETTINGS_SAVE_DELAY_MS) {
    return;
  }

  prefs.putInt("target", match_target);
  prefs.putULong("time", initial_time_seconds);
  prefs.putUChar("delay", delay_seconds);
  prefs.putUChar("inc", increment_seconds);
  prefs.putUChar("bright", brightness);
  prefs.putUChar("timing", (uint8_t)timingMode);
  prefs.putUChar("scope", (uint8_t)timerScope);
  prefs.putUChar("theme", (uint8_t)displayTheme);
  prefs.putUChar("scoreView", (uint8_t)scoreDisplayMode);
  settingsDirty = false;
}

void savePlayerNames() {
  prefs.putString("nameA", playerA_name);
  prefs.putString("nameB", playerB_name);
  prefs.putUShort("colorA", playerA_color);
  prefs.putUShort("colorB", playerB_color);
}

void markSettingsDirty() {
  settingsDirty = true;
  settingsDirtyMs = millis();
  markDisplayDirty();
}

void markDisplayDirty() {
  displayDirty = true;
}

void setBrightness(uint8_t duty) {
  if (duty == 0) {
    digitalWrite(PIN_TFT_BACKLIGHT, TFT_BACKLIGHT_ACTIVE_HIGH ? LOW : HIGH);
    return;
  }

  digitalWrite(PIN_TFT_BACKLIGHT, TFT_BACKLIGHT_ACTIVE_HIGH ? HIGH : LOW);
}

void wakeActivity() {
  lastActivityMs = millis();
}

void maybeSleep() {
  if (clock_running) {
    return;
  }

  if (PIN_PLAYER_A_BUTTON == 255 || PIN_PLAYER_B_BUTTON == 255 || PIN_MENU_SELECT == 255) {
    return;
  }

  uint32_t nowMs = millis();
  if (nowMs - lastActivityMs < SLEEP_AFTER_IDLE_MS) {
    return;
  }

  tft.fillScreen(ILI9341_BLACK);
  setBrightness(0);
  delay(20); // Tiny hardware settle before deep sleep.

  // Wake on the two large player buttons and menu button. Add more pins if
  // your board supports the required EXT1 wake bitmask for those GPIOs.
  uint64_t wakeMask = (1ULL << PIN_PLAYER_A_BUTTON) |
                      (1ULL << PIN_PLAYER_B_BUTTON) |
                      (1ULL << PIN_MENU_SELECT);
  esp_sleep_enable_ext1_wakeup(wakeMask, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

// ---------------------------------------------------------------------------
// Buzzer feedback
// ---------------------------------------------------------------------------
void beep(uint16_t frequency, uint16_t durationMs) {
  if (PIN_BUZZER == 255) {
    return;
  }

  tone(PIN_BUZZER, frequency, durationMs);
}

void playButtonBeep() {
  beep(1800, 20);
}

void playTimeoutBeep() {
  beep(900, 300);
}

void playMatchOverBeep() {
  beep(1600, 120);
  // tone() is non-blocking on most Arduino cores, so keep this simple.
}
