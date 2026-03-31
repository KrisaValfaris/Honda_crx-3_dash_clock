#include <U8g2lib.h>
#include <Wire.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "work_anim_frames.h"
#include "alert_trunk_bitmap.h"
#include "alert_door_bitmap.h"
#include "alert_roof_bitmap.h"

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

// OLED 128x64 I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Przyciski
#define BUTTON_SET 8
#define BUTTON_UP 9
#define BUTTON_DOWN 7
#define BUTTON_DEBOUNCE_MS 130

// Wejścia stanu (ASS1/ASS2/ASS3) - ustaw swoje GPIO.
// Jeśli pin = -1, wejście jest wyłączone.
#define ASS1_PIN 6
#define ASS2_PIN 5
#define ASS3_PIN 4

// Tryb wejścia: INPUT / INPUT_PULLUP / INPUT_PULLDOWN (zależnie od podłączenia)
#define ASS_INPUT_MODE INPUT

// Stan wysoki = aktywny (np. otwarte)
#define ASS1_ACTIVE_HIGH 0
#define ASS2_ACTIVE_HIGH 1
#define ASS3_ACTIVE_HIGH 0

// Debug na Serial (1 = wypisuje zmiany stanów ASS1/2/3)
#define ASS_DEBUG 0

#define DS1307_ADDRESS 0x68
#define RTC_MIN_YEAR 2000
#define RTC_MAX_YEAR 2099

enum State {
  DISPLAY_TIME,
  SET_HOURS,
  SET_MINUTES,
  SET_YEAR,
  SET_MONTH,
  SET_DAY
};

volatile bool buttonPressed = false;
volatile uint8_t buttonId = 0;
volatile unsigned long lastButtonSetMs = 0;
volatile unsigned long lastButtonUpMs = 0;
volatile unsigned long lastButtonDownMs = 0;

State currentState = DISPLAY_TIME;

struct tm rtc_time;

bool ass1Active = false;
bool ass2Active = false;
bool ass3Active = false;

unsigned long lastUpdate = 0;
unsigned long lastBlink = 0;
bool blinkState = true;

static uint8_t decToBcd(uint8_t value) {
  return (uint8_t)(((value / 10) << 4) | (value % 10));
}

static uint8_t bcdToDec(uint8_t value) {
  return (uint8_t)(((value >> 4) * 10) + (value & 0x0F));
}

static bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int daysInMonth(int year, int month0) {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month0 == 1) return days[month0] + (isLeapYear(year) ? 1 : 0);
  return days[month0];
}

static void clampDayToMonth(struct tm *t) {
  int year = t->tm_year + 1900;
  int maxDay = daysInMonth(year, t->tm_mon);
  if (t->tm_mday < 1) t->tm_mday = 1;
  if (t->tm_mday > maxDay) t->tm_mday = maxDay;
}

static void clampRtcYear(struct tm *t) {
  int year = t->tm_year + 1900;
  if (year < RTC_MIN_YEAR) {
    t->tm_year = RTC_MAX_YEAR - 1900;
  } else if (year > RTC_MAX_YEAR) {
    t->tm_year = RTC_MIN_YEAR - 1900;
  }
}

static void normalizeRtcTime(struct tm *t) {
  clampRtcYear(t);
  clampDayToMonth(t);
  t->tm_sec = (t->tm_sec < 0 || t->tm_sec > 59) ? 0 : t->tm_sec;
  t->tm_isdst = -1;
  mktime(t);
}

static bool syncSystemClockFromTm(const struct tm *t) {
  struct tm normalized = *t;
  normalizeRtcTime(&normalized);

  time_t unixTime = mktime(&normalized);
  if (unixTime == (time_t)-1) {
    return false;
  }

  struct timeval now;
  now.tv_sec = unixTime;
  now.tv_usec = 0;

  if (settimeofday(&now, NULL) != 0) {
    return false;
  }

  rtc_time = normalized;
  return true;
}

