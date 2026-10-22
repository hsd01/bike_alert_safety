
#include <SoftwareSerial.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <math.h>
#include <stdio.h>

// ========== BLUETOOTH ==========
SoftwareSerial BT(10, 11);

// ========== SH1106 OLED ==========
U8G2_SH1106_128X64_NONAME_1_HW_I2C oled(
  U8G2_R0, U8X8_PIN_NONE
);

// ========== ADXL345 ==========
Adafruit_ADXL345_Unified accel(12345);
bool adxlReady = false;

// ========== MASTER DATA ==========
char buffer[80];
byte bufferIndex = 0;

int irValue = -1;
int limitValue = -1;
int helmetFlag = -1;
int alcoholFlag = -1;
int mq3Value = 0;

bool hasMasterData = false;
unsigned long lastReceived = 0;
const unsigned long BT_TIMEOUT = 5000;

// ========== BIKE TILT ==========
const float FALL_ANGLE = 65.0;
const float UPRIGHT_ANGLE = 30.0;
const unsigned long FALL_CONFIRM_MS = 1500;
const unsigned long RECOVER_CONFIRM_MS = 3000;

enum BikeState {
  BIKE_NORMAL,
  BIKE_FALLEN_LEFT,
  BIKE_FALLEN_RIGHT
};

BikeState bikeState = BIKE_NORMAL;

int candidateDirection = 0;
unsigned long fallStart = 0;
unsigned long uprightStart = 0;

float x = 0;
float y = 0;
float z = 0;
float rollAngle = 0;

// ========== TIMERS ==========
unsigned long lastAccelRead = 0;
unsigned long lastOledUpdate = 0;

// ========== RECEIVE BLUETOOTH ==========
void processMessage(char *message) {
  int ir, lim, flag, alcohol, mq3;

  int parsed = sscanf(
    message,
    "IR=%d,limit=%d,FLAG=%d,ALC=%d,MQ3=%d",
    &ir, &lim, &flag, &alcohol, &mq3
  );

  if (parsed == 5 &&
      (ir == 0 || ir == 1) &&
      (lim == 0 || lim == 1) &&
      (flag == ((ir == 0 && lim == 0) ? 1 : 0)) &&
      (alcohol >= -1 && alcohol <= 1) &&
      (mq3 >= 0 && mq3 <= 1023)) {

    irValue = ir;
    limitValue = lim;
    helmetFlag = flag;
    alcoholFlag = alcohol;
    mq3Value = mq3;

    lastReceived = millis();
    hasMasterData = true;

    Serial.print("IR=");
    Serial.print(irValue);

    Serial.print(" LIMIT=");
    Serial.print(limitValue);

    Serial.print(" FLAG=");
    Serial.print(helmetFlag);

    Serial.print(" ALC=");
    Serial.print(alcoholFlag);

    Serial.print(" MQ3=");
    Serial.println(mq3Value);

  } else {
    Serial.print("Invalid data: ");
    Serial.println(message);
  }
}

// ========== BIKE FALL DETECTION ==========
void detectBikeFall(float angle) {
  unsigned long now = millis();
  int direction = 0;

  if (angle >= FALL_ANGLE) {
    direction = 1;
  } else if (angle <= -FALL_ANGLE) {
    direction = -1;
  }

  if (bikeState == BIKE_NORMAL) {
    if (direction == 0) {
      candidateDirection = 0;
      fallStart = 0;
      return;
    }

    if (direction != candidateDirection) {
      candidateDirection = direction;
      fallStart = now;
    }
    else if (now - fallStart >= FALL_CONFIRM_MS) {
      bikeState = (direction == 1) ?
        BIKE_FALLEN_RIGHT : BIKE_FALLEN_LEFT;

      Serial.println(
        direction == 1 ?
        "!!! BIKE FALLEN RIGHT !!!" :
        "!!! BIKE FALLEN LEFT !!!"
      );

      candidateDirection = 0;
      uprightStart = 0;
    }
  }
  else {
    if (fabs(angle) <= UPRIGHT_ANGLE) {
      if (uprightStart == 0) {
        uprightStart = now;
      }
      else if (now - uprightStart >= RECOVER_CONFIRM_MS) {
        bikeState = BIKE_NORMAL;
        uprightStart = 0;
        Serial.println("BIKE RECOVERED");
      }
    }
    else {
      uprightStart = 0;
    }
  }
}

// ========== OLED TEXT HELPER ==========
void drawCentered(const char *str, int y) {
  int width = oled.getStrWidth(str);
  int posX = (128 - width) / 2;

  if (posX < 0) posX = 0;
  oled.drawStr(posX, y, str);
}

