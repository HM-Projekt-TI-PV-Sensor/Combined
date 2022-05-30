#include <OneWire.h>
#include <DallasTemperature.h>
#include <SD.h>
#include <RTClib.h>

#define PIN_SPI_CS 10
#define ONE_WIRE_BUS 2
#define PV_SENSOR_PIN A1
#define PV_CONVERT_SCALAR 1.27F

RTC_DS3231 rtc;
File logFile;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
String fileName = "log.txt";
int measurementTimeMillis = 500;

float readTemp();
float readPV();
void initRTC();
void initSD();
void createFile();
uint32_t unixTimestamp();
bool writeToLog(float temp, float pv);
void tick();

void setup() {
  Serial.begin(9600);
  sensors.begin();
  initRTC();
  initSD();
}

void loop() {
  tick();
  delay(measurementTimeMillis);
}

void tick() {
  Serial.println(F("Tick"));
  float temp = readTemp();
  float pv = readPV();
  int retries = 0;
  while(!writeToLog(temp, pv)) {
    retries++;
    Serial.print(F("Retry write: "));
    Serial.print(retries);
    Serial.println();
  }
}

float readPV() {
  float raw = (float) analogRead(PV_SENSOR_PIN);
  return raw * PV_CONVERT_SCALAR;
}

float readTemp() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

void initRTC() {
  if (! rtc.begin()) {
    Serial.println(F("Couldn't find RTC"));
    return;
  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  Serial.println(F("RTC started"));
}

void initSD() {
  if (!SD.begin(PIN_SPI_CS)) {
    Serial.println(F("SD CARD FAILED!"));
    while (1);
  }
  logFile = SD.open(fileName, FILE_WRITE);
  Serial.println(F("SD card init"));
}

void createFile() {
  if (!SD.exists(fileName)) {
    logFile = SD.open(fileName, FILE_WRITE);
    logFile.close();
  }
}

uint32_t unixTimestamp() {
  DateTime current = rtc.now();
  uint32_t stamp = current.unixtime();
  return stamp;
}

bool writeToLog(float temp, float pv) {
  if(logFile) {
      Serial.print(temp);
      Serial.print(F(" | "));
      Serial.print(pv);
      Serial.println();
      logFile = SD.open(fileName, FILE_WRITE);
      logFile.print(unixTimestamp());
      logFile.print(F(" > "));
      logFile.print(temp);
      logFile.print(F(" | "));
      logFile.print(pv);
      logFile.println();
      logFile.close();
      return true;
  } else {
    Serial.println(F("Error opening file."));
    return false;
  }
}