static bool readDs1307(struct tm *t) {
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t bytesRequested = 7;
  uint8_t bytesRead = Wire.requestFrom((uint8_t)DS1307_ADDRESS, bytesRequested);
  if (bytesRead != bytesRequested) {
    return false;
  }

  uint8_t rawSeconds = Wire.read();
  uint8_t rawMinutes = Wire.read();
  uint8_t rawHours = Wire.read();
  Wire.read();
  uint8_t rawDay = Wire.read();
  uint8_t rawMonth = Wire.read();
  uint8_t rawYear = Wire.read();

  if (rawSeconds & 0x80) {
    return false;
  }

  uint8_t seconds = bcdToDec(rawSeconds & 0x7F);
  uint8_t minutes = bcdToDec(rawMinutes & 0x7F);

  uint8_t hours = 0;
  if (rawHours & 0x40) {
    uint8_t hour12 = bcdToDec(rawHours & 0x1F);
    bool isPm = (rawHours & 0x20) != 0;
    if (hour12 == 12) {
      hours = isPm ? 12 : 0;
    } else {
      hours = isPm ? (uint8_t)(hour12 + 12) : hour12;
    }
  } else {
    hours = bcdToDec(rawHours & 0x3F);
  }

  uint8_t day = bcdToDec(rawDay & 0x3F);
  uint8_t month = bcdToDec(rawMonth & 0x1F);
  int year = RTC_MIN_YEAR + bcdToDec(rawYear);

  if (seconds > 59 || minutes > 59 || hours > 23 || month < 1 || month > 12) {
    return false;
  }

  int maxDay = daysInMonth(year, month - 1);
  if (day < 1 || day > maxDay) {
    return false;
  }

  memset(t, 0, sizeof(*t));
  t->tm_year = year - 1900;
  t->tm_mon = month - 1;
  t->tm_mday = day;
  t->tm_hour = hours;
  t->tm_min = minutes;
  t->tm_sec = seconds;
  t->tm_isdst = -1;

  mktime(t);
  return true;
}

static bool writeDs1307(const struct tm *t) {
  struct tm normalized = *t;
  normalizeRtcTime(&normalized);

  int year = normalized.tm_year + 1900;
  if (year < RTC_MIN_YEAR || year > RTC_MAX_YEAR) {
    return false;
  }

  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write((uint8_t)0x00);
  Wire.write(decToBcd((uint8_t)normalized.tm_sec));
  Wire.write(decToBcd((uint8_t)normalized.tm_min));
  Wire.write(decToBcd((uint8_t)normalized.tm_hour));
  Wire.write(decToBcd((uint8_t)(normalized.tm_wday + 1)));
  Wire.write(decToBcd((uint8_t)normalized.tm_mday));
  Wire.write(decToBcd((uint8_t)(normalized.tm_mon + 1)));
  Wire.write(decToBcd((uint8_t)(year - RTC_MIN_YEAR)));

  return Wire.endTransmission() == 0;
}

static void drawBitmapSequence(const unsigned char *const *frames,
                               int frameCount,
                               uint16_t x,
                               uint16_t y,
                               uint16_t w,
                               uint16_t h,
                               uint16_t frameDelayMs,
                               uint32_t *lastFrameMs,
                               uint8_t *frameIndex) {
  if (frameCount <= 0) return;

  if (frameDelayMs == 0) {
    *frameIndex = 0;
  } else {
    uint32_t now = millis();
    if (now - *lastFrameMs >= (uint32_t)frameDelayMs) {
      *lastFrameMs = now;
      *frameIndex = (uint8_t)((*frameIndex + 1) % (uint8_t)frameCount);
    }
  }

  const unsigned char *frame = frames[*frameIndex];
  u8g2.drawXBMP(x, y, w, h, frame);
}

static void drawWorkAnimation(uint16_t x, uint16_t y) {
#if WORK_ANIM_ENABLED
  static uint32_t lastFrameMs = 0;
  static uint8_t frameIndex = 0;

  int frameCount = WORK_ANIM_FRAME_COUNT;
  drawBitmapSequence(WORK_ANIM_FRAMES,
                     frameCount,
                     x,
                     y,
                     WORK_ANIM_W,
                     WORK_ANIM_H,
                     WORK_ANIM_FRAME_DELAY_MS,
                     &lastFrameMs,
                     &frameIndex);
#else
  (void)x;
  (void)y;
#endif
}

static bool readAssPin(int pin, bool activeHigh) {
  if (pin < 0) return false;
  int value = digitalRead(pin);
  return activeHigh ? (value == HIGH) : (value == LOW);
}

