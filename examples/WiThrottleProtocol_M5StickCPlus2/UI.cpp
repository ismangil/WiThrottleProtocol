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

constexpr int HEADER_H = 22;     // taller title bar so size-2 text fits
constexpr int BODY_TOP = HEADER_H + 4;

void clear() { M5.Display.fillScreen(COL_BG); }

// Shrink a string until it fits in `maxPx` at the current text size.
void truncateToWidth(String &s, int maxPx) {
    while (s.length() > 0 && M5.Display.textWidth(s) > maxPx) {
        s.remove(s.length() - 1);
    }
}

// Word-wrap `text` and draw each line centred horizontally on the display.
// Returns the next free y coordinate.
int drawWrappedCentered(const String &text, int yTop, int maxW,
                        int textSize, uint16_t color, uint16_t bg) {
    if (text.length() == 0) return yTop;
    M5.Display.setTextSize(textSize);
    M5.Display.setTextColor(color, bg);
    M5.Display.setTextDatum(top_center);
    const int lineH = 8 * textSize + 4;
    String line, word;
    int y = yTop;
    auto flush = [&]() {
        if (line.length()) {
            M5.Display.drawString(line, TFT_W / 2, y);
            y += lineH;
            line = "";
        }
    };
    for (size_t i = 0; i <= text.length(); i++) {
        const char c = (i < text.length()) ? text[i] : ' ';
        if (c == ' ' || c == '\n') {
            String candidate = line.length() ? line + " " + word : word;
            if (M5.Display.textWidth(candidate) > maxW) {
                flush();
                line = word;
            } else {
                line = candidate;
            }
            word = "";
            if (c == '\n') flush();
        } else {
            word += c;
        }
    }
    flush();
    return y;
}

// Header bar with size-2 left text and size-1 right text. The left side
// gets truncated first if both don't fit.
void drawHeader(const String &left, const String &right) {
    M5.Display.fillRect(0, 0, TFT_W, HEADER_H, COL_DIM);
    M5.Display.setTextColor(COL_BG, COL_DIM);

    // Right side: size 1, capped to ~60% of the bar to leave room for left.
    M5.Display.setTextSize(1);
    String r = right;
    truncateToWidth(r, (TFT_W * 60) / 100);
    const int rightW = M5.Display.textWidth(r);
    M5.Display.setTextDatum(top_right);
    M5.Display.drawString(r, TFT_W - 3, (HEADER_H - 8) / 2);

    // Left side: size 2, takes remaining space.
    M5.Display.setTextSize(2);
    String l = left;
    const int leftAvail = TFT_W - rightW - 10;
    truncateToWidth(l, leftAvail);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString(l, 3, (HEADER_H - 16) / 2);
}

}  // namespace

void begin() {
    M5.Display.setRotation(0);   // portrait
    M5.Display.setTextWrap(false);
    M5.Display.setBrightness(DISPLAY_BRIGHT);
    clear();
}

void setBright(uint8_t v) { M5.Display.setBrightness(v); }

void splash(const String &line1, const String &line2) {
    clear();
    const int wrapW = TFT_W - 8;
    int y = TFT_H / 2 - 32;
    y = drawWrappedCentered(line1, y, wrapW, 2, COL_FG, COL_BG);
    if (line2.length()) {
        y += 8;
        drawWrappedCentered(line2, y, wrapW, 1, COL_DIM, COL_BG);
    }
}

void provisioning(const String &apSsid, const IPAddress &ip) {
    clear();
    drawHeader("Setup", "AP mode");
    const int wrapW = TFT_W - 8;
    int y = BODY_TOP;
    y = drawWrappedCentered("Connect a phone to:", y, wrapW, 1, COL_FG, COL_BG);
    y += 4;
    y = drawWrappedCentered(apSsid, y, wrapW, 2, COL_ACCENT, COL_BG);
    y += 10;
    y = drawWrappedCentered("then open:", y, wrapW, 1, COL_FG, COL_BG);
    y += 4;
    y = drawWrappedCentered("http://" + ip.toString(), y, wrapW, 2,
                            COL_ACCENT, COL_BG);
    drawWrappedCentered("Hold BtnB at boot to redo setup.",
                        TFT_H - 28, wrapW, 1, COL_DIM, COL_BG);
}

void serverDiscovery(const String &status) {
    splash("Finding server", status);
}

