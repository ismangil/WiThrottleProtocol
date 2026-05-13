// Thin I2C wrapper for the M5Stack MiniEncoderC HAT (SKU U157).
//
// The HAT exposes a signed int32 incremental counter at 0x10 that is cleared
// on every read, plus a momentary push-button on the encoder shaft at 0x20.
// This class polls both and emits debounced edge events suitable for the
// state machine in the main sketch.

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "config.h"

class EncoderHat {
  public:
    enum class ButtonEvent : uint8_t {
        None,
        ShortPress,
        LongPress,
    };

    // Initialise the I2C bus and probe the HAT. Returns true if the device
    // ACKed at ENCODER_HAT_ADDR.
    bool begin(TwoWire &wire = Wire);

    // Poll the encoder. Returns true if anything changed (delta != 0 or a
    // button event was produced). Safe to call every loop().
    bool poll();

    // Net rotation since the last poll() call. Positive = clockwise.
    int16_t consumeDelta();

    // Drains and returns the most recent button event (or None).
    ButtonEvent consumeButtonEvent();

    // Optional: drive the on-board RGB LED. Value is 0xRRGGBB; the driver
    // handles the BGR byte order the HAT firmware expects. Silently ignored
    // if the HAT is not present.
    void setLed(uint32_t rgb888);

    bool isPresent() const { return present_; }

  private:
    TwoWire *wire_ = nullptr;
    bool present_ = false;

    // Held until consumeDelta() / consumeButtonEvent() is called.
    int16_t pendingDelta_ = 0;
    ButtonEvent pendingEvent_ = ButtonEvent::None;

    // Button tracking
    bool buttonDown_ = false;
    bool longFired_ = false;
    uint32_t buttonDownAt_ = 0;
    uint32_t lastPollAt_ = 0;
};