static void setupAssPins() {
#if ASS1_PIN >= 0
  pinMode(ASS1_PIN, ASS_INPUT_MODE);
#endif
#if ASS2_PIN >= 0
  pinMode(ASS2_PIN, ASS_INPUT_MODE);
#endif
#if ASS3_PIN >= 0
  pinMode(ASS3_PIN, ASS_INPUT_MODE);
#endif
}

static void updateAssStates() {
  bool newAss1 = readAssPin(ASS1_PIN, ASS1_ACTIVE_HIGH);
  bool newAss2 = readAssPin(ASS2_PIN, ASS2_ACTIVE_HIGH);
  bool newAss3 = readAssPin(ASS3_PIN, ASS3_ACTIVE_HIGH);

#if ASS_DEBUG
  if (newAss1 != ass1Active) Serial.printf("ASS1=%d\n", newAss1 ? 1 : 0);
  if (newAss2 != ass2Active) Serial.printf("ASS2=%d\n", newAss2 ? 1 : 0);
  if (newAss3 != ass3Active) Serial.printf("ASS3=%d\n", newAss3 ? 1 : 0);
#endif

  ass1Active = newAss1;
  ass2Active = newAss2;
  ass3Active = newAss3;
}

static void drawAssBitmaps(bool drawOverClock) {
#if TRUNK_BITMAP_ENABLED
  static uint32_t trunkLastFrameMs = 0;
  static uint8_t trunkFrameIndex = 0;
  if (ass1Active && (TRUNK_BITMAP_DRAW_OVER_CLOCK == (drawOverClock ? 1 : 0))) {
    int frameCount = TRUNK_BITMAP_FRAME_COUNT;
    if (frameCount > 0) {
      drawBitmapSequence(TRUNK_BITMAP_FRAMES,
                         frameCount,
                         TRUNK_BITMAP_X,
                         TRUNK_BITMAP_Y,
                         TRUNK_BITMAP_W,
                         TRUNK_BITMAP_H,
                         TRUNK_BITMAP_FRAME_DELAY_MS,
                         &trunkLastFrameMs,
                         &trunkFrameIndex);
    }
  }
#endif

#if DOOR_BITMAP_ENABLED
  static uint32_t doorLastFrameMs = 0;
  static uint8_t doorFrameIndex = 0;
  if (ass2Active && (DOOR_BITMAP_DRAW_OVER_CLOCK == (drawOverClock ? 1 : 0))) {
    int frameCount = DOOR_BITMAP_FRAME_COUNT;
    if (frameCount > 0) {
      drawBitmapSequence(DOOR_BITMAP_FRAMES,
                         frameCount,
                         DOOR_BITMAP_X,
                         DOOR_BITMAP_Y,
                         DOOR_BITMAP_W,
                         DOOR_BITMAP_H,
                         DOOR_BITMAP_FRAME_DELAY_MS,
                         &doorLastFrameMs,
                         &doorFrameIndex);
    }
  }
#endif

#if ROOF_BITMAP_ENABLED
  static uint32_t roofLastFrameMs = 0;
  static uint8_t roofFrameIndex = 0;
  if (ass3Active && (ROOF_BITMAP_DRAW_OVER_CLOCK == (drawOverClock ? 1 : 0))) {
    int frameCount = ROOF_BITMAP_FRAME_COUNT;
    if (frameCount > 0) {
      drawBitmapSequence(ROOF_BITMAP_FRAMES,
                         frameCount,
                         ROOF_BITMAP_X,
                         ROOF_BITMAP_Y,
                         ROOF_BITMAP_W,
                         ROOF_BITMAP_H,
                         ROOF_BITMAP_FRAME_DELAY_MS,
                         &roofLastFrameMs,
                         &roofFrameIndex);
    }
  }
#endif
}

void IRAM_ATTR buttonISR_SET() {
  unsigned long now = millis();
  if (now - lastButtonSetMs < BUTTON_DEBOUNCE_MS) return;
  lastButtonSetMs = now;
  buttonPressed = true;
  buttonId = 1;
}

void IRAM_ATTR buttonISR_UP() {
  unsigned long now = millis();
  if (now - lastButtonUpMs < BUTTON_DEBOUNCE_MS) return;
  lastButtonUpMs = now;
  buttonPressed = true;
  buttonId = 2;
}

