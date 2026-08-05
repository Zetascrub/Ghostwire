#include "lora_screen.h"

#include <M5Cardputer.h>
#include <algorithm>
#include <cmath>

#include "branding.h"
#include "screen_chrome.h"

void LoRaScreen::draw(bool fullDraw) {
    const uint32_t signature =
        (static_cast<uint32_t>(service_.isReady()) << 31) ^
        (static_cast<uint32_t>(logger_.isActive()) << 30) ^
        (static_cast<uint32_t>(service_.profile()) << 28) ^
        service_.packetCount() ^ (logger_.rowCount() << 8);
    if (!fullDraw && signature == lastSignature_) return;
    lastSignature_ = signature;
    ScreenChrome::beginContentUpdate("Mesh Field Client", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(service_.isReady() ? Branding::accent
                                             : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    if (service_.isReady()) {
        display.printf("SX1262 ready %.3f MHz", service_.frequencyMhz());
    } else {
        display.printf("Radio init failed: %d", service_.status());
    }
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 47);
    display.printf("Packets: %lu  Nodes: %u  Msgs: %u",
                   static_cast<unsigned long>(service_.packetCount()),
                   static_cast<unsigned>(service_.nodes().size()),
                   static_cast<unsigned>(service_.messages().size()));
    if (logger_.isActive()) {
        display.printf("  REC %lu",
                       static_cast<unsigned long>(logger_.rowCount()));
    }
    display.setCursor(8, 64);
    if (service_.packetCount() > 0) {
        display.printf("RSSI %.1f dBm  SNR %.1f dB", service_.lastRssi(),
                       service_.lastSnr());
        display.setCursor(8, 82);
        const auto& decoded = service_.lastDecoded();
        if (decoded.valid) {
            display.printf("Mesh %08lX  %s",
                           static_cast<unsigned long>(decoded.from),
                           MeshtasticDecoder::portName(decoded.port));
            display.setCursor(8, 99);
            display.print(decoded.summary.substring(0, 37));
        } else {
            display.print(service_.lastPacket().substring(0, 36));
            display.setCursor(8, 99);
            display.setTextColor(Branding::muted, Branding::background);
            display.print("Encrypted / unsupported payload");
        }
    } else {
        display.setTextColor(Branding::muted, Branding::background);
        display.print(service_.profileName());
        display.setCursor(8, 82);
        if (service_.profile() == LoRaService::Profile::MeshtasticEuLongFast) {
            display.print("BW250 SF11 CR4/5 sync 0x2B");
        } else {
            display.print("BW125 SF12 CR4/5 sync 0x34");
        }
    }
    if (fullDraw) ScreenChrome::drawFooter("N: nodes  M: messages  Tab: actions");
}

void LoRaScreen::drawNodes(size_t selection, size_t offset) {
    const auto& nodes = service_.nodes();
    ScreenChrome::drawHeader("Mesh Nodes");
    ScreenChrome::drawHeaderPosition(nodes.empty() ? 0 : selection + 1,
                                     nodes.size());
    if (nodes.empty()) {
        auto& display = M5Cardputer.Display;
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 42);
        display.print("No decoded nodes yet");
        display.setCursor(8, 59);
        display.print("Listening continues in background");
    } else {
        const size_t end = std::min(nodes.size(),
                                    offset + ScreenChrome::kVisibleRows);
        for (size_t i = offset; i < end; ++i) {
            const auto& node = nodes[i];
            ScreenChrome::drawListRow(
                static_cast<int>(i - offset), service_.nodeDisplayName(node.id),
                i == selection, String(node.lastRssi, 0) + "dB");
        }
    }
    ScreenChrome::drawFooter("Enter: details   Q: dashboard");
}

void LoRaScreen::drawNodeDetail(size_t selection) {
    const auto& nodes = service_.nodes();
    ScreenChrome::drawHeader("Mesh Node");
    if (selection >= nodes.size()) return;
    const auto& node = nodes[selection];
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::accent, Branding::background);
    display.setCursor(8, 29);
    display.print(service_.nodeDisplayName(node.id).substring(0, 35));
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 47);
    display.printf("ID !%08lX  packets %lu", static_cast<unsigned long>(node.id),
                   static_cast<unsigned long>(node.packets));
    display.setCursor(8, 64);
    display.printf("RSSI %.1f  SNR %.1f", node.lastRssi, node.lastSnr);
    if (node.hasDeviceMetrics) {
        display.printf("  BAT %lu%%", static_cast<unsigned long>(node.batteryLevel));
    }
    display.setCursor(8, 81);
    if (node.hasPosition) {
        display.printf("%.5f, %.5f", node.latitude, node.longitude);
        display.setCursor(8, 98);
        if (gnss_.hasFix()) {
            constexpr double radians = 0.017453292519943295;
            constexpr double earthRadius = 6371000.0;
            const double lat1 = gnss_.latitude() * radians;
            const double lat2 = node.latitude * radians;
            const double dLat = lat2 - lat1;
            const double dLon = (node.longitude - gnss_.longitude()) * radians;
            const double a = sin(dLat / 2) * sin(dLat / 2) + cos(lat1) *
                             cos(lat2) * sin(dLon / 2) * sin(dLon / 2);
            const double distance = earthRadius * 2 * atan2(sqrt(a), sqrt(1 - a));
            const double y = sin(dLon) * cos(lat2);
            const double x = cos(lat1) * sin(lat2) -
                             sin(lat1) * cos(lat2) * cos(dLon);
            double bearing = atan2(y, x) / radians;
            if (bearing < 0) bearing += 360.0;
            display.printf("Range %.0f m  bearing %.0f deg", distance, bearing);
        } else {
            display.printf("Altitude %ld m", static_cast<long>(node.altitude));
        }
    } else {
        display.setTextColor(Branding::muted, Branding::background);
        display.print("No position received");
    }
    ScreenChrome::drawFooter("Q: node list");
}

