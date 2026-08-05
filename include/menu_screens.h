#pragma once

#include <Arduino.h>

#include "cyber_familiar.h"
#include "familiar_patrol_service.h"
#include "war_drive_service.h"
#include "wifi_guardian_service.h"

// Static list-menu screens: home/category menus that show a fixed item
// list in either the compact list view or the one-at-a-time Cards view
// (see ScreenChrome::drawNavigationCard), plus a small status badge on a
// few items. None of these carry state of their own -- they only read the
// shared list cursor, the Cards/Compact toggle, and a handful of services
// for badges -- so they're grouped into one module rather than a class per
// screen. See docs/screen-extraction.md.
//
// All draw*() methods are zero-argument, matching the original
// parameterless free functions: they read the bound references directly,
// same as those functions read the globals.
class MenuScreens {
public:
    MenuScreens(size_t& listSelection, size_t& listOffset,
               size_t& menuSelection, bool& cardNavigationEnabled,
               CyberFamiliar& familiar, WifiGuardianService& wifiGuardian,
               FamiliarPatrolService& patrol, WarDriveService& warDrive,
               bool& sdAvailable)
        : listSelection_(listSelection),
          listOffset_(listOffset),
          menuSelection_(menuSelection),
          cardNav_(cardNavigationEnabled),
          familiar_(familiar),
          wifiGuardian_(wifiGuardian),
          patrol_(patrol),
          warDrive_(warDrive),
          sdAvailable_(sdAvailable) {}

    void drawMain();
    void drawObserve();
    void drawFieldKit();
    void drawWifi();
    void drawBle();
    void drawDevices();
    void drawRfid();
    void drawTools();
    void drawGps();
    void drawMesh();
    void drawNetwork();
    void drawSettings();

    // Main menu item labels/count. Public (and static) because main.cpp's
    // input handler also needs them for Up/Down wraparound -- this is the
    // one shared source of truth for what the main menu contains, same
    // reasoning as BleSpamScreen::modeForSelection().
    static const char* const kMenuItems[];
    static constexpr size_t kMenuCount = 6;

private:
    size_t& listSelection_;
    size_t& listOffset_;
    size_t& menuSelection_;
    bool& cardNav_;
    CyberFamiliar& familiar_;
    WifiGuardianService& wifiGuardian_;
    FamiliarPatrolService& patrol_;
    WarDriveService& warDrive_;
    bool& sdAvailable_;
};