void IRAM_ATTR buttonISR_DOWN() {
  unsigned long now = millis();
  if (now - lastButtonDownMs < BUTTON_DEBOUNCE_MS) return;
  lastButtonDownMs = now;
  buttonPressed = true;
  buttonId = 3;
}

void setup() {

  Serial.begin(115200);

  Wire.begin(1, 2);  // SDA, SCL
  Wire.setClock(100000);

  u8g2.begin();
  u8g2.setFont(u8g2_font_10x20_tf);

  pinMode(BUTTON_SET, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON_SET), buttonISR_SET, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_UP), buttonISR_UP, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_DOWN), buttonISR_DOWN, FALLING);

  setupAssPins();

  bool rtcReadOk = readDs1307(&rtc_time);
  if (rtcReadOk) {
    syncSystemClockFromTm(&rtc_time);
    Serial.println("DS1307: odczyt czasu OK");
  } else {
    rtc_time.tm_year = 2024 - 1900;
    rtc_time.tm_mon = 0;
    rtc_time.tm_mday = 1;
    rtc_time.tm_hour = 12;
    rtc_time.tm_min = 0;
    rtc_time.tm_sec = 0;
    syncSystemClockFromTm(&rtc_time);
    Serial.println("DS1307: brak poprawnego czasu, ustaw domyslny");
  }

  // początkowy czas
  u8g2.clearBuffer();
  u8g2.drawStr(10, 30, rtcReadOk ? "RTC OK" : "RTC SET");
  u8g2.sendBuffer();
  delay(2000);
}

void loop() {

  if (currentState == DISPLAY_TIME && millis() - lastUpdate > 1000) {
    lastUpdate = millis();

    if (!readDs1307(&rtc_time)) {
      time_t now;
      time(&now);
      localtime_r(&now, &rtc_time);
    }
  }

  if (buttonPressed) {
    handleButtonPress(buttonId);
    buttonPressed = false;
  }

  if (currentState != DISPLAY_TIME) {
    if (millis() - lastBlink > 500) {
      lastBlink = millis();
      blinkState = !blinkState;
    }
  } else {
    blinkState = true;
  }

  updateAssStates();
  displayTime(blinkState);

  delay(10);
}

void handleButtonPress(uint8_t button) {

  switch(button) {

    case 1:
      if (currentState == DISPLAY_TIME) {
        if (!readDs1307(&rtc_time)) {
          time_t now;
          time(&now);
          localtime_r(&now, &rtc_time);
        }
        currentState = SET_HOURS;
      } else if (currentState == SET_HOURS) {
        currentState = SET_MINUTES;
      } else if (currentState == SET_MINUTES) {
        currentState = SET_YEAR;
      } else if (currentState == SET_YEAR) {
        currentState = SET_MONTH;
      } else if (currentState == SET_MONTH) {
        currentState = SET_DAY;
      } else {
        normalizeRtcTime(&rtc_time);
        rtc_time.tm_sec = 0;
        syncSystemClockFromTm(&rtc_time);
        if (!writeDs1307(&rtc_time)) {
          Serial.println("DS1307: blad zapisu czasu");
        }
        lastUpdate = 0;
        currentState = DISPLAY_TIME;
      }
      break;

    case 2:

      if (currentState == SET_HOURS)
        rtc_time.tm_hour = (rtc_time.tm_hour + 1) % 24;

      if (currentState == SET_MINUTES)
        rtc_time.tm_min = (rtc_time.tm_min + 1) % 60;

      if (currentState == SET_YEAR) {
        rtc_time.tm_year += 1;
        clampRtcYear(&rtc_time);
        clampDayToMonth(&rtc_time);
      }

      if (currentState == SET_MONTH) {
        rtc_time.tm_mon = (rtc_time.tm_mon + 1) % 12;
        clampDayToMonth(&rtc_time);
      }

      if (currentState == SET_DAY) {
        clampDayToMonth(&rtc_time);
        int year = rtc_time.tm_year + 1900;
        int maxDay = daysInMonth(year, rtc_time.tm_mon);
        rtc_time.tm_mday = (rtc_time.tm_mday % maxDay) + 1;
      }

      break;

    case 3:

      if (currentState == SET_HOURS)
        rtc_time.tm_hour = (rtc_time.tm_hour + 23) % 24;

      if (currentState == SET_MINUTES)
        rtc_time.tm_min = (rtc_time.tm_min + 59) % 60;

      if (currentState == SET_YEAR) {
        rtc_time.tm_year -= 1;
        clampRtcYear(&rtc_time);
        clampDayToMonth(&rtc_time);
      }

      if (currentState == SET_MONTH) {
        rtc_time.tm_mon = (rtc_time.tm_mon + 11) % 12;
        clampDayToMonth(&rtc_time);
      }

      if (currentState == SET_DAY) {
        clampDayToMonth(&rtc_time);
        int year = rtc_time.tm_year + 1900;
        int maxDay = daysInMonth(year, rtc_time.tm_mon);
        rtc_time.tm_mday = (rtc_time.tm_mday <= 1) ? maxDay : (rtc_time.tm_mday - 1);
      }

      break;
  }
}

