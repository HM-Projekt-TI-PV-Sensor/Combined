  #include <OneWire.h>
  #include <DallasTemperature.h>
  #include <SD.h>
  #include <RTClib.h>
  
  #define PIN_SPI_CS 10
  #define ONE_WIRE_BUS 2
  #define PV_SENSOR_PIN A1
  #define PV_CONVERT_SCALAR 1.27F
  
  RTC_DS3231 rtc;
  OneWire oneWire(ONE_WIRE_BUS);
  DallasTemperature sensors(&oneWire);
  String fileName = "log.txt";
  int measurementTimeMillis = 250;
  
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
      delay(100);
      retries++;
      if(retries == 10) {
        Serial.print(F("Stopping retries..."));
        break;
      }
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
    Serial.println(F("SD card init"));
  }
  
  void createFile() {
    if (!SD.exists(fileName)) {
      File logFile = SD.open(fileName, FILE_WRITE);
      logFile.close();
    }
  }
  
  uint32_t unixTimestamp() {
    DateTime current = rtc.now();
    uint32_t stamp = current.unixtime();
    return stamp;
  }
  
  bool writeToLog(float temp, float pv) {
  
    String dataString = "";
  
    dataString += unixTimestamp();
    dataString += " > ";
    dataString += temp;
    dataString += " | ";
    dataString += pv;
  
    initSD();
  
    File logFile = SD.open(fileName, FILE_WRITE);
    
    if(logFile) {
        logFile.println(dataString);
        logFile.close();
  
        Serial.print(temp);
        Serial.print(F(" | "));
        Serial.print(pv);
        Serial.println();
        
        return true;
    } else {
      Serial.println(F("Error opening file."));
      return false;
    }
  
  }
