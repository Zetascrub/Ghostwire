#include "familiar_screens.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <algorithm>

#include "branding.h"
#include "screen_chrome.h"

void AiChatScreen::drawComposer() {
    auto& display = M5Cardputer.Display;
    display.fillRect(4, 86, display.width() - 8, 31, Branding::background);
    display.drawFastHLine(4, 85, display.width() - 8, Branding::panel);
    display.setTextSize(1);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(6, 91);
    const String promptLine = "> " + prompt_;
    display.print(promptLine.substring(
        promptLine.length() > 38 ? promptLine.length() - 38 : 0));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(6, 106);
    const String notice = notice_.isEmpty() ? service_.status() : notice_;
    display.print(notice.substring(0, 39));
}

namespace {
// Word-wraps `prefix + text` to 38 columns, same rule AiChatScreen::draw()
// always used inline before extraction: break on the last space within the
// column budget, or mid-word if there's no good break.
void appendWrappedAiLines(std::vector<String>& lines, const String& prefix,
                          const String& text) {
    constexpr size_t width = 38;
    String remaining = prefix + text;
    remaining.replace("\r", "");
    while (!remaining.isEmpty()) {
        int newline = remaining.indexOf('\n');
        size_t take = std::min(width, remaining.length());
        if (newline >= 0 && static_cast<size_t>(newline) < take) {
            take = static_cast<size_t>(newline);
        } else if (take < remaining.length()) {
            const int space = remaining.substring(0, take + 1).lastIndexOf(' ');
            if (space > 4) take = static_cast<size_t>(space);
        }
        lines.push_back(remaining.substring(0, take));
        size_t remove = take;
        while (remove < remaining.length() &&
               (remaining[remove] == ' ' || remaining[remove] == '\n')) {
            ++remove;
        }
        remaining.remove(0, remove);
        if (take == 0 && remove == 0) break;
    }
}
}  // namespace

void AiChatScreen::draw() {
    String title = "AI: " + String(service_.providerName());
    ScreenChrome::drawHeader(title.c_str());
    auto& display = M5Cardputer.Display;
    display.setTextSize(1);
    std::vector<String> lines;
    for (const auto& turn : service_.history()) {
        appendWrappedAiLines(lines, turn.role == "user" ? "> " : "< ", turn.text);
    }
    constexpr size_t shown = 4;
    const size_t maxScroll = lines.size() > shown ? lines.size() - shown : 0;
    if (scrollLines_ > maxScroll) scrollLines_ = maxScroll;
    const size_t end =
        lines.size() > scrollLines_ ? lines.size() - scrollLines_ : 0;
    const size_t start = end > shown ? end - shown : 0;
    for (size_t index = start; index < end; ++index) {
        display.setTextColor(
            lines[index].startsWith("> ") ? Branding::accent : Branding::text,
            Branding::background);
        display.setCursor(6, 27 + (index - start) * 14);
        display.print(lines[index].substring(0, 39));
    }
    drawComposer();
    ScreenChrome::drawFooter("Up/Down scroll Enter send Tab provider");
}

void FamiliarScreens::drawSpeechBubble(int x, int y, int width) {
    if (speechBubble_.isEmpty() || millis() >= speechBubbleUntil_) return;
    auto& display = M5Cardputer.Display;
    display.fillRoundRect(x, y, width, 18, 5, Branding::text);
    display.fillTriangle(x + 12, y + 17, x + 20, y + 17, x + 16, y + 23,
                         Branding::text);
    display.setTextColor(Branding::background, Branding::text);
    display.setCursor(x + 5, y + 5);
    display.print(speechBubble_.substring(0, (width - 10) / 6));
}

const char* FamiliarScreens::face() const {
    const bool blink = (millis() / 180U) % 23U == 0;
    if (blink) return "(-_-)";
    if (millis() < reactionUntil_) {
        switch (reaction_) {
            case FamiliarReaction::HostFound: return "(^.^)!";
            case FamiliarReaction::ServiceFound: return "(o.o)+";
            case FamiliarReaction::Warning: return "(O.O)!";
            case FamiliarReaction::Complete: return "(^.^)7";
            case FamiliarReaction::Searching: return "(o.o)?";
            default: break;
        }
    }
    switch (familiar_.mood()) {
        case FamiliarMood::Curious: return "(o_o)?";
        case FamiliarMood::Excited: return "(^o^)";
        case FamiliarMood::Sleepy: return "(-.-)z";
        case FamiliarMood::Proud: return "(^_^)7";
        case FamiliarMood::Worried: return "(O_O)!";
        case FamiliarMood::Dizzy: return "(@_@)";
        default: return "(._.)";
    }
}