// ========== OLED DISPLAY ==========
void drawScreen() {
  bool masterConnected =
    hasMasterData &&
    (millis() - lastReceived < BT_TIMEOUT);

  oled.firstPage();

  do {
    oled.setFont(u8g2_font_6x10_tf);
    drawCentered("BIKE SAFETY SYSTEM", 10);
    oled.drawHLine(0, 13, 128);

    // Highest priority: Bike fallen
    if (adxlReady && bikeState != BIKE_NORMAL) {
      oled.setFont(u8g2_font_9x15B_tf);
      drawCentered("BIKE FALLEN", 33);

      oled.setFont(u8g2_font_7x13B_tf);

      if (bikeState == BIKE_FALLEN_RIGHT) {
        drawCentered("RIGHT", 51);
      } else {
        drawCentered("LEFT", 51);
      }
    }

    // No Bluetooth data
    else if (!masterConnected) {
      oled.setFont(u8g2_font_9x15B_tf);
      drawCentered("NOT SAFE", 34);

      oled.setFont(u8g2_font_6x10_tf);
      drawCentered("NO MASTER DATA", 51);
    }

    // Alcohol sensor triggered
    else if (alcoholFlag == 1) {
      oled.setFont(u8g2_font_9x15B_tf);
      drawCentered("ALCOHOL!", 34);

      oled.setFont(u8g2_font_6x10_tf);
      drawCentered("DETECTED", 51);
    }

    // Helmet NOT SAFE whenever FLAG is 0
    else if (helmetFlag == 0) {
      oled.setFont(u8g2_font_9x15B_tf);
      drawCentered("NOT SAFE", 34);

      oled.setFont(u8g2_font_6x10_tf);
      drawCentered("CHECK HELMET", 51);
    }

    // Helmet on but alcohol sensor warming up
    else if (alcoholFlag == -1) {
      oled.setFont(u8g2_font_9x15B_tf);
      drawCentered("HELM ON", 32);

      oled.setFont(u8g2_font_6x10_tf);
      drawCentered("MQ3 WARMING UP", 49);
    }

    // Bike significantly tilted
    else if (adxlReady && fabs(rollAngle) >= 30.0) {
      oled.setFont(u8g2_font_9x15B_tf);
      drawCentered("BIKE TILTED", 34);

      oled.setFont(u8g2_font_6x10_tf);
      drawCentered(
        rollAngle > 0 ? "RIGHT" : "LEFT", 51
      );
    }

    // All monitored conditions satisfied
    else if (helmetFlag == 1 && alcoholFlag == 0) {
      // Check mark
      oled.drawLine(5, 32, 13, 41);
      oled.drawLine(13, 41, 28, 22);

      oled.setFont(u8g2_font_9x15B_tf);
      oled.drawStr(34, 33, "HELM ON");

      oled.setFont(u8g2_font_7x13B_tf);
      drawCentered("SAFE", 51);
    }

    // Bottom diagnostic information
    oled.setFont(u8g2_font_5x7_tf);

    char line[28];

    if (masterConnected) {
      snprintf(
        line, sizeof(line),
        "F:%d A:%d T:%d",
        helmetFlag, alcoholFlag, (int)rollAngle
      );
    } else {
      snprintf(
        line, sizeof(line),
        "BT LOST T:%d",
        (int)rollAngle
      );
    }

    drawCentered(line, 63);

  } while (oled.nextPage());
}

// ========== SETUP ==========
void setup() {
  Serial.begin(9600);
  BT.begin(9600);
  Wire.begin();

  Serial.println("BIKE SAFETY SLAVE STARTED");

  // OLED address usually 0x3C
  // Uncomment if OLED uses 0x3D:
  // oled.setI2CAddress(0x3D << 1);

  oled.begin();
  Serial.println("SH1106 OLED INITIALIZED");

  // ADXL345 initialization
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

// ========== MAIN LOOP ==========
void loop() {

  // Read Bluetooth data
  while (BT.available()) {
    char c = BT.read();

    if (c == '\n') {
      buffer[bufferIndex] = '\0';

      if (bufferIndex > 0) {
        processMessage(buffer);
      }

      bufferIndex = 0;
    }
    else if (c != '\r') {
      if (bufferIndex < sizeof(buffer) - 1) {
        buffer[bufferIndex++] = c;
      }
      else {
        bufferIndex = 0;
      }
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

    rollAngle = atan2(y, z) * 180.0 / PI;

    detectBikeFall(rollAngle);

    Serial.print("X:");
    Serial.print(x, 2);

    Serial.print(" Y:");
    Serial.print(y, 2);

    Serial.print(" Z:");
    Serial.print(z, 2);

    Serial.print(" Roll:");
    Serial.println(rollAngle, 1);
  }

  // Update OLED every 250 ms
  if (millis() - lastOledUpdate >= 250) {
    lastOledUpdate = millis();
    drawScreen();
  }
}
