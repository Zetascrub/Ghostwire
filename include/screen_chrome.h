#pragma once

// Shared chrome primitives that extracted screen modules use to draw a
// consistent header/footer and to recover keyboard state after a blocking
// radio/IO call. Implemented in main.cpp (see the ScreenChrome:: forwarders
// just after the anonymous namespace closes) so extracted screens do not
// need to duplicate M5Cardputer/Branding drawing code.
//
// This is a deliberately small, hand-picked surface -- add to it only as
// more screens are extracted and need a primitive that isn't here yet.
// See docs/screen-extraction.md for the extraction pattern this supports.

namespace ScreenChrome {

// Clears the screen, draws the title bar, and refreshes the status icons
// (wifi/capture/clock/battery).
void drawHeader(const char* title);

// Draws the footer hint bar (usually key hints for the current screen).
void drawFooter(const char* text);

// Reinitializes the keyboard and pumps a few update cycles. Call this after
// a blocking radio/IO operation so a held Enter key isn't misread as a
// fresh press when the screen becomes responsive again.
void recoverKeyboard();

}  // namespace ScreenChrome
