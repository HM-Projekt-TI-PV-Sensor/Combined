#include <OneWire.h>
#include <DallasTemperature.h>
#include <SD.h>
#include <RTClib.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

// PINS
#define PIN_SPI_CS 10
#define ONE_WIRE_BUS 2
#define PV_SENSOR_PIN A1
#define PV_CONVERT_SCALAR 1.27F
#define DCDC_SWITCH 4
#define STATUS_LED 5
#define RESET_RTC false

// INIT VARIABLES
String fileName = "log.txt";
int measurmentTimeSeconds = 60;

// SENSOR INIT VARIABLES
RTC_DS3231 rtc;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
uint32_t lastMeasurement = 0;

// DATA FUNCTIONS
float readTemp();
float readPV();

// REAL TIME CLOCK INIT
void initRTC();

// SD CARD FUNCTIONS
void initSD();
void createFile();
uint32_t unixTimestamp();
bool writeToLog(float temp, float pv);
void tick();

/* INIT
 *  INITIALIZE temperature SENSOR
 *  SET PINMODES TO OUTPUT |  LED Indicators:
 *                            - Blink for 1/2s: Read and write data successful
 *                            - Blink for 2s-ON|1/2s-OFF if SD Card is not initialized
 *                            DCDC Switch is used to turn on/off Power for insolation sensor
 *  INITIALIZE RTC
 */
 
void setup() {
  // Serial.begin(9600);
  sensors.begin();
  pinMode(STATUS_LED, OUTPUT);
  pinMode(DCDC_SWITCH, OUTPUT);
  initRTC();
}

void loop() {

  uint32_t time_now = unixTimestamp();
  
  // WRITE SD
  digitalWrite(DCDC_SWITCH, HIGH);
  delay(5000);    // Delay to get valid data from insolation sensor
  tick();
  
  digitalWrite(DCDC_SWITCH, LOW);

  // Signal of data written
  digitalWrite(STATUS_LED, HIGH);
  delay(500);
  digitalWrite(STATUS_LED, LOW);
  delay(500);

  // Go to sleep for 45secs
  int sleep_time = 45;
  for (int i = 0; i < sleep_time; i++) {
    myWatchdogEnable ();
  }

  // Calc remaining time to 60secs and delay to get frequency of 1 minute
  uint32_t delta = unixTimestamp() - time_now;
  delay((measurmentTimeSeconds - delta)*1000);
  
}

// Routine for getting values and save
void tick() {
  
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

// Get insolation value
float readPV() {
  float raw = (float) analogRead(PV_SENSOR_PIN);
  return raw * PV_CONVERT_SCALAR;
}

// Get temperature value
float readTemp() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

// Initialize RTC and set blinkink LED if failure
void initRTC() {
  if (!rtc.begin()) {
    // Serial.println(F("Couldn't find RTC"));
    while(1) {
      digitalWrite(STATUS_LED, HIGH);
      delay(2000);
      digitalWrite(STATUS_LED, LOW);
      delay(500);  
    }
  }

  if(RESET_RTC) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  // Serial.println(F("RTC started"));
}

// Get unixTimestamp from RTC
uint32_t unixTimestamp() {
  DateTime current = rtc.now();
  uint32_t stamp = current.unixtime();
  return stamp;
}

// Initialize SD Card via SPI
void initSD() {
  if (!SD.begin(PIN_SPI_CS)) {
    // Serial.println(F("SD CARD FAILED!"));
    while (1) {
      // ERROR if SD Card could not be written
      digitalWrite(STATUS_LED, HIGH);
      delay(1000);
      digitalWrite(STATUS_LED, LOW);
      delay(1000);                 
    }
  }
}

// Create File ***NOT NEEDED*** ?!
void createFile() {
  if (!SD.exists(fileName)) {
    File logFile = SD.open(fileName, FILE_WRITE);
    logFile.close();
  }
}

// Write data values to SD Card
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
      
      return true;
  } else {
    Serial.println(F("Error opening file."));
    return false;
  }

}

// Watchdog Dog ISR
ISR (WDT_vect) 
{
   wdt_disable();  // disable watchdog
} 

// Configure Watchdog Timer for 1s and got to sleep
void myWatchdogEnable() 
{
  // clear various "reset" flags
  MCUSR = 0;     
  // allow changes, disable reset
  WDTCSR = bit (WDCE) | bit (WDE);
  // set interrupt mode and an interval 
  WDTCSR = bit (WDIE) | bit (WDP2) | bit (WDP1);    // set WDIE, and 1 second delay
  wdt_reset();

  set_sleep_mode (SLEEP_MODE_PWR_DOWN);  
  sleep_mode(); 
}
