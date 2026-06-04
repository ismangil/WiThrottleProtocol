// WiThrottleProtocol portable throttle for M5StickC Plus 2 + MiniEncoderC HAT
// (SKU U157).
//
// Features:
//   * Captive-portal WiFi provisioning on first boot (or hold BtnB at power-on
//     to redo it). Credentials and optional server host/port are stored in
//     NVS via the Preferences API.
//   * mDNS auto-discovery of WiThrottle servers (e.g. JMRI). The first match
//     is used; the manual host from setup takes precedence if present.
//   * Roster picker on the TFT — rotate the encoder to highlight a loco,
//     push the encoder to acquire it.
//   * Centre-zero bipolar slider: rotate clockwise from zero to add forward
//     speed; counter-clockwise to add reverse speed. Crossing zero issues
//     a stop-then-flip-direction sequence automatically.
//   * F0..F12 function support (F0 on BtnA, F1-F12 in a 4x3 grid).
//   * Reconnect ladder when the server disappears.
//
// See README.md alongside this sketch for build and wiring instructions.

#include <ESPmDNS.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiThrottleProtocol.h>

#include "AppDelegate.h"
#include "EncoderHat.h"
#include "Provision.h"
#include "UI.h"
#include "config.h"

// ----------------------- global state -----------------------

enum class AppState : uint8_t {
    Boot,
    Provisioning,
    WifiConnect,
    DiscoverServer,
    ConnectThrottle,
    Roster,
    Drive,
    Functions,
    Layout,
    Status,
    Reconnect,
};

AppState               state = AppState::Boot;

WiFiClient             tcpClient;
WiThrottleProtocol     wit;
AppDelegate            delegateImpl;
EncoderHat             encoder;

Provision::Stored      cfg;
String                 apSsid;
IPAddress              apIp;

// Discovered/effective server target.
IPAddress              serverIp;
uint16_t               serverPort = DEFAULT_WITHROTTLE_PORT;
String                 serverHostLabel;

// Drive view state
int16_t  throttlePos = 0;          // signed: -126..+126
bool     polarityFlipped = false;  // from NVS, per-device

// Roster picker state
int      rosterIndex = 0;
int      rosterScroll = 0;
int      rosterDeltaAccum = 0;     // accumulator: 2 detents = 1 row move
constexpr int ROSTER_DETENTS_PER_ROW = 2;

// Functions view state
uint8_t  fnSelected = 1; // F1..F12

// Layout (turnouts/routes) view state
UI::LayoutTab layoutTab = UI::LayoutTab::Turnouts;
int      turnoutIndex = 0;
int      turnoutScroll = 0;
int      routeIndex = 0;
int      routeScroll = 0;

// Reconnect ladder
size_t   backoffStep = 0;
uint32_t lastConnectAttempt = 0;

// Idle / dim tracking
uint32_t lastInputAt = 0;
bool     dimmed = false;

// Repaint flag — set when any view needs to be redrawn.
bool     needRepaint = true;

Preferences prefs;  // for per-device prefs (polarity, last loco)

// --------------------------------------------------------------
// Small helpers
// --------------------------------------------------------------

void noteInput() {
    lastInputAt = millis();
    if (dimmed) {
        UI::setBright(DISPLAY_BRIGHT);
        dimmed = false;
    }
}

void loadDevicePrefs() {
    prefs.begin(NVS_NAMESPACE, true);
    polarityFlipped = prefs.getUChar(NVS_KEY_POLARITY, 0) != 0;
    prefs.end();
}

void saveDevicePolarity() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putUChar(NVS_KEY_POLARITY, polarityFlipped ? 1 : 0);
    prefs.end();
}

void sendThrottleUpdate() {
    if (!delegateImpl.locoAcquired) return;
    const int magnitude = abs(throttlePos);
    const Direction newDir = (throttlePos >= 0) ? Forward : Reverse;
    // Always send direction (the library de-duplicates) so the server stays
    // aligned across zero-crossings.
    wit.setDirection(THROTTLE_SLOT, newDir);
    wit.setSpeed(THROTTLE_SLOT, magnitude);
}

