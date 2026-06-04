# WiThrottleProtocol on M5StickC Plus 2 / M5StickS3 (with MiniEncoderC HAT)

A complete portable WiThrottle throttle that runs on an
[M5StickC Plus 2](https://docs.m5stack.com/en/core/M5StickC%20PLUS2)
or an [M5StickS3](https://docs.m5stack.com/en/core/StickS3) with the
[M5Stack MiniEncoderC HAT (SKU U157)](https://docs.m5stack.com/en/hat/MiniEncoderC%20Hat)
plugged into the top 8-pin connector. Connects to JMRI (or any other
WiThrottle server) over WiFi, lets you pick a loco from the JMRI roster on
the device, and drives it with a centre-zero rotary throttle.

A single source builds for either Stick — the two devices share the same
135x240 TFT, the same 8-pin HAT pinout, and the same M5Unified APIs. Pick
the appropriate PlatformIO env or Arduino board entry below.

## Hardware

| Part                                | Notes                                |
|-------------------------------------|--------------------------------------|
| M5StickC Plus 2 (SKU K128)          | ESP32-PICO-V3-02, 135x240 TFT in     |
|                                     | portrait, 200 mAh battery            |
| M5StickS3 (SKU K150)                | ESP32-S3 sibling of the Plus 2 —     |
|                                     | same 135x240 TFT, same HAT pinout,   |
|                                     | 250 mAh battery, USB-C OTG           |
| M5Stack MiniEncoderC HAT (U157)     | I2C @ 0x42, rotary encoder + push    |
|                                     | button + single RGB LED              |

Press the HAT onto the 8-pin connector — no soldering. The HAT speaks I²C on
`SDA = GPIO 0` and `SCL = GPIO 26`, the bus M5Unified leaves alone.

## Getting started

The whole flow, from a fresh Stick to driving a loco, takes about
five minutes.

### 1. Install the toolchain

Pick one — both work. PlatformIO gives you headless builds and faster
flashing; Arduino IDE has a friendlier UI for tweaking single files.

**PlatformIO** (recommended for repeat builds):

Create `platformio.ini` in a folder containing a copy of this sketch.
Use the env block that matches your hardware — both share the same
`lib_deps` and `build_flags`.

```ini
[env:m5stickc-plus2]
platform = espressif32
board = m5stick-c-plus2          ; needs platform-espressif32 >= 6.6.0
framework = arduino
monitor_speed = 115200
upload_speed = 1500000
lib_deps =
    m5stack/M5Unified
    https://github.com/flash62au/WiThrottleProtocol.git
build_flags = -DCORE_DEBUG_LEVEL=0

[env:m5sticks3]
platform = espressif32
board = esp32-s3-devkitc-1       ; verify against your platform-espressif32;
                                  ; some versions ship m5stack-sticks3
framework = arduino
monitor_speed = 115200
upload_speed = 1500000
lib_deps =
    m5stack/M5Unified
    https://github.com/flash62au/WiThrottleProtocol.git
build_flags = -DCORE_DEBUG_LEVEL=0 -DARDUINO_USB_CDC_ON_BOOT=1
```

If your platform-espressif32 is older and doesn't have the `m5stick-c-plus2`
board, use `board = m5stick-c-plus` and add
`board_build.partitions = default.csv` — the binary is small enough. The
`m5sticks3` env likewise falls back to any generic ESP32-S3 board id if the
M5-specific one is missing from your platform version.

**Arduino IDE 2.x**:

1. Boards Manager → install **M5Stack** (the package by M5Stack
   Technology Co.). Version 2.1.0 or newer adds both the "M5StickC Plus 2"
   and "M5StickS3" entries.
2. Tools → Board → M5Stack → **M5StickC Plus 2** or **M5StickS3** depending
   on which device you have.
3. Library Manager → install **M5Unified** and **WiThrottleProtocol**.
4. Open `WiThrottleProtocol_M5StickCPlus2.ino` (the sketch folder name is
   historical — it builds for both Sticks). The IDE compiles every `.h` /
   `.cpp` in the sketch folder automatically.

### 2. Flash the firmware

USB-C, hold the power button (the red one) for ~2 s to power on, then upload
from your IDE. Reset (six-second hold) if the bootloader doesn't catch the
first time. You should see a "WiThrottle / M5Stick" splash.

### 3. Connect to your WiFi (first boot)

The device starts an open WiFi access point named `WiThrottle-XXXX` (last
four hex digits of the MAC) and shows the SSID + portal URL on the TFT.

1. On a phone or laptop, join that SSID. Most OSes pop a captive-portal
   sheet; if not, browse to `http://192.168.4.1/`.
2. Pick your home WiFi network from the scan list, enter the password.
3. **Server host / port** — leave blank to auto-discover JMRI via mDNS, or
   enter `192.168.x.y` and `12090` to skip discovery.
4. Press **Save & connect**. The device reboots and connects.

If you want to redo this later, **hold BtnB** (the side button) while
powering on — that clears the stored credentials.

### 4. Start JMRI's WiThrottle server

In JMRI's main menu: **Tools → Throttles → Start WiThrottle Server**. JMRI
advertises itself via mDNS, so the throttle finds it automatically on the
same subnet. Make sure your JMRI host is on the same WiFi network and on
**channel 10 or below** (the ESP32 can't see higher 2.4 GHz channels).

### 5. Pick a loco and drive

You'll land in the roster picker. Rotate the encoder to highlight a loco
and **push** the encoder to acquire it. After a brief "Acquiring…" splash
the drive view appears, with a centre-zero vertical slider on the right.

Rotate clockwise to add forward speed, counter-clockwise for reverse — the
encoder LED turns green or red to match. Crossing zero automatically issues
a stop-then-flip-direction. Push the encoder to e-stop.

Press **BtnB** to cycle through additional screens: Functions (F1–F12),
**Layout** (turnouts and routes from JMRI), and Status. **BtnA** returns to
Drive from any of them. On the Layout screen, the encoder selects an item
within the active tab, encoder short-press activates it (toggles a turnout
or fires a route), and encoder long-press flips between the Turnouts and
Routes tabs.

## Controls

| Input                          | Action                                              |
|--------------------------------|-----------------------------------------------------|
| Encoder rotation               | Signed throttle in `[-126 .. +126]`. CW from zero   |
|                                | adds forward speed; CCW from zero adds reverse.     |
| Encoder push (short)           | E-stop — throttle snaps to centre and sends         |
|                                | `emergencyStop` to the server.                      |
| Encoder push (long, 1 s)       | Soft centre — sets speed to 0 without e-stop.       |
| Encoder LED                    | Green = forward, red = reverse, off = stopped.      |
| BtnA (front, short)            | Toggle F0 (lights).                                 |
| BtnA (long)                    | Flip polarity (swap CW/CCW meaning, for locos       |
|                                | facing the other way). Persisted to NVS.            |
| BtnB (side, short)             | Cycle screens: Drive → Functions → Layout → Status. |
| BtnB (long)                    | Release the loco and return to the roster picker.   |
| BtnB (held at boot)            | Clear NVS and re-enter setup.                       |

Layout screen controls (turnouts and routes):

| Input                          | Action                                              |
|--------------------------------|-----------------------------------------------------|
| Encoder rotate                 | Move highlight within the active tab.               |
| Encoder push (short)           | Turnout: toggle Close ↔ Throw via `setTurnout`.     |
|                                | Route: activate via `setRoute`.                     |
| Encoder push (long, 1 s)       | Flip tab: Turnouts ↔ Routes.                        |
| BtnA                           | Back to Drive (loco stays acquired).                |

Zero-crossings work like this: as you turn through zero the sketch snaps
the throttle to exactly 0 for one detent and sends `setSpeed(0)`. The next
detent on the other side issues `setDirection(...)` and starts ramping on
the new side. Both the local state and the server stay aligned even if
another client (e.g. WiThrottle on iOS) changes the loco — the
`receivedSpeedMultiThrottle` / `receivedDirectionMultiThrottle` callbacks
fold the server's view back into the local slider.

## Troubleshooting

- **Splash says "MiniEncoderC not detected on I2C 0x42"** — the HAT isn't
  seated, or you have a different M5 encoder. Pop it on firmly. If you
  swap to the original ENCODER HAT (A031) you'll need to change
  `ENCODER_HAT_ADDR` and register offsets in `config.h`.
- **WiFi setup loops back to the captive portal** — verify your AP is on
  2.4 GHz and channel 10 or below. The ESP32 silently refuses higher
  channels.
- **Roster never loads** — the throttle connected to *something* on port
  12090 that wasn't JMRI WiThrottle. Hold BtnB at boot and enter a manual
  host explicitly.
- **Throttle disconnects after a minute of idle** — should not happen
  (WiThrottleProtocol sends heartbeats automatically). If it does, check
  JMRI's WiThrottle preferences for a heartbeat interval > 0.
- **LED is too bright** — adjust the `0x002000` / `0x200000` levels in
  `updateThrottleLed()` in the `.ino`.

## Constraints worth knowing

- ESP32 only supports 2.4 GHz WiFi and struggles above channel 10. The
  provisioning portal flags any AP above ch 10.
- The built-in 200 mAh battery gives roughly three to four hours of
  continuous use. The TFT auto-dims after 30 s of input inactivity.
- Only the first WiThrottle slot (`'0'`) is used. Multi-loco consists are
  still supported because the WiThrottle protocol lets you add multiple
  locos to the same slot.
- Roster, turnouts and routes loaded from JMRI are kept in RAM. The MAX
  loco roster size is bounded only by free heap (~200 kB on this part).

## Files

```
WiThrottleProtocol_M5StickCPlus2.ino  state machine, setup() and loop()
AppDelegate.h                          WiThrottleProtocolDelegate subclass
UI.h / UI.cpp                          M5GFX rendering helpers
EncoderHat.h / EncoderHat.cpp          I2C driver for the MiniEncoderC HAT
Provision.h / Provision.cpp            SoftAP + captive portal + NVS
config.h                               pin/I2C constants, NVS keys, tunables
```
