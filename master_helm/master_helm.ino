
#include <SoftwareSerial.h>

SoftwareSerial BT(10, 11);

const int IR_PIN = 3;
const int LIMIT_PIN = 4;

// Change to HIGH if your IR sensor outputs
// HIGH when helmet is detected.
const int IR_HELMET_DETECTED = LOW;

unsigned long lastSend = 0;
const unsigned long sendInterval = 500;

void setup() {
  Serial.begin(9600);
  BT.begin(9600);

  pinMode(IR_PIN, INPUT);
  pinMode(LIMIT_PIN, INPUT_PULLUP);

  Serial.println("MASTER READY");
}

void loop() {
  if (millis() - lastSend >= sendInterval) {
    lastSend = millis();

    int ir = digitalRead(IR_PIN);
    int limitSwitch = digitalRead(LIMIT_PIN);

    // Derive flag from IR sensor
    int flag = (ir == IR_HELMET_DETECTED) ? 1 : 0;

    // Send to Slave
    BT.print("IR=");
    BT.print(ir);

    BT.print(",limit=");
    BT.print(limitSwitch);
    if(ir == 0 && limitSwitch == 0){
    BT.print(",FLAG=");
    BT.println(flag);
    }

    // Debug
    Serial.print("IR=");
    Serial.print(ir);

    Serial.print(" limit=");
    Serial.print(limitSwitch);

    Serial.print(" FLAG=");
    Serial.println(flag);
  }
}