void FamiliarScreens::drawCreature(int centerX, int baseY, bool large) {
    auto& display = M5Cardputer.Display;
    const uint32_t phase = millis() / 180U;
    const int bob = (phase % 8U == 1U || phase % 8U == 2U) ? -1 : 0;
    const int width = large ? 68 : 50;
    const int height = large ? 40 : 29;
    const int x = centerX - width / 2;
    const int y = baseY - height + bob;
    const uint16_t color = reaction_ == FamiliarReaction::Warning &&
                                   millis() < reactionUntil_
                               ? Branding::warning
                               : Branding::accent;

    // Tail, ears and rounded body are redrawn procedurally: no frame buffers.
    const int tailLift = (phase / 2U) % 2U == 0U ? 0 : 4;
    display.drawLine(x + width - 2, y + height - 10, x + width + 10,
                     y + height - 15 - tailLift, color);
    display.drawLine(x + width + 10, y + height - 15 - tailLift, x + width + 14,
                     y + height - 10 - tailLift, color);
    display.fillTriangle(x + 7, y + 7, x + 14, y - 6, x + 23, y + 5, color);
    display.fillTriangle(x + width - 23, y + 5, x + width - 14, y - 6,
                         x + width - 7, y + 7, color);
    display.fillRoundRect(x, y, width, height, large ? 10 : 7, color);
    display.fillRoundRect(x + 3, y + 3, width - 6, height - 6, large ? 8 : 5,
                          Branding::background);

    display.setTextSize(large ? 2 : 1);
    display.setTextColor(color, Branding::background);
    const String faceText = face();
    display.setCursor(
        centerX - static_cast<int>(faceText.length()) * (large ? 6 : 3),
        y + (large ? 12 : 10));
    display.print(faceText);

    const bool searching =
        patrol_.isActive() && patrol_.state() == FamiliarPatrolState::Discovery;
    if (searching) {
        const int radius = 4 + static_cast<int>((phase % 4U) * 3U);
        display.drawCircle(x - 8, y + height / 2, radius, color);
        display.drawPixel(x - 8, y + height / 2, Branding::text);
    }
    display.setTextSize(1);
}

void FamiliarScreens::drawFamiliar(bool fullDraw) {
    if (fullDraw || page_ != 0) {
        ScreenChrome::beginContentUpdate(familiar_.name().c_str(), fullDraw);
    } else {
        // Preserve the static dashboard; only invalidate the animated zone.
        M5Cardputer.Display.fillRect(74, 22, 162, 46, Branding::background);
    }
    auto& display = M5Cardputer.Display;
    if (page_ == 0) {
        drawCreature(120, 66, false);
        drawSpeechBubble(145, 29, 88);
        display.setTextColor(Branding::text, Branding::background);
        display.setCursor(8, 70);
        display.printf("Lv %u  %s  Bond %u",
                       static_cast<unsigned>(familiar_.level()),
                       familiar_.evolutionName(),
                       static_cast<unsigned>(familiar_.bond()));
        const uint32_t levelStart =
            static_cast<uint32_t>(familiar_.level() - 1U) * 100U;
        const uint32_t progress = familiar_.xp() - levelStart;
        display.drawRect(8, 84, 224, 8, Branding::muted);
        display.fillRect(10, 86, std::min<uint32_t>(220, progress * 220 / 100),
                         4, Branding::accent);
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 98);
        display.print((workflowStatus_.isEmpty() ? familiar_.lastMessage()
                                                  : workflowStatus_)
                          .substring(0, 37));
    } else if (page_ == 1) {
        display.setTextColor(Branding::text, Branding::background);
        display.setCursor(8, 30);
        display.printf("Mood:        %s", familiar_.moodName());
        display.setCursor(8, 47);
        const uint32_t seconds = familiar_.ageSeconds();
        const uint32_t days = seconds / 86400U;
        const uint8_t hours = (seconds / 3600U) % 24U;
        display.printf("Age:         %lud %uh", static_cast<unsigned long>(days),
                       hours);
        display.setCursor(8, 64);
        display.printf("Wi-Fi seen:  %lu",
                       static_cast<unsigned long>(familiar_.wifiDiscoveries()));
        display.setCursor(8, 81);
        display.printf("BLE seen:    %lu",
                       static_cast<unsigned long>(familiar_.bleDiscoveries()));
        display.setCursor(8, 98);
        display.printf("Tools known: %lu",
                       static_cast<unsigned long>(familiar_.toolCount()));
    } else {
        const auto& journal = familiar_.journal();
        display.setTextColor(Branding::text, Branding::background);
        const size_t shown = std::min<size_t>(6, journal.size());
        const size_t start = journal.size() - shown;
        for (size_t row = 0; row < shown; ++row) {
            display.setCursor(6, 27 + static_cast<int>(row) * 15);
            display.print(("> " + journal[start + row]).substring(0, 39));
        }
    }
    if (fullDraw) ScreenChrome::drawFooter("Up/Down: pages   Tab: menu");
}

