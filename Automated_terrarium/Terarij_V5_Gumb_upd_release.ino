#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_SHT31.h>
#include <RTClib.h>

// =========================
// PINI
// =========================
const int PIN_FAN = 35;
const int PIN_RELAY_HEAT_WIRE = 41;
const int PIN_RELAY_LAMP = 40;
const int PIN_RELAY_MISTER = 11;

const int PIN_LCD_BUTTON = 5;
const int PIN_TFT_BL = 6;

// TFT pini
#define TFT_CS   53
#define TFT_DC   49
#define TFT_RST  48

// =========================
// NASTAVITVE
// =========================
bool RELAY_ACTIVE_LOW = true;
bool MISTER_ACTIVE_LOW = false;
bool FAN_ACTIVE_HIGH = true;

const float HUMIDITY_THRESHOLD = 30.0;
const float HEAT_ON_TEMP = 29.0;
const float HEAT_OFF_TEMP = 32.0;

const int CO2_LIMIT = 1800;

const unsigned long MISTER_DURATION = 5000UL;
const unsigned long FAN_INTERVAL = 1800000UL;
const unsigned long FAN_DURATION = 60000UL;
const unsigned long SENSOR_PRINT_INTERVAL = 2000UL;
const unsigned long SCREEN_UPDATE_INTERVAL = 500UL;
const unsigned long BUTTON_DEBOUNCE = 250UL;

// =========================
// OBJEKTI
// =========================
Adafruit_SHT31 sht31 = Adafruit_SHT31();
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
RTC_DS1307 rtc;

// =========================
// SPREMENLJIVKE
// =========================
unsigned long lastPrint = 0;
unsigned long lastScreenUpdate = 0;

bool screenOn = true;
bool lastButtonState = HIGH;
unsigned long lastButtonPress = 0;

bool misterOn = false;
unsigned long misterStartTime = 0;

bool fanOn = false;
bool fanCycleOn = false;
bool co2FanOn = false;

unsigned long fanStartTime = 0;
unsigned long lastFanCycleTime = 0;

bool heatPadOn = true;

// CO2
byte mhzCmdReadCO2[9] = {0xFF, 0x01, 0x86, 0, 0, 0, 0, 0, 0x79};
byte mhzResponse[9];

int co2ppm = -1;
bool co2Ok = false;

// =========================
// FUNKCIJE
// =========================
void setRelay(int pin, bool on) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, on ? LOW : HIGH);
  } else {
    digitalWrite(pin, on ? HIGH : LOW);
  }
}

void setMister(bool on) {
  if (MISTER_ACTIVE_LOW) {
    digitalWrite(PIN_RELAY_MISTER, on ? LOW : HIGH);
  } else {
    digitalWrite(PIN_RELAY_MISTER, on ? HIGH : LOW);
  }
}

void setFan(bool on) {
  fanOn = on;

  if (FAN_ACTIVE_HIGH) {
    digitalWrite(PIN_FAN, on ? HIGH : LOW);
  } else {
    digitalWrite(PIN_FAN, on ? LOW : HIGH);
  }
}

bool lampShouldBeOn(int hourNow) {
  return (hourNow >= 8 && hourNow < 20);
}

byte getCheckSum(byte *packet) {
  byte checksum = 0;

  for (int i = 1; i < 8; i++) {
    checksum += packet[i];
  }

  checksum = 0xFF - checksum;
  checksum += 1;

  return checksum;
}

bool readCO2(int &ppm) {
  while (Serial1.available()) {
    Serial1.read();
  }

  Serial1.write(mhzCmdReadCO2, 9);

  unsigned long startTime = millis();

  while (Serial1.available() < 9) {
    if (millis() - startTime > 250) {
      return false;
    }
  }

  for (int i = 0; i < 9; i++) {
    mhzResponse[i] = Serial1.read();
  }

  if (mhzResponse[0] != 0xFF || mhzResponse[1] != 0x86) {
    return false;
  }

  if (mhzResponse[8] != getCheckSum(mhzResponse)) {
    return false;
  }

  ppm = mhzResponse[2] * 256 + mhzResponse[3];
  return true;
}

void printTwoDigits(int value) {
  if (value < 10) Serial.print('0');
  Serial.print(value);
}