void displayTime(bool showValue) {

  char timeStr[20];
  char dateStr[20];
  char stateStr[20];

  u8g2.clearBuffer();

  sprintf(timeStr, "%02d:%02d", rtc_time.tm_hour, rtc_time.tm_min);

  sprintf(dateStr,"%04d-%02d-%02d",
  rtc_time.tm_year+1900,
  rtc_time.tm_mon+1,
  rtc_time.tm_mday);

  if (currentState == SET_HOURS && !showValue)
    sprintf(timeStr, "  :%02d", rtc_time.tm_min);

  if (currentState == SET_MINUTES && !showValue)
    sprintf(timeStr, "%02d:  ", rtc_time.tm_hour);

  if (currentState == SET_YEAR && !showValue)
    sprintf(dateStr, "    -%02d-%02d", rtc_time.tm_mon + 1, rtc_time.tm_mday);

  if (currentState == SET_MONTH && !showValue)
    sprintf(dateStr, "%04d-  -%02d", rtc_time.tm_year + 1900, rtc_time.tm_mday);

  if (currentState == SET_DAY && !showValue)
    sprintf(dateStr, "%04d-%02d-  ", rtc_time.tm_year + 1900, rtc_time.tm_mon + 1);

  // Duży czas, wyśrodkowany
  // Animacja/bitmapa jako tło (czas i data zawsze będą nad nią)
  if (!WORK_ANIM_DRAW_OVER_CLOCK) {
    drawWorkAnimation(WORK_ANIM_X, WORK_ANIM_Y);
  }
  drawAssBitmaps(false);

  u8g2.setFont(u8g2_font_logisoso32_tf);
  int16_t timeWidth = u8g2.getStrWidth(timeStr);
  int16_t timeX = (128 - timeWidth) / 2;
  int16_t timeAscent = u8g2.getAscent();
  int16_t timeDescent = u8g2.getDescent();
  int16_t timeY = (64 + timeAscent + timeDescent) / 2;

  u8g2.drawStr(timeX, timeY, timeStr);

  switch(currentState){

    case DISPLAY_TIME:
      stateStr[0] = '\0';
      break;

    case SET_HOURS:
      sprintf(stateStr, "USTAW GODZ.");
      break;

    case SET_MINUTES:
      sprintf(stateStr, "USTAW MIN.");
      break;

    case SET_YEAR:
      sprintf(stateStr, "USTAW ROK");
      break;

    case SET_MONTH:
      sprintf(stateStr, "USTAW MIES.");
      break;

    case SET_DAY:
      sprintf(stateStr, "USTAW DZIEN");
      break;
  }

  // Mały tryb (tylko podczas ustawiania)
  if (stateStr[0] != '\0') {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 10, stateStr);
  }

  // Mała data na samym dole
  u8g2.setFont(u8g2_font_5x8_tf);
  int16_t dateWidth = u8g2.getStrWidth(dateStr);
  int16_t dateX = (128 - dateWidth) / 2;
  int16_t dateDescent = u8g2.getDescent();
  int16_t dateY = (64 - 1) + dateDescent;

  u8g2.drawStr(dateX, dateY, dateStr);

  drawAssBitmaps(true);
  if (WORK_ANIM_DRAW_OVER_CLOCK) {
    drawWorkAnimation(WORK_ANIM_X, WORK_ANIM_Y);
  }

  u8g2.sendBuffer();
}
