// Captive-portal WiFi provisioning. On first boot (or when forced) the device
// starts a SoftAP, runs a DNS catch-all + a small web server, lets the user
// pick an SSID, enter a password, and optionally a manual WiThrottle host
// and port. Settings are persisted to NVS via Preferences.

#pragma once

#include <Arduino.h>
#include <IPAddress.h>

namespace Provision {

struct Stored {
    String ssid;
    String password;
    String host;   // optional manual host; empty => use mDNS discovery
    uint16_t port = 0;  // 0 => default
};

// Load stored credentials from NVS. Returns false if no SSID is set.
bool load(Stored &out);

// Persist credentials to NVS. Empty strings clear the slot.
void save(const Stored &s);

// Wipe all stored credentials. Used when the user holds BtnB at boot.
void clearAll();

// Start the captive portal. SSID is built as PROVISION_AP_PREFIX + last 4
// hex chars of the MAC. Returns the actual AP SSID and IP via the out
// params. Non-blocking: call tick() each loop until isDone() returns true.
void begin(String &outSsid, IPAddress &outIp);

void tick();

bool isDone();

// After isDone() == true, retrieve the credentials the user submitted.
const Stored &result();

// Tear down the SoftAP, DNS server and web server. Call before switching to
// STA mode.
void end();

}  // namespace Provision
