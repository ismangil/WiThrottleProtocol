// TFT rendering for the portable throttle. All M5GFX calls live here so the
// state machine stays focused on logic.

#pragma once

#include <Arduino.h>
#include <WiThrottleProtocol.h>
#include <vector>

#include "AppDelegate.h"
#include "config.h"

namespace UI {

void begin();

// Top-of-screen banner + clear. Useful for "connecting…" etc.
void splash(const String &line1, const String &line2 = "");

// WiFi provisioning screen — shows the SoftAP details to the user.
void provisioning(const String &apSsid, const IPAddress &ip);

// "Scanning for WiThrottle servers via mDNS…" status.
void serverDiscovery(const String &status);

// Roster list view. selectedIndex is the highlighted row; scrollOffset is the
// index of the first visible row. Pass scrollOffset by reference so the UI
// can adjust it to keep the selection on screen.
void roster(const std::vector<RosterEntry> &entries,
            int selectedIndex,
            int &scrollOffset);

// Main drive view. throttlePos is the signed slider position
// (-MAX_SPEED..+MAX_SPEED). polarityFlipped inverts the on-screen orientation
// only; sign convention stays the same.
struct DriveStatus {
    int16_t throttlePos;
    Direction direction;            // mirrored from server
    String  locoName;
    String  locoAddress;            // e.g. "S10"
    bool    wifiOk;
    bool    serverOk;
    int8_t  batteryPct;             // 0..100, -1 if unknown
    bool    polarityFlipped;
    const bool *functions;          // pointer to MAX_FUNCTIONS bool array
};
void drive(const DriveStatus &s);

// Function grid: 4x3 buttons F1..F12. selected is the focused function
// number (1..12).
void functions(const bool *fnState, uint8_t selected);

// Layout view: turnouts and routes on one screen with a tab toggle.
enum class LayoutTab : uint8_t { Turnouts, Routes };

void layout(const std::vector<TurnoutEntry> &turnouts,
            const std::vector<RouteEntry> &routes,
            LayoutTab tab,
            int turnoutIdx, int &turnoutScroll,
            int routeIdx, int &routeScroll);

// Status / about screen: WiFi RSSI, IP, server host, version, heartbeat,
// battery, mode, build date.
struct StatusInfo {
    String wifiSsid;
    String wifiIp;
    int    wifiRssi;
    String serverHost;
    int    serverPort;
    String witVersion;
    int    heartbeatSec;
    int8_t batteryPct;
};
void statusScreen(const StatusInfo &s);

// A one-line transient banner (alerts, broadcast messages, errors). The
// caller is responsible for clearing it by triggering a full redraw.
void banner(const String &text, uint16_t color);

// Brightness helpers.
void setBright(uint8_t v);

}  // namespace UI
