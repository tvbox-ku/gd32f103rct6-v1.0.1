/**
 * GD32F103RCT6 正压防爆控制系统 - 3.5寸 480x320 横屏
 *
 * 引脚: PB0-PB15=TFT_D0-D15, PA2=DC, PA4=CS, PA5=WR, PA3=RD, PA1=RST
 * 按键: PC7=KEY1, PC8=KEY2, PC9=KEY3, PA12=蜂鸣器
 * 继电器: PC0=排气, PC1=警报, PC2=进气, PC3=送电
 * ADC: PC5=温度(ADC15), PC4=压力(ADC14), PC2=ADC1(ADC12), PC3=ADC2(ADC13)
 */

#include <TFT_eSPI.h>
#include "my_font.h"
#include "title_font_32.h"
#include "ascii_font_24.h"
#include "logo_bitmap.h"

extern TFT_eSPI tft;
void drawScreen();

// Logo 绘制：用 const 签名让 TFT_eSPI 走 pgm_read_word 路径从 Flash 读取
void drawLogo(int x, int y) {
  tft.pushImage(x, y, LOGO_W, LOGO_H, logoBitmap);
}

// 使用 HAL_FLASH 标准 API 读写 Flash（参考 E:\\main.cpp 稳定实现）
#define FLASH_SAVE_ADDR 0x0803F800
#define FLASH_MAGIC 0xA5E5
TFT_eSPI tft = TFT_eSPI();

// ====== 硬件引脚 ======
const uint8_t BEEP_PIN = PA12, KEY1_PIN = PC7, KEY2_PIN = PC8, KEY3_PIN = PC9;
const uint8_t POWER_RELAY = PC1, INLET_RELAY = PC0, ALARM_RELAY = PC12, EXHAUST_RELAY = PD2;
const uint8_t TEMP_ADC_PIN = PC5, PRESS_ADC_PIN = PC4;
const uint8_t ADC1_PIN = PC2, ADC2_PIN = PC3;
const int8_t TFT_BL_PIN = PC6;
const uint8_t TEMP_ADC_CH = 15, PRESS_ADC_CH = 14;
const uint8_t ADC1_CH = 12, ADC2_CH = 13;

#define W 480
#define H 320

#define COLOR_SPACEGREY 0x39E8   // 深空灰

// ====== 系统参数 ======
static int32_t sysParams[6] = {150, 80, 300, 400, 80, 10};
const int32_t DEFAULT_SYSPARAMS[6] = {150, 80, 300, 400, 80, 10};
const uint16_t DEFAULT_PASSWORD = 555;
const uint16_t UNIVERSAL_PASSWORD = 995; // 万能密码：忘记密码时可直接进入设置页
static bool universalPwdUsed = false;
static uint16_t targetPassword = DEFAULT_PASSWORD;
static uint8_t mode = 0;

// 背光控制：PC6 高电平 → S8050 导通 → 背光亮
static void setBacklight(bool on) {
  if (TFT_BL_PIN >= 0) {
    digitalWrite(TFT_BL_PIN, on ? HIGH : LOW);
    
  }
}

// 息屏定时器：15分钟无操作关背光
#define SLEEP_TIMEOUT_MS 900000UL
static bool screenSleeping = false;
static bool wakePending = false;

// ====== 状态变量 ======
static bool systemPoweredOn = false;
static bool globalAlarm = false, muteOn = false;
static bool initialCheckDone = false;
static bool blinkOn = true, prevBlinkOn = false;
static uint32_t blinkTimer = 0;
static int32_t pressVal = 0;
static float tempVal = 0.0f;
static bool tempSensorAbnormal = false;
static bool pressSensorAbnormal = false;
static int32_t countdownRemain = 0;
static bool inPositiveMode = false, needRedraw = false;
static uint32_t modeTimer = 0;
static uint32_t sampleTimer = 0;
static bool systemActive = false;
// ====== 新增：界面锁定标志（切换至第二界面后永不回退） ======
static bool systemRunningNormal = false;

// ====== 120秒超时返回主页 ======
static uint32_t idleTimer = 0;
#define IDLE_TIMEOUT_MS 120000

// ====== 压力控制与欠压保护 ======
static bool powerTripLatched = false;       // 欠压断电锁定（10秒未恢复）
static uint32_t underPressureTimer = 0;     // 欠压计时起始时刻
static bool countdownDoneFirstRun = false;  // 倒计时已完成标记
static bool powerOnDelivered = false;       // 首次送电是否已完成

// ====== 压力恢复超时报警（换气时间内不能恢复正常就报警） ======
static uint32_t recoveryTimeoutTimer = 0;   // 压力恢复超时计时器
static bool recoveryTimeoutAlarm = false;   // 压力恢复超时报警标记

// ====== 压力滞后防抖 ======
#define PRESS_HYSTERESIS 15      // 滞后范围（Pa）
static int32_t lastPressState = 0; // 0=正常, -1=低, 1=高

// ====== 校准 ======
static float calibTempVal = 0.0f, calibPressVal = 0.0f;
static float calibAdc1Val = 0.0f, calibAdc2Val = 0.0f;
static int32_t calibTempRaw = 0, calibPressRaw = 0;
static int32_t calibAdc1Raw = 0, calibAdc2Raw = 0;
#define TEMP_COEFF 0.07310f
// 温度传感器：PT100 正温度系数，温度升高ADC升高。T = TEMP_RANGE_MIN + ADC × (TEMP_COEFF×3.3/4095×1000)
#define TEMP_RANGE_MIN (0.0f)
#define PRESS_SLOPE 1.0f
#define ADC1_SLOPE 1.0f
#define ADC2_SLOPE 1.0f

// ====== 校准界面变量 ======
static uint8_t calibSel = 0, calibDpos = 0;
static int32_t calibEditTemp = 0, calibEditPress = 0;
static bool calibTempNeg = false; // 温度符号（独立标志，支持 0 的正负切换）
// 进入校准页时备份的初始值，放弃时恢复到此值而非清零
static int32_t calibInitTemp = 0, calibInitPress = 0;

// ====== 保存标志 ======
static bool calibSaved = false;

// ====== 参数编辑 ======
static uint8_t paramMode = 0;
static uint8_t paramSel = 0, paramDpos = 0;
static int32_t paramEditVal[6] = {0};
static uint8_t paramLastSel = 255;

// ====== 系统设置选择 ======
static uint8_t settingsSel = 0;

// ====== 密码界面 ======
static uint8_t pwdMode = 0;
static uint16_t inputPwd = 0;
static uint8_t pwdDpos = 0;
static uint8_t verifyAttempts = 3;

// ====== 密码验证目标模式（新增） ======
static uint8_t pwdTargetMode = 2;   // 默认进入系统设置 (mode=2)

// ====== 菜单按钮通用布局 ======
#define BTN_GAP 10
#define BTN_W ((W - BTN_GAP * 4) / 3)
#define BTN_H 32
#define BTN_Y (H - BTN_H - 2)

// ====== ADC ======
static bool adcHwReady = false;
static void adcInit() {
  __HAL_RCC_ADC1_CLK_ENABLE();
  ADC1->CR2 |= ADC_CR2_RSTCAL;
  uint32_t to = 200000;
  while ((ADC1->CR2 & ADC_CR2_RSTCAL) && --to);
  ADC1->CR2 |= ADC_CR2_CAL; to = 200000;
  while ((ADC1->CR2 & ADC_CR2_CAL) && --to);
  ADC1->CR1 &= ~ADC_CR1_SCAN;
  ADC1->CR2 &= ~(ADC_CR2_CONT | ADC_CR2_EXTTRIG);
  ADC1->SMPR1 = ADC_SMPR1_SMP10_2 | ADC_SMPR1_SMP10_1 | ADC_SMPR1_SMP10_0;
  ADC1->SMPR2 = ADC_SMPR2_SMP0_2 | ADC_SMPR2_SMP0_1 | ADC_SMPR2_SMP0_0;
  ADC1->CR2 |= ADC_CR2_ADON;
  for (volatile int i = 0; i < 1000; i++) __NOP();
  adcHwReady = true;
}
static inline uint16_t adcReadChannel(uint8_t ch) {
  if (!adcHwReady) adcInit();
  if (!adcHwReady) return 0;
  ADC1->CR2 &= ~ADC_CR2_CONT;
  ADC1->SQR3 = ch & 0x1F; ADC1->SQR1 = 0; ADC1->SR = 0;
  for (volatile int i = 0; i < 200; i++) __NOP();
  ADC1->CR2 |= ADC_CR2_SWSTART;
  uint32_t t = 500000;
  while (!(ADC1->SR & ADC_SR_EOC) && --t);
  return ADC1->DR;
}
// 16 次采样取平均：用于校准基准、恢复出厂等关键采点，避免单次采样受噪声/干扰影响
static inline uint16_t adcReadAvg(uint8_t ch) {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += adcReadChannel(ch);
    for (volatile int j = 0; j < 200; j++) __NOP();  // 两次转换之间留出稳定时间
  }
  return (uint16_t)(sum / 16);
}

// ====== 蜂鸣器 ======
void beep(uint8_t n, uint16_t onMs = 100, uint16_t offMs = 100) {
  (void)n; (void)onMs; (void)offMs; // 蜂鸣器已屏蔽
}

// ====== 中文字库绘制 ======
void drawAsciiChar24(char ch, int x, int y, uint16_t color, float scale = 1.0f);
void drawChineseChar(const char* ch, int x, int y, uint16_t color, float scale = 1.0f, bool bold = false) {
  for (int i = 0; i < FONT_COUNT; i++) {
    if (strcmp(font_24[i].index, ch) == 0) {
      for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 24; col++) {
          int byteIdx = row * 3 + col / 8;
          if (font_24[i].matrix[byteIdx] & (0x01 << (col % 8))) {
            int px = (int)(col * scale), py = (int)(row * scale);
            int pw = (int)((col + 1) * scale) - px, ph = (int)((row + 1) * scale) - py;
            if (pw < 1) pw = 1; if (ph < 1) ph = 1;
            tft.fillRect(x + px, y + py, pw, ph, color);
            if (bold) tft.fillRect(x + px + 1, y + py + 1, pw, ph, color);
          }
        }
      }
      return;
    }
  }
}
void drawMixedString(const char* str, int x, int y, uint16_t color, float scale = 1.0f, bool bold = false) {
  int cx = x, len = strlen(str);
  for (int i = 0; i < len;) {
    unsigned char c = (unsigned char)str[i];
    if (c >= 0xE0 && i + 2 < len) {
      char ch[4] = {str[i], str[i + 1], str[i + 2], 0};
      drawChineseChar(ch, cx, y, color, scale, bold);
      cx += (int)(26 * scale) + (bold ? 1 : 0); i += 3;
    } else if (c < 0x80) {
      drawAsciiChar24((char)c, cx, y, color, scale);
      cx += (int)(12 * scale) + (bold ? 1 : 0);
      i++;
    } else i++;
  }
}

