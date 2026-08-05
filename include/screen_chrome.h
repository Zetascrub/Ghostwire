#pragma once

#include <Arduino.h>

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

// Number of list rows visible at once in the compact list view. Screens
// with more items than this scroll via the shared listOffset cursor.
inline constexpr size_t kVisibleRows = 6;

// Clears the screen, draws the title bar, and refreshes the status icons
// (wifi/capture/clock/battery).
void drawHeader(const char* title);

// Draws the footer hint bar (usually key hints for the current screen).
void drawFooter(const char* text);

// Live screens repaint only their content pane on periodic refresh so the
// header/footer don't flicker just because telemetry changed. Pass
// fullDraw=true (via drawHeader) when entering the screen or restoring it
// after an overlay, false for periodic updates.
void beginContentUpdate(const char* title, bool fullDraw);

// Reinitializes the keyboard and pumps a few update cycles. Call this after
// a blocking radio/IO operation so a held Enter key isn't misread as a
// fresh press when the screen becomes responsive again.
void recoverKeyboard();

// Draws one row of a selectable list at the given zero-based visible row
// (not list index -- scrolling/offset bookkeeping stays with main.cpp's
// shared list cursor). `selected` highlights it; `suffix` right-aligns
// extra text such as a status label.
void drawListRow(int row, const String& label, bool selected,
                 const String& suffix = "");

// Clamps main.cpp's shared listSelection/listOffset cursor to `count` items
// (0 selects nothing). Mutates those globals directly, same as it always
// has -- this is a forwarder, not a copy, so a reference to listSelection/
// listOffset taken elsewhere still sees the update.
void normalizeListPosition(size_t count);

// Small "n/nn" position readout in the header, between the title and the
// status icons. Call right after drawHeader(title).
void drawHeaderPosition(size_t oneBasedIndex, size_t total);

// One-item-at-a-time "Cards" navigation view: a big icon, title,
// description, page dots, and an optional status badge. The compact list
// view (drawListRow) is the alternative when Cards mode is off.
void drawNavigationCard(const char* header, const String& label,
                        const String& description, size_t selected,
                        size_t count, uint8_t icon, const String& badge = "");

// Draws a single-line "label: value" text-entry field at the given y,
// right-clipping the value to fit. `masked` shows asterisks instead of the
// actual value (passwords). Used both by draw functions and directly from
// live text-entry input handling, so it repaints just that row.
void drawTextEntryRow(int y, const char* label, const String& value,
                      bool masked = false);

}  // namespace ScreenChrome
