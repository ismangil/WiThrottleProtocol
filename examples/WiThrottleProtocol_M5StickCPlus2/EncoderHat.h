// Thin I2C wrapper for the M5Stack ENCODER HAT (SKU A031).
//
// The HAT's ATtiny202 maintains a signed 16-bit incremental counter that is
// cleared on every read, plus a momentary push-button on the encoder shaft.
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

    // Optional: drive the two on-board RGB LEDs. Each value is 0xRRGGBB.
    // Silently ignored if the HAT is not present.
    void setLeds(uint32_t left, uint32_t right);

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