void printTwoDigitsTFT(int value) {
  if (value < 10) tft.print('0');
  tft.print(value);
}

void drawLabel(int x, int y, const char* label, uint16_t color = ST77XX_WHITE) {
  tft.setTextColor(color);
  tft.setTextSize(1);
  tft.setCursor(x, y);
  tft.print(label);
}

void clearValueArea(int x, int y, int w = 80, int h = 10) {
  tft.fillRect(x, y, w, h, ST77XX_BLACK);
}

void drawOnOffValue(int x, int y, bool on) {
  clearValueArea(x, y, 50, 10);
  tft.setTextSize(1);
  tft.setTextColor(on ? ST77XX_GREEN : ST77XX_RED);
  tft.setCursor(x, y);
  tft.print(on ? "ON" : "OFF");
}

void drawStaticUI() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  drawLabel(4, 4,   "Ura:");
  drawLabel(4, 20,  "Temp:", ST77XX_YELLOW);
  drawLabel(4, 35,  "Vlaga:", ST77XX_CYAN);
  drawLabel(4, 50,  "CO2:", ST77XX_GREEN);
  drawLabel(4, 65,  "Luc:");
  drawLabel(4, 80,  "Pad:");
  drawLabel(4, 95,  "Mister:");
  drawLabel(4, 110, "Fan:");
}

void updateStatusScreen(float airTemp, float humidity, DateTime now, bool lampOnWanted) {
  clearValueArea(34, 4, 80, 10);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(34, 4);

  printTwoDigitsTFT(now.hour());
  tft.print(':');
  printTwoDigitsTFT(now.minute());
  tft.print(':');
  printTwoDigitsTFT(now.second());

  clearValueArea(40, 20, 85, 10);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(40, 20);
  if (isnan(airTemp)) {
    tft.print("NAPAKA");
  } else {
    tft.print(airTemp, 1);
    tft.print(" C");
  }

  clearValueArea(46, 35, 85, 10);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(46, 35);
  if (isnan(humidity)) {
    tft.print("NAPAKA");
  } else {
    tft.print(humidity, 1);
    tft.print(" %");
  }

  clearValueArea(34, 50, 90, 10);
  tft.setCursor(34, 50);
  if (co2Ok) {
    tft.setTextColor(co2ppm > CO2_LIMIT ? ST77XX_RED : ST77XX_GREEN);
    tft.print(co2ppm);
    tft.print(" ppm");
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.print("NAPAKA");
  }

  drawOnOffValue(34, 65, lampOnWanted);
  drawOnOffValue(34, 80, heatPadOn);
  drawOnOffValue(46, 95, misterOn);
  drawOnOffValue(34, 110, fanOn);
}

void handleScreenButton() {
  bool buttonState = digitalRead(PIN_LCD_BUTTON);

  if (lastButtonState == HIGH && buttonState == LOW && millis() - lastButtonPress > BUTTON_DEBOUNCE) {
    lastButtonPress = millis();
    screenOn = !screenOn;

    if (screenOn) {
      digitalWrite(PIN_TFT_BL, HIGH);
      drawStaticUI();
    } else {
      tft.fillScreen(ST77XX_BLACK);
      digitalWrite(PIN_TFT_BL, LOW);
    }
  }

  lastButtonState = buttonState;
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  delay(500);

  pinMode(PIN_FAN, OUTPUT);
  pinMode(PIN_RELAY_HEAT_WIRE, OUTPUT);
  pinMode(PIN_RELAY_LAMP, OUTPUT);
  pinMode(PIN_RELAY_MISTER, OUTPUT);

  pinMode(PIN_LCD_BUTTON, INPUT_PULLUP);
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);

  pinMode(53, OUTPUT);
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);

  setFan(false);
  setRelay(PIN_RELAY_HEAT_WIRE, false);
  setRelay(PIN_RELAY_LAMP, false);
  setMister(false);

  if (!sht31.begin(0x44)) {
    Serial.println("NAPAKA: SHT3X ni zaznan na 0x44!");
  } else {
    Serial.println("SHT3X inicializiran.");
  }

  if (!rtc.begin()) {
    Serial.println("NAPAKA: RTC ni zaznan!");
  } else {
    Serial.println("RTC inicializiran.");

//rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    if (!rtc.isrunning()) {
      Serial.println("RTC ni tekel. Nastavljam cas.");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  drawStaticUI();

  lastFanCycleTime = millis();

  Serial.println("Sistem zagnan.");
  Serial.println("CO2 aktiven.");
  Serial.println("LCD button: D5");
  Serial.println("TFT BL: D6");
  Serial.println();
}