void roster(const std::vector<RosterEntry> &entries, int selectedIndex,
            int &scrollOffset) {
    clear();
    drawHeader("Pick loco", String(entries.size()));

    constexpr int rowH = 32;        // generous row height for fat fingers
    constexpr int innerH = 26;      // leaves a 6 px gap between rows
    constexpr int accentW = 5;      // left-edge marker on the selected row
    const int top = HEADER_H + 4;
    const int visibleRows = (TFT_H - top) / rowH;

    if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
    if (selectedIndex >= scrollOffset + visibleRows)
        scrollOffset = selectedIndex - visibleRows + 1;
    if (scrollOffset < 0) scrollOffset = 0;

    for (int row = 0; row < visibleRows; row++) {
        const int idx = scrollOffset + row;
        if (idx >= (int)entries.size()) break;
        const RosterEntry &e = entries[idx];
        const int y = top + row * rowH;
        const bool sel = (idx == selectedIndex);
        const uint16_t bg = sel ? COL_ACCENT : COL_BG;
        const uint16_t fg = sel ? COL_BG : COL_FG;

        // Inner row body — short of full row height to leave a gap.
        M5.Display.fillRect(0, y, TFT_W, innerH, bg);
        if (sel) {
            // Extra accent stripe on the left so the selection is
            // unmistakable even at a glance.
            M5.Display.fillRect(0, y, accentW, innerH, COL_FG);
        } else {
            // Subtle divider between unselected rows.
            M5.Display.drawFastHLine(8, y + innerH + 1, TFT_W - 16, COL_DIM);
        }

        // Address on the right (size 2), name on the left with truncation.
        M5.Display.setTextColor(fg, bg);
        M5.Display.setTextSize(2);
        String addr = String(e.length) + String(e.address);
        const int addrW = M5.Display.textWidth(addr);
        const int textLeft = sel ? (accentW + 4) : 4;
        String name = e.name;
        truncateToWidth(name, TFT_W - addrW - textLeft - 8);
        M5.Display.setTextDatum(top_left);
        M5.Display.drawString(name, textLeft, y + (innerH - 16) / 2);
        M5.Display.setTextDatum(top_right);
        M5.Display.drawString(addr, TFT_W - 4, y + (innerH - 16) / 2);
    }
}

void drive(const DriveStatus &s) {
    clear();

    // Header: loco name on the left, compact battery/link state on the
    // right. WiFi/server outages already drop us to the Reconnect screen,
    // so the right side just shows battery when available.
    const String headerLeft =
        s.locoName.length() ? s.locoName : s.locoAddress;
    String headerRight;
    if (!s.wifiOk)        headerRight = "!WiFi";
    else if (!s.serverOk) headerRight = "!JMRI";
    else if (s.batteryPct >= 0) headerRight = String(s.batteryPct) + "%";
    drawHeader(headerLeft, headerRight);

    // ----- Vertical centre-zero slider on the right edge -----
    constexpr int barW = 30;
    constexpr int barRight = TFT_W - 4;
    constexpr int barX = barRight - barW;
    constexpr int barTop = BODY_TOP;
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

    // Big signed speed number, centred in the left content area. Size 3
    // keeps "+126" inside the ~95 px wide content column in portrait.
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(COL_FG, COL_BG);
    M5.Display.setTextSize(3);
    char buf[8];
    if (s.throttlePos == 0) {
        snprintf(buf, sizeof(buf), "0");
    } else {
        snprintf(buf, sizeof(buf), "%+d", s.throttlePos);
    }
    M5.Display.drawString(buf, contentMidX, BODY_TOP + 30);

    // Direction arrow under the number — vertical arrows match the slider
    M5.Display.setTextSize(2);
    const char *arrow = (s.direction == Forward) ? "/\\" : "\\/";
    const uint16_t dirCol = (s.direction == Forward) ? COL_FWD : COL_REV;
    M5.Display.setTextColor(dirCol, COL_BG);
    M5.Display.drawString(arrow, contentMidX, BODY_TOP + 62);
    if (s.polarityFlipped) {
        M5.Display.setTextColor(COL_DIM, COL_BG);
        M5.Display.setTextSize(1);
        M5.Display.drawString("polarity flipped", contentMidX,
                              BODY_TOP + 82);
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
    constexpr int rows = 4;   // portrait: stack F1-F12 as 4 rows of 3
    constexpr int cols = 3;
    const int top = BODY_TOP;
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

namespace {

const char *turnoutGlyph(TurnoutState st) {
    switch (st) {
        case TurnoutClosed:       return "C";
        case TurnoutThrown:       return "T";
        case TurnoutInconsistent: return "!";
        case TurnoutUnknown:
        default:                  return "?";
    }
}

// Draw a single highlightable row of size-2 text with an optional
// right-aligned glyph. Used by both list tabs of the Layout screen.
void drawLayoutRow(int y, int rowH, bool selected, const String &left,
                   const String &right) {
    const uint16_t bg = selected ? COL_ACCENT : COL_BG;
    const uint16_t fg = selected ? COL_BG : COL_FG;
    M5.Display.fillRect(0, y, TFT_W, rowH - 2, bg);
    M5.Display.setTextColor(fg, bg);
    M5.Display.setTextSize(2);
    String r = right;
    const int rW = M5.Display.textWidth(r);
    String l = left;
    truncateToWidth(l, TFT_W - rW - 12);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString(l, 4, y + 4);
    if (r.length()) {
        M5.Display.setTextDatum(top_right);
        M5.Display.drawString(r, TFT_W - 4, y + 4);
    }
}

}  // namespace

void layout(const std::vector<TurnoutEntry> &turnouts,
            const std::vector<RouteEntry> &routes,
            LayoutTab tab,
            int turnoutIdx, int &turnoutScroll,
            int routeIdx, int &routeScroll) {
    clear();
    const bool onTurnouts = (tab == LayoutTab::Turnouts);
    const String title = onTurnouts ? "Turnouts" : "Routes";
    String counts = "T:" + String((int)turnouts.size()) +
                    " R:" + String((int)routes.size());
    drawHeader(title, counts);

    // Tab strip just below the header so the inactive tab stays visible.
    // The active tab gets a ">" marker; the inactive tab is dimmed and
    // labelled with the long-press hint so the operator can find the
    // toggle without guessing.
    constexpr int tabH = 14;
    const int tabY = HEADER_H;
    const int halfW = TFT_W / 2;
    M5.Display.fillRect(0, tabY, halfW, tabH,
                        onTurnouts ? COL_ACCENT : COL_DIM);
    M5.Display.fillRect(halfW, tabY, TFT_W - halfW, tabH,
                        onTurnouts ? COL_DIM : COL_ACCENT);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(COL_BG, onTurnouts ? COL_ACCENT : COL_DIM);
    M5.Display.drawString(onTurnouts ? ">Turnouts" : "Turnouts",
                          halfW / 2, tabY + tabH / 2);
    M5.Display.setTextColor(COL_BG, onTurnouts ? COL_DIM : COL_ACCENT);
    M5.Display.drawString(onTurnouts ? "Routes" : ">Routes",
                          halfW + halfW / 2, tabY + tabH / 2);

    // Footer: a one-line hint anchored to the bottom of the screen so the
    // user can discover the tab toggle.
    constexpr int footerH = 10;
    const int footerY = TFT_H - footerH;
    M5.Display.fillRect(0, footerY, TFT_W, footerH, COL_BG);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(COL_DIM, COL_BG);
    M5.Display.drawString("hold push: switch tab",
                          TFT_W / 2, footerY + footerH / 2);

    // List rows below the tab strip, above the footer hint.
    constexpr int rowH = 26;
    const int listTop = HEADER_H + tabH + 2;
    const int listBottom = footerY - 2;
    const int visibleRows = (listBottom - listTop) / rowH;
    const int total = onTurnouts ? (int)turnouts.size() : (int)routes.size();

    if (total == 0) {
        const int wrapW = TFT_W - 8;
        const String msg = onTurnouts ? "No turnouts" : "No routes";
        drawWrappedCentered(msg, listTop + 20, wrapW, 2, COL_DIM, COL_BG);
        return;
    }

    int &scroll = onTurnouts ? turnoutScroll : routeScroll;
    const int sel = onTurnouts ? turnoutIdx : routeIdx;
    if (sel < scroll) scroll = sel;
    if (sel >= scroll + visibleRows) scroll = sel - visibleRows + 1;
    if (scroll < 0) scroll = 0;

    for (int row = 0; row < visibleRows; row++) {
        const int idx = scroll + row;
        if (idx >= total) break;
        const int y = listTop + row * rowH;
        const bool isSel = (idx == sel);
        if (onTurnouts) {
            const TurnoutEntry &t = turnouts[idx];
            const String label = t.userName.length() ? t.userName : t.sysName;
            drawLayoutRow(y, rowH, isSel, label, turnoutGlyph(t.state));
        } else {
            const RouteEntry &r = routes[idx];
            const String label = r.userName.length() ? r.userName : r.sysName;
            drawLayoutRow(y, rowH, isSel, label, "");
        }
    }
}

void statusScreen(const StatusInfo &s) {
    clear();
    drawHeader("Status", "back");
    M5.Display.setTextDatum(top_left);
    M5.Display.setTextColor(COL_FG, COL_BG);
    int y = BODY_TOP;
    // In portrait the row is too narrow for label + value side-by-side, so
    // stack them: dim label, then value below.
    auto line = [&](const String &k, const String &v) {
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(COL_DIM, COL_BG);
        M5.Display.drawString(k, 4, y);
        y += 10;
        M5.Display.setTextSize(1);
        M5.Display.setTextColor(COL_FG, COL_BG);
        String vv = v;
        truncateToWidth(vv, TFT_W - 8);
        M5.Display.drawString(vv, 4, y);
        y += 14;
    };
    line("SSID",    s.wifiSsid);
    line("IP",      s.wifiIp);
    line("RSSI",    String(s.wifiRssi) + " dBm");
    line("Server",  s.serverHost + ":" + String(s.serverPort));
    line("WiT ver", s.witVersion);
    line("HBeat",   String(s.heartbeatSec) + " s");
    if (s.batteryPct >= 0) {
        line("Batt", String(s.batteryPct) + "%");
    }
}

void banner(const String &text, uint16_t color) {
    constexpr int H = 18;
    M5.Display.fillRect(0, TFT_H - H, TFT_W, H, color);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(COL_BG, color);
    String t = text;
    truncateToWidth(t, TFT_W - 8);
    M5.Display.drawString(t, TFT_W / 2, TFT_H - H / 2);
}

}  // namespace UI
