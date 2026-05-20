// WiThrottleProtocolDelegate subclass that buffers roster entries and mirrors
// the loco state pushed from the server. The state machine in the .ino reads
// from these fields each loop.

#pragma once

#include <Arduino.h>
#include <WiThrottleProtocol.h>
#include <vector>

#include "config.h"

struct RosterEntry {
    String name;
    int address;     // raw DCC address
    char length;     // 'S' or 'L'

    // WiThrottle address string ("S10" / "L1234") suitable for addLocomotive().
    String witAddress() const {
        return String(length) + String(address);
    }
};

struct TurnoutEntry {
    String       sysName;   // e.g. "LT92" — passed to setTurnout()
    String       userName;  // friendly label for the UI
    TurnoutState state = TurnoutUnknown;
};

struct RouteEntry {
    String     sysName;     // e.g. "IO:AUTO:0008" — passed to setRoute()
    String     userName;
    RouteState state = RouteInconsistent;
};

class AppDelegate : public WiThrottleProtocolDelegate {
  public:
    // ---- server-pushed state (read-only from the sketch's POV) ----
    String  version;
    String  serverType;
    String  serverDescription;
    String  lastAlert;
    String  lastMessage;

    int     heartbeatPeriod = 0;  // seconds; 0 = unknown

    // Roster buffering. populated == true once we've received all entries.
    int     expectedRosterSize = -1;
    std::vector<RosterEntry> roster;
    bool    rosterPopulated = false;

    // Turnout / route buffering. The library does not keep these for us.
    int     expectedTurnoutSize = -1;
    std::vector<TurnoutEntry> turnouts;
    bool    turnoutsPopulated = false;

    int     expectedRouteSize = -1;
    std::vector<RouteEntry> routes;
    bool    routesPopulated = false;

    // Mirror of throttle slot state (we only use slot THROTTLE_SLOT here).
    int       mirroredSpeed = 0;
    Direction mirroredDirection = Forward;
    bool      mirroredFunctions[MAX_FUNCTIONS] = {false};

    // Set true any time mirrored* changes so the UI knows to repaint.
    volatile bool dirty = false;

    // Set true when the server confirms a loco was added/removed.
    String  acquiredAddress;
    bool    locoAcquired = false;

    // ---- WiThrottleProtocolDelegate overrides ----
    void receivedVersion(String v) override {
        version = v; dirty = true;
    }
    void receivedServerType(String s) override {
        serverType = s; dirty = true;
    }
    void receivedServerDescription(String s) override {
        serverDescription = s; dirty = true;
    }
    void receivedAlert(String a) override {
        lastAlert = a; dirty = true;
    }
    void receivedMessage(String m) override {
        lastMessage = m; dirty = true;
    }
    void heartbeatConfig(int seconds) override {
        heartbeatPeriod = seconds;
    }

    void receivedRosterEntries(int rosterSize) override {
        expectedRosterSize = rosterSize;
        roster.clear();
        roster.reserve(rosterSize);
        rosterPopulated = (rosterSize == 0);
        dirty = true;
    }
    void receivedRosterEntry(int /*index*/, String name, int address,
                             char length) override {
        RosterEntry e;
        e.name = name;
        e.address = address;
        e.length = length;
        roster.push_back(e);
        if (expectedRosterSize > 0 &&
            (int)roster.size() >= expectedRosterSize) {
            rosterPopulated = true;
        }
        dirty = true;
    }

    void addressAddedMultiThrottle(char multiThrottle, String address,
                                   String /*entry*/) override {
        if (multiThrottle != THROTTLE_SLOT) return;
        acquiredAddress = address;
        locoAcquired = true;
        // Reset mirrored state; server will push real values next.
        mirroredSpeed = 0;
        mirroredDirection = Forward;
        for (auto &f : mirroredFunctions) f = false;
        dirty = true;
    }
    void addressRemovedMultiThrottle(char multiThrottle, String /*addr*/,
                                     String /*cmd*/) override {
        if (multiThrottle != THROTTLE_SLOT) return;
        acquiredAddress = "";
        locoAcquired = false;
        mirroredSpeed = 0;
        dirty = true;
    }

    void receivedSpeedMultiThrottle(char multiThrottle, int speed) override {
        if (multiThrottle != THROTTLE_SLOT) return;
        mirroredSpeed = speed;
        dirty = true;
    }
    void receivedDirectionMultiThrottle(char multiThrottle,
                                        Direction dir) override {
        if (multiThrottle != THROTTLE_SLOT) return;
        mirroredDirection = dir;
        dirty = true;
    }
    void receivedFunctionStateMultiThrottle(char multiThrottle, uint8_t func,
                                            bool state) override {
        if (multiThrottle != THROTTLE_SLOT) return;
        if (func < MAX_FUNCTIONS) mirroredFunctions[func] = state;
        dirty = true;
    }

    // ---- turnouts ----
    void receivedTurnoutEntries(int n) override {
        expectedTurnoutSize = n;
        turnouts.clear();
        turnouts.reserve(n);
        turnoutsPopulated = (n == 0);
        dirty = true;
    }
    void receivedTurnoutEntry(int /*index*/, String sysName, String userName,
                              int state) override {
        TurnoutEntry t;
        t.sysName = sysName;
        t.userName = userName;
        t.state = (TurnoutState)state;
        turnouts.push_back(t);
        if (expectedTurnoutSize > 0 &&
            (int)turnouts.size() >= expectedTurnoutSize) {
            turnoutsPopulated = true;
        }
        dirty = true;
    }
    void receivedTurnoutAction(String systemName, TurnoutState state) override {
        for (auto &t : turnouts) {
            if (t.sysName == systemName) {
                t.state = state;
                dirty = true;
                return;
            }
        }
    }

    // ---- routes ----
    void receivedRouteEntries(int n) override {
        expectedRouteSize = n;
        routes.clear();
        routes.reserve(n);
        routesPopulated = (n == 0);
        dirty = true;
    }
    void receivedRouteEntry(int /*index*/, String sysName, String userName,
                            int state) override {
        RouteEntry r;
        r.sysName = sysName;
        r.userName = userName;
        r.state = (RouteState)state;
        routes.push_back(r);
        if (expectedRouteSize > 0 &&
            (int)routes.size() >= expectedRouteSize) {
            routesPopulated = true;
        }
        dirty = true;
    }
    void receivedRouteAction(String systemName, RouteState state) override {
        for (auto &r : routes) {
            if (r.sysName == systemName) {
                r.state = state;
                dirty = true;
                return;
            }
        }
    }
};