// ====== 32x32标题字体绘制 ======
void drawChineseChar32(const char* ch, int x, int y, uint16_t color) {
  for (int i = 0; i < FONT_TITLE_COUNT; i++) {
    if (strcmp(font_title_32[i].index, ch) == 0) {
      for (int row = 0; row < 32; row++) {
        int runStart = -1;
        for (int col = 0; col <= 32; col++) {
          bool pixelOn = false;
          if (col < 32) {
            int byteIdx = row * 4 + col / 8;
            pixelOn = (font_title_32[i].matrix[byteIdx] & (0x01 << (col % 8))) != 0;
          }
          if (pixelOn) {
            if (runStart < 0) runStart = col;
          } else {
            if (runStart >= 0) {
              tft.fillRect(x + runStart, y + row, col - runStart, 1, color);
              runStart = -1;
            }
          }
        }
      }
      return;
    }
  }
}
void drawTitleString(const char* str, int x, int y, uint16_t color) {
  int cx = x, len = strlen(str);
  for (int i = 0; i < len;) {
    unsigned char c = (unsigned char)str[i];
    if (c >= 0xE0 && i + 2 < len) {
      char ch[4] = {str[i], str[i + 1], str[i + 2], 0};
      drawChineseChar32(ch, cx, y, color);
      cx += 34; i += 3;
    } else if (c < 0x80) {
      i++; cx += 16;
    } else i++;
  }
}

// ====== 24x16 ASCII字符绘制（统一修正连字符偏移） ======
void drawAsciiChar24(char ch, int x, int y, uint16_t color, float scale) {
  for (int i = 0; i < ASCII_24_COUNT; i++) {
    if (font_ascii_24[i].ch == ch) {
      for (int row = 0; row < 24; row++) {
        int runStart = -1;
        for (int col = 0; col <= 16; col++) {
          bool pixelOn = false;
          if (col < 16) {
            int byteIdx = row * 2 + col / 8;
            pixelOn = (font_ascii_24[i].matrix[byteIdx] & (0x01 << (col % 8))) != 0;
          }
          if (pixelOn) {
            if (runStart < 0) runStart = col;
          } else {
            if (runStart >= 0) {
              int offsetY = 0;
              if (ch == '-') offsetY = 4;
              else if (ch == '~') offsetY = 8;   // 波浪号同样下移 4 像素
              
              if (scale == 1.0f) {
                tft.fillRect(x + runStart, y + offsetY + row, col - runStart, 1, color);
              } else {
                int px = (int)(runStart * scale), py = (int)((row + offsetY) * scale);
                int pw = (int)(col * scale) - px;
                int ph = (int)((row + 1 + offsetY) * scale) - py;
                if (pw < 1) pw = 1; if (ph < 1) ph = 1;
                tft.fillRect(x + px, y + py, pw, ph, color);
              }
              runStart = -1;
            }
          }
        }
      }
      return;
    }
  }
}
void drawAsciiString24(const char* str, int x, int y, uint16_t color) {
  int cx = x;
  for (int i = 0; str[i]; i++) {
    drawAsciiChar24(str[i], cx, y, color);
    cx += 12;
  }
}

// ====== 压力/温度计算 ======
static float filteredPressRaw = -1.0f; // 低通滤波历史缓冲区 (-1 表示未初始化)
static float filteredTempRaw = -1.0f;
static float filteredAdc1Raw = -1.0f;
static float filteredAdc2Raw = -1.0f;

float calcDisplayPress() {
  int sum = 0, n = 16;
  int minRaw = 4095, maxRaw = 0;
  for (int i = 0; i < n; i++) {
    int raw = adcReadChannel(PRESS_ADC_CH);
    sum += raw;
    if (raw < minRaw) minRaw = raw;
    if (raw > maxRaw) maxRaw = raw;
  }
  float currentRaw = (float)sum / n;
  pressSensorAbnormal = (minRaw <= 5 || maxRaw >= 4090);

  if (filteredPressRaw < 0.0f) {
    filteredPressRaw = currentRaw;
  } else {
    const float alpha = 0.35f; 
    filteredPressRaw = filteredPressRaw * (1.0f - alpha) + currentRaw * alpha;
  }

  float v = calibPressVal + (filteredPressRaw - calibPressRaw) * PRESS_SLOPE;
  return (v < 0) ? 0 : v;
}
float calcDisplayTemp() {
  int sum = 0, n = 16;
  int minRaw = 4095, maxRaw = 0;
  for (int i = 0; i < n; i++) {
    int raw = adcReadChannel(TEMP_ADC_CH);
    sum += raw;
    if (raw < minRaw) minRaw = raw;
    if (raw > maxRaw) maxRaw = raw;
  }
  float currentRaw = (float)sum / n;
  tempSensorAbnormal = (minRaw <= 5 || maxRaw >= 4090);

  if (filteredTempRaw < 0.0f) {
    filteredTempRaw = currentRaw;
  } else {
    const float alpha = 0.12f; 
    filteredTempRaw = filteredTempRaw * (1.0f - alpha) + currentRaw * alpha;
  }

  return calibTempVal + (filteredTempRaw - calibTempRaw) * TEMP_COEFF * 3.3f / 4095.0f * 1000.0f;
}
float calcAdc1() {
  int sum = 0, n = 16;
  int minRaw = 4095, maxRaw = 0;
  for (int i = 0; i < n; i++) {
    int raw = adcReadChannel(ADC1_CH);
    sum += raw;
    if (raw < minRaw) minRaw = raw;
    if (raw > maxRaw) maxRaw = raw;
  }
  float currentRaw = (float)sum / n;

  if (filteredAdc1Raw < 0.0f) {
    filteredAdc1Raw = currentRaw;
  } else {
    const float alpha = 0.12f; 
    filteredAdc1Raw = filteredAdc1Raw * (1.0f - alpha) + currentRaw * alpha;
  }

  float v = calibAdc1Val + (filteredAdc1Raw - calibAdc1Raw) * ADC1_SLOPE;
  return (v < 0) ? 0 : v;
}
float calcAdc2() {
  int sum = 0, n = 16;
  int minRaw = 4095, maxRaw = 0;
  for (int i = 0; i < n; i++) {
    int raw = adcReadChannel(ADC2_CH);
    sum += raw;
    if (raw < minRaw) minRaw = raw;
    if (raw > maxRaw) maxRaw = raw;
  }
  float currentRaw = (float)sum / n;

  if (filteredAdc2Raw < 0.0f) {
    filteredAdc2Raw = currentRaw;
  } else {
    const float alpha = 0.12f; 
    filteredAdc2Raw = filteredAdc2Raw * (1.0f - alpha) + currentRaw * alpha;
  }

  float v = calibAdc2Val + (filteredAdc2Raw - calibAdc2Raw) * ADC2_SLOPE;
  return (v < 0) ? 0 : v;
}

static bool hasSafetyAlert() {
  bool pressureAlert = systemActive && countdownRemain == 0 &&
                       ((int)pressVal < sysParams[1] || (int)pressVal > sysParams[3]);
  return tempSensorAbnormal || pressSensorAbnormal || pressureAlert;
}

// ====== 菜单按钮绘制 ======
void drawBtn(int idx, const char* label, uint16_t color) {
  int x = BTN_GAP + idx * (BTN_W + BTN_GAP);
  tft.fillRect(x, BTN_Y, BTN_W, BTN_H, color);
  int tw = strlen(label) / 3 * 18;
  if (tw > 0) {
    drawMixedString(label, x + (BTN_W - tw) / 2 - 15, BTN_Y + 4, TFT_BLACK, 1.0f);
  }
}

// ====== 更新警报状态（只在系统激活后生效） ======
void updateGlobalAlarmState() {
    if (!systemActive) return;

    // 压力恢复超时报警（仅换气倒计时结束后才触发）
    if (recoveryTimeoutAlarm && countdownRemain == 0) {
        globalAlarm = true;
        if (!muteOn) digitalWrite(ALARM_RELAY, HIGH);
        return;
    }

    bool canAlarm = (countdownRemain == 0);
    if (!canAlarm) return;
    int p = (int)pressVal;
    bool alarmNow = (p < sysParams[1] || p > sysParams[3]);
    if (alarmNow) {
        globalAlarm = true;
        if (!muteOn) digitalWrite(ALARM_RELAY, HIGH);
    } else {
        globalAlarm = false;
        muteOn = false;
        digitalWrite(ALARM_RELAY, LOW);
    }
}

// 换气倒计时阶段通风控制：
// 压力未达正常值(低于下限) → 只开进气；压力超过正常值(高于上限) → 只开排气；
// 压力处于正常值范围[下限,上限]内 → 进气和排气同时执行（充分换气）
static void applyCountdownVentilation(int pressure) {
  if (pressure < sysParams[0]) {
    digitalWrite(INLET_RELAY, HIGH);
    digitalWrite(EXHAUST_RELAY, LOW);
  } else if (pressure > sysParams[2]) {
    digitalWrite(INLET_RELAY, LOW);
    digitalWrite(EXHAUST_RELAY, HIGH);
  } else {
    digitalWrite(INLET_RELAY, HIGH);
    digitalWrite(EXHAUST_RELAY, HIGH);
  }
}

// ====== 压力控制与欠压断电保护（全新 50% 中点稳压逻辑） ======
void updatePressureControl() {
    if (!systemActive) return;

    int p = (int)pressVal;
    static bool isFilling = false; 

    // ====== 关键：动态计算 50% 中点，不管数值多少 ======
    int targetPressure = (sysParams[0] + sysParams[2]) / 2; 
    // ===================================================

    // --- 欠压 10 秒断电保护（始终运行，倒计时期间也需安全保护） ---
    if (p < sysParams[1]) {
        if (underPressureTimer == 0) {
            underPressureTimer = millis();
        } else if (millis() - underPressureTimer >= 10000) {
            if (!powerTripLatched) {
                powerTripLatched = true;
                powerOnDelivered = false;
                digitalWrite(POWER_RELAY, LOW);
            }
        }
    } else {
        underPressureTimer = 0;
        powerTripLatched = false;
    }

    // --- 压力恢复超时报警（仅换气倒计时结束后检测，倒计时期间不触发任何压力警报） ---
    if (countdownRemain == 0) {
        if (p >= sysParams[0] && p <= sysParams[2]) {
            recoveryTimeoutTimer = 0;
            recoveryTimeoutAlarm = false;
        } else if (!recoveryTimeoutAlarm) {
            if (recoveryTimeoutTimer == 0) {
                recoveryTimeoutTimer = millis();
            } else if (millis() - recoveryTimeoutTimer >= (uint32_t)sysParams[5] * 1000) {
                recoveryTimeoutAlarm = true;
            }
        }
    }

    // 倒计时期间按压力区间控制通风方向，直到倒计时结束
    if (countdownRemain > 0) {
        isFilling = false;
      applyCountdownVentilation(p);
        return;
    }

    bool pressureOk = (p >= sysParams[0]);

    // --- 进气/排气控制 ---
    static bool isExhausting = false;

    // 1. 超压泄压触发：超过压力上限
    if (p > sysParams[2]) {
        isExhausting = true;
        isFilling = false;
    }

    // 2. 处理泄压状态（排到 50% 中点即停止）
    if (isExhausting) {
        digitalWrite(INLET_RELAY, LOW);
        digitalWrite(EXHAUST_RELAY, HIGH);

        if (p <= targetPressure) {
            isExhausting = false;
            digitalWrite(EXHAUST_RELAY, LOW);
        }
    }
    // 3. 处理补气状态（补到 50% 中点即停止）
    else if (isFilling) {
        digitalWrite(INLET_RELAY, HIGH);
        digitalWrite(EXHAUST_RELAY, LOW);

        if (p >= targetPressure) {
            digitalWrite(INLET_RELAY, LOW);
            isFilling = false;
        }
    }
    // 4. 空闲状态（低于下限才开始补气）
    else {
        if (p < sysParams[0]) {
            digitalWrite(INLET_RELAY, HIGH);
            digitalWrite(EXHAUST_RELAY, LOW);
            isFilling = true;
        } else {
            digitalWrite(INLET_RELAY, LOW);
            digitalWrite(EXHAUST_RELAY, LOW);
        }
    }

    // --- 送电控制：逻辑不变 ---
    if (countdownDoneFirstRun && !powerTripLatched) {
        if (pressureOk) {
            powerOnDelivered = true;
            digitalWrite(POWER_RELAY, HIGH);
        }
    }
}