// =========================
// LOOP
// =========================
void loop() {
  handleScreenButton();

  DateTime now = rtc.now();

  int hourNow = now.hour();
  int minuteNow = now.minute();
  int secondNow = now.second();

  bool lampOnWanted = lampShouldBeOn(hourNow);

  setRelay(PIN_RELAY_LAMP, lampOnWanted);

  float airTemp = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  // CO2
  co2Ok = readCO2(co2ppm);

  if (co2Ok) {
    if (co2ppm > CO2_LIMIT) {
      co2FanOn = true;
    } else if (co2ppm < CO2_LIMIT) {
      co2FanOn = false;
    }
  }

  // Heating pad
  if (!isnan(airTemp)) {
    if (!heatPadOn && airTemp <= HEAT_ON_TEMP) {
      heatPadOn = true;
      setRelay(PIN_RELAY_HEAT_WIRE, true);
    } else if (heatPadOn && airTemp >= HEAT_OFF_TEMP) {
      heatPadOn = false;
      setRelay(PIN_RELAY_HEAT_WIRE, false);
    }
  }

  // Mister
  if (!isnan(humidity)) {
    if (!misterOn && humidity < HUMIDITY_THRESHOLD) {
      misterOn = true;
      misterStartTime = millis();
      setMister(true);
    }
  }

  if (misterOn && (millis() - misterStartTime >= MISTER_DURATION)) {
    misterOn = false;
    setMister(false);
  }

  // Ventilator časovni cikel
  if (!fanCycleOn && (millis() - lastFanCycleTime >= FAN_INTERVAL)) {
    fanCycleOn = true;
    fanStartTime = millis();
    lastFanCycleTime = millis();
  }

  if (fanCycleOn && (millis() - fanStartTime >= FAN_DURATION)) {
    fanCycleOn = false;
  }

  setFan(fanCycleOn || co2FanOn);

  // TFT update samo če je ekran prižgan
  if (screenOn && millis() - lastScreenUpdate >= SCREEN_UPDATE_INTERVAL) {
    lastScreenUpdate = millis();
    updateStatusScreen(airTemp, humidity, now, lampOnWanted);
  }

  // Serial monitor
  if (millis() - lastPrint >= SENSOR_PRINT_INTERVAL) {
    lastPrint = millis();

    Serial.println("===== STATUS =====");

    Serial.print("RTC ura: ");
    printTwoDigits(hourNow);
    Serial.print(":");
    printTwoDigits(minuteNow);
    Serial.print(":");
    printTwoDigits(secondNow);
    Serial.println();

    Serial.print("Temperatura zraka: ");
    if (isnan(airTemp)) {
      Serial.println("NAPAKA");
    } else {
      Serial.print(airTemp);
      Serial.println(" °C");
    }

    Serial.print("Vlaga: ");
    if (isnan(humidity)) {
      Serial.println("NAPAKA");
    } else {
      Serial.print(humidity);
      Serial.println(" %");
    }

    Serial.print("CO2: ");
    if (co2Ok) {
      Serial.print(co2ppm);
      Serial.println(" ppm");
    } else {
      Serial.println("NAPAKA / ni odgovora");
    }

    Serial.print("Luč: ");
    Serial.println(lampOnWanted ? "ON" : "OFF");

    Serial.print("Heating pad: ");
    Serial.println(heatPadOn ? "ON" : "OFF");

    Serial.print("Mister: ");
    Serial.println(misterOn ? "ON" : "OFF");

    Serial.print("Ventilator: ");
    Serial.println(fanOn ? "ON" : "OFF");

    Serial.print("LCD ekran: ");
    Serial.println(screenOn ? "ON" : "OFF");

    Serial.println("==================\n");
  }
}