# Arduino Helmet Safety System — HC-05 Master/Slave, ADXL345 and SH1106

This project uses **two Arduino Uno/Nano boards** and **two HC-05 Bluetooth Classic modules**. The **Master** reads an IR sensor and a limit switch, computes a helmet flag, and periodically transmits a structured message. The **Slave** receives that message, reads an ADXL345 accelerometer, and displays **HELM ON**, **NOT SAFE**, or a **suspected rollover** alert on a 1.3-inch **SH1106 128×64 I²C OLED**.

> **Prototype warning:** The ADXL345 measures acceleration, not angular velocity. The calibrated Y/Z ranges below are *observed tilt signatures*, not proof of a 90° rotation or an actual crash. Do not use this demonstration as life-safety equipment.

## Project files

| File | Purpose |
|---|---|
| `hc05_at_bridge/hc05_at_bridge.ino` | Serial Monitor bridge for AT commands |
| `master/master.ino` | Automatic IR, limit-switch and `FLAG` sender |
| `slave/slave.ino` | Bluetooth receiver + ADXL345 + SH1106 status display |

## 1. Parts and wiring

- 2 × Arduino Uno or Nano (ATmega328P)
- 2 × HC-05 breakout modules with suitable 5 V VCC input
- 1 × IR sensor module (digital OUT)
- 1 × mechanical limit switch
- 1 × ADXL345 I²C breakout
- 1 × 1.3-inch SH1106 128×64 I²C OLED
- Jumper wires and resistors/level shifter as appropriate

### Both Arduino boards → HC-05

| HC-05 | Arduino | Notes |
|---|---|---|
| VCC | 5V | **Only** when the breakout is specified for 5 V supply; the bare chip is 3.3 V |
| GND | GND | Shared ground |
| TXD | D10 | SoftwareSerial RX |
| RXD | D11 | Through a 5 V → 3.3 V divider/level shifter |
| KEY/EN | Per board AT-mode instructions | Assert for AT mode, not normal operation |

A simple divider: **Arduino D11 → 1 kΩ → HC-05 RXD → 2 kΩ → GND**. The junction feeds HC-05 RXD. HC-05 TXD (3.3 V logic) normally reads HIGH on an ATmega328P 5 V Arduino.

### Master extras

| Component | Arduino | Notes |
|---|---|---|
| IR digital OUT | D3 | Logic polarity depends on module |
| IR VCC/GND | Appropriate supply / GND | Follow your sensor label |
| Limit switch COM | GND | Mechanical contact |
| Limit switch NO | D4 | Uses `INPUT_PULLUP`, normally 1, pressed 0 |

### Slave extras (shared I²C bus)

| Component | Pin | Uno/Nano |
|---|---|---|
| ADXL345 | SDA | A4 |
| ADXL345 | SCL | A5 |
| ADXL345 | VCC | 3.3 V if bare 3.3 V module |
| ADXL345 | GND | GND |
| ADXL345 | CS | 3.3 V for I²C if not already pulled up |
| ADXL345 | SDO/ALT ADDRESS | GND for address `0x53` if not already strapped |
| SH1106 | SDA | A4 |
| SH1106 | SCL | A5 |
| SH1106 | VCC/GND | According to breakout specification / GND |

**I²C electrical caution:** A bare ADXL345 is **not 5 V tolerant**. Use a bidirectional I²C level shifter where needed; do not allow 5 V pull-ups on the 3.3 V sensor side. Check OLED module logic levels too. Some sensor/OLED breakout boards already provide regulation and level shifting, others do not. Typical addresses: ADXL345 `0x53` (alternatively `0x1D`), SH1106 `0x3C` (sometimes `0x3D`).

### Flow

```text
IR OUT(D3) ──┐
             ├── Arduino MASTER ─ D10/D11 ─ HC-05 MASTER
Limit(D4) ──┘                                     ))) Bluetooth (((
                                         HC-05 SLAVE ─ D10/D11 ─ Arduino SLAVE
                                                               ├── I²C A4/A5 ─ ADXL345
                                                               └── I²C A4/A5 ─ SH1106 OLED
```

## 2. Configure each HC-05 using AT commands

**Do this before uploading the final Master/Slave programs.** AT commands configure the HC-05 module, **not** the Arduino sketch. HC-05 firmware clones vary; commands and reply formats may differ.

1. Upload `hc05_at_bridge/hc05_at_bridge.ino` to the Arduino used for configuration.
2. Disconnect power; hold the HC-05 button (or assert KEY where supported) while powering the module to request **full AT mode**. On many modules the LED blinks slowly. Keep KEY asserted if your board requires it.
3. Open Arduino Serial Monitor at **9600 baud**, line ending **Both NL & CR**. The bridge talks to the HC-05 at **38400 baud**, a common full AT-mode rate. If `AT` fails, first inspect wiring, KEY behavior, and firmware baud requirements.
4. Type `AT` and verify `OK` before proceeding.

