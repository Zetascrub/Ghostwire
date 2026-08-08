#include "system_screens.h"

#include <M5Cardputer.h>
#include <ctime>

#include "branding.h"
#include "screen_chrome.h"

void SystemScreens::drawSystem(const std::vector<SystemDiagnostic>& diagnostics) {
    ScreenChrome::drawHeader("System Diagnostics");
    ScreenChrome::normalizeListPosition(diagnostics.size());
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, diagnostics.size());
    for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                         row + listOffset_ < diagnostics.size();
        ++row) {
        const auto& diagnostic = diagnostics[row + listOffset_];
        String label = diagnostic.label;
        while (label.length() < 13) label += ' ';
        label += diagnostic.value;
        ScreenChrome::drawListRow(row, label, row + listOffset_ == listSelection_);
    }
    ScreenChrome::drawFooter(
        diagnosticExportStatus_.isEmpty()
            ? "W/S browse Enter clock Tab menu Q back"
            : diagnosticExportStatus_.c_str());
}

namespace {
// "+HH:MM"/"-HH:MM" label for the configured display offset. The offset only
// ever shifts what's shown here -- the RTC and every logged timestamp stay
// true UTC (see main.cpp's utcTimestamp()).
String timezoneOffsetLabel(int16_t offsetMinutes) {
    const char sign = offsetMinutes < 0 ? '-' : '+';
    const int16_t magnitude = offsetMinutes < 0 ? -offsetMinutes : offsetMinutes;
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%c%02d:%02d", sign, magnitude / 60,
             magnitude % 60);
    return String(buffer);
}
}  // namespace

void SystemScreens::drawTimeReadouts() {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, 45, display.width(), 40, Branding::background);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 52);
    if (clockSynced_) {
        const time_t now = time(nullptr);
        // Local display time is computed by hand -- epoch shifted by the
        // configured offset, formatted as UTC -- rather than through libc's
        // TZ/localtime_r, so there's no DST rule to keep correct or wrong.
        const time_t shifted =
            now + static_cast<time_t>(clockUtcOffsetMinutes_) * 60;
        struct tm local {};
        gmtime_r(&shifted, &local);
        char localValue[24];
        strftime(localValue, sizeof(localValue), "%Y-%m-%d %H:%M:%S", &local);
        display.printf("Local %s: %s",
                       timezoneOffsetLabel(clockUtcOffsetMinutes_).c_str(),
                       localValue);
        display.setCursor(8, 69);
        // Matches main.cpp's utcTimestamp() (kept there -- it's used by
        // over a dozen CSV loggers outside this screen); duplicated rather
        // than exposed across the translation-unit boundary since it's this
        // small and stable.
        struct tm utc {};
        gmtime_r(&now, &utc);
        char utcValue[25];
        strftime(utcValue, sizeof(utcValue), "%Y-%m-%dT%H:%M:%SZ", &utc);
        display.printf("UTC:   %s", utcValue);
    } else {
        display.printf("Local %s: ----/--/-- --:--:--",
                       timezoneOffsetLabel(clockUtcOffsetMinutes_).c_str());
        display.setCursor(8, 69);
        display.print("UTC:   ----/--/-- --:--:--");
    }
}

void SystemScreens::drawTimeStatus() {
    ScreenChrome::drawHeader("System Clock");
    auto& display = M5Cardputer.Display;
    if (clockManualEntryActive_) {
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 32);
        display.print("Enter UTC date/time:");
        ScreenChrome::drawTextEntryRow(52, "> ", clockManualInput_);
        display.setCursor(8, 74);
        display.print("Format: YYYY-MM-DD HH:MM");
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(8, 90);
        display.print(clockStatus_.startsWith("Invalid")
                          ? clockStatus_.substring(0, 37)
                          : "");
        ScreenChrome::drawFooter("Enter: set   Esc: cancel");
        return;
    }
    display.setTextColor(clockSynced_ ? Branding::accent : Branding::warning,
                         Branding::background);
    display.setCursor(8, 32);
    display.print(clockSynced_ ? "System clock synchronized"
                               : "System clock not synchronized");
    drawTimeReadouts();
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 87);
    String sourceLine = clockStatus_;
    if (clockSynced_ && lastClockSyncMs_ != 0) {
        const uint32_t ageSeconds = (millis() - lastClockSyncMs_) / 1000;
        sourceLine += ageSeconds < 60
                          ? " (" + String(ageSeconds) + "s ago)"
                          : " (" + String(ageSeconds / 60) + "m ago)";
    }
    display.print(sourceLine.substring(0, 40));
    display.setCursor(8, 103);
    display.print(clockSynced_ ? clockProvenance_.substring(0, 40)
                               : "Left/Right: adjust displayed time zone");
    ScreenChrome::drawFooter("Tab: actions  Left/Right: zone  Q: back");
}
