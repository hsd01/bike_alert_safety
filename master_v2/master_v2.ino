
#include <SoftwareSerial.h>

// HC-05 Bluetooth
// Arduino RX = D10, TX = D11
SoftwareSerial BT(10, 11);

// Sensor pins
const byte IR_PIN = 3;
const byte LIMIT_PIN = 4;
const byte MQ3_DO_PIN = 2;
const byte MQ3_AO_PIN = A0;

// Most MQ-3 modules trigger D0 at LOW.
// Change to HIGH if yours is reversed.
const byte MQ3_ACTIVE_LEVEL = LOW;

// Transmission interval
const unsigned long SEND_INTERVAL = 500;
unsigned long lastSend = 0;

// Prototype MQ-3 warm-up interval
const unsigned long MQ3_WARMUP_MS = 300000UL;

void setup() {
  Serial.begin(9600);
  BT.begin(9600);

  pinMode(IR_PIN, INPUT);

  // Limit switch between D4 and GND
  pinMode(LIMIT_PIN, INPUT_PULLUP);

  pinMode(MQ3_DO_PIN, INPUT);
  pinMode(MQ3_AO_PIN, INPUT);

  Serial.println("MASTER READY");
  Serial.println("Helmet + MQ3 monitoring started");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSend >= SEND_INTERVAL) {
    lastSend = now;

    // Read sensors
    int ir = digitalRead(IR_PIN);
    int limitSwitch = digitalRead(LIMIT_PIN);

    int mq3Digital = digitalRead(MQ3_DO_PIN);
    int mq3Analog = analogRead(MQ3_AO_PIN);

    // Helmet is ON only when:
    // IR = 0 AND Limit Switch = 0
    int helmetFlag =
      (ir == LOW && limitSwitch == LOW) ? 1 : 0;

    // -1 = MQ3 warming up / not ready
    //  0 = no threshold crossing
    //  1 = alcohol threshold triggered
    int alcoholFlag = -1;

    if (now >= MQ3_WARMUP_MS) {
      alcoholFlag =
        (mq3Digital == MQ3_ACTIVE_LEVEL) ? 1 : 0;
    }

    // Send complete message over Bluetooth
    BT.print("IR=");
    BT.print(ir);

    BT.print(",limit=");
    BT.print(limitSwitch);

    BT.print(",FLAG=");
    BT.print(helmetFlag);

    BT.print(",ALC=");
    BT.print(alcoholFlag);

    BT.print(",MQ3=");
    BT.println(mq3Analog);

    // Debug output
    Serial.print("IR=");
    Serial.print(ir);

    Serial.print(" Limit=");
    Serial.print(limitSwitch);

    Serial.print(" FLAG=");
    Serial.print(helmetFlag);

    Serial.print(" Alcohol=");
    Serial.print(alcoholFlag);

    Serial.print(" MQ3=");
    Serial.println(mq3Analog);
  }
}
