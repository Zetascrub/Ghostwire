#pragma once

#include <Arduino.h>
#include <vector>

// System diagnostic row state/type. Moved here (from main.cpp) since both
// main.cpp's systemDiagnostics() aggregator and SystemScreens need it -- see
// docs/screen-extraction.md.
enum class DiagnosticState {
    Information,
    Ready,
    Warning,
};

struct SystemDiagnostic {
    String label;
    String value;
    DiagnosticState state;

    SystemDiagnostic(const String& diagnosticLabel, const String& diagnosticValue,
                     DiagnosticState diagnosticState = DiagnosticState::Information)
        : label(diagnosticLabel), value(diagnosticValue), state(diagnosticState) {}
};

// System Diagnostics and System Clock screens. Draw-only (see
// docs/screen-extraction.md). systemDiagnostics() itself -- the aggregator
// that reads state from a couple dozen subsystems -- stays in main.cpp and
// is passed in to drawSystem() as a snapshot rather than called from here,
// same reasoning as the other screens that redirect on stale state: keeps
// this screen from needing a reference to everything the diagnostics list
// touches.
class SystemScreens {
public:
    SystemScreens(size_t& listSelection, size_t& listOffset,
                 String& diagnosticExportStatus, bool& clockSynced,
                 String& clockStatus)
        : listSelection_(listSelection),
          listOffset_(listOffset),
          diagnosticExportStatus_(diagnosticExportStatus),
          clockSynced_(clockSynced),
          clockStatus_(clockStatus) {}

    void drawSystem(const std::vector<SystemDiagnostic>& diagnostics);
    void drawTimeStatus();
    // Partial redraw of just the local/UTC readout rows, used by both
    // drawTimeStatus() and main.cpp's periodic tick while that screen is
    // shown.
    void drawTimeReadouts();

private:
    size_t& listSelection_;
    size_t& listOffset_;
    String& diagnosticExportStatus_;
    bool& clockSynced_;
    String& clockStatus_;
};