// Drive the MiniEncoderC's RGB LED to mirror the throttle direction:
// green = forward, red = reverse, off = stopped/centred.
void updateThrottleLed() {
    if (throttlePos > 0)      encoder.setLed(0x002000);  // dim green
    else if (throttlePos < 0) encoder.setLed(0x200000);  // dim red
    else                      encoder.setLed(0x000000);  // off
}

void applyDelta(int16_t rawDelta) {
    if (rawDelta == 0) return;
    int16_t delta = polarityFlipped ? -rawDelta : rawDelta;

    // Step size — accelerate when the user rotates quickly.
    static uint32_t lastTickAt = 0;
    const uint32_t now = millis();
    const int16_t step = ((now - lastTickAt) < THROTTLE_FAST_WINDOW_MS)
                             ? THROTTLE_STEP_FAST
                             : THROTTLE_STEP_SLOW;
    lastTickAt = now;

    const int16_t prev = throttlePos;
    int32_t next = (int32_t)throttlePos + (int32_t)delta * step;
    if (next > THROTTLE_MAX_SPEED) next = THROTTLE_MAX_SPEED;
    if (next < -THROTTLE_MAX_SPEED) next = -THROTTLE_MAX_SPEED;

    // Snap through zero — if a single tick would cross zero, hold at zero
    // for one detent so the operator can feel the centre.
    if ((prev > 0 && next < 0) || (prev < 0 && next > 0)) {
        throttlePos = 0;
    } else {
        throttlePos = (int16_t)next;
    }
    sendThrottleUpdate();
    updateThrottleLed();
    needRepaint = true;
}

// Pull the latest mirrored state from the server back into throttlePos.
// Called when delegateImpl.dirty is set.
void absorbMirroredState() {
    const int magnitude = delegateImpl.mirroredSpeed;
    const int16_t signed_ =
        (delegateImpl.mirroredDirection == Forward) ? magnitude : -magnitude;
    // Only override if the values disagree; we don't want to fight the local
    // encoder while the user is turning it.
    const bool localMatches =
        (abs(throttlePos) == magnitude) &&
        ((throttlePos >= 0) == (delegateImpl.mirroredDirection == Forward));
    if (!localMatches) {
        throttlePos = signed_;
        updateThrottleLed();
    }
    delegateImpl.dirty = false;
    needRepaint = true;
}

int8_t batteryPercent() {
    const int32_t lvl = M5.Power.getBatteryLevel();
    if (lvl < 0) return -1;
    return (int8_t)constrain(lvl, 0, 100);
}

// --------------------------------------------------------------
// State entry / transitions
// --------------------------------------------------------------

void enterProvisioning() {
    state = AppState::Provisioning;
    Provision::begin(apSsid, apIp);
    UI::provisioning(apSsid, apIp);
    needRepaint = false;
}

void enterWifiConnect() {
    state = AppState::WifiConnect;
    UI::splash("WiFi", "connecting to " + cfg.ssid);
    needRepaint = false;
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.ssid.c_str(), cfg.password.c_str());
}

void enterDiscoverServer() {
    state = AppState::DiscoverServer;
    UI::serverDiscovery("looking up JMRI…");
    needRepaint = false;
}

void enterConnectThrottle() {
    state = AppState::ConnectThrottle;
    UI::splash("Server",
               serverHostLabel + ":" + String(serverPort));
    needRepaint = false;
}

void enterRoster() {
    state = AppState::Roster;
    rosterIndex = 0;
    rosterScroll = 0;
    rosterDeltaAccum = 0;
    encoder.setLed(0x000000);  // not actively driving
    needRepaint = true;
}

void enterDrive() {
    state = AppState::Drive;
    updateThrottleLed();
    needRepaint = true;
}

void enterFunctions() {
    state = AppState::Functions;
    needRepaint = true;
}

void enterLayout() {
    state = AppState::Layout;
    needRepaint = true;
}

void enterStatus() {
    state = AppState::Status;
    needRepaint = true;
}