// ====== 仅刷新警报/消音状态 ======
void drawAlarmStatus(int x, int y, int w, int h) {
  tft.fillRect(x, y, w, h, TFT_WHITE);
  if (muteOn) {
    tft.fillRect(x, y, w, h, TFT_YELLOW);
    drawMixedString("消音", x+10, 5, TFT_BLACK, 1.0f);
  } else if (globalAlarm) {
    bool redBg = blinkOn;
    tft.fillRect(x, y, w, h, redBg ? TFT_RED : TFT_WHITE);
    drawMixedString("警报", x+10, 5, redBg ? TFT_WHITE : TFT_BLACK, 1.0f);
  }
}

// ====== 顶栏状态指示 ======
void drawTopStatusBar() {
  bool inletOn = digitalRead(INLET_RELAY);
  bool exhaustOn = digitalRead(EXHAUST_RELAY);
  bool powerOn = digitalRead(POWER_RELAY);
  const int sw = 62, sh = 28, gap = 2;
  int x = 2;
  tft.fillRect(x, 2, sw, sh, TFT_WHITE);
  if (inletOn) { tft.fillRect(x, 2, sw, sh, COLOR_SPACEGREY); drawMixedString("进气", x+10, 5, TFT_WHITE, 1.0f); }
  x += sw+gap;
  tft.fillRect(x, 2, sw, sh, TFT_WHITE);
  if (exhaustOn) { tft.fillRect(x, 2, sw, sh, COLOR_SPACEGREY); drawMixedString("排气", x+10, 5, TFT_WHITE, 1.0f); }
  x += sw+gap;
  tft.fillRect(x, 2, sw, sh, TFT_WHITE);
  if (powerOn) { tft.fillRect(x, 2, sw, sh, TFT_GREEN); drawMixedString("送电", x+10, 5, TFT_WHITE, 1.0f); }
  x += sw+gap;
  drawAlarmStatus(x, 2, sw, sh);
}

// ====== 主页 ======
void drawMainPage() {
  tft.fillScreen(TFT_WHITE);
  tft.fillRect(0, H - BTN_H - BTN_GAP - 10, W, 3, TFT_BLUE);
  drawTitleString("欢迎使用正压防爆系统", 60, 80, TFT_BLUE);
  drawMixedString("服务电话:13023456789", 100, 130, TFT_BLACK);
  drawTitleString("谷子防爆电气有限公司", 60, 180, TFT_BLUE);
  drawBtn(0, systemActive && globalAlarm ? (muteOn ? "取消消音" : "取消警报") : "正压启动", systemActive && globalAlarm ? (muteOn ? TFT_YELLOW : TFT_RED) : TFT_DARKGREY);
  drawBtn(1, "系统设置", TFT_DARKGREY);
  drawBtn(2, "系统调试", TFT_DARKGREY);
  if (systemActive) drawTopStatusBar();
}

