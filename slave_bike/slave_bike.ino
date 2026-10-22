
#include <SoftwareSerial.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <stdio.h>

// HC-05
SoftwareSerial BT(10, 11);

// SH1106 - page buffer saves Arduino RAM
U8G2_SH1106_128X64_NONAME_1_HW_I2C oled(
  U8G2_R0, U8X8_PIN_NONE
);

// ADXL345
Adafruit_ADXL345_Unified accel(12345);
bool adxlReady = false;

// Bluetooth data
char buffer[64];
byte bufferIndex = 0;

int irValue = -1;
int limitValue = -1;
int helmetFlag = -1;

unsigned long lastReceived = 0;
const unsigned long BT_TIMEOUT = 5000;

// Tilt detection
enum TiltState { NORMAL, RIGHT, LEFT };

TiltState currentState = NORMAL;
TiltState candidateState = NORMAL;

unsigned long candidateStart = 0;
const unsigned long CONFIRM_TIME = 500;

// Timers
unsigned long lastAccelRead = 0;
unsigned long lastOledUpdate = 0;

float x = 0, y = 0, z = 0;

void processMessage(char *message) {
  int ir, lim, flag;

  int count = sscanf(
    message,
    "IR=%d,limit=%d,FLAG=%d",
    &ir, &lim, &flag
  );

  if (count == 3 &&
      (ir == 0 || ir == 1) &&
      (lim == 0 || lim == 1) &&
      (flag == 0 || flag == 1)) {

    irValue = ir;
    limitValue = lim;
    helmetFlag = flag;
    lastReceived = millis();

    Serial.print("IR:");
    Serial.print(irValue);
    Serial.print(" Limit:");
    Serial.print(limitValue);
    Serial.print(" FLAG:");
    Serial.println(helmetFlag);
  }
  else {
    Serial.print("Invalid BT: ");
    Serial.println(message);
  }
}

TiltState detectTilt(float y, float z) {
  if (y >= 8.0 && y <= 10.0 &&
      z >= 2.0 && z <= 6.0)
    return RIGHT;

  if (y >= -10.0 && y <= -8.0 &&
      z >= 2.0 && z <= 6.0)
    return LEFT;

  return NORMAL;
}

void updateTilt(TiltState detected) {
  unsigned long now = millis();

  if (detected != candidateState) {
    candidateState = detected;
    candidateStart = now;
  }

  if (currentState != candidateState &&
      now - candidateStart >= CONFIRM_TIME) {
    currentState = candidateState;

    if (currentState == RIGHT)
      Serial.println("CRASH - RIGHT");
    else if (currentState == LEFT)
      Serial.println("CRASH - LEFT");
    else
      Serial.println("NORMAL - RECOVERED");
  }
}

void drawScreen() {
  bool dataValid =
    helmetFlag != -1 &&
    millis() - lastReceived < BT_TIMEOUT;

  oled.firstPage();

  do {
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(3, 10, "HELMET SAFETY SYSTEM");
    oled.drawHLine(0, 14, 128);

    if (currentState != NORMAL) {
      oled.setFont(u8g2_font_9x15B_tf);
      oled.drawStr(30, 36, "CRASH!");

      oled.setFont(u8g2_font_6x10_tf);
      oled.drawStr(44, 52,
        currentState == RIGHT ? "RIGHT" : "LEFT");
    }
    else if (!dataValid) {
      oled.setFont(u8g2_font_9x15B_tf);
      oled.drawStr(15, 35, "NOT SAFE");

      oled.setFont(u8g2_font_6x10_tf);
      oled.drawStr(19, 53, "NO MASTER DATA");
    }
    else if (helmetFlag == 1) {
      // Check mark
      oled.drawLine(4, 32, 12, 42);
      oled.drawLine(12, 42, 27, 23);

      oled.setFont(u8g2_font_9x15B_tf);
      oled.drawStr(33, 36, "HELM ON");

      oled.setFont(u8g2_font_6x10_tf);
      oled.drawStr(48, 54, "SAFE");
    }
    else {
      // X mark
      oled.drawLine(5, 23, 26, 44);
      oled.drawLine(26, 23, 5, 44);

      oled.setFont(u8g2_font_9x15B_tf);
      oled.drawStr(34, 35, "NOT SAFE");
    }

    oled.setFont(u8g2_font_5x7_tf);
    oled.drawStr(4, 63,
      adxlReady ? "ADXL345 OK" : "ADXL ERROR");

  } while (oled.nextPage());
}

void setup() {
  Serial.begin(9600);
  BT.begin(9600);
  Wire.begin();

  Serial.println("SLAVE READY");

  // If OLED uses 0x3D instead of 0x3C:
  // oled.setI2CAddress(0x3D << 1);

  oled.begin();
  Serial.println("SH1106 OLED INITIALIZED");

  if (accel.begin(0x53)) {
    adxlReady = true;
    accel.setRange(ADXL345_RANGE_16_G);
    Serial.println("ADXL345 CONNECTED");
  }
  else {
    Serial.println("ADXL345 NOT FOUND");
  }

  drawScreen();
}

void loop() {
  // Receive Bluetooth messages
  while (BT.available()) {
    char c = BT.read();

    if (c == '\n') {
      buffer[bufferIndex] = '\0';

      if (bufferIndex > 0)
        processMessage(buffer);

      bufferIndex = 0;
    }
    else if (c != '\r') {
      if (bufferIndex < sizeof(buffer) - 1)
        buffer[bufferIndex++] = c;
      else
        bufferIndex = 0;
    }
  }

  // Read ADXL345 every 100 ms
  if (adxlReady &&
      millis() - lastAccelRead >= 100) {
    lastAccelRead = millis();

    sensors_event_t event;
    accel.getEvent(&event);

    x = event.acceleration.x;
    y = event.acceleration.y;
    z = event.acceleration.z;

    updateTilt(detectTilt(y, z));

    Serial.print("X:");
    Serial.print(x, 2);
    Serial.print(" Y:");
    Serial.print(y, 2);
    Serial.print(" Z:");
    Serial.println(z, 2);
  }

  // Refresh OLED every 250 ms
  if (millis() - lastOledUpdate >= 250) {
    lastOledUpdate = millis();
    drawScreen();
  }
}
