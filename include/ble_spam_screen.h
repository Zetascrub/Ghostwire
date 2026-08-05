#pragma once

#include "ble_spam_service.h"

// BLE Spam screens: a small mode-select list followed by a live status
// screen. Third extraction out of src/main.cpp (see
// docs/screen-extraction.md); first one touching the shared list cursor.
//
// main.cpp still owns the cursor itself (listSelection/listOffset) and the
// normalizeListPosition()/moveSelection() bookkeeping around it -- that's
// shared infrastructure used by dozens of other menu screens, not state this
// screen owns. drawSelect() just takes the current selection to render, and
// modeForSelection() gives main.cpp's input handler and drawSelect() a
// single shared source of truth for what each row means, instead of the
// index-to-mode mapping being duplicated between them (as it was before this
// extraction).
class BleSpamScreen {
public:
    explicit BleSpamScreen(BleSpamService& service) : service_(service) {}

    static constexpr size_t kModeCount = 4;
    static const char* modeLabel(size_t selection);
    static BleSpamMode modeForSelection(size_t selection);

    void drawSelect(size_t selection);
    void drawActive(bool fullDraw = true);

private:
    BleSpamService& service_;
};
