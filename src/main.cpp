#include <Arduino.h>

#include <SPI.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "epd13in3b.h"

#include <vector>
#include <map>
#include <time.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include "SD_MMC.h"

#include <LovyanGFX.hpp>

#include "PCEvent.h"

#define SD_MMC_CMD 38
#define SD_MMC_CLK 39
#define SD_MMC_D0 40

#define LED_BUILTIN 2
#define PIN_BUTTON 4

#define VOLTAGE_TEST 8
#define VOLTAGE_READ 9
#define VOLTAGE_ADC_CHANNEL ADC_CHANNEL_8

#define HEADER_HEIGHT 10
#define FOOTER_HEIGHT 20
#define COLUMN_WIDTH 137
#define DAY_HEIGHT 42

#define WHITE 255
#define BLACK 0

#define LARGE_FONT FreeSansBold24pt7b
#define SMALL_FONT efontJA_14
#define SMALL_FONT_HEIGHT 16

#define uS_TO_S_FACTOR 1000000ULL

#define prefName "PaperCal"
#define holidayCacheKey "Holiday"
#define bootCountKey "Boot"

#define EPD_ENABLE true
#define LOG_ENABLE false
#define LOG_VOLTAGE false
#define LOG_HEAP true

String pemFileName = "/root_ca.pem";
std::vector<String> iCalendarURLs;
String iCalendarHolidayURL;
String rootCA = "";
boolean loaded = false;
boolean loginScreen = false;
float timezone = 0;

int currentYear = 0;
int currentMonth = 0;
int nextMonthYear = 0;
int nextMonth = 0;
String dateString = "";

Preferences pref;
String holidayCacheString;
int bootCount;

LGFX_Sprite blackSprite;
LGFX_Sprite redSprite;

void showCalendar();
uint32_t readVoltage();
void logLine(String line);
void shutdown(int wakeUpSeconds);

void setup()
{
  // put your setup code here, to run once:

  blackSprite.setColorDepth(1);
  blackSprite.createSprite(EPD_WIDTH, EPD_HEIGHT);
  blackSprite.setTextWrap(false);

  redSprite.setColorDepth(1);
  redSprite.createSprite(EPD_WIDTH, EPD_HEIGHT);
  redSprite.setTextWrap(false);

  // SD Card
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
  if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5))
  {
    log_printf("Card Mount Failed\n");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE)
  {
    log_printf("No SD_MMC card attached\n");
    return;
  }

  // Load settings from "settings.txt" in SD card
  String wifiIDString = "wifiID";
  String wifiPWString = "wifiPW";

  File settingFile = SD_MMC.open("/settings.txt");
  if (settingFile)
  {
    while (settingFile.available() > 0)
    {
      String line = settingFile.readStringUntil('\n');
      if (line.startsWith("//"))
        continue;
      int separatorLocation = line.indexOf(":");
      if (separatorLocation > -1)
      {
        String key = line.substring(0, separatorLocation);
        String content = line.substring(separatorLocation + 1);

        // WiFi SSID and paassword
        if (key == "SSID")
          wifiIDString = content;

        else if (key == "PASS")
          wifiPWString = content;

        // HTTPS access
        else if (key == "pemFileName")
          pemFileName = content;

        else if (key == "iCalendarURL")
          iCalendarURLs.push_back(content);

        else if (key == "holidayURL")
          iCalendarHolidayURL = content;

        else if (key == "timezone")
          PCEvent::defaultTimezone = content.toFloat();
      }
    }
    settingFile.close();

    // Start Wifi connection
    WiFi.begin(wifiIDString.c_str(), wifiPWString.c_str());
    // Wait until wifi connected
    int i = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      log_printf(".");
      i++;
      if (i > 120)
        break;
    }
    log_printf("\n");

    // Setup NTP
    configTime(60 * 60 * PCEvent::defaultTimezone, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

    // Load PEM file in SD card
    File pemFile = SD_MMC.open(pemFileName.c_str());
    if (pemFile)
    {
      PCEvent::setRootCA(pemFile.readString());
      pemFile.close();
      log_printf("pem file loaded:%s\n", pemFileName.c_str());
    }

    // Load Holidays cache for this month
    pref.begin(prefName, false);
    holidayCacheString = pref.getString(holidayCacheKey, "");
    bootCount = pref.getInt(bootCountKey, 0);
    pref.end();

    // Boot count
    esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
    switch (wakeupCause)
    {
    case ESP_SLEEP_WAKEUP_TIMER:
    {
      bootCount++;
      break;
    }
    default:
    {
      bootCount = 0;
    }
    }

    // Prepare voltage
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(VOLTAGE_TEST, OUTPUT);
    pinMode(VOLTAGE_READ, ANALOG);
  }

  // End SD_MMC if log is not enabled
  if (!LOG_ENABLE)
  {
    SD_MMC.end();
  }

  // light sleep wake up source
  esp_sleep_enable_gpio_wakeup();
  gpio_wakeup_enable(GPIO_NUM_17, GPIO_INTR_LOW_LEVEL); // BUSY_PIN
}

