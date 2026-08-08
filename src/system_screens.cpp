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

void SystemScreens::drawTimeReadouts() {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, 45, display.width(), 40, Branding::background);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 52);
    if (clockSynced_) {
        const time_t now = time(nullptr);
        struct tm local {};
        localtime_r(&now, &local);
        char localValue[32];
        strftime(localValue, sizeof(localValue), "%Y-%m-%d %H:%M:%S %Z", &local);
        display.printf("Local: %s", localValue);
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
        display.print("Local: ----/--/-- --:--:--");
        display.setCursor(8, 69);
        display.print("UTC:   ----/--/-- --:--:--");
    }
}

void SystemScreens::drawTimeStatus() {
    ScreenChrome::drawHeader("System Clock");
    auto& display = M5Cardputer.Display;
    display.setTextColor(clockSynced_ ? Branding::accent : Branding::warning,
                         Branding::background);
    display.setCursor(8, 32);
    display.print(clockSynced_ ? "System clock synchronized"
                               : "System clock not synchronized");
    drawTimeReadouts();
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 87);
    display.print(clockStatus_.substring(0, 37));
    display.setCursor(8, 103);
    display.print("Clock resets after full power-off.");
    ScreenChrome::drawFooter("Tab: actions   Q: back");
}