void enterReconnect() {
    state = AppState::Reconnect;
    backoffStep = 0;
    lastConnectAttempt = 0;
    encoder.setLed(0x000000);  // disconnected: no throttle authority
    needRepaint = true;
}

// --------------------------------------------------------------
// Per-state ticks
// --------------------------------------------------------------

void tickProvisioning() {
    Provision::tick();
    if (Provision::isDone()) {
        cfg = Provision::result();
        Provision::end();
        delay(200);
        ESP.restart();  // simplest way to get a clean WiFi state
    }
}

void tickWifiConnect() {
    static uint32_t startedAt = 0;
    if (startedAt == 0) startedAt = millis();

    if (WiFi.status() == WL_CONNECTED) {
        startedAt = 0;
        if (cfg.host.length()) {
            // Manual server configured — bypass mDNS.
            serverHostLabel = cfg.host;
            serverPort = cfg.port ? cfg.port : DEFAULT_WITHROTTLE_PORT;
            if (!serverIp.fromString(cfg.host)) {
                // Resolve a hostname like "jmri.local"
                IPAddress resolved;
                if (WiFi.hostByName(cfg.host.c_str(), resolved)) {
                    serverIp = resolved;
                } else {
                    UI::splash("DNS failed", cfg.host);
                    delay(1500);
                    enterDiscoverServer();
                    return;
                }
            }
            enterConnectThrottle();
        } else {
            enterDiscoverServer();
        }
        return;
    }

    if (millis() - startedAt > WIFI_CONNECT_TIMEOUT_MS) {
        startedAt = 0;
        UI::splash("WiFi failed", "starting setup…");
        delay(1500);
        enterProvisioning();
    }
}

void tickDiscoverServer() {
    if (!MDNS.begin("withrottle-m5stick")) {
        UI::splash("mDNS failed", "");
        delay(1500);
        enterProvisioning();
        return;
    }
    const int n = MDNS.queryService(MDNS_SERVICE_NAME, MDNS_SERVICE_PROTO);
    if (n <= 0) {
        UI::splash("No server",
                   "is JMRI running? Hold BtnB to redo setup.");
        delay(2500);
        // try again next loop
        return;
    }
    // Pick the first hit. (A future iteration could let the user pick.)
#if ESP_IDF_VERSION_MAJOR < 5
    serverIp = MDNS.IP(0);
#else
    serverIp = MDNS.address(0);
#endif
    serverPort = MDNS.port(0);
    serverHostLabel = MDNS.hostname(0);
    enterConnectThrottle();
}

void tickConnectThrottle() {
    if (!tcpClient.connect(serverIp, serverPort)) {
        UI::splash("Connect failed", serverHostLabel);
        delay(1500);
        enterReconnect();
        return;
    }
    wit.setDelegate(&delegateImpl);
    wit.connect(&tcpClient);
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char tail[5];
    snprintf(tail, sizeof(tail), "%02X%02X", mac[4], mac[5]);
    const String devName = String("M5StickCPlus2-") + tail;
    wit.setDeviceName(devName);
    wit.setDeviceID(devName);
    enterRoster();
}

void tickRoster() {
    // Pull encoder. The picker needs to feel deliberate — a single detent
    // nudge while reaching for the push button shouldn't slip the selection
    // onto the next loco. Accumulate detents and only step when we have at
    // least ROSTER_DETENTS_PER_ROW worth in either direction.
    const int16_t delta = encoder.consumeDelta();
    if (delta != 0) {
        noteInput();
        rosterDeltaAccum += delta;
        const int n = (int)delegateImpl.roster.size();
        if (n > 0) {
            int step = 0;
            while (rosterDeltaAccum >= ROSTER_DETENTS_PER_ROW) {
                rosterDeltaAccum -= ROSTER_DETENTS_PER_ROW;
                step++;
            }
            while (rosterDeltaAccum <= -ROSTER_DETENTS_PER_ROW) {
                rosterDeltaAccum += ROSTER_DETENTS_PER_ROW;
                step--;
            }
            if (step != 0) {
                rosterIndex = (rosterIndex + step) % n;
                if (rosterIndex < 0) rosterIndex += n;
                needRepaint = true;
            }
        }
    }
    const auto ev = encoder.consumeButtonEvent();
    if (ev == EncoderHat::ButtonEvent::ShortPress &&
        rosterIndex >= 0 &&
        rosterIndex < (int)delegateImpl.roster.size()) {
        noteInput();
        const String addr = delegateImpl.roster[rosterIndex].witAddress();
        if (wit.addLocomotive(THROTTLE_SLOT, addr)) {
            // Persist last loco
            prefs.begin(NVS_NAMESPACE, false);
            prefs.putString(NVS_KEY_LASTLOCO, addr);
            prefs.end();
            UI::splash("Acquiring", addr);
            delay(300);
            enterDrive();
            return;
        }
    }

    M5.update();
    // (No BtnB action in the roster picker for now.)

    if (delegateImpl.dirty || needRepaint) {
        if (delegateImpl.expectedRosterSize < 0 ||
            delegateImpl.roster.empty()) {
            UI::splash("Loading roster",
                       "waiting for JMRI…");
        } else {
            UI::roster(delegateImpl.roster, rosterIndex, rosterScroll);
        }
        delegateImpl.dirty = false;
        needRepaint = false;
    }
}

void renderDrive() {
    UI::DriveStatus s;
    s.throttlePos = throttlePos;
    s.direction = delegateImpl.mirroredDirection;
    s.locoAddress = delegateImpl.acquiredAddress;
    // Try to find a friendly name from the roster
    for (const auto &e : delegateImpl.roster) {
        if (e.witAddress() == delegateImpl.acquiredAddress) {
            s.locoName = e.name;
            break;
        }
    }
    if (s.locoName.length() == 0) s.locoName = s.locoAddress;
    s.wifiOk = (WiFi.status() == WL_CONNECTED);
    s.serverOk = tcpClient.connected();
    s.batteryPct = batteryPercent();
    s.polarityFlipped = polarityFlipped;
    s.functions = delegateImpl.mirroredFunctions;
    UI::drive(s);
}

void tickDrive() {
    // Encoder rotation -> speed change
    const int16_t delta = encoder.consumeDelta();
    if (delta != 0) {
        noteInput();
        applyDelta(delta);
    }
    // Encoder button
    const auto ev = encoder.consumeButtonEvent();
    if (ev == EncoderHat::ButtonEvent::ShortPress) {
        noteInput();
        // E-stop: snap to zero and tell server
        throttlePos = 0;
        wit.emergencyStop(THROTTLE_SLOT);
        updateThrottleLed();
        needRepaint = true;
    } else if (ev == EncoderHat::ButtonEvent::LongPress) {
        noteInput();
        // Gentle centre: ramp to zero in one step (no e-stop)
        throttlePos = 0;
        sendThrottleUpdate();
        updateThrottleLed();
        needRepaint = true;
    }

    M5.update();
    // BtnA: long => flip polarity; short => toggle F0
    if (M5.BtnA.wasReleaseFor(LONG_PRESS_MS)) {
        noteInput();
        polarityFlipped = !polarityFlipped;
        saveDevicePolarity();
        needRepaint = true;
    } else if (M5.BtnA.wasReleased()) {
        noteInput();
        const bool now = !delegateImpl.mirroredFunctions[0];
        wit.setFunction(THROTTLE_SLOT, 0, now);
        delegateImpl.mirroredFunctions[0] = now;
        needRepaint = true;
    }
    // BtnB: long => release loco + back to roster; short => open functions
    if (M5.BtnB.wasReleaseFor(LONG_PRESS_MS)) {
        noteInput();
        wit.releaseLocomotive(THROTTLE_SLOT);
        delegateImpl.locoAcquired = false;
        delegateImpl.acquiredAddress = "";
        enterRoster();
        return;
    } else if (M5.BtnB.wasReleased()) {
        noteInput();
        enterFunctions();
        return;
    }

    if (delegateImpl.dirty) absorbMirroredState();
    if (needRepaint) {
        renderDrive();
        needRepaint = false;
    }
}

void tickFunctions() {
    const int16_t delta = encoder.consumeDelta();
    if (delta != 0) {
        noteInput();
        int n = (int)fnSelected + delta;
        while (n < 1) n += 12;
        while (n > 12) n -= 12;
        fnSelected = (uint8_t)n;
        needRepaint = true;
    }
    const auto ev = encoder.consumeButtonEvent();
    if (ev == EncoderHat::ButtonEvent::ShortPress) {
        noteInput();
        const bool now = !delegateImpl.mirroredFunctions[fnSelected];
        wit.setFunction(THROTTLE_SLOT, fnSelected, now);
        delegateImpl.mirroredFunctions[fnSelected] = now;
        needRepaint = true;
    }
    M5.update();
    if (M5.BtnB.wasReleased()) {
        noteInput();
        enterLayout();
        return;
    }
    if (M5.BtnA.wasReleased()) {
        noteInput();
        enterDrive();
        return;
    }
    if (delegateImpl.dirty) { delegateImpl.dirty = false; needRepaint = true; }
    if (needRepaint) {
        UI::functions(delegateImpl.mirroredFunctions, fnSelected);
        needRepaint = false;
    }
}

void tickLayout() {
    const int16_t delta = encoder.consumeDelta();
    if (delta != 0) {
        noteInput();
        if (layoutTab == UI::LayoutTab::Turnouts) {
            const int n = (int)delegateImpl.turnouts.size();
            if (n > 0) {
                turnoutIndex = (turnoutIndex + delta) % n;
                if (turnoutIndex < 0) turnoutIndex += n;
            }
        } else {
            const int n = (int)delegateImpl.routes.size();
            if (n > 0) {
                routeIndex = (routeIndex + delta) % n;
                if (routeIndex < 0) routeIndex += n;
            }
        }
        needRepaint = true;
    }

    const auto ev = encoder.consumeButtonEvent();
    if (ev == EncoderHat::ButtonEvent::ShortPress) {
        noteInput();
        if (layoutTab == UI::LayoutTab::Turnouts) {
            if (turnoutIndex >= 0 &&
                turnoutIndex < (int)delegateImpl.turnouts.size()) {
                TurnoutEntry &t = delegateImpl.turnouts[turnoutIndex];
                wit.setTurnout(t.sysName, TurnoutToggle);
                // Optimistic local flip so the UI feels instant.
                if      (t.state == TurnoutClosed) t.state = TurnoutThrown;
                else if (t.state == TurnoutThrown) t.state = TurnoutClosed;
                needRepaint = true;
            }
        } else {
            if (routeIndex >= 0 &&
                routeIndex < (int)delegateImpl.routes.size()) {
                const RouteEntry &r = delegateImpl.routes[routeIndex];
                wit.setRoute(r.sysName);
                // Brief acknowledgement banner — WiThrottle has no "route
                // activated" callback we can rely on.
                UI::layout(delegateImpl.turnouts, delegateImpl.routes,
                           layoutTab, turnoutIndex, turnoutScroll,
                           routeIndex, routeScroll);
                UI::banner("Route activated", TFT_GREEN);
                delay(500);
                needRepaint = true;
            }
        }
    } else if (ev == EncoderHat::ButtonEvent::LongPress) {
        noteInput();
        layoutTab = (layoutTab == UI::LayoutTab::Turnouts)
                        ? UI::LayoutTab::Routes
                        : UI::LayoutTab::Turnouts;
        needRepaint = true;
    }

    M5.update();
    if (M5.BtnB.wasReleased()) {
        noteInput();
        enterStatus();
        return;
    }
    if (M5.BtnA.wasReleased()) {
        noteInput();
        enterDrive();
        return;
    }

    if (delegateImpl.dirty) { delegateImpl.dirty = false; needRepaint = true; }
    if (needRepaint) {
        UI::layout(delegateImpl.turnouts, delegateImpl.routes,
                   layoutTab, turnoutIndex, turnoutScroll,
                   routeIndex, routeScroll);
        needRepaint = false;
    }
}