void loop()
{
  // put your main code here, to run repeatedly:
  if (!loaded)
  {
    showCalendar();
    loaded = true;
    delay(1000);
  }
}

void showCalendar()
{
  digitalWrite(LED_BUILTIN, HIGH);
  // Get local time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    log_printf("Waiting getLocalTime\n");
    delay(500);
    return;
  }

  PCEvent::setTimeInfo(timeinfo);
  PCEvent::setHolidayCacheString(holidayCacheString);

  int year = PCEvent::currentYear;
  int month = PCEvent::currentMonth;
  int day = PCEvent::currentDay;

  // Load iCalendar
  for (auto &urlString : iCalendarURLs)
  {
    PCEvent::loadICalendar(urlString, false);
  }
  // Load iCalendar for holidays
  if (!PCEvent::isCacheValid() && !iCalendarHolidayURL.isEmpty())
  {
    PCEvent::loadICalendar(iCalendarHolidayURL, true);
    pref.begin(prefName, false);
    pref.putString(holidayCacheKey, PCEvent::holidayCacheString());
    pref.end();
  }

  WiFi.disconnect(true);

  // Draw calendar
  int firstDayOfWeek = dayOfWeek(year, month, 1);
  int numberOfDays = numberOfDaysInMonth(year, month);
  int numberOfRows = (firstDayOfWeek + numberOfDays - 1) / 7 + 1;
  int rowHeight = (EPD_HEIGHT - (HEADER_HEIGHT + FOOTER_HEIGHT)) / numberOfRows;

  blackSprite.fillScreen(WHITE);
  redSprite.fillScreen(WHITE);

  // draw horizontal lines
  for (int i = 1; i <= numberOfRows; i++)
  {
    blackSprite.drawFastHLine(0, i * rowHeight + HEADER_HEIGHT, EPD_WIDTH, BLACK);
  }

  // draw vertical lines
  int lineHeight = numberOfRows * rowHeight;
  for (int i = 1; i < 7; i++)
  {
    blackSprite.drawFastVLine(i * COLUMN_WIDTH, HEADER_HEIGHT, lineHeight, BLACK);
  }

  // draw days in month
  LGFX_Sprite *selectedSprite = &blackSprite;

  for (int i = 1; i <= numberOfDays; i++)
  {
    int row = (firstDayOfWeek + i - 1) / 7;
    int column = (6 + firstDayOfWeek + i) % 7;
    boolean holiday = (column == 0 || column == 6 || (PCEvent::numberOfHolidaysInDayOfThisMonth(i) > 0)) ? true : false;
    selectedSprite = holiday ? &redSprite : &blackSprite;

    // invert color if it is today
    uint16_t dayColor = BLACK;
    if (day == i)
    {
      selectedSprite->fillRect(column * COLUMN_WIDTH, row * rowHeight + HEADER_HEIGHT, COLUMN_WIDTH, DAY_HEIGHT, BLACK);
      dayColor = WHITE;
    }

    // draw day
    selectedSprite->setFont(&fonts::LARGE_FONT);
    int dayWidth = selectedSprite->textWidth(String(i));
    selectedSprite->setTextColor(dayColor);
    selectedSprite->setCursor(column * COLUMN_WIDTH + (COLUMN_WIDTH - dayWidth) / 2, row * rowHeight + 4 + HEADER_HEIGHT);
    selectedSprite->printf("%d", i);

    // draw events
    std::vector<PCEvent> eventsInToday;
    auto holidaysInDay = PCEvent::holidaysInDayOfThisMonth(i);
    eventsInToday.insert(eventsInToday.end(), holidaysInDay.begin(), holidaysInDay.end());
    auto eventsInDay = PCEvent::eventsInDayOfThisMonth(i);
    eventsInToday.insert(eventsInToday.end(), eventsInDay.begin(), eventsInDay.end());

    blackSprite.setFont(&fonts::SMALL_FONT);
    redSprite.setFont(&fonts::SMALL_FONT);
    blackSprite.setTextColor(BLACK);
    redSprite.setTextColor(BLACK);
    blackSprite.setClipRect(column * COLUMN_WIDTH, row * rowHeight + DAY_HEIGHT + HEADER_HEIGHT, COLUMN_WIDTH, rowHeight - DAY_HEIGHT);
    redSprite.setClipRect(column * COLUMN_WIDTH, row * rowHeight + DAY_HEIGHT + HEADER_HEIGHT, COLUMN_WIDTH, rowHeight - DAY_HEIGHT);
    if (eventsInToday.size() > 0)
    {
      int i = 0;
      for (auto &event : eventsInToday)
      {
        selectedSprite = event.isHolidayEvent ? &redSprite : &blackSprite;
        selectedSprite->setCursor(column * COLUMN_WIDTH + 2, row * rowHeight + DAY_HEIGHT + HEADER_HEIGHT + 4 + SMALL_FONT_HEIGHT * i);
        selectedSprite->print("・" + event.getTitle());
        i++;
        if (i >= 3)
          break;
      }
    }
    blackSprite.clearClipRect();
    redSprite.clearClipRect();
  }

  // Log date
  char logBuffer[32];
  sprintf(logBuffer, "%d/%d/%d %02d:%02d:%02d", year, month, day, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  String logString = String(logBuffer);

  // Log events
  logString += ", Events:";
  logString += String(PCEvent::numberOfEventsInThisMonth());

  // Log boot count
  logString += ", Boot:";
  logString += String(bootCount);

  // Log voltage
  if (LOG_VOLTAGE)
  {
    uint32_t voltage = readVoltage();
    logString += ", mV:";
    logString += String(voltage);
  }

  // Log heap memory size
  if (LOG_HEAP)
  {
    logString += ", Heap:";
    logString += String(esp_get_free_heap_size());
  }

  // Write log if enabled
  if (LOG_ENABLE)
  {
    logLine(logString);
    SD_MMC.end();
  }

  // Save boot count
  pref.begin(prefName, false);
  pref.putInt(bootCountKey, bootCount);
  pref.end();

  // Footer
  blackSprite.setFont(&fonts::SMALL_FONT);
  blackSprite.setCursor(8, EPD_HEIGHT - FOOTER_HEIGHT);
  blackSprite.print(logString);

  // Display in EPD
  if (EPD_ENABLE)
  {
    Epd epd;
    int initResult = epd.Init();
    if (initResult != 0)
    {
      log_printf("e-Paper init failed: %d", initResult);
      return;
    }
    epd.Displaypart((unsigned char *)(blackSprite.getBuffer()), 0, 0, EPD_WIDTH, EPD_HEIGHT, 0);
    epd.Displaypart((unsigned char *)(redSprite.getBuffer()), 0, 0, EPD_WIDTH, EPD_HEIGHT, 1);
    epd.Sleep();
    digitalWrite(PWR_PIN, LOW);
  }

  // Deep sleep (wake up at 0:05)
  loaded = true;
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
  shutdown(24 * 3600 - (timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec) + 300);
}

uint32_t readVoltage()
{
  uint32_t voltage = 0;
  esp_adc_cal_characteristics_t characteristics;
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_12Bit, 1100, &characteristics);
  digitalWrite(VOLTAGE_TEST, HIGH);
  if (esp_adc_cal_get_voltage(ADC_CHANNEL_8, &characteristics, &voltage) == ESP_OK)
  {

    digitalWrite(VOLTAGE_TEST, LOW);
    return voltage;
  }
  digitalWrite(VOLTAGE_TEST, LOW);
  return 0;
}

void logLine(String line)
{
  File logFile = SD_MMC.open("/log.txt", FILE_APPEND, true);
  if (logFile)
  {
    logFile.println(line);
  }
  logFile.close();
}

void shutdown(int wakeUpSeconds)
{
  gpio_wakeup_disable(GPIO_NUM_17);
  esp_sleep_enable_timer_wakeup(wakeUpSeconds * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}