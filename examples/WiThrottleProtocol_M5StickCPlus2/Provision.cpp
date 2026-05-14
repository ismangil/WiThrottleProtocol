#include "Provision.h"

#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"

namespace Provision {

namespace {

Preferences prefs;
DNSServer   dns;
WebServer   web(80);
Stored      submitted;
bool        done = false;
String      apSsid;
IPAddress   apIp;

String htmlEscape(const String &s) {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        switch (c) {
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out += c;
        }
    }
    return out;
}

String scanOptions() {
    // Build a <select> with the visible APs. Synchronous scan is fine
    // because we are not yet servicing real traffic.
    int n = WiFi.scanNetworks();
    if (n < 0) n = 0;
    String html;
    html += "<select name=\"ssid\" required>";
    html += "<option value=\"\" disabled selected>-- pick a network --</option>";
    for (int i = 0; i < n; i++) {
        const String s = WiFi.SSID(i);
        const int ch = WiFi.channel(i);
        const int rssi = WiFi.RSSI(i);
        html += "<option value=\"";
        html += htmlEscape(s);
        html += "\">";
        html += htmlEscape(s);
        html += " (ch ";
        html += ch;
        html += ", ";
        html += rssi;
        html += "dBm)";
        if (ch > 10) html += " *5GHz/ch&gt;10*";
        html += "</option>";
    }
    html += "</select>";
    WiFi.scanDelete();
    return html;
}

void handleRoot() {
    String page;
    page.reserve(2048);
    page += F("<!doctype html><html><head><meta name=\"viewport\" "
              "content=\"width=device-width,initial-scale=1\">"
              "<title>WiThrottle setup</title>"
              "<style>body{font-family:sans-serif;margin:1em;max-width:32em}"
              "label{display:block;margin-top:1em}"
              "input,select{width:100%;padding:.5em;font-size:1em}"
              "button{margin-top:1.5em;padding:.7em 1.2em;font-size:1em}"
              "small{color:#666}</style></head><body>"
              "<h2>WiThrottle portable throttle</h2>"
              "<p>Configure WiFi and (optionally) the JMRI server.</p>"
              "<form method=\"POST\" action=\"/save\">"
              "<label>WiFi network");
    page += scanOptions();
    page += F("</label>"
              "<label>Password<input type=\"password\" name=\"pass\"></label>"
              "<label>Server host <small>(blank = auto-discover via mDNS)"
              "</small><input name=\"host\" placeholder=\"192.168.0.10 or "
              "jmri.local\"></label>"
              "<label>Server port <small>(blank = ");
    page += DEFAULT_WITHROTTLE_PORT;
    page += F(")</small><input name=\"port\" type=\"number\" "
              "min=\"1\" max=\"65535\"></label>"
              "<button type=\"submit\">Save &amp; connect</button>"
              "</form>"
              "<p><small>The ESP32 in this device only supports 2.4 GHz "
              "and struggles above channel 10.</small></p>"
              "</body></html>");
    web.send(200, "text/html", page);
}

void handleSave() {
    submitted.ssid     = web.arg("ssid");
    submitted.password = web.arg("pass");
    submitted.host     = web.arg("host");
    submitted.port     = (uint16_t)web.arg("port").toInt();

    if (submitted.ssid.length() == 0) {
        web.send(400, "text/plain", "SSID is required");
        return;
    }
    save(submitted);
    done = true;
    web.send(200, "text/html",
             "<html><body><h3>Saved.</h3>"
             "<p>The throttle will now reboot and connect.</p></body></html>");
}

void handleCaptive() {
    // Many captive-portal probes look for specific URLs (generate_204,
    // hotspot-detect.html, etc.). Redirect them all to "/" so the OS pops
    // the portal sheet.
    web.sendHeader("Location", "/", true);
    web.send(302, "text/plain", "");
}

}  // namespace

bool load(Stored &out) {
    prefs.begin(NVS_NAMESPACE, true);
    out.ssid     = prefs.getString(NVS_KEY_SSID, "");
    out.password = prefs.getString(NVS_KEY_PASS, "");
    out.host     = prefs.getString(NVS_KEY_HOST, "");
    out.port     = prefs.getUShort(NVS_KEY_PORT, 0);
    prefs.end();
    return out.ssid.length() > 0;
}

void save(const Stored &s) {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString(NVS_KEY_SSID, s.ssid);
    prefs.putString(NVS_KEY_PASS, s.password);
    prefs.putString(NVS_KEY_HOST, s.host);
    prefs.putUShort(NVS_KEY_PORT, s.port);
    prefs.end();
}

void clearAll() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
}

void begin(String &outSsid, IPAddress &outIp) {
    done = false;
    submitted = Stored{};

    // Build "WiThrottle-XXXX" from the last 4 hex of the MAC.
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char tail[5];
    snprintf(tail, sizeof(tail), "%02X%02X", mac[4], mac[5]);
    apSsid = String(PROVISION_AP_PREFIX) + tail;

    WiFi.mode(WIFI_AP);
    const char *pwd = strlen(PROVISION_AP_PASSWORD) ? PROVISION_AP_PASSWORD
                                                    : nullptr;
    WiFi.softAP(apSsid.c_str(), pwd);
    delay(100);
    apIp = WiFi.softAPIP();

    dns.start(53, "*", apIp);

    web.on("/", HTTP_GET, handleRoot);
    web.on("/save", HTTP_POST, handleSave);
    web.on("/generate_204", HTTP_GET, handleCaptive);
    web.on("/hotspot-detect.html", HTTP_GET, handleCaptive);
    web.on("/ncsi.txt", HTTP_GET, handleCaptive);
    web.onNotFound(handleCaptive);
    web.begin();

    outSsid = apSsid;
    outIp   = apIp;
}

void tick() {
    dns.processNextRequest();
    web.handleClient();
}

bool isDone() { return done; }

const Stored &result() { return submitted; }

void end() {
    web.stop();
    dns.stop();
    WiFi.softAPdisconnect(true);
}

}  // namespace Provision
