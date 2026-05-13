// Compile-time defaults and pin/I2C constants for the
// M5StickC Plus 2 portable throttle.

#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// HAT I2C bus (M5StickC Plus 2 8-pin top connector)
// ---------------------------------------------------------------------------
// On M5StickC / M5StickC Plus 2 the HAT pins expose I2C on:
//   SDA = GPIO 0
//   SCL = GPIO 26
// Speed kept at standard 100 kHz; the Encoder HAT's MCU does not need fast mode.
constexpr int HAT_I2C_SDA = 0;
constexpr int HAT_I2C_SCL = 26;
constexpr uint32_t HAT_I2C_HZ = 100000UL;

// ---------------------------------------------------------------------------
// M5Stack ENCODER HAT (SKU A031) register map
// ---------------------------------------------------------------------------
// The HAT mounts a small MCU that exposes the rotary encoder + push button
// over I2C @ 0x5E with this layout (matches the upstream M5HAT-Encoder
// Arduino driver):
//   0x00          absolute encoder value, signed int32 little-endian
//   0x10          incremental encoder count, signed int32 little-endian
//                 (cleared on read)
//   0x20          button state, 1 byte (0 = pressed, 1 = released)
//   0x70..0x75    RGB LEDs (2 x 3 bytes, 0xRR 0xGG 0xBB per LED)
// If you have a different M5 encoder product (Unit, 8-Angle, etc.) adjust
// the address and register names; everything below is isolated in
// EncoderHat.cpp.
constexpr uint8_t ENCODER_HAT_ADDR        = 0x5E;
constexpr uint8_t ENCODER_REG_INC_COUNT   = 0x10;
constexpr uint8_t ENCODER_REG_BUTTON      = 0x20;
constexpr uint8_t ENCODER_REG_LED_RGB     = 0x70;

// ---------------------------------------------------------------------------
// Throttle behaviour
// ---------------------------------------------------------------------------
// Centre-zero bipolar slider: position in [-MAX_SPEED .. +MAX_SPEED].
// Positive = Forward, negative = Reverse, zero = stop.
// WiThrottle speeds are 0..126 in 128-step mode; we mirror that.
constexpr int16_t THROTTLE_MAX_SPEED = 126;

// Detent multipliers. The HAT returns one count per detent; the sketch
// optionally accelerates if the user rotates quickly.
constexpr int16_t THROTTLE_STEP_SLOW = 1;
constexpr int16_t THROTTLE_STEP_FAST = 4;
constexpr uint32_t THROTTLE_FAST_WINDOW_MS = 60; // detents arriving within
                                                  // this window count as fast

// Encoder poll cadence (ms). 50 Hz is plenty for human input and leaves the
// CPU free for wiThrottleProtocol.check() and TFT draws.
constexpr uint32_t ENCODER_POLL_MS = 20;

// Button long-press threshold (ms). Same for encoder push and BtnA/BtnB.
constexpr uint32_t LONG_PRESS_MS = 1000;

// ---------------------------------------------------------------------------
// WiFi / server
// ---------------------------------------------------------------------------
// SoftAP SSID prefix for the captive-portal provisioning step. The sketch
// appends the last 4 hex digits of the MAC.
#define PROVISION_AP_PREFIX "WiThrottle-"
// Captive-portal password. Empty string => open AP. Keeping it open here
// because the AP is short-lived and only used for first-time setup.
#define PROVISION_AP_PASSWORD ""

// STA connect timeout before falling back to provisioning.
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;

// mDNS service to browse for. JMRI publishes "_withrottle._tcp.local."
#define MDNS_SERVICE_NAME "withrottle"
#define MDNS_SERVICE_PROTO "tcp"
constexpr uint32_t MDNS_QUERY_TIMEOUT_MS = 10000;

// Default WiThrottle TCP port if a manual host:port is configured without a
// port suffix.
constexpr int DEFAULT_WITHROTTLE_PORT = 12090;

// Throttle slot ('0' = first multiThrottle; library README recommends '0'
// for single-throttle clients).
constexpr char THROTTLE_SLOT = '0';

// Backoff schedule (ms) for the server reconnect ladder.
constexpr uint32_t RECONNECT_BACKOFF_MS[] = {1000, 2000, 4000, 8000, 16000};
constexpr size_t RECONNECT_BACKOFF_COUNT =
    sizeof(RECONNECT_BACKOFF_MS) / sizeof(RECONNECT_BACKOFF_MS[0]);

// ---------------------------------------------------------------------------
// NVS keys (kept short; Preferences allows max 15 chars per key)
// ---------------------------------------------------------------------------
#define NVS_NAMESPACE   "wit-throttle"
#define NVS_KEY_SSID    "ssid"
#define NVS_KEY_PASS    "pass"
#define NVS_KEY_HOST    "host"   // optional manual host (string)
#define NVS_KEY_PORT    "port"   // optional manual port (uint16)
#define NVS_KEY_LASTLOCO "lastloco" // last acquired loco, e.g. "S10"
#define NVS_KEY_POLARITY "polarity" // 0 = normal, 1 = flipped (per-device)

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------
// M5StickC Plus 2 TFT is 240 wide x 135 tall when rotated to landscape.
constexpr int16_t TFT_W = 240;
constexpr int16_t TFT_H = 135;

// Display auto-dim after this much idle time. Set to 0 to disable.
constexpr uint32_t DISPLAY_DIM_AFTER_MS = 30000;
constexpr uint8_t DISPLAY_BRIGHT = 200;
constexpr uint8_t DISPLAY_DIM    = 40;
