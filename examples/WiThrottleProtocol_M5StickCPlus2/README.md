# WiThrottleProtocol on M5StickC Plus 2 (with MiniEncoderC HAT)

A complete portable WiThrottle throttle that runs on an
[M5StickC Plus 2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2)
with the [M5Stack MiniEncoderC HAT (SKU U157)](https://docs.m5stack.com/en/hat/MiniEncoderC%20Hat)
plugged into the top 8-pin connector. Connects to JMRI (or any other
WiThrottle server) over WiFi, lets you pick a loco from the JMRI roster on
the device, and drives it with a centre-zero rotary throttle.

## Hardware

| Part                                | Notes                                |
|-------------------------------------|--------------------------------------|
| M5StickC Plus 2                     | ESP32-PICO-V3-02, 240x135 TFT        |
| M5Stack MiniEncoderC HAT (U157)     | I2C @ 0x42, rotary + push button +   |
|                                     | RGB LED                              |

Just press the HAT onto the 8-pin connector. No soldering. The HAT speaks
I²C on `SDA = GPIO 0` and `SCL = GPIO 26` — the same bus M5Unified leaves
alone.

## Toolchain

This sketch is an Arduino sketch and builds with either Arduino IDE or
PlatformIO. It targets the standard `m5stack-stickc-plus2` board.

### Required libraries

- `M5Unified` — pulled with `M5GFX`
- `WiThrottleProtocol` (this library)
- Everything else (`WiFi`, `WebServer`, `DNSServer`, `Preferences`, `ESPmDNS`,
  `Wire`) ships with the Arduino-ESP32 core.

### PlatformIO

A minimal `platformio.ini` for this sketch:

```ini
[env:m5stack-stickc-plus2]
platform = espressif32
board = m5stick-c
framework = arduino
board_build.mcu = esp32
upload_speed = 1500000
monitor_speed = 115200
lib_deps =
    m5stack/M5Unified
    https://github.com/flash62au/WiThrottleProtocol.git
build_flags = -DCORE_DEBUG_LEVEL=0
```

### Arduino IDE

1. Install board package "M5Stack" via Boards Manager.
2. Select board "M5StickC-Plus2".
3. Install libraries: `M5Unified`, `WiThrottleProtocol`.
4. Open this folder as a sketch (the `.ino` plus the `.h/.cpp` siblings are
   compiled together automatically).

## First boot

1. The device powers on and immediately starts an open WiFi access point
   named `WiThrottle-XXXX` (last four hex digits of the MAC).
2. Connect a phone or laptop to that SSID. Most OSes pop a captive-portal
   sheet; if not, browse to `http://192.168.4.1/`.
3. Pick your home WiFi network from the scan list. Enter the password.
   Optionally enter a server host/port (leave blank to auto-discover JMRI
   via mDNS).
4. Hit **Save & connect**. The device reboots and connects to your WiFi.

Hold **BtnB** (the small side button) while powering on to clear stored
credentials and re-run setup.

## Driving

| Input                          | Action                                              |
|--------------------------------|-----------------------------------------------------|
| Encoder rotation               | Signed throttle in `[-126 .. +126]`. CW from zero   |
|                                | adds forward speed; CCW from zero adds reverse.     |
| Encoder push (short)           | E-stop — throttle snaps to centre and sends         |
|                                | `emergencyStop` to the server.                      |
| Encoder push (long, 1 s)       | Gentle centre — sets speed to 0 without e-stop.     |
| BtnA (front, short)            | Toggle F0 (lights).                                 |
| BtnA (long)                    | Flip polarity (swap CW/CCW meaning, in case the     |
|                                | loco is facing the other way). Saved to NVS.        |
| BtnB (short)                   | Open the F1-F12 function grid.                      |
| BtnB (long)                    | Release the loco and return to the roster picker.   |
| BtnB (held at boot)            | Clear NVS and re-enter setup.                       |

Zero-crossings work like this: as you turn through zero the sketch snaps
the throttle to exactly 0 for one detent and sends `setSpeed(0)`. The next
detent on the other side issues `setDirection(...)` and starts ramping on
the new side. Both the local state and the server stay aligned even if
another client (e.g. WiThrottle on iOS) changes the loco — the
`receivedSpeedMultiThrottle` / `receivedDirectionMultiThrottle` callbacks
fold the server's view back into the local slider.

## Constraints worth knowing

- The ESP32 only supports 2.4 GHz WiFi and struggles above channel 10. The
  provisioning portal flags any AP above ch 10.
- The built-in 200 mAh battery gives roughly three to four hours of
  continuous use. The TFT auto-dims after 30 s of input inactivity.
- Only the first WiThrottle slot (`'0'`) is used. Multi-loco consists are
  still supported because the WiThrottle protocol lets you add multiple
  locos to the same slot.

## Files

```
WiThrottleProtocol_M5StickCPlus2.ino  state machine, setup() and loop()
AppDelegate.h                          WiThrottleProtocolDelegate subclass
UI.h / UI.cpp                          M5GFX rendering helpers
EncoderHat.h / EncoderHat.cpp          I2C driver for the MiniEncoderC HAT
Provision.h / Provision.cpp            SoftAP + captive portal + NVS
config.h                               pin/I2C constants, NVS keys, tunables
```