void LoRaScreen::drawMessages(size_t selection, size_t offset) {
    const auto& messages = service_.messages();
    ScreenChrome::drawHeader("Mesh Messages");
    ScreenChrome::drawHeaderPosition(messages.empty() ? 0 : selection + 1,
                                     messages.size());
    if (messages.empty()) {
        auto& display = M5Cardputer.Display;
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 42);
        display.print("No public-channel messages yet");
    } else {
        const size_t end = std::min(messages.size(),
                                    offset + ScreenChrome::kVisibleRows);
        for (size_t i = offset; i < end; ++i) {
            const auto& message = messages[messages.size() - 1 - i];
            const String label = service_.nodeDisplayName(message.from) + ": " +
                                 message.text;
            ScreenChrome::drawListRow(static_cast<int>(i - offset), label,
                                      i == selection);
        }
    }
    ScreenChrome::drawFooter("Enter: read   Q: dashboard");
}

void LoRaScreen::drawMessageDetail(size_t selection) {
    const auto& messages = service_.messages();
    ScreenChrome::drawHeader("Mesh Message");
    if (selection >= messages.size()) return;
    const auto& message = messages[messages.size() - 1 - selection];
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::accent, Branding::background);
    display.setCursor(8, 29);
    display.print(service_.nodeDisplayName(message.from).substring(0, 35));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 46);
    display.printf("ID %08lX  to %08lX",
                   static_cast<unsigned long>(message.packetId),
                   static_cast<unsigned long>(message.to));
    display.setTextColor(Branding::text, Branding::background);
    for (size_t line = 0; line < 3; ++line) {
        const size_t start = line * 36;
        if (start >= message.text.length()) break;
        display.setCursor(8, 64 + static_cast<int>(line) * 16);
        display.print(message.text.substring(start, start + 36));
    }
    ScreenChrome::drawFooter("Q: message inbox");
}

void LoRaScreen::drawRadar() {
    ScreenChrome::drawHeader("Mesh Position Radar");
    auto& display = M5Cardputer.Display;
    constexpr int centerX = 120;
    constexpr int centerY = 74;
    constexpr int radius = 42;
    display.drawCircle(centerX, centerY, radius, Branding::muted);
    display.drawCircle(centerX, centerY, radius / 2, Branding::muted);
    display.drawLine(centerX - radius, centerY, centerX + radius, centerY,
                     Branding::muted);
    display.drawLine(centerX, centerY - radius, centerX, centerY + radius,
                     Branding::muted);
    display.setTextColor(Branding::accent, Branding::background);
    display.setCursor(centerX - 2, 25);
    display.print("N");
    display.fillCircle(centerX, centerY, 3, Branding::accent);
    if (!gnss_.hasFix()) {
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(8, 111);
        display.print("Waiting for local GNSS fix");
        ScreenChrome::drawFooter("Q: mesh menu");
        return;
    }
    constexpr double radians = 0.017453292519943295;
    constexpr double earthRadius = 6371000.0;
    struct Plot { double distance; double bearing; };
    std::vector<Plot> plots;
    double furthest = 1.0;
    for (const auto& node : service_.nodes()) {
        if (!node.hasPosition) continue;
        const double lat1 = gnss_.latitude() * radians;
        const double lat2 = node.latitude * radians;
        const double dLat = lat2 - lat1;
        const double dLon = (node.longitude - gnss_.longitude()) * radians;
        const double a = sin(dLat / 2) * sin(dLat / 2) + cos(lat1) *
                         cos(lat2) * sin(dLon / 2) * sin(dLon / 2);
        const double distance = earthRadius * 2 * atan2(sqrt(a), sqrt(1 - a));
        double bearing = atan2(sin(dLon) * cos(lat2),
            cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon));
        plots.push_back({distance, bearing});
        furthest = std::max(furthest, distance);
    }
    for (const auto& plot : plots) {
        const double scale = std::min(1.0, plot.distance / furthest);
        const int x = centerX + static_cast<int>(sin(plot.bearing) * radius * scale);
        const int y = centerY - static_cast<int>(cos(plot.bearing) * radius * scale);
        display.fillCircle(x, y, 3, Branding::warning);
    }
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 111);
    display.printf("%u positioned  scale %.0f m",
                   static_cast<unsigned>(plots.size()), furthest);
    ScreenChrome::drawFooter("R: refresh   Q: mesh menu");
}