### 2A. Slave AT configuration

Send **one command per line**:

```text
AT
AT+ROLE=0
AT+NAME=Arduino_Slave
AT+PSWD="1234"
AT+UART=9600,0,0
AT+ADDR?
```

Some firmware accepts `AT+PSWD=1234`, but if you see `ERROR:(1D)`, try the **quoted** form above. If your password is already `1234`, no change is required. If errors persist, check `AT+VERSION?` and `AT+PSWD?` rather than resetting blindly.

**Your previously reported Slave address:**

```text
+ADDR:0:13:10408A
```

If the address differs on your current module, **use the actual result**, not this example.

### 2B. Master AT configuration

Move the AT bridge to the **Master** module (or use its own Arduino in AT mode), then:

```text
AT
AT+ROLE=1
AT+CMODE=0
AT+PSWD="1234"
AT+UART=9600,0,0
AT+BIND=0,13,10408A
AT+BIND?
```

**Address-format detail:** `AT+ADDR?` typically uses colons; `AT+BIND=` uses **commas**. `0,13,10408A` corresponds to Slave address `0:13:10408A`. Some firmware needs leading zeros, e.g. `AT+BIND=0000,13,10408A`. Verify the result with `AT+BIND?`.

If it still says:

```text
+BIND:0000:00:000000
OK
```

then the Master has **not stored a target address**. Confirm `AT+ROLE?` is `1`, `AT+CMODE?` is `0`, read `AT+VERSION?`, and retry `AT+BIND=` in **full AT mode**. Do not infer a connection from the Arduino printing “Sent”.

### 2C. Optional pairing and link commands

For HC-05 firmware that supports these AT commands, with the Slave powered in **normal mode**, try on the Master in AT mode:

```text
AT+INIT
AT+PAIR=0,13,10408A,20
AT+LINK=0,13,10408A
```

`AT+INIT` may report an error if already initialized; command support depends on firmware. Use `AT+VERSION?` to identify it. Binding alone is sufficient for automatic connection on some modules; others need additional pairing/link setup.

### 2D. Return to normal data mode

1. Power off both modules and release KEY/button.
2. Upload final `master/master.ino` and `slave/slave.ino`.
3. Power on the Slave, then Master, in **normal mode**.
4. Both HC-05 UART data baud rates must match `BT.begin(9600)`.
5. LED patterns depend on firmware, but fast blinking often suggests disconnected and a changed/slower pattern often suggests connected.

## 3. Install Arduino libraries

In **Arduino IDE → Sketch → Include Library → Manage Libraries**, install:

- **U8g2** (by oliver) — SH1106 display; page-buffer constructor saves SRAM.
- **Adafruit ADXL345** — accelerometer.
- **Adafruit Unified Sensor** — dependency.

`SoftwareSerial` and `Wire` are part of the usual AVR Arduino core.

Select the correct **board**, **processor** (Nano old bootloader if applicable) and **COM port** before uploading each sketch.

## 4. Upload Master sketch

Upload `master/master.ino` to Arduino 1. It sends a message every **500 ms** in this exact format, with a terminating newline:

```text
IR=0,limit=1,FLAG=1
IR=1,limit=1,FLAG=0
```

**Default flag logic:** `FLAG=1` when IR reads `LOW`, `FLAG=0` otherwise. **Verify** your IR sensor's detection polarity. If your helmet-detected state reads HIGH, change `IR_HELMET_DETECTED` to `HIGH`. This example does **not** require the limit switch to be active to set the flag. Adapt that condition if your helmet design requires both sensors.

`INPUT_PULLUP` on D4 means unpressed/open = `1` and pressed/closed = `0` for a switch connected between D4 and GND.

## 5. Upload Slave sketch

Upload `slave/slave.ino` to Arduino 2. It:

- Receives **newline-terminated** HC-05 messages and validates `IR`, `limit`, `FLAG`.
- Shows **HELM ON** + check mark if `FLAG=1`, **NOT SAFE** + X if `FLAG=0`.
- Shows **NO MASTER DATA** after 5 seconds without a valid packet (fail-safe display behavior).
- Samples ADXL345 every ~100 ms and prints `X`, `Y`, `Z` to Serial Monitor at 9600 baud.
- Recognizes sustained observed right/left tilt patterns (below) after **500 ms** and shows **CRASH RIGHT/LEFT** as a **suspected rollover indication**, not verified crash detection.
- Refreshes OLED every ~250 ms.

### Calibrated *observations* (m/s²)

| Pose | X | Y | Z | Outcome |
|---|---|---|---|---|
| Normal | about ±1 | about ±1 | 9–11 | Normal |
| Right tilted | varies | 8–10 | 4–6 | CRASH RIGHT after 500 ms |
| Left tilted | varies | −10 to −8 | 2–5 | CRASH LEFT after 500 ms |

These ranges are based on user measurements; they are **not an exact 90° calculation**. A static 90° tilt with Y near ±9.8 m/s² would usually have Z near 0 m/s² in this coordinate arrangement. For angle estimation, use `atan2(y,z) * 180 / PI` when stationary; moving/vibrating systems need filtering and potentially a gyroscope.

### OLED status priority

1. Confirmed tilt pattern → `CRASH / RIGHT` or `CRASH / LEFT`.
2. No recent valid Bluetooth data → `NOT SAFE / NO MASTER DATA`.
3. `FLAG=1` → `HELM ON ✓`.
4. `FLAG=0` → `NOT SAFE X`.

The SH1106 is **monochrome**; symbols and text are white/on, not colored.

## 6. Verify output

**Master Serial Monitor (9600):**

```text
MASTER READY
Sent: IR=0,limit=1,FLAG=1
Sent: IR=1,limit=1,FLAG=0
```

**Slave Serial Monitor (9600):**

```text
SLAVE READY
SH1106 INITIALIZED
ADXL345 CONNECTED
Received: IR=0 limit=1 FLAG=1
X:0.12 Y:-0.20 Z:9.83
```

### Individual test order

1. **AT response**: Confirm each module responds `OK` to `AT` in AT mode.
2. **Master address**: Confirm `AT+BIND?` returns the Slave address, not all zeros.
3. **Bluetooth link**: Power-cycle in normal mode. Check module LED behavior.
4. **Raw serial**: Ensure Master sends and Slave receives complete `IR=...,limit=...,FLAG=...` lines.
5. **ADXL345**: Confirm `ADXL345 CONNECTED` and realistic X/Y/Z (flat Z ≈ 9.8 m/s²).
6. **SH1106**: Confirm helmet status updates when IR state changes; tilt to test alert.

## 7. Troubleshooting

| Symptom | Checks |
|---|---|
| Slave prints nothing | Master printing “Sent” **does not** prove Bluetooth is linked. Check roles, Slave address, `AT+BIND?`, HC-05 LED behavior, cross TX/RX wiring, baud 9600, and newline. |
| `AT` gives no `OK` | Check full AT mode / KEY, AT UART speed 38400, voltage/wiring, Serial Monitor 9600 + Both NL & CR. |
| `AT+PSWD=1234` returns `ERROR:(1D)` | Try `AT+PSWD="1234"`; inspect `AT+VERSION?` and `AT+PSWD?`. |
| `AT+BIND?` returns `0000:00:000000` | Binding has not been saved; re-enter full AT mode, set Master role and CMODE=0, retry correct address syntax. |
| Messages are concatenated/not parsed | Master **must use `BT.println(flag)`** to terminate each message with newline. Both sides must use identical field names: `IR`, `limit`, `FLAG`. |
| OLED blank | Check 4-pin SH1106 model, 0x3C vs 0x3D, SDA=A4, SCL=A5, power supply, library/constructor. An I²C scanner can identify detected addresses. |
| ADXL345 not detected | Check 0x53 vs 0x1D, I²C wiring, CS high for I²C, address strap, 3.3 V logic. |
| Tilt doesn't trigger | Your actual Y/Z readings may fall outside the empirical ranges. Watch Serial Monitor and adjust thresholds to match mounting orientation. |
| Random restarts / unstable | Ensure adequate supply, common ground, appropriate level shifting, and no conflicting 5 V I²C pull-ups. |

## 8. Future enhancements

- Fuse ADXL345 readings with a gyroscope (e.g. MPU6050) for dynamic rotation estimation.
- Require IR **and** limit-switch conditions for helmet confirmation if your mechanical design calls for it.
- Add Bluetooth connection watchdog, buzzer, event logging, and a latching alarm requiring reset.
- Use sensor debouncing and optional packet checksums/sequence counters for improved robustness.

## References

- [Analog Devices ADXL345 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/adxl345.pdf)
- [U8g2 project](https://github.com/olikraus/u8g2)
- [Adafruit ADXL345 library](https://github.com/adafruit/Adafruit_ADXL345)

> HC-05 AT commands vary across firmware revisions. The commands shown here reflect the commonly used `AT+ROLE`, `AT+CMODE`, `AT+BIND`, `AT+PAIR` and `AT+LINK` family. Always verify replies on your own modules.