// ====== 正压启动 (mode=1) ======
void drawPressureScreen() {
  tft.fillScreen(TFT_WHITE);
  drawTopStatusBar();
  char buf[32];

  // 倒计时结束即进入第二界面（压力异常时在第二界面显示警报，不阻塞界面切换）
  bool conditionMet = (countdownRemain == 0 && countdownDoneFirstRun);
  if (conditionMet) {
    systemRunningNormal = true;
  }

  bool showDisplay = systemRunningNormal; 

  // --- 第一行：倒计时或温度 ---
  if (showDisplay) {
    drawMixedString("柜内温度:", 20, 70, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%02d℃", (int)tempVal);
    drawMixedString(buf, 200, 72, TFT_BLACK);
  } else {
    drawMixedString("换气倒计时:", 20, 70, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%04ds", countdownRemain);
    drawAsciiString24(buf, 200, 72, TFT_BLACK);
  }

  // --- 第二行：柜内压力 ---
  drawMixedString("柜内压力:", 20, 110, TFT_BLACK);
  snprintf(buf, sizeof(buf), "%04dPa", (int)pressVal);
  drawAsciiString24(buf, 200, 112, TFT_BLACK);

  // --- 状态框文字与颜色逻辑（带滞后防抖） ---
  int p = (int)pressVal;
  uint16_t statusColor = TFT_GREEN;
  const char* statusText = "";
  bool alarmNow = false;
  int32_t pressState = 0;  // 0=正常, -1=低, 1=高
  
  // 使用滞后机制判断压力状态
  if (p < sysParams[0] - PRESS_HYSTERESIS) {
    pressState = -1;  // 低
  } else if (p > sysParams[2] + PRESS_HYSTERESIS) {
    pressState = 1;   // 高
  } else if (p >= sysParams[0] && p <= sysParams[2]) {
    pressState = 0;   // 正常
  } else {
    // 在滞后边界内，保持上一次的状态
    pressState = lastPressState;
  }
  lastPressState = pressState;
  
  // 根据状态判断报警
  if (pressState == -1 || pressState == 1) {
    if (p < sysParams[1] || p > sysParams[3]) alarmNow = true;
  }

  if (pressState == -1) {
    statusColor = TFT_RED;
    statusText = "柜内压力低";
  } else if (pressState == 1) {
    statusColor = TFT_RED;
    statusText = "柜内压力高";
  } else {
    if (showDisplay && powerOnDelivered) {
      statusText = "系统运行中";
      statusColor = TFT_GREEN;
    } else {
      statusText = "柜内压力正常";
      statusColor = TFT_GREEN;
    }
  }

  updateGlobalAlarmState();
  static bool prevAlarmNow = false;
  if (alarmNow && !prevAlarmNow) { blinkOn = true; }
  prevAlarmNow = alarmNow;
  bool canAlarm = (countdownRemain == 0);
  bool isAlarmActive = canAlarm && alarmNow;
  tft.fillRect(20, 150, 440, 38, isAlarmActive ? (blinkOn ? statusColor : TFT_WHITE) : statusColor);
  uint16_t textColor = isAlarmActive ? (blinkOn ? TFT_WHITE : TFT_BLACK) : TFT_BLACK;
  
// 动态居中计算
int textWidth = strlen(statusText) * 26;
int centerX = (W - textWidth) / 2+120;   // 全屏居中
drawMixedString(statusText, centerX, 154, textColor, 1.0f);

  // --- 第三、四行：完整保留区间显示 ---
  if (showDisplay) {
    drawMixedString("压力正常值:", 20, 190, TFT_BLACK, 1.0f);
    snprintf(buf, sizeof(buf), "%04d ~ %04dPa", sysParams[0], sysParams[2]); // 注意加了空格
    drawMixedString(buf, 200, 190, TFT_BLACK, 1.0f);
    // 欠压断电倒计时显示（已屏蔽）
  } else {
    drawMixedString("换气开启压力:", 20, 190, TFT_BLACK, 1.0f);
    snprintf(buf, sizeof(buf), "%04d", sysParams[0]);
    drawAsciiString24(buf, 200, 190, TFT_BLACK);
    drawMixedString("Pa", 250, 190, TFT_BLACK, 1.0f);

    drawMixedString("总换气时间:", 20, 230, TFT_BLACK, 1.0f);
    snprintf(buf, sizeof(buf), "%04d", sysParams[5]);
    drawAsciiString24(buf, 200, 230, TFT_BLACK);
    drawMixedString("s", 250, 230, TFT_BLACK, 1.0f);
  }

  // --- 底部按钮 ---
  tft.fillRect(BTN_GAP, BTN_Y, BTN_W, BTN_H, muteOn ? TFT_YELLOW : (globalAlarm ? TFT_RED : TFT_DARKGREY));
  drawMixedString(muteOn ? "取消消音" : "取消警报", BTN_GAP + 15, BTN_Y + 4, TFT_BLACK);
  tft.fillRect(BTN_GAP + 2 * (BTN_W + BTN_GAP), BTN_Y, BTN_W, BTN_H, TFT_DARKGREY);
  drawMixedString("返回主页", BTN_GAP + 2 * (BTN_W + BTN_GAP) + 20, BTN_Y + 4, TFT_WHITE);
}


// ====== 正压启动 局部刷新 ======
void updatePressureScreen() {
  char buf[32];
  static int32_t lastTempVal = -999;
  static int32_t lastCountdown = -1;
  static int32_t lastPressVal = -1;
  static uint8_t lastPressureState = 255;
  static bool lastMuteOn_Status = false;
  static bool lastGlobalAlarm_UI = false;
  static bool lastMuteOn_UI = false;
  static bool lastGlobalAlarm_Button = false;
  static bool lastMuteOn_Button = false;
  static bool lastBlinkOn = false;
  static bool lastBlinkOn_UI = false;
  static bool lastPowerBar = false;
  static bool lastInletBar = false;
  static bool lastExhaustBar = false;
  static bool lastShowDisplay = false;
  static int32_t lastUpdatePressState = 0;  // 本函数独立的压力状态缓存，防止与 drawMainPage 的全局状态混淆
  static bool lastPressStateWasNormal = true;  // 上一次压力状态是否为正常，用于检测状态转换
  static int32_t lastUnderPressureSec = -1;   // 欠压断电倒计时缓存
  static bool lastPowerTripped = false;        // 已断电状态缓存
  static bool lastUnderPressureBlink = false;  // 欠压断电闪烁缓存

  int p = (int)pressVal;
  uint16_t statusColor = TFT_GREEN;
  const char* statusText = "";
  bool alarmNow = false;
  uint8_t currentState = 0;
  
  // 使用滞后机制判断压力状态（本函数独立处理，不影响全局 lastPressState）
  int32_t pressState = 0;  // 0=正常, -1=低, 1=高
  if (p < sysParams[0] - PRESS_HYSTERESIS) {
    pressState = -1;
  } else if (p > sysParams[2] + PRESS_HYSTERESIS) {
    pressState = 1;
  } else if (p >= sysParams[0] && p <= sysParams[2]) {
    pressState = 0;
  } else {
    // 在滞后边界内，保持上一次本地状态
    pressState = lastUpdatePressState;
  }
  lastUpdatePressState = pressState;  // 更新本函数的状态缓存
  
  // 【修复】：只在压力状态从正常转换到异常时触发闪烁，而不是持续闪烁
  bool stateJustChanged = (lastPressStateWasNormal && (pressState != 0));
  if (stateJustChanged) {
    blinkOn = true;  // 触发一次闪烁
  }
  lastPressStateWasNormal = (pressState == 0);  // 记录当前状态是否为正常
  
  // alarmNow 仅在状态刚转换时为 true，之后保持 false（允许闪烁闲置直至停止）
  if ((pressState == -1 || pressState == 1) && p >= sysParams[1] && p <= sysParams[3]) {
    // 处于异常状态但不在最严重的告警范围内，不触发新的闪烁
    alarmNow = false;
  } else if (pressState == -1 || pressState == 1) {
    // 处于异常状态且在最严重的告警范围内
    if (p < sysParams[1] || p > sysParams[3]) alarmNow = true;
  }

  // 逻辑判断（使用滞后后的 pressState）
  if (pressState == -1) {
    statusColor = TFT_RED;
    statusText = "柜内压力低";
    currentState = 1;
  } else if (pressState == 1) {
    statusColor = TFT_RED;
    statusText = "柜内压力高";
    currentState = 2;
  } else {
    bool conditionMet = (countdownRemain == 0 && countdownDoneFirstRun);
    if (conditionMet) {
      systemRunningNormal = true;
    }
    bool showDisplay = systemRunningNormal;
    if (showDisplay && powerOnDelivered) {
      statusText = "系统运行中";
      statusColor = TFT_GREEN;
      currentState = 3;
    } else {
      statusText = "柜内压力正常";
      statusColor = TFT_GREEN;
      currentState = 0;
    }
  }

  updateGlobalAlarmState();
  static bool prevAlarmNow = false;
  if (alarmNow && !prevAlarmNow) { blinkOn = true; }
  prevAlarmNow = alarmNow;

  // 布局显示判定
  bool conditionMet = (countdownRemain == 0 && countdownDoneFirstRun);
  if (conditionMet) {
    systemRunningNormal = true;
  }
  bool showDisplay = systemRunningNormal; 

    // ---------- 布局切换检测 ----------
  if (showDisplay != lastShowDisplay) {
    if (showDisplay) {
      // 切换到第二布局
      tft.fillRect(20, 70, 180, 28, TFT_WHITE);
      drawMixedString("柜内温度:", 20, 70, TFT_BLACK);
      tft.fillRect(200, 70, 100, 28, TFT_WHITE);
      snprintf(buf, sizeof(buf), "%02d℃", (int)tempVal);
      drawMixedString(buf, 200, 72, TFT_BLACK);

      tft.fillRect(20, 190, 180, 28, TFT_WHITE);
      drawMixedString("压力正常值:", 20, 190, TFT_BLACK, 1.0f);
      tft.fillRect(200, 190, 200, 28, TFT_WHITE);
      snprintf(buf, sizeof(buf), "%04d ~ %04dPa", sysParams[0], sysParams[2]);
      drawMixedString(buf, 200, 190, TFT_BLACK, 1.0f);

      tft.fillRect(20, 230, 300, 28, TFT_WHITE);
      lastCountdown = -1;
    } else {
      // 切换到第一布局
      tft.fillRect(20, 70, 180, 28, TFT_WHITE);
      drawMixedString("换气倒计时:", 20, 70, TFT_BLACK);
      tft.fillRect(200, 70, 100, 28, TFT_WHITE);
      snprintf(buf, sizeof(buf), "%04ds", countdownRemain);
      drawAsciiString24(buf, 200, 72, TFT_BLACK);

      tft.fillRect(20, 190, 180, 28, TFT_WHITE);
      drawMixedString("换气开启压力:", 20, 190, TFT_BLACK, 1.0f);
      tft.fillRect(200, 190, 200, 28, TFT_WHITE);
      snprintf(buf, sizeof(buf), "%04d", sysParams[0]);
      drawAsciiString24(buf, 200, 190, TFT_BLACK);
      drawMixedString("Pa", 250, 190, TFT_BLACK, 1.0f);

      tft.fillRect(20, 230, 300, 28, TFT_WHITE);
      drawMixedString("总换气时间:", 20, 230, TFT_BLACK, 1.0f);
      snprintf(buf, sizeof(buf), "%04d", sysParams[5]);
      drawAsciiString24(buf, 200, 230, TFT_BLACK);
      drawMixedString("s", 250, 230, TFT_BLACK, 1.0f);

      lastTempVal = -999;
    }
    lastShowDisplay = showDisplay;
    
    // ====== 新增：切换布局后，强制重置状态框和顶栏的缓存，彻底洗掉第一屏残留 ======
    lastPressureState = 255;    // 设为一个永远匹配不到的数值
    lastMuteOn_Status = !muteOn;
    lastBlinkOn = !blinkOn;
    // 重置顶栏状态缓存
    lastMuteOn_UI = !muteOn;
    lastGlobalAlarm_UI = !globalAlarm;
    lastPowerBar = !powerOnDelivered;
    // 重置欠压断电显示缓存
    lastUnderPressureSec = -1;
    lastPowerTripped = !powerTripLatched;
    lastUnderPressureBlink = false;
    // ================================================================
  }

  // ---------- 动态数值刷新 ----------
  if (showDisplay) {
    if (lastTempVal != (int)tempVal) {
      tft.fillRect(200, 70, 80, 28, TFT_WHITE);
      snprintf(buf, sizeof(buf), "%02d℃", (int)tempVal);
      drawMixedString(buf, 200, 72, TFT_BLACK);
      lastTempVal = (int)tempVal;
    }
  } else {
    if (lastCountdown != countdownRemain) {
      tft.fillRect(198, 70, 80, 28, TFT_WHITE);
      snprintf(buf, sizeof(buf), "%04ds", countdownRemain);
      drawAsciiString24(buf, 200, 72, TFT_BLACK);
      lastCountdown = countdownRemain;
    }
  }

  if (lastPressVal != pressVal) {
    snprintf(buf, sizeof(buf), "%04dPa", (int)pressVal);
    int textWidth = strlen(buf) * 12;
    tft.fillRect(198, 112, textWidth + 4, 28, TFT_WHITE);
    drawAsciiString24(buf, 200, 112, TFT_BLACK);
    lastPressVal = pressVal;
  }

 // ========== 【修复】状态框显示：异常状态红色背景闪烁 ==========
    // 当压力处于异常状态时，红色背景闪烁提醒
    bool isBlinkMode = (countdownRemain == 0) && (pressState != 0);  // 处于异常状态时闪烁
    bool finalBlinkOn = isBlinkMode && blinkOn;

    // 仅在会影响显示的条件发生变化时才重绘：状态、消音或“实际闪烁显示”(finalBlinkOn)
    if (lastPressureState != currentState || 
        lastMuteOn_Status != muteOn || 
        lastBlinkOn != finalBlinkOn) 
    {
        // 计算颜色
        uint16_t backColor = statusColor;
        uint16_t textColor = TFT_WHITE;
        
        // 异常状态下背景闪烁，正常状态下稳定显示
        if (pressState != 0) {
            // 异常状态（低/高）：红色背景闪烁
            if (isBlinkMode) {
                backColor = finalBlinkOn ? statusColor : TFT_WHITE;      // RED ↔ WHITE
                textColor = finalBlinkOn ? TFT_WHITE : TFT_BLACK;        // WHITE ↔ BLACK
            } else {
                backColor = statusColor;  // RED（稳定显示）
                textColor = TFT_WHITE;
            }
        } else {
            // 正常状态：绿色稳定显示
            backColor = TFT_GREEN;
            textColor = TFT_WHITE;
        }

        // 【关键】先计算文字位置（一次性计算，不要每次重新算）
        static int lastCenterX = 120;  // 缓存上一次的文字位置
        int estimatedWidth = 0;
        const char* pStr = statusText;
        while (*pStr) {
            unsigned char c = *pStr;
            if (c >= 0xE0) { estimatedWidth += 26; pStr += 3; }
            else if (c >= 0xC0) { estimatedWidth += 16; pStr += 2; }
            else { estimatedWidth += 12; pStr++; }
        }
        int centerX = (W - estimatedWidth) / 2;
        if (centerX < 0) centerX = 0;
        
        // 只有当文字内容改变时才重新计算位置
        if (lastPressureState != currentState) {
            lastCenterX = centerX;
        }

        // 【关键】填充+绘制之间插入短暂延时，让TFT控制器准备好接收连续数据
        tft.fillRect(0, 150, W, 38, backColor);
        delayMicroseconds(50); // 给TFT控制器一个呼吸周期，防止FIFO欠载
        
        // 使用缓存的文字位置，避免每次都重新计算导致微小位置偏差
        drawMixedString(statusText, lastCenterX, 154, textColor, 1.0f);

        lastPressureState = currentState;
        lastMuteOn_Status = muteOn;
        lastBlinkOn = finalBlinkOn;  // 缓存“实际用于绘制的闪烁状态”，避免无意义重绘
    }

  // ---------- 顶部状态栏 ----------
  if (lastMuteOn_UI != muteOn || lastGlobalAlarm_UI != globalAlarm || lastBlinkOn_UI != blinkOn || lastPowerBar != powerOnDelivered || lastInletBar != (bool)digitalRead(INLET_RELAY) || lastExhaustBar != (bool)digitalRead(EXHAUST_RELAY)) {
    if (lastMuteOn_UI == muteOn && lastGlobalAlarm_UI == globalAlarm && lastPowerBar == powerOnDelivered && lastInletBar == digitalRead(INLET_RELAY) && lastExhaustBar == digitalRead(EXHAUST_RELAY)) {
      drawAlarmStatus(2 + 3 * (62 + 2), 2, 62, 28);
    } else {
      drawTopStatusBar();
    }
    lastMuteOn_UI = muteOn;
    lastGlobalAlarm_UI = globalAlarm;
    lastBlinkOn_UI = blinkOn;
    lastPowerBar = powerOnDelivered;
    lastInletBar = digitalRead(INLET_RELAY);
    lastExhaustBar = digitalRead(EXHAUST_RELAY);
  }

  // ---------- 欠压断电倒计时显示（已屏蔽） ----------

  // ---------- 底部按钮 ----------
  if (lastMuteOn_Button != muteOn || lastGlobalAlarm_Button != globalAlarm) {
    tft.fillRect(BTN_GAP, BTN_Y, BTN_W, BTN_H, muteOn ? TFT_YELLOW : (globalAlarm ? TFT_RED : TFT_DARKGREY));
    drawMixedString(muteOn ? "取消消音" : "取消警报", BTN_GAP + 15, BTN_Y + 4, TFT_BLACK);
    lastMuteOn_Button = muteOn;
    lastGlobalAlarm_Button = globalAlarm;
  }
}

// ====== 系统设置菜单 (mode=2) ======
void drawSettingsMenu() {
  tft.fillScreen(TFT_WHITE);
  drawMixedString("系统设置", 5, 5, TFT_BLACK, 1.0f);
  const char* items[] = {"修改密码", "校准传感器", "设置参数", "恢复出厂"};
  int ys[] = {60, 100, 140, 180};
  for (int i = 0; i < 4; i++) {
    if (i == settingsSel) {
      tft.fillRect(20, ys[i] - 5, 160, 30, COLOR_SPACEGREY);
      tft.fillTriangle(8, ys[i] + 5, 8, ys[i] + 15, 16, ys[i] + 10, TFT_DARKGREY);
      drawMixedString(items[i], 30, ys[i], TFT_WHITE, 1.0f);
    } else {
      tft.fillRect(20, ys[i] - 5, 160, 30, TFT_WHITE);
      drawMixedString(items[i], 30, ys[i], TFT_BLACK, 1.0f);
    }
  }
  drawBtn(0, "返回主页", TFT_DARKGREY);
  drawBtn(1, "下一选项", TFT_DARKGREY);
  drawBtn(2, "确认", TFT_DARKGREY);
}

// ====== 系统设置菜单局部刷新 ======
void updateSettingsMenu() {
  const char* items[] = {"修改密码", "校准传感器", "设置参数", "恢复出厂"};
  int ys[] = {60, 100, 140, 180};
  int oldSel = (settingsSel == 0) ? 3 : settingsSel - 1;
  int newSel = settingsSel;
  tft.fillRect(20, ys[oldSel] - 5, 200, 30, TFT_WHITE);
  tft.fillRect(8, ys[oldSel] + 5, 10, 10, TFT_WHITE);
  drawMixedString(items[oldSel], 30, ys[oldSel], TFT_BLACK, 1.0f);
  tft.fillRect(20, ys[newSel] - 5, 200, 30, COLOR_SPACEGREY);
  tft.fillTriangle(8, ys[newSel] + 5, 8, ys[newSel] + 15, 16, ys[newSel] + 10, TFT_DARKGREY);
  drawMixedString(items[newSel], 30, ys[newSel], TFT_WHITE, 1.0f);
}

// ====== 系统调试 (mode=3) ======
void drawDebugScreen() {
  tft.fillScreen(TFT_WHITE);
  tft.fillRect(W - 95, 3, 70, 30, TFT_WHITE);
  drawMixedString("系统调试", 5, 5, TFT_BLACK, 1.0f);
  drawTitleString("危险区域严禁在现场使用", 60, 80, TFT_RED);
  drawTitleString("可能导致爆炸请确认安全", 60, 120, TFT_RED);
  drawBtn(0, "返回主页", TFT_DARKGREY);
  drawBtn(1, "", TFT_WHITE);
  drawBtn(2, "确认调试", TFT_DARKGREY);
}

// ====== 调试确认页 (mode=7) ====== 
void drawDebugConfirmScreen() {
  tft.fillScreen(TFT_WHITE);
  tft.fillRect(W - 95, 3, 70, 30, TFT_WHITE);
  tft.fillRect(W - 95, 3, 70, 30, TFT_RED);
  drawMixedString("送电", W - 85, 5, TFT_WHITE, 1.0f);
  drawTitleString("系统已送电 请确认安全", 60, 80, TFT_RED);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d", (int)pressVal);
  drawMixedString("压力:", 30, 200, TFT_BLACK, 1.0f);
  drawAsciiString24(buf, 112, 200, TFT_BLACK);
  drawMixedString("Pa", 170, 200, TFT_BLACK, 1.0f);
  snprintf(buf, sizeof(buf), "%02d", (int)tempVal);
  drawMixedString("温度:", 230, 200, TFT_BLACK, 1.0f);
  drawAsciiString24(buf, 310, 200, TFT_BLACK);
  drawMixedString("℃", 350, 200, TFT_BLACK, 1.0f);
  drawBtn(0, "", TFT_WHITE);
  drawBtn(1, "", TFT_WHITE);
  drawBtn(2, "调试结束", TFT_DARKGREY);
}

// ====== 调试确认页 局部刷新 ======
void updateDebugConfirmScreen() {
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d", (int)pressVal);
  int textWidth = strlen(buf) * 12;
  tft.fillRect(110, 200, textWidth + 4, 28, TFT_WHITE);
  drawAsciiString24(buf, 112, 200, TFT_BLACK);
  snprintf(buf, sizeof(buf), "%02d", (int)tempVal);
  textWidth = strlen(buf) * 12;
  tft.fillRect(310, 200, textWidth + 4, 28, TFT_WHITE);
  drawAsciiString24(buf, 310, 200, TFT_BLACK);
}

// ====== 密码验证页 (mode=8) ======
void drawPasswordVerifyScreen() {
  tft.fillScreen(TFT_BLACK);
  if (pwdTargetMode == 2) {
      drawMixedString("请输入密码进入设置", 5, 5, TFT_YELLOW, 1.0f);
  } else if (pwdTargetMode == 3) {
      drawMixedString("请输入密码进入调试", 5, 5, TFT_YELLOW, 1.0f);
  } else {
      drawMixedString("请输入密码", 5, 5, TFT_YELLOW, 1.0f);
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "输入次数：%d", verifyAttempts);
  drawMixedString(buf, 30, 60, TFT_CYAN, 1.0f);
  drawMixedString("输入密码:", 30, 100, TFT_WHITE, 1.0f);
  int d0 = inputPwd / 100;
  int d1 = (inputPwd / 10) % 10;
  int d2 = inputPwd % 10;
  int xBase = 150;
  char d[2] = {0};
  for (int i = 0; i < 3; i++) {
    int digit = (i == 0) ? d0 : (i == 1) ? d1 : d2;
    d[0] = '0' + digit;
    uint16_t color = (i == pwdDpos) ? TFT_YELLOW : TFT_WHITE;
    drawAsciiChar24(d[0], xBase + i * 14, 100, color, 1.0f);
  }
  int triX = xBase + pwdDpos * 14 + 4;
  tft.fillTriangle(triX, 128, triX + 6, 128, triX + 3, 124, TFT_YELLOW);
  int btnY = BTN_Y;
  int col0X = BTN_GAP;
  int col1X = BTN_GAP + BTN_W + BTN_GAP;
  int col2X = BTN_GAP + 2 * (BTN_W + BTN_GAP);
  tft.fillRect(col0X, btnY, BTN_W, BTN_H, TFT_DARKGREY);
  drawMixedString("值加1", col0X + 20, btnY + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col1X, btnY, BTN_W, BTN_H, TFT_DARKGREY);
  drawMixedString("下一位", col1X + 20, btnY + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col2X, btnY, BTN_W, BTN_H, TFT_DARKGREY);
  drawMixedString("确认", col2X + 40, btnY + 6, TFT_WHITE, 1.0f);
}

// ====== 修改密码 (mode=4) ======
void drawPasswordScreen() {
  tft.fillScreen(TFT_BLACK);
  drawMixedString("修改密码", 5, 5, TFT_YELLOW, 1.0f);
  char buf[32];
  snprintf(buf, sizeof(buf), "当前密码:%03d", targetPassword);
  drawMixedString(buf, 30, 60, TFT_CYAN, 1.0f);
  drawMixedString("输入密码:", 30, 100, TFT_WHITE, 1.0f);
  int d0 = inputPwd / 100;
  int d1 = (inputPwd / 10) % 10;
  int d2 = inputPwd % 10;
  int xBase = 150;
  char d[2] = {0};
  for (int i = 0; i < 3; i++) {
    int digit = (i == 0) ? d0 : (i == 1) ? d1 : d2;
    d[0] = '0' + digit;
    uint16_t color = (i == pwdDpos) ? TFT_YELLOW : TFT_WHITE;
    drawAsciiChar24(d[0], xBase + i * 14, 100, color, 1.0f);
  }
  int triX = xBase + pwdDpos * 14 + 4;
  tft.fillTriangle(triX, 128, triX + 6, 128, triX + 3, 124, TFT_YELLOW);
  int btnRow1Y = BTN_Y - BTN_H - BTN_GAP;
  int btnRow2Y = BTN_Y;
  int col0X = BTN_GAP;
  int col1X = BTN_GAP + (W - 2 * BTN_GAP) / 3;
  int col2X = BTN_GAP + 2 * (W - 2 * BTN_GAP) / 3;
  int btnW = (W - 2 * BTN_GAP) / 3 - BTN_GAP;
  tft.fillRect(col0X, btnRow2Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("值加", col0X + 40, btnRow2Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col1X, btnRow1Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("下一位", col1X + 30, btnRow1Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col1X, btnRow2Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("放弃", col1X + 40, btnRow2Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col2X, btnRow1Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("下一项", col2X + 30, btnRow1Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col2X, btnRow2Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("保存", col2X + 40, btnRow2Y + 6, TFT_WHITE, 1.0f);
}

// ====== 修改密码 局部刷新 ======
void updatePasswordScreen() {
  int xBase = 150;
  tft.fillRect(xBase, 100, 50, 30, TFT_BLACK);
  int d0 = inputPwd / 100;
  int d1 = (inputPwd / 10) % 10;
  int d2 = inputPwd % 10;
  char d[2] = {0};
  for (int i = 0; i < 3; i++) {
    int digit = (i == 0) ? d0 : (i == 1) ? d1 : d2;
    d[0] = '0' + digit;
    uint16_t color = (i == pwdDpos) ? TFT_YELLOW : TFT_WHITE;
    drawAsciiChar24(d[0], xBase + i * 14, 100, color, 1.0f);
  }
  tft.fillRect(xBase, 124, 50, 6, TFT_BLACK);
  int triX = xBase + pwdDpos * 14 + 4;
  tft.fillTriangle(triX, 128, triX + 6, 128, triX + 3, 124, TFT_YELLOW);
}

// ====== 校准传感器 (mode=5) ======
void drawCalibScreen() {
  tft.fillScreen(TFT_BLACK);
  drawMixedString("校准传感器", 5, 5, TFT_YELLOW, 1.0f);
  int xBase = 200;
  char d[2] = {0};
  drawMixedString("校准温度", 30, 80, TFT_WHITE, 1.0f);
  drawMixedString("℃", 260, 80, TFT_WHITE, 1.0f);
  // 温度：符号位(+/-) + 3位数字，calibDpos=0 为符号位，1=百位 2=十位 3=个位
  int absT = calibEditTemp < 0 ? -calibEditTemp : calibEditTemp;
  d[0] = calibTempNeg ? '-' : '+';
  drawAsciiChar24(d[0], xBase, 80, (calibSel == 1 && calibDpos == 0) ? TFT_YELLOW : TFT_CYAN, 1.0f);
  int td[3] = {(absT / 100) % 10, (absT / 10) % 10, absT % 10};
  for (int i = 0; i < 3; i++) {
    d[0] = '0' + td[i];
    drawAsciiChar24(d[0], xBase + (i + 1) * 14, 80, (calibSel == 1 && i + 1 == calibDpos) ? TFT_YELLOW : TFT_CYAN, 1.0f);
  }
  if (calibSel == 1) {
    int triX = xBase + calibDpos * 14 + 4;
    tft.fillTriangle(triX, 108, triX + 6, 108, triX + 3, 104, TFT_YELLOW);
  }
  drawMixedString("校准压力", 30, 140, TFT_WHITE, 1.0f);
  drawMixedString("Pa", 260, 140, TFT_WHITE, 1.0f);
  int p0 = calibEditPress / 1000;
  int p1 = (calibEditPress / 100) % 10;
  int p2 = (calibEditPress / 10) % 10;
  int p3 = calibEditPress % 10;
  for (int i = 0; i < 4; i++) {
    int digit = (i == 0) ? p0 : (i == 1) ? p1 : (i == 2) ? p2 : p3;
    d[0] = '0' + digit;
    uint16_t color = (calibSel == 0 && i == calibDpos) ? TFT_YELLOW : TFT_CYAN;
    drawAsciiChar24(d[0], xBase + i * 14, 140, color, 1.0f);
  }
  if (calibSel == 0) {
    int triX = xBase + calibDpos * 14 + 4;
    tft.fillTriangle(triX, 168, triX + 6, 168, triX + 3, 164, TFT_YELLOW);
  }
  int btnRow1Y = BTN_Y - BTN_H - BTN_GAP;
  int btnRow2Y = BTN_Y;
  int col0X = BTN_GAP;
  int col1X = BTN_GAP + (W - 2 * BTN_GAP) / 3;
  int col2X = BTN_GAP + 2 * (W - 2 * BTN_GAP) / 3;
  int btnW = (W - 2 * BTN_GAP) / 3 - BTN_GAP;
  tft.fillRect(col0X, btnRow2Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("值加", col0X + 40, btnRow2Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col1X, btnRow1Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("下一位", col1X + 30, btnRow1Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col1X, btnRow2Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("放弃", col1X + 40, btnRow2Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col2X, btnRow1Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("下一项", col2X + 30, btnRow1Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col2X, btnRow2Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("保存", col2X + 40, btnRow2Y + 6, TFT_WHITE, 1.0f);
}

// ====== 校准传感器 局部刷新 ======
void updateCalibScreen() {
  int xBase = 200;
  char d[2] = {0};
  tft.fillRect(xBase, 80, 60, 30, TFT_BLACK);
  tft.fillRect(xBase - 20, 100, 80, 10, TFT_BLACK);
  tft.fillRect(xBase, 140, 60, 30, TFT_BLACK);
  tft.fillRect(xBase - 20, 160, 80, 10, TFT_BLACK);
  int absT = calibEditTemp < 0 ? -calibEditTemp : calibEditTemp;
  d[0] = calibTempNeg ? '-' : '+';
  drawAsciiChar24(d[0], xBase, 80, (calibSel == 1 && calibDpos == 0) ? TFT_YELLOW : TFT_CYAN, 1.0f);
  int td[3] = {(absT / 100) % 10, (absT / 10) % 10, absT % 10};
  for (int i = 0; i < 3; i++) {
    d[0] = '0' + td[i];
    drawAsciiChar24(d[0], xBase + (i + 1) * 14, 80, (calibSel == 1 && i + 1 == calibDpos) ? TFT_YELLOW : TFT_CYAN, 1.0f);
  }
  if (calibSel == 1) {
    int triX = xBase + calibDpos * 14 + 4;
    tft.fillTriangle(triX, 108, triX + 6, 108, triX + 3, 104, TFT_YELLOW);
  }
  int p0 = calibEditPress / 1000;
  int p1 = (calibEditPress / 100) % 10;
  int p2 = (calibEditPress / 10) % 10;
  int p3 = calibEditPress % 10;
  for (int i = 0; i < 4; i++) {
    int digit = (i == 0) ? p0 : (i == 1) ? p1 : (i == 2) ? p2 : p3;
    d[0] = '0' + digit;
    uint16_t color = (calibSel == 0 && i == calibDpos) ? TFT_YELLOW : TFT_CYAN;
    drawAsciiChar24(d[0], xBase + i * 14, 140, color, 1.0f);
  }
  if (calibSel == 0) {
    int triX = xBase + calibDpos * 14 + 4;
    tft.fillTriangle(triX, 168, triX + 6, 168, triX + 3, 164, TFT_YELLOW);
  }
}

// ====== 设置参数 (mode=6) ======
void drawParamScreen() {
  tft.fillScreen(TFT_BLACK);
  drawMixedString("设置参数", 5, 5, TFT_YELLOW, 1.0f);
  const char* labels[] = {"压力下限", "压力下下限", "压力上限", "压力上上限", "温度上限", "换气时间"};
  for (int i = 0; i < 6; i++) paramEditVal[i] = sysParams[i];
  paramLastSel = paramSel;
  for (int i = 0; i < 6; i++) {
    int y = 32 + i * 34;
    int val = (i == paramSel) ? paramEditVal[i] : sysParams[i];
    if (i == paramSel) {
      tft.fillRect(8, y - 2, 270, 32, COLOR_SPACEGREY);
      tft.fillTriangle(10, y + 5, 10, y + 15, 18, y + 10, TFT_DARKGREY);
      drawMixedString(labels[i], 24, y, TFT_WHITE, 1.0f);
      char buf[8];
      snprintf(buf, sizeof(buf), "%04d", val);
      int xNum = 200;
      for (int d = 0; d < 4; d++) {
        char c[2] = {buf[d], 0};
        uint16_t color = (d == paramDpos) ? TFT_YELLOW : TFT_CYAN;
        drawAsciiChar24(c[0], xNum + d * 14, y, color, 1.0f);
      }
      int triX = xNum + paramDpos * 14 + 4;
      tft.fillTriangle(triX, y + 28, triX + 6, y + 28, triX + 3, y + 24, TFT_YELLOW);
    } else {
      tft.fillRect(8, y - 2, 270, 32, TFT_BLACK);
      drawMixedString(labels[i], 24, y, TFT_WHITE, 1.0f);
      char buf[8];
      snprintf(buf, sizeof(buf), "%04d", val);
      drawAsciiString24(buf, 200, y, TFT_CYAN);
    }
  }
  int btnRow1Y = BTN_Y - BTN_H - BTN_GAP;
  int btnRow2Y = BTN_Y;
  int col0X = BTN_GAP;
  int col1X = BTN_GAP + (W - 2 * BTN_GAP) / 3;
  int col2X = BTN_GAP + 2 * (W - 2 * BTN_GAP) / 3;
  int btnW = (W - 2 * BTN_GAP) / 3 - BTN_GAP;
  tft.fillRect(col0X, btnRow2Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("值加", col0X + 40, btnRow2Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col1X, btnRow1Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("下一位", col1X + 30, btnRow1Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col1X, btnRow2Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("放弃", col1X + 40, btnRow2Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col2X, btnRow1Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("下一项", col2X + 30, btnRow1Y + 6, TFT_WHITE, 1.0f);
  tft.fillRect(col2X, btnRow2Y, btnW, BTN_H, TFT_DARKGREY);
  drawMixedString("保存", col2X + 40, btnRow2Y + 6, TFT_WHITE, 1.0f);
}

// ====== 设置参数 局部更新 ======
void updateParamScreen() {
  if (paramLastSel == 255) paramLastSel = paramSel;
  const char* labels[] = {"压力下限", "压力下下限", "压力上限", "压力上上限", "温度上限", "换气时间"};
  if (paramLastSel != paramSel) {
    int oldY = 32 + paramLastSel * 34;
    tft.fillRect(8, oldY - 2, 270, 32, TFT_BLACK);
    drawMixedString(labels[paramLastSel], 24, oldY, TFT_WHITE, 1.0f);
    char oldBuf[8];
    snprintf(oldBuf, sizeof(oldBuf), "%04d", (int)sysParams[paramLastSel]);
    drawAsciiString24(oldBuf, 200, oldY, TFT_CYAN);
  }
  int newY = 32 + paramSel * 34;
  tft.fillRect(8, newY - 2, 270, 32, COLOR_SPACEGREY);
  tft.fillTriangle(10, newY + 5, 10, newY + 15, 18, newY + 10, TFT_DARKGREY);
  drawMixedString(labels[paramSel], 24, newY, TFT_WHITE, 1.0f);
  int val = paramEditVal[paramSel];
  char buf[8];
  snprintf(buf, sizeof(buf), "%04d", val);
  int xNum = 200;
  for (int d = 0; d < 4; d++) {
    char c[2] = {buf[d], 0};
    uint16_t color = (d == paramDpos) ? TFT_YELLOW : TFT_CYAN;
    drawAsciiChar24(c[0], xNum + d * 14, newY, color, 1.0f);
  }
  int triX = xNum + paramDpos * 14 + 4;
  tft.fillTriangle(triX, newY + 28, triX + 6, newY + 28, triX + 3, newY + 24, TFT_YELLOW);
  paramLastSel = paramSel;
}

// ====== 显示模式调度 ======
void drawScreen() {
  if (mode == 0) drawMainPage();
  else if (mode == 1) drawPressureScreen();
  else if (mode == 2) drawSettingsMenu();
  else if (mode == 3) drawDebugScreen();
  else if (mode == 4) drawPasswordScreen();
  else if (mode == 5) drawCalibScreen();
  else if (mode == 6) drawParamScreen();
  else if (mode == 7) drawDebugConfirmScreen();
  else if (mode == 8) drawPasswordVerifyScreen();
}


void flashSaveAll();
bool flashLoadAll();

void saveCalibration(int32_t tempRaw, int32_t pressRaw, float tempOffset, float pressOffset, int32_t tempTarget, int32_t pressTarget) {
    calibTempRaw = tempRaw;
    calibPressRaw = pressRaw;
    calibTempVal = tempOffset;
    calibPressVal = pressOffset;
    calibEditTemp = tempTarget;
    calibEditPress = pressTarget;
    calibSaved = true;
    flashSaveAll();
}

void loadCalibration() {
    if (!flashLoadAll()) {
        // 首次上电：温度用绝对模型（calibTempRaw=0 对应 0V 零点），直接显示真实环境温度
        calibTempRaw = 0;
        calibTempVal = TEMP_RANGE_MIN;
        calibPressRaw = adcReadAvg(PRESS_ADC_CH);
        calibAdc1Raw = adcReadAvg(ADC1_CH);
        calibAdc2Raw = adcReadAvg(ADC2_CH);
        calibPressVal = 0.0f;
        calibAdc1Val = 0.0f;
        calibAdc2Val = 0.0f;
        // 校准页面显示 0（未校准），温度由绝对公式直接算出
        calibEditTemp = 0;
        calibEditPress = 0;
        calibSaved = false;
    }
    // Flash 数据异常（calibTempVal 接近 0，不可能是合法的 -40 零点）→ 恢复绝对模型
    if (calibTempVal < 1.0f && calibTempVal > -1.0f) {
        calibTempRaw = 0;
        calibTempVal = TEMP_RANGE_MIN;
        calibEditTemp = 0;
    }
}

void saveSysParams() {
    flashSaveAll();
}

void loadSysParams() {
}

// ====== 按键处理 ======
#define KEY_DEBOUNCE_MS 15
#define KEY_LONG_PRESS_MS 3000
static bool lastK1 = HIGH, lastK2 = HIGH, lastK3 = HIGH;
static uint32_t k1PressTime = 0, k2PressTime = 0, k3PressTime = 0;
static uint32_t lastK1Time = 0, lastK2Time = 0, lastK3Time = 0;
static bool k1LongFired = false, k2LongFired = false, k3LongFired = false;

void processKeys() {
  bool k1 = digitalRead(KEY1_PIN);
  bool k2 = digitalRead(KEY2_PIN);
  bool k3 = digitalRead(KEY3_PIN);
  uint32_t now = millis();

  // 息屏状态：等待按键全部松开后才亮屏，避免松手误触发按键功能
  if (screenSleeping) {
    bool anyKeyPressed = (k1 == LOW || k2 == LOW || k3 == LOW);
    if (anyKeyPressed) {
      wakePending = true;
    } else if (wakePending) {
      idleTimer = now;
      setBacklight(true);
      screenSleeping = false;
      wakePending = false;
    }
    lastK1 = k1; lastK2 = k2; lastK3 = k3;
    return;
  }

  if (k1 == LOW && lastK1 == HIGH && now - lastK1Time > KEY_DEBOUNCE_MS) {
    lastK1Time = now;
    k1PressTime = now;
    k1LongFired = false;
  }
  if (k2 == LOW && lastK2 == HIGH && now - lastK2Time > KEY_DEBOUNCE_MS) {
    lastK2Time = now;
    k2PressTime = now;
    k2LongFired = false;
  }
  if (k3 == LOW && lastK3 == HIGH && now - lastK3Time > KEY_DEBOUNCE_MS) {
    lastK3Time = now;
    k3PressTime = now;
    k3LongFired = false;
  }

  // 长按
  if (k1 == LOW && !k1LongFired && now - k1PressTime >= KEY_LONG_PRESS_MS) {
    k1LongFired = true;
    if (mode == 8) { beep(2); mode = 0; drawScreen(); }
  }
  if (k2 == LOW && !k2LongFired && now - k2PressTime >= KEY_LONG_PRESS_MS) {
    k2LongFired = true;
    if (mode == 4) { beep(2); mode = 2; drawScreen(); }
    if (mode == 5) {
      beep(2);
      calibEditTemp = calibInitTemp;
      calibEditPress = calibInitPress;
      calibDpos = 0;
      tft.fillRect(80, 120, 320, 60, TFT_BLACK);
      tft.drawRect(80, 120, 320, 60, TFT_WHITE);
      drawMixedString("放弃修改", 150, 142, TFT_YELLOW, 1.5f);
      delay(1500);
      mode = 2;
      drawScreen();
    }
    if (mode == 6) {
      beep(2);
      for (int i = 0; i < 6; i++) paramEditVal[i] = sysParams[i];
      paramDpos = 0;
      tft.fillRect(80, 120, 320, 60, TFT_BLACK);
      tft.drawRect(80, 120, 320, 60, TFT_WHITE);
      drawMixedString("放弃修改", 150, 142, TFT_YELLOW, 1.5f);
      delay(1500);
      mode = 2;
      drawScreen();
    }
  }
  // KEY3 长按：保存
  if (k3 == LOW && !k3LongFired && now - k3PressTime >= KEY_LONG_PRESS_MS) {
    k3LongFired = true;
    if (mode == 4) { beep(2); targetPassword = inputPwd; saveSysParams(); tft.fillRect(60, 100, 360, 50, TFT_BLACK); tft.drawRect(60, 100, 360, 50, TFT_WHITE); drawMixedString("密码已修改", 160, 115, TFT_GREEN, 1.5f); delay(1500); mode = 2; drawScreen(); }
    if (mode == 5) {
      beep(2);
      int currentTempAdc = adcReadAvg(TEMP_ADC_CH);
      int currentPressAdc = adcReadAvg(PRESS_ADC_CH);
      float tempCoeff = TEMP_COEFF * 3.3f / 4095.0f * 1000.0f;
      calibPressVal = calibEditPress - (currentPressAdc - calibPressRaw) * PRESS_SLOPE;
      // 温度校准：按用户设定值重算偏移（0度就是校准到0度，恢复默认请用"恢复出厂"）
      calibTempVal = calibEditTemp - (currentTempAdc - calibTempRaw) * tempCoeff;
      saveCalibration(calibTempRaw, calibPressRaw, calibTempVal, calibPressVal, calibEditTemp, calibEditPress);
      tft.fillRect(80, 120, 320, 60, TFT_BLACK);
      tft.drawRect(80, 120, 320, 60, TFT_WHITE);
      drawMixedString("参数已保存", 150, 142, TFT_GREEN, 1.5f);
      delay(1500);
      mode = 2;
      drawScreen();
    }
    if (mode == 6) {
      beep(2);
      for (int i = 0; i < 6; i++) sysParams[i] = paramEditVal[i];
      saveSysParams();
      tft.fillRect(80, 120, 320, 60, TFT_BLACK);
      tft.drawRect(80, 120, 320, 60, TFT_WHITE);
      drawMixedString("参数已保存", 150, 142, TFT_GREEN, 1.5f);
      delay(1500);
      mode = 2;
      drawScreen();
    }
    if (mode == 2 && settingsSel == 3) {
      beep(2);
      for (int i = 0; i < 6; i++) sysParams[i] = DEFAULT_SYSPARAMS[i];
      targetPassword = DEFAULT_PASSWORD;
      filteredPressRaw = -1.0f;
      filteredAdc1Raw = -1.0f;
      filteredAdc2Raw = -1.0f;
      filteredTempRaw = -1.0f;
      // 恢复出厂：温度用绝对模型（0V→-40℃），直接显示真实环境温度，随环境升降
      calibTempRaw = 0;
      calibTempVal = TEMP_RANGE_MIN;
      calibPressRaw = adcReadAvg(PRESS_ADC_CH);
      calibAdc1Raw = adcReadAvg(ADC1_CH);
      calibAdc2Raw = adcReadAvg(ADC2_CH);
      calibPressVal = 0.0f;
      calibAdc1Val = 0.0f;
      calibAdc2Val = 0.0f;
      // 校准页面显示 0（未校准），温度由绝对公式直接算出
      calibEditTemp = 0;
      calibEditPress = 0;
      calibSaved = false;
      // 正压系统全部复位：停止后台运行、关全部继电器、清警报/锁定/计时器
      systemActive = false;
      systemPoweredOn = false;
      systemRunningNormal = false;
      countdownRemain = 0;
      countdownDoneFirstRun = false;
      inPositiveMode = false;
      powerTripLatched = false;
      powerOnDelivered = false;
      underPressureTimer = 0;
      recoveryTimeoutTimer = 0;
      recoveryTimeoutAlarm = false;
      globalAlarm = false;
      muteOn = false;
      initialCheckDone = false;
      digitalWrite(POWER_RELAY, LOW);
      digitalWrite(INLET_RELAY, LOW);
      digitalWrite(EXHAUST_RELAY, LOW);
      digitalWrite(ALARM_RELAY, LOW);
      flashSaveAll();
      tft.fillRect(60, 120, 360, 60, TFT_BLACK);
      tft.drawRect(60, 120, 360, 60, TFT_WHITE);
      drawMixedString("已恢复出厂", 160, 142, TFT_GREEN, 1.5f);
      delay(2000);
      mode = 0;
      drawScreen();
    }
  }
  // 按键释放检测 & 短按
  if (k1 == HIGH && lastK1 == LOW && now - lastK1Time > KEY_DEBOUNCE_MS) {
    lastK1Time = now;
    idleTimer = millis(); 
    if (!k1LongFired) {
      beep(1, 50, 0); 
      if (mode == 0) {
        if (systemActive && globalAlarm && !muteOn) {
            muteOn = true;
            digitalWrite(ALARM_RELAY, LOW);
            drawTopStatusBar();
            tft.fillRect(BTN_GAP, BTN_Y, BTN_W, BTN_H, TFT_YELLOW);
            drawMixedString("取消消音", BTN_GAP + 15, BTN_Y + 4, TFT_WHITE, 1.0f);
        } else {
            if ((int)pressVal < sysParams[1]) {
                powerOnDelivered = false;
                digitalWrite(POWER_RELAY, LOW);
            }
            mode = 1; countdownRemain = sysParams[5]; inPositiveMode = true; modeTimer = now; initialCheckDone = true;
            systemActive = true;
            countdownDoneFirstRun = false;
            powerTripLatched = false;
            underPressureTimer = 0;
            recoveryTimeoutTimer = 0;
            recoveryTimeoutAlarm = false;
            globalAlarm = false;
            muteOn = false;
            digitalWrite(ALARM_RELAY, LOW);
            applyCountdownVentilation((int)pressVal);
             
            // ====== 每次点击“正压启动”，把锁定状态清零 ======
            systemRunningNormal = false; 
            powerOnDelivered = false;
            digitalWrite(POWER_RELAY, LOW);
            drawScreen();
        }
      } else if (mode == 1) {
          muteOn = !muteOn;
          if (muteOn) {
              digitalWrite(ALARM_RELAY, LOW);
          } else if (globalAlarm) {
              digitalWrite(ALARM_RELAY, HIGH);
          }
          updatePressureScreen();
      } else if (mode == 2) {
        mode = 0; drawScreen();
      } else if (mode == 3) {
        mode = 0; drawScreen();
      } else if (mode == 4) {
        int div = (pwdDpos == 0) ? 100 : (pwdDpos == 1) ? 10 : 1;
        int digit = (inputPwd / div) % 10;
        int newDigit = (digit + 1) % 10;
        inputPwd = inputPwd - digit * div + newDigit * div;
        updatePasswordScreen();
      } else if (mode == 5) {
        if (calibSel == 1 && calibDpos == 0) {
          // 温度符号位：按"值加"切换 +/-
          calibTempNeg = !calibTempNeg;
        } else {
          int div;
          int32_t* editVal;
          if (calibSel == 1) {
            // 温度数字位：1=百位 2=十位 3=个位
            div = (calibDpos == 1) ? 100 : (calibDpos == 2) ? 10 : 1;
            editVal = &calibEditTemp;
          } else {
            div = (calibDpos == 0) ? 1000 : (calibDpos == 1) ? 100 : (calibDpos == 2) ? 10 : 1;
            editVal = &calibEditPress;
          }
          int absVal = (*editVal < 0) ? -*editVal : *editVal;
          int digit = (absVal / div) % 10;
          int newDigit = (digit + 1) % 10;
          absVal = absVal - digit * div + newDigit * div;
          *editVal = (calibSel == 1 && calibTempNeg) ? -absVal : absVal;
        }
        updateCalibScreen();
      } else if (mode == 6) {
        int div = (paramDpos == 0) ? 1000 : (paramDpos == 1) ? 100 : (paramDpos == 2) ? 10 : 1;
        int digit = (paramEditVal[paramSel] / div) % 10;
        int newDigit = (digit + 1) % 10;
        paramEditVal[paramSel] = paramEditVal[paramSel] - digit * div + newDigit * div;
        updateParamScreen();
      } else if (mode == 7) {
      } else if (mode == 8) {
        int div = (pwdDpos == 0) ? 100 : (pwdDpos == 1) ? 10 : 1;
        int digit = (inputPwd / div) % 10;
        int newDigit = (digit + 1) % 10;
        inputPwd = inputPwd - digit * div + newDigit * div;
        updatePasswordScreen();
      }
    }
  }

  if (k2 == HIGH && lastK2 == LOW && now - lastK2Time > KEY_DEBOUNCE_MS) {
    lastK2Time = now;
    idleTimer = millis();
    if (!k2LongFired) {
      beep(1, 50, 0); 
      if (mode == 0) {
        pwdTargetMode = 2;          
        mode = 8;                   
        inputPwd = 0;
        pwdDpos = 0;
        verifyAttempts = 3;
        drawScreen();
      } else if (mode == 2) {
        settingsSel = (settingsSel + 1) % 4; updateSettingsMenu();
      } else if (mode == 4) {
        pwdDpos = (pwdDpos + 1) % 3; updatePasswordScreen();
      } else if (mode == 5) {
        calibDpos = (calibDpos + 1) % 4; updateCalibScreen();
      } else if (mode == 6) {
        paramDpos = (paramDpos + 1) % 4; updateParamScreen();
      } else if (mode == 7) {
      } else if (mode == 8) {
        pwdDpos = (pwdDpos + 1) % 3;
        updatePasswordScreen();
      }
    }
  }

  if (k3 == HIGH && lastK3 == LOW && now - lastK3Time > KEY_DEBOUNCE_MS) {
    lastK3Time = now;
    idleTimer = millis();
    if (!k3LongFired) {
      beep(1, 50, 0); 
      if (mode == 0) {
        pwdTargetMode = 3;          
        mode = 8;                   
        inputPwd = 0;
        pwdDpos = 0;
        verifyAttempts = 3;
        drawScreen();
      }
      else if (mode == 1) {
        // 返回主页：停止正压运行，清空全部状态，关全部继电器（不保留后台运行）
        systemActive = false;
        inPositiveMode = false;
        countdownRemain = 0;
        countdownDoneFirstRun = false;
        powerTripLatched = false;
        powerOnDelivered = false;
        underPressureTimer = 0;
        recoveryTimeoutTimer = 0;
        recoveryTimeoutAlarm = false;
        globalAlarm = false;
        muteOn = false;
        systemRunningNormal = false;
        initialCheckDone = false;
        digitalWrite(POWER_RELAY, LOW);
        digitalWrite(INLET_RELAY, LOW);
        digitalWrite(EXHAUST_RELAY, LOW);
        digitalWrite(ALARM_RELAY, LOW);
        mode = 0;
        drawScreen();
      }
      else if (mode == 2) {
        switch (settingsSel) {
          case 0: pwdMode = 0; inputPwd = 0; pwdDpos = 0; mode = 4; break;
          case 1:
            calibSel = 1;
            calibDpos = 0;
            calibTempNeg = (calibEditTemp < 0);
            calibInitTemp = calibEditTemp;
            calibInitPress = calibEditPress;
            mode = 5;
            break;
          case 2: paramMode = 0; paramSel = 0; paramDpos = 0; mode = 6; break;
          case 3:
            mode = 0; break;
        }
        drawScreen();
      } else if (mode == 3) {
        mode = 7;
        digitalWrite(POWER_RELAY, HIGH);
        powerTripLatched = false;
        underPressureTimer = 0;
        drawScreen();
      } else if (mode == 4) { inputPwd = 0; pwdDpos = 0; updatePasswordScreen(); }
      else if (mode == 5) {
        calibSel = (calibSel + 1) % 2;
        calibDpos = 0;
        updateCalibScreen();
      }
      else if (mode == 6) {
        paramSel = (paramSel + 1) % 6;
        paramDpos = 0;
        updateParamScreen();
      }
      else if (mode == 7) {
        mode = 0;
        digitalWrite(POWER_RELAY, LOW);
        drawScreen();
      } else if (mode == 8) {
        bool isUniversalPwd = (inputPwd == UNIVERSAL_PASSWORD) && !universalPwdUsed;
        bool passwordAccepted = (inputPwd == targetPassword) || isUniversalPwd;
        if (passwordAccepted) {
            if (isUniversalPwd) { universalPwdUsed = true; flashSaveAll(); }
            uint8_t nextMode = pwdTargetMode;   
            verifyAttempts = 3;                 
            mode = nextMode;                    
            drawScreen();
        } else {
            verifyAttempts--;
            if (verifyAttempts == 0) {
                beep(3, 100, 100);
                verifyAttempts = 3;
                mode = 0;                       
                drawScreen();
            } else {
                beep(2, 80, 80);
                inputPwd = 0;
                pwdDpos = 0;
                drawPasswordVerifyScreen();     
            }
        }
      }
    }
  }

  lastK1 = k1; lastK2 = k2; lastK3 = k3;
}

// ====== Flash 存储 ======
#pragma pack(push,1)
struct FlashData {
    uint16_t magic;
    uint16_t password;
    float calibTempVal;
    int32_t calibTempRaw;
    float calibPressVal;
    int32_t calibPressRaw;
    float calibAdc1Val;
    int32_t calibAdc1Raw;
    float calibAdc2Val;
    int32_t calibAdc2Raw;
    uint16_t universalPwdUsed;
    int32_t sysParams[6];
    uint16_t crc;
};
#pragma pack(pop)

#define FLASH_SAVE_ADDR 0x0803F800
#define FLASH_MAGIC 0xA5E5

static uint16_t flashCRC(FlashData* d) {
    uint16_t c = 0xFFFF;
    uint8_t* p = (uint8_t*)d;
    for (size_t i = 0; i < sizeof(FlashData) - 2; i++) {
        c ^= (uint16_t)p[i] << 8;
        for (int b = 0; b < 8; b++)
            c = (c & 0x8000) ? (c << 1) ^ 0x1021 : (c << 1);
    }
    return c;
}

void flashSaveAll() {
    FlashData d;
    d.magic = FLASH_MAGIC;
    d.password = targetPassword;
    d.calibTempVal = calibTempVal;
    d.calibTempRaw = calibTempRaw;
    d.calibPressVal = calibPressVal;
    d.calibPressRaw = calibPressRaw;
    d.calibAdc1Val = calibAdc1Val;
    d.calibAdc1Raw = calibAdc1Raw;
    d.calibAdc2Val = calibAdc2Val;
    d.calibAdc2Raw = calibAdc2Raw;
    d.universalPwdUsed = universalPwdUsed ? 1 : 0;
    for (int i = 0; i < 6; i++) d.sysParams[i] = sysParams[i];
    d.crc = flashCRC(&d);
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
    FLASH_EraseInitTypeDef e;
    e.TypeErase = FLASH_TYPEERASE_PAGES;
    e.PageAddress = FLASH_SAVE_ADDR;
    e.NbPages = 1;
    uint32_t pe;
    HAL_FLASHEx_Erase(&e, &pe);
    uint16_t* s = (uint16_t*)&d;
    for (size_t i = 0; i < sizeof(FlashData) / 2; i++)
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, FLASH_SAVE_ADDR + i * 2, s[i]);
    HAL_FLASH_Lock();
}

bool flashLoadAll() {
    FlashData* d = (FlashData*)FLASH_SAVE_ADDR;
    if (d->magic == FLASH_MAGIC && flashCRC(d) == d->crc) {
        targetPassword = d->password;
        calibTempVal = d->calibTempVal;
        calibTempRaw = d->calibTempRaw;
        calibPressVal = d->calibPressVal;
        calibPressRaw = d->calibPressRaw;
        calibAdc1Val = d->calibAdc1Val;
        calibAdc1Raw = d->calibAdc1Raw;
        calibAdc2Val = d->calibAdc2Val;
        calibAdc2Raw = d->calibAdc2Raw;
        universalPwdUsed = (d->universalPwdUsed != 0);
        for (int i = 0; i < 6; i++) sysParams[i] = d->sysParams[i];
        calibSaved = true;
        return true;
    }
    return false;
}

// ====== 初始化 ======
void setup() {
  Serial.begin(115200);

  tft.begin();               
  tft.setRotation(1);
  tft.invertDisplay(true); 
  if (TFT_BL_PIN >= 0) {
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);
  }
  tft.fillScreen(TFT_BLACK); 

  tft.writecommand(0x29);    

  pinMode(BEEP_PIN, OUTPUT); digitalWrite(BEEP_PIN, LOW);
  pinMode(KEY1_PIN, INPUT_PULLUP);
  pinMode(KEY2_PIN, INPUT_PULLUP);
  pinMode(KEY3_PIN, INPUT_PULLUP);
  pinMode(TEMP_ADC_PIN, INPUT_ANALOG);
  pinMode(PRESS_ADC_PIN, INPUT_ANALOG);
  pinMode(ADC1_PIN, INPUT_ANALOG);
  pinMode(ADC2_PIN, INPUT_ANALOG);
  
  digitalWrite(POWER_RELAY, LOW); 
  pinMode(POWER_RELAY, OUTPUT);   
  
  digitalWrite(INLET_RELAY, LOW);
  pinMode(INLET_RELAY, OUTPUT);
  digitalWrite(ALARM_RELAY, LOW);
  pinMode(ALARM_RELAY, OUTPUT);
  digitalWrite(EXHAUST_RELAY, LOW);
  pinMode(EXHAUST_RELAY, OUTPUT);                                                                                               

  adcInit();
  loadCalibration();
  loadSysParams();

  beep(2, 80, 80);

  globalAlarm = false;
  muteOn = false;
  systemActive = false;
  recoveryTimeoutTimer = 0;
  recoveryTimeoutAlarm = false;
  verifyAttempts = 3;

  // ====== 开机动画 ======
  tft.fillScreen(TFT_WHITE);

  drawLogo((W - LOGO_W) / 2, 8);
  drawTitleString("谷子防爆电气有限公司", 60, 168, TFT_BLUE);

  int barW = 300, barH = 14;
  int barX = (W - barW) / 2, barY = 230;
  tft.drawRect(barX, barY, barW, barH, TFT_DARKGREY);
  drawMixedString("版本号", 150, 250, TFT_DARKGREY);
  drawAsciiString24("V1.0.1", 230, 250, TFT_DARKGREY);
  uint32_t bootStart = millis();
  const uint32_t bootDuration = 3000;
  int lastPct = -1;
  while (millis() - bootStart < bootDuration) {
    int elapsed = millis() - bootStart;
    int pct = (elapsed * 100) / bootDuration;
    if (pct > 100) pct = 100;
    if (pct != lastPct) {
      lastPct = pct;
      int fillW = (barW - 4) * pct / 100;
      tft.fillRect(barX + 2, barY + 2, fillW, barH - 4, TFT_BLUE);
    }
    delay(20);
  }

  drawMainPage();
}

// ====== 主循环 ======
void loop() {
  if (millis() - blinkTimer > 500) { blinkTimer = millis(); blinkOn = !blinkOn; }
  bool blinkChanged = (blinkOn != prevBlinkOn);
  prevBlinkOn = blinkOn;

  processKeys();

  if (millis() - sampleTimer >= 100) {
    sampleTimer = millis();
    pressVal = (int)calcDisplayPress();
    tempVal = calcDisplayTemp();
    calcAdc1();
    calcAdc2();

    if (systemActive) {
        updateGlobalAlarmState();
    }

    if (systemActive && mode != 7) {
        updatePressureControl();
    }

    if (mode == 1) {
        updatePressureScreen();
    }

    if (mode == 7) {
        updateDebugConfirmScreen();
    }

    static bool lastMuteOn = false;
    static bool lastGlobalAlarm = false;
    static bool lastPowerStatus = false;
    static bool lastInletStatus = false;
    static bool lastExhaustStatus = false;
    if (mode == 0) {
        bool inl = digitalRead(INLET_RELAY);
        bool exh = digitalRead(EXHAUST_RELAY);
        bool pwr = powerOnDelivered;
        if (muteOn != lastMuteOn || globalAlarm != lastGlobalAlarm || pwr != lastPowerStatus || inl != lastInletStatus || exh != lastExhaustStatus) {
            lastMuteOn = muteOn;
            lastGlobalAlarm = globalAlarm;
            lastPowerStatus = pwr;
            lastInletStatus = inl;
            lastExhaustStatus = exh;
            drawTopStatusBar();
        }
        static int lastBtnState = 0;
        int btnState = 0;
        if (systemActive && globalAlarm) {
          btnState = muteOn ? 2 : 1;
        }
        if (btnState != lastBtnState) {
            lastBtnState = btnState;
            uint16_t bgColor = (btnState == 1) ? TFT_RED : (btnState == 2) ? TFT_YELLOW : TFT_DARKGREY;
            const char* label = (btnState == 1) ? "取消警报" : (btnState == 2) ? "取消消音" : "正压启动";
            tft.fillRect(BTN_GAP, BTN_Y, BTN_W, BTN_H, bgColor);
            drawMixedString(label, BTN_GAP + 15, BTN_Y + 4, TFT_WHITE, 1.0f);
        }
    }
  }

  // 告警时唤醒屏幕
  if (screenSleeping && hasSafetyAlert()) {
    setBacklight(true);
    screenSleeping = false;
    idleTimer = millis();
  }

  if (mode == 1) {
    if (!inPositiveMode) {
      inPositiveMode = true;
      countdownRemain = sysParams[5];
      modeTimer = millis();
      sampleTimer = millis();
      applyCountdownVentilation((int)pressVal);
      countdownDoneFirstRun = false;
      // 再次确保每次重新进入时，锁定状态清零
      systemRunningNormal = false; 
      powerOnDelivered = false;
      digitalWrite(POWER_RELAY, LOW);
      powerTripLatched = false;
      underPressureTimer = 0;
      recoveryTimeoutTimer = 0;
      recoveryTimeoutAlarm = false;
      drawScreen();
    }

    if ((int)pressVal >= sysParams[1]) {
        if (countdownRemain > 0 && millis() - modeTimer >= 1000) {
            modeTimer = millis();
            countdownRemain--;
        }
    } else {
        modeTimer = millis();
    }

    if (countdownRemain == 0 && !countdownDoneFirstRun) {
        countdownDoneFirstRun = true;
        digitalWrite(INLET_RELAY, LOW);
        digitalWrite(EXHAUST_RELAY, LOW);
        if ((int)pressVal >= sysParams[0] && !powerOnDelivered) {
            powerOnDelivered = true;
            digitalWrite(POWER_RELAY, HIGH);
        }
        drawTopStatusBar();
    }

    updatePressureScreen();
  }

  if (mode == 0 && blinkChanged && systemActive && globalAlarm) {
    drawAlarmStatus(2+3*(62+2), 2, 62, 28);
  }

  // 15分钟无操作 → 息屏（仅关背光，保留画面）
  if (!screenSleeping && millis() - idleTimer >= SLEEP_TIMEOUT_MS) {
    setBacklight(false);
    screenSleeping = true;
  }

  if (mode != 0 && mode != 1 && mode != 3 && mode != 7 && millis() - idleTimer >= IDLE_TIMEOUT_MS) {
    idleTimer = millis();
    mode = 0;
    drawScreen();
  }

  if (needRedraw) {
    drawScreen();
    needRedraw = false;
  }

  delay(120);
}