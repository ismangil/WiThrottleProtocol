#include "UI.h"

#include <M5Unified.h>

namespace UI {

namespace {

constexpr uint16_t COL_BG     = TFT_BLACK;
constexpr uint16_t COL_FG     = TFT_WHITE;
constexpr uint16_t COL_DIM    = 0x7BEF; // medium grey
constexpr uint16_t COL_FWD    = TFT_GREEN;
constexpr uint16_t COL_REV    = TFT_ORANGE;
constexpr uint16_t COL_ZERO   = TFT_DARKGREY;
constexpr uint16_t COL_ACCENT = TFT_CYAN;
constexpr uint16_t COL_BAD    = TFT_RED;
constexpr uint16_t COL_GOOD   = TFT_GREEN;

void clear() { M5.Display.fillScreen(COL_BG); }

void drawHeader(const String &left, const String &right) {
    M5.Display.fillRect(0, 0, TFT_W, 14, COL_DIM);
    M5.Display.setTextColor(COL_BG, COL_DIM);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(1);
    M5.Display.drawString(left, 2, 3);
    M5.Display.setTextDatum(top_right);
    M5.Display.drawString(right, TFT_W - 2, 3);
}

}  // namespace

void begin() {
    M5.Display.setRotation(1);   // landscape, USB-C on the right
    M5.Display.setTextWrap(false);
    M5.Display.setBrightness(DISPLAY_BRIGHT);
    clear();
}

void setBright(uint8_t v) { M5.Display.setBrightness(v); }

void splash(const String &line1, const String &line2) {
    clear();
    M5.Display.setTextColor(COL_FG, COL_BG);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    M5.Display.drawString(line1, TFT_W / 2, TFT_H / 2 - 12);
    if (line2.length()) {
        M5.Display.setTextSize(1);
        M5.Display.drawString(line2, TFT_W / 2, TFT_H / 2 + 14);
    }
}

void provisioning(const String &apSsid, const IPAddress &ip) {
    clear();
    drawHeader("Setup", "AP mode");
    M5.Display.setTextColor(COL_FG, COL_BG);
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(1);
    M5.Display.drawString("Connect a phone to:", 4, 22);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(COL_ACCENT, COL_BG);
    M5.Display.drawString(apSsid, 4, 36);
    M5.Display.setTextColor(COL_FG, COL_BG);
    M5.Display.setTextSize(1);
    M5.Display.drawString("then open:", 4, 64);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(COL_ACCENT, COL_BG);
    M5.Display.drawString("http://" + ip.toString(), 4, 78);
    M5.Display.setTextColor(COL_DIM, COL_BG);
    M5.Display.setTextSize(1);
    M5.Display.drawString("Hold BtnB at boot to redo setup.", 4, TFT_H - 12);
}

void serverDiscovery(const String &status) {
    splash("Finding server", status);
}

void roster(const std::vector<RosterEntry> &entries, int selectedIndex,
            int &scrollOffset) {
    clear();
    drawHeader("Pick a loco", String(entries.size()) + " in roster");

    constexpr int rowH = 18;
    const int top = 16;
    const int visibleRows = (TFT_H - top) / rowH;

    if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
    if (selectedIndex >= scrollOffset + visibleRows)
        scrollOffset = selectedIndex - visibleRows + 1;
    if (scrollOffset < 0) scrollOffset = 0;

    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_left);

    for (int row = 0; row < visibleRows; row++) {
        const int idx = scrollOffset + row;
        if (idx >= (int)entries.size()) break;
        const RosterEntry &e = entries[idx];
        const int y = top + row * rowH;
        const bool sel = (idx == selectedIndex);
        const uint16_t bg = sel ? COL_ACCENT : COL_BG;
        const uint16_t fg = sel ? COL_BG : COL_FG;
        M5.Display.fillRect(0, y, TFT_W, rowH, bg);
        M5.Display.setTextColor(fg, bg);
        String addr = String(e.length) + String(e.address);
        M5.Display.drawString(e.name, 4, y + 4);
        M5.Display.setTextDatum(top_right);
        M5.Display.drawString(addr, TFT_W - 4, y + 4);
        M5.Display.setTextDatum(top_left);
    }
}

void drive(const DriveStatus &s) {
    clear();

    // Header: loco name + status icons
    const String headerLeft =
        s.locoName.length() ? s.locoName : s.locoAddress;
    String headerRight;
    headerRight += s.wifiOk ? "WiFi" : "wifi";
    headerRight += " | ";
    headerRight += s.serverOk ? "JMRI" : "jmri";
    if (s.batteryPct >= 0) {
        headerRight += " | ";
        headerRight += String(s.batteryPct);
        headerRight += "%";
    }
    drawHeader(headerLeft, headerRight);

    // ----- Vertical centre-zero slider on the right edge -----
    constexpr int barW = 30;
    constexpr int barRight = TFT_W - 4;
    constexpr int barX = barRight - barW;
    constexpr int barTop = 18;
    constexpr int barBot = TFT_H - 4;
    constexpr int barH = barBot - barTop;
    constexpr int mid = barTop + barH / 2;

    M5.Display.drawRect(barX, barTop, barW, barH, COL_DIM);
    // Centre tick: a horizontal line across the bar
    M5.Display.drawFastHLine(barX - 2, mid, barW + 4, COL_ZERO);

    if (s.throttlePos != 0) {
        const int halfH = barH / 2 - 2;
        const int fillPx =
            (int)((long)abs(s.throttlePos) * halfH / THROTTLE_MAX_SPEED);
        const uint16_t fillCol =
            (s.throttlePos > 0) ? COL_FWD : COL_REV;
        if (s.throttlePos > 0) {
            // Positive (forward) grows up from centre
            M5.Display.fillRect(barX + 1, mid - fillPx, barW - 2, fillPx,
                                fillCol);
        } else {
            // Negative (reverse) grows down from centre
            M5.Display.fillRect(barX + 1, mid + 1, barW - 2, fillPx, fillCol);
        }
    }

    // ----- Left content area -----
    const int contentRight = barX - 4;
    const int contentMidX = contentRight / 2 + 2;

    // Big signed speed number, centred in the left content area
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(COL_FG, COL_BG);
    M5.Display.setTextSize(4);
    char buf[8];
    if (s.throttlePos == 0) {
        snprintf(buf, sizeof(buf), "0");
    } else {
        snprintf(buf, sizeof(buf), "%+d", s.throttlePos);
    }
    M5.Display.drawString(buf, contentMidX, 46);

    // Direction arrow under the number — vertical arrows match the slider
    M5.Display.setTextSize(2);
    const char *arrow = (s.direction == Forward) ? "/\\" : "\\/";
    const uint16_t dirCol = (s.direction == Forward) ? COL_FWD : COL_REV;
    M5.Display.setTextColor(dirCol, COL_BG);
    M5.Display.drawString(arrow, contentMidX, 78);
    if (s.polarityFlipped) {
        M5.Display.setTextColor(COL_DIM, COL_BG);
        M5.Display.setTextSize(1);
        M5.Display.drawString("polarity flipped", contentMidX, 96);
    }

    // F-button mini-indicators along the bottom of the left area (F0..F9)
    if (s.functions) {
        const int fy = TFT_H - 12;
        const int contentW = contentRight - 4;
        const int fw = contentW / 10;
        for (int i = 0; i < 10; i++) {
            const int fx = 4 + i * fw;
            uint16_t col = s.functions[i] ? COL_ACCENT : COL_DIM;
            M5.Display.drawRect(fx, fy, fw - 2, 9, col);
            if (s.functions[i]) {
                M5.Display.fillRect(fx + 1, fy + 1, fw - 4, 7, col);
            }
        }
    }
}

void functions(const bool *fnState, uint8_t selected) {
    clear();
    drawHeader("Functions", "F1-F12");
    constexpr int rows = 3;
    constexpr int cols = 4;
    const int top = 18;
    const int cellW = TFT_W / cols;
    const int cellH = (TFT_H - top) / rows;
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(2);
    for (int i = 0; i < rows * cols; i++) {
        const int r = i / cols;
        const int c = i % cols;
        const int x = c * cellW;
        const int y = top + r * cellH;
        const uint8_t fn = (uint8_t)(i + 1);  // F1..F12
        const bool on = fnState ? fnState[fn] : false;
        const bool sel = (fn == selected);
        const uint16_t bg = on ? COL_ACCENT : COL_BG;
        const uint16_t fg = on ? COL_BG : COL_FG;
        M5.Display.fillRect(x + 2, y + 2, cellW - 4, cellH - 4, bg);
        const uint16_t border = sel ? COL_FG : COL_DIM;
        M5.Display.drawRect(x + 2, y + 2, cellW - 4, cellH - 4, border);
        M5.Display.setTextColor(fg, bg);
        char buf[4];
        snprintf(buf, sizeof(buf), "F%u", fn);
        M5.Display.drawString(buf, x + cellW / 2, y + cellH / 2);
    }
}

void statusScreen(const StatusInfo &s) {
    clear();
    drawHeader("Status", "BtnB=back");
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(COL_FG, COL_BG);
    int y = 18;
    auto line = [&](const String &k, const String &v) {
        M5.Display.setTextColor(COL_DIM, COL_BG);
        M5.Display.drawString(k, 4, y);
        M5.Display.setTextColor(COL_FG, COL_BG);
        M5.Display.drawString(v, 70, y);
        y += 12;
    };
    line("SSID:",   s.wifiSsid);
    line("IP:",     s.wifiIp);
    line("RSSI:",   String(s.wifiRssi) + " dBm");
    line("Server:", s.serverHost + ":" + String(s.serverPort));
    line("WiT ver:",s.witVersion);
    line("HBeat:",  String(s.heartbeatSec) + " s");
    if (s.batteryPct >= 0) {
        line("Batt:", String(s.batteryPct) + "%");
    }
}

void banner(const String &text, uint16_t color) {
    M5.Display.fillRect(0, TFT_H - 14, TFT_W, 14, color);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(COL_BG, color);
    M5.Display.drawString(text, TFT_W / 2, TFT_H - 7);
}

}  // namespace UI
