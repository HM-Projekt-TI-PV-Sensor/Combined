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
uint32_t lastMeasurement = millis();

// DATA FUNCTIONS
float readTemp();
float readPV();
void printData(float temp, float pv);

// REAL TIME CLOCK INIT
void initRTC();

// SD CARD FUNCTIONS
void initSD();
void createFile();
uint32_t unixTimestamp();
bool writeToLog(float temp, float pv);
void tick();

// RGB LED FUNCTION
// void RGB_color(char red_light_value, char green_light_value, char blue_light_value);

// DEEP SLEEP VARIABLES
volatile int sleepcounter = 0; // Schlafzyklen mitzählen
int k = 0;

/* INIT
 *  INITIALIZE SENSORS
 *  SET PINMODES TO OUTPUT |  LED Indicators:
 *                            - Blink for 1s: Read and write data successful
 *                            - ...
 *                            DCDC Switch is used to turn on/off Power for insolation sensor
 *  TURN ON WATCHDOF | Creates interrupt for arduino deep sleep in one minute interval
 *  INITIALIZE RTC
 *  TURN ON
 */
 
void setup() {
  Serial.begin(9600);
  sensors.begin();
  pinMode(STATUS_LED, OUTPUT);
  pinMode(DCDC_SWITCH, OUTPUT);
  watchdogOn(); // Watchdog timer einschalten.
  initRTC();
}

void loop() {

  /*
  // Debug Switch and sleep mode
  float temp = readTemp(); // Vergleichsmessung 
  float pv = readPV();
  printData(temp, pv);
  digitalWrite(DCDC_SWITCH, HIGH);
  delay(5000);
  temp = readTemp(); // Vergleichsmessung 
  pv = readPV();
  printData(temp, pv);
  digitalWrite(DCDC_SWITCH, LOW);

  
  // SLEEP MODE
  digitalWrite(STATUS_LED, HIGH);
  delay(100); // delay für serielle Ausgabe
  Serial.println(k);  
  k++;
  delay(900); // delay für serielle Ausgabe beenden
  digitalWrite(STATUS_LED, LOW);
  pwrDown(measurmentTimeSeconds); // ATmega328 fährt runter für 60 Sekunden
  wdt_reset();  // Reset Watchdog Timer
  */
  uint32_t start = millis();
  wdt_reset();  // Reset Watchdog Timer
  uint32_t measureDelta = millis() - lastMeasurement;
  lastMeasurement = millis();
  double secs = measureDelta / 1000.0;
  Serial.print(F("Measurement delta: "));
  Serial.print(secs);
  Serial.print(F("s"));
  Serial.println();
  
  // WRITE SD
  digitalWrite(DCDC_SWITCH, HIGH);
  delay(5000);    // Delay to get valid data from insolation sensor
  tick();
  
  digitalWrite(DCDC_SWITCH, LOW);

  // Signal of data written
  digitalWrite(STATUS_LED, HIGH);
  delay(1000);
  digitalWrite(STATUS_LED, LOW);
  delay(1000);

  uint32_t delta = millis() - start;
  pwrDown(measurmentTimeSeconds - (delta / 1000));
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
  if (!rtc.begin()) {
    Serial.println(F("Couldn't find RTC"));
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
  Serial.println(F("RTC started"));
}

void initSD() {
  if (!SD.begin(PIN_SPI_CS)) {
    Serial.println(F("SD CARD FAILED!"));
    while (1) {
      // ERROR if SD Card could not be written
      digitalWrite(STATUS_LED, HIGH);
      delay(1000);
      digitalWrite(STATUS_LED, LOW);
      delay(1000);                 
    }
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

      printData(temp, pv);
      
      return true;
  } else {
    Serial.println(F("Error opening file."));
    return false;
  }

}

void pwrDown(int sekunden) {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // den tiefsten Schlaf auswählen PWR_DOWN
  for(int i=0; i < sekunden; i++) {
    sleep_enable(); // sleep mode einschalten
    sleep_mode(); // in den sleep mode gehen
    sleep_disable(); // sleep mode ausschalten nach dem Erwachen
  }
}

void watchdogOn() {
  MCUSR = MCUSR & B11110111; // Reset flag ausschalten, WDRF bit3 vom MCUSR.
  WDTCSR = WDTCSR | B00011000; // Bit 3+4 um danach den Prescaler setzen zu können
  WDTCSR = B00000110; // Watchdog Prescaler auf 128k setzen > ergibt ca. 1 Sekunde
  WDTCSR = WDTCSR | B01000000; // Watchdog Interrupt einschalten
  MCUSR = MCUSR & B11110111;
}

ISR(WDT_vect) {
  sleepcounter ++; // Schlafzyklen mitzählen
}

void printData(float temp, float pv){
  Serial.print(temp);
  Serial.print(F(" | "));
  Serial.print(pv);
  Serial.println();
}