void FamiliarScreens::drawResetConfirm() {
    ScreenChrome::drawHeader("Reset Cyber Familiar");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 34);
    display.print("This clears progression, bond,");
    display.setCursor(8, 50);
    display.print("discoveries, journal and tools.");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 76);
    display.print("Name and idle preference remain.");
    ScreenChrome::drawFooter("Enter: reset   Esc: cancel");
}

void FamiliarScreens::drawMissions() {
    ScreenChrome::drawHeader("Familiar Missions");
    ScreenChrome::normalizeListPosition(kMissionCount);
    const char* const labels[kMissionCount] = {
        "Network Recon",
        "Handshake Capture",
    };
    const String values[kMissionCount] = {
        patrol_.isActive() ? "ACTIVE" : "",
        handshakeMissionRunning_ ? "ACTIVE" : "",
    };
    for (size_t row = 0; row < kMissionCount; ++row) {
        ScreenChrome::drawListRow(row, labels[row], row == listSelection_,
                                  values[row]);
    }
    ScreenChrome::drawFooter("Enter: open   Q: back");
}

void FamiliarScreens::drawPatrolConfirm() {
    ScreenChrome::drawHeader("Confirm Familiar Patrol");
    auto& display = M5Cardputer.Display;
    if (!sdAvailable_ || WiFi.status() != WL_CONNECTED) {
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(8, 34);
        display.print(!sdAvailable_ ? "microSD card required"
                                    : "Connect Wi-Fi first");
        ScreenChrome::drawFooter("Esc: cancel");
        return;
    }
    const String scope =
        FamiliarPatrolService::cidrText(WiFi.localIP(), WiFi.subnetMask());
    const uint32_t count = FamiliarPatrolService::usableAddressCount(
        WiFi.localIP(), WiFi.subnetMask());
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 28);
    display.print("AUTHORIZED SCOPE REQUIRED");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 47);
    display.printf("Scope: %s", scope.c_str());
    display.setCursor(8, 63);
    display.printf("Addresses: %lu", static_cast<unsigned long>(count));
    display.setCursor(8, 79);
    display.printf("Mode: %s", patrolContinuousChoice_ ? "Continuous Watch"
                                                        : "One-shot scout");
    display.setCursor(8, 95);
    display.printf(
        "Interval: %lus",
        static_cast<unsigned long>(
            kFamiliarPatrolIntervals[patrolIntervalIndex_] / 1000));
    ScreenChrome::drawFooter("C: mode  V: interval  Enter: go");
}

void FamiliarScreens::drawPatrol(bool fullDraw) {
    ScreenChrome::beginContentUpdate("Familiar Patrol", fullDraw);
    auto& display = M5Cardputer.Display;
    const FamiliarPatrolState state = patrol_.state();
    display.setTextColor(state == FamiliarPatrolState::Error ? Branding::warning
                          : patrol_.isActive()                ? Branding::accent
                                                               : Branding::text,
                         Branding::background);
    display.setCursor(8, 28);
    display.printf("%s", patrol_.stateName());
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(158, 28);
    display.printf("Mood:%s", familiar_.moodName());
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 44);
    if (state == FamiliarPatrolState::WatchWait) {
        display.printf(
            "Cycle %lu; next scan in %lus",
            static_cast<unsigned long>(patrol_.cycle()),
            static_cast<unsigned long>(patrol_.nextScanSeconds()));
    } else if (state == FamiliarPatrolState::Discovery) {
        display.printf(
            "Addresses: %lu / %lu",
            static_cast<unsigned long>(patrol_.discoveryScanned()),
            static_cast<unsigned long>(patrol_.addressCount()));
    } else {
        display.printf(
            "Host: %lu / %lu", static_cast<unsigned long>(patrol_.hostIndex() + 1),
            static_cast<unsigned long>(patrol_.cycleHostsFound()));
    }
    display.setCursor(8, 60);
    display.printf("Found: %lu hosts  %lu open ports",
                   static_cast<unsigned long>(patrol_.hostsFound()),
                   static_cast<unsigned long>(patrol_.openPortsFound()));
    display.setCursor(8, 76);
    const uint32_t port = patrol_.currentPort();
    if (port > 0) {
        display.printf("%s : %lu", patrol_.currentHost().toString().c_str(),
                       static_cast<unsigned long>(port));
    } else {
        display.print(patrol_.status().substring(0, 37));
    }
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 94);
    if (!speechBubble_.isEmpty() && millis() < speechBubbleUntil_) {
        drawSpeechBubble(7, 91, 226);
    } else {
        String path = patrol_.sessionPath();
        const int slash = path.lastIndexOf('/');
        if (slash >= 0) path = path.substring(slash + 1);
        display.print(path.substring(0, 37));
    }
    if (fullDraw) {
        ScreenChrome::drawFooter(patrol_.isActive() ? "Tab: stop menu   Q: background"
                                                    : "Q: back");
    }
}