void tickStatus() {
    M5.update();
    if (M5.BtnB.wasReleased() || M5.BtnA.wasReleased()) {
        noteInput();
        enterDrive();
        return;
    }
    if (needRepaint) {
        UI::StatusInfo s;
        s.wifiSsid = WiFi.SSID();
        s.wifiIp = WiFi.localIP().toString();
        s.wifiRssi = WiFi.RSSI();
        s.serverHost = serverHostLabel;
        s.serverPort = serverPort;
        s.witVersion = delegateImpl.version;
        s.heartbeatSec = delegateImpl.heartbeatPeriod;
        s.batteryPct = batteryPercent();
        UI::statusScreen(s);
        needRepaint = false;
    }
}

void tickReconnect() {
    const uint32_t now = millis();
    const uint32_t backoff =
        RECONNECT_BACKOFF_MS[backoffStep < RECONNECT_BACKOFF_COUNT
                                 ? backoffStep
                                 : RECONNECT_BACKOFF_COUNT - 1];
    if (needRepaint) {
        UI::splash("Reconnecting",
                   "next try in " + String(backoff / 1000) + "s");
        needRepaint = false;
    }
    if (lastConnectAttempt == 0 || (now - lastConnectAttempt) >= backoff) {
        lastConnectAttempt = now;
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.reconnect();
        } else if (tcpClient.connect(serverIp, serverPort)) {
            wit.connect(&tcpClient);
            if (delegateImpl.locoAcquired &&
                delegateImpl.acquiredAddress.length()) {
                wit.addLocomotive(THROTTLE_SLOT,
                                  delegateImpl.acquiredAddress);
            }
            backoffStep = 0;
            if (delegateImpl.locoAcquired) enterDrive(); else enterRoster();
            return;
        }
        if (backoffStep + 1 < RECONNECT_BACKOFF_COUNT) backoffStep++;
        needRepaint = true;
    }
}

// --------------------------------------------------------------
// Arduino entry points
// --------------------------------------------------------------

void setup() {
    auto mcfg = M5.config();
    M5.begin(mcfg);
    Serial.begin(115200);

    UI::begin();
    UI::splash("WiThrottle", "M5StickC Plus 2");

    if (!encoder.begin()) {
        UI::splash("MiniEncoderC",
                   "not detected on I2C 0x42");
        delay(2000);
    }

    loadDevicePrefs();

    // Hold BtnB at boot => force re-provisioning.
    M5.update();
    bool forceProvision = M5.BtnB.isPressed();

    if (forceProvision) {
        Provision::clearAll();
        cfg = Provision::Stored{};
    } else {
        Provision::load(cfg);
    }

    if (cfg.ssid.length() == 0) {
        enterProvisioning();
    } else {
        enterWifiConnect();
    }

    lastInputAt = millis();
}

void loop() {
    encoder.poll();
    wit.check();

    // Auto-dim
    if (!dimmed && DISPLAY_DIM_AFTER_MS > 0 &&
        (millis() - lastInputAt) > DISPLAY_DIM_AFTER_MS) {
        UI::setBright(DISPLAY_DIM);
        dimmed = true;
    }

    // Detect server-side disconnects from any state that owns a TCP client.
    if (state == AppState::Roster || state == AppState::Drive ||
        state == AppState::Functions || state == AppState::Layout ||
        state == AppState::Status) {
        if (!tcpClient.connected()) {
            enterReconnect();
        }
    }

    switch (state) {
        case AppState::Provisioning:    tickProvisioning(); break;
        case AppState::WifiConnect:     tickWifiConnect();  break;
        case AppState::DiscoverServer:  tickDiscoverServer(); break;
        case AppState::ConnectThrottle: tickConnectThrottle(); break;
        case AppState::Roster:          tickRoster(); break;
        case AppState::Drive:           tickDrive(); break;
        case AppState::Functions:       tickFunctions(); break;
        case AppState::Layout:          tickLayout(); break;
        case AppState::Status:          tickStatus(); break;
        case AppState::Reconnect:       tickReconnect(); break;
        case AppState::Boot:            break;
    }
}
