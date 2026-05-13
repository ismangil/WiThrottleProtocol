#include "EncoderHat.h"

namespace {

bool readReg(TwoWire *wire, uint8_t reg, uint8_t *buf, size_t n) {
    wire->beginTransmission(ENCODER_HAT_ADDR);
    wire->write(reg);
    if (wire->endTransmission(false) != 0) return false;
    size_t got = wire->requestFrom((int)ENCODER_HAT_ADDR, (int)n);
    if (got != n) return false;
    for (size_t i = 0; i < n; i++) buf[i] = wire->read();
    return true;
}

bool writeReg(TwoWire *wire, uint8_t reg, const uint8_t *buf, size_t n) {
    wire->beginTransmission(ENCODER_HAT_ADDR);
    wire->write(reg);
    for (size_t i = 0; i < n; i++) wire->write(buf[i]);
    return wire->endTransmission() == 0;
}

}  // namespace

bool EncoderHat::begin(TwoWire &wire) {
    wire_ = &wire;
    // The HAT pins are wired to GPIO 0 (SDA) and GPIO 26 (SCL) on M5StickC
    // Plus 2. M5Unified may have already set Wire up on a different pair, so
    // re-initialise explicitly.
    wire_->end();
    wire_->begin(HAT_I2C_SDA, HAT_I2C_SCL, HAT_I2C_HZ);

    // Probe with a one-byte read of the button register.
    uint8_t b;
    present_ = readReg(wire_, ENCODER_REG_BUTTON, &b, 1);
    return present_;
}

bool EncoderHat::poll() {
    if (!present_) return false;
    const uint32_t now = millis();
    lastPollAt_ = now;

    bool changed = false;

    // ----- rotation -----
    // The HAT exposes the incremental counter as a signed int32 that is
    // cleared on every read. Clamp to int16 because nobody can spin the knob
    // 32 000 detents in 20 ms.
    uint8_t raw[4] = {0, 0, 0, 0};
    if (readReg(wire_, ENCODER_REG_INC_COUNT, raw, 4)) {
        int32_t delta32 = (int32_t)((uint32_t)raw[0] |
                                    ((uint32_t)raw[1] << 8) |
                                    ((uint32_t)raw[2] << 16) |
                                    ((uint32_t)raw[3] << 24));
        if (delta32 != 0) {
            if (delta32 > INT16_MAX) delta32 = INT16_MAX;
            if (delta32 < INT16_MIN) delta32 = INT16_MIN;
            pendingDelta_ += (int16_t)delta32;
            changed = true;
        }
    }

    // ----- button -----
    uint8_t bs = 1;
    if (readReg(wire_, ENCODER_REG_BUTTON, &bs, 1)) {
        const bool down = (bs == 0);
        if (down && !buttonDown_) {
            buttonDown_ = true;
            longFired_ = false;
            buttonDownAt_ = now;
        } else if (!down && buttonDown_) {
            buttonDown_ = false;
            if (!longFired_) {
                pendingEvent_ = ButtonEvent::ShortPress;
                changed = true;
            }
        } else if (down && !longFired_ &&
                   (now - buttonDownAt_) >= LONG_PRESS_MS) {
            longFired_ = true;
            pendingEvent_ = ButtonEvent::LongPress;
            changed = true;
        }
    }

    return changed;
}

int16_t EncoderHat::consumeDelta() {
    const int16_t d = pendingDelta_;
    pendingDelta_ = 0;
    return d;
}

EncoderHat::ButtonEvent EncoderHat::consumeButtonEvent() {
    const ButtonEvent e = pendingEvent_;
    pendingEvent_ = ButtonEvent::None;
    return e;
}

void EncoderHat::setLed(uint32_t rgb888) {
    if (!present_) return;
    // MiniEncoderC firmware expects 3 bytes in BGR order (B, G, R).
    uint8_t buf[3] = {
        (uint8_t)(rgb888 & 0xFF),         // B
        (uint8_t)((rgb888 >> 8) & 0xFF),  // G
        (uint8_t)((rgb888 >> 16) & 0xFF), // R
    };
    writeReg(wire_, ENCODER_REG_LED_BGR, buf, sizeof(buf));
}
