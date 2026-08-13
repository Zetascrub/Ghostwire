#pragma once

#include <Arduino.h>
#include <vector>

#include "app_screen.h"

// SD card file entry and evidence/log session entry. Moved here (from
// main.cpp) since both main.cpp's loading/sorting code and these screen
// modules need them -- see docs/screen-extraction.md.
struct FileEntry {
    String name;
    bool directory;
    uint64_t size;
};

struct LogEntry {
    String name;
    String type;
    String path;
    uint64_t size;
};

// SD file browser screens: directory listing, file detail, and a bounded
// text/CSV preview. Draw-only; navigation, directory loading, and preview
// parsing stay in main.cpp. drawFileDetail() redirects back to drawFiles()/
// Screen::Files if the selection goes stale, same as WifiScreens/BleScreens.
class FileScreens {
public:
    FileScreens(std::vector<FileEntry>& files, size_t& listSelection,
               size_t& listOffset, String& currentPath, bool& sdAvailable,
               std::vector<String>& previewLines, size_t& previewTopLine,
               size_t& previewColumn, bool& previewTruncated,
               Screen& currentScreen)
        : files_(files),
          listSelection_(listSelection),
          listOffset_(listOffset),
          currentPath_(currentPath),
          sdAvailable_(sdAvailable),
          previewLines_(previewLines),
          previewTopLine_(previewTopLine),
          previewColumn_(previewColumn),
          previewTruncated_(previewTruncated),
          currentScreen_(currentScreen) {}

    void drawFiles();
    void drawFileDetail();
    void drawTextPreview();

    // Shared with main.cpp's file-loading/playback/preview code (single
    // source of truth, not screen-local logic).
    static String formatFileSize(uint64_t bytes);
    static bool isMp3File(const String& name);
    static bool isPreviewableFile(const String& name);

private:
    std::vector<FileEntry>& files_;
    size_t& listSelection_;
    size_t& listOffset_;
    String& currentPath_;
    bool& sdAvailable_;
    std::vector<String>& previewLines_;
    size_t& previewTopLine_;
    size_t& previewColumn_;
    bool& previewTruncated_;
    Screen& currentScreen_;
};

// One entry in the Evidence category submenu (Screen::LogCategories) --
// `path` is what loadLogSessions() (main.cpp) scopes its directory scan to;
// "" means unscoped ("All", the original flat-list behavior). Declared here
// rather than file-local to either file_screens.cpp or main.cpp since both
// need the same list: this file draws the menu, main.cpp handles selecting
// an entry and re-scanning.
struct LogCategoryOption {
    const char* label;
    const char* path;
};
inline constexpr LogCategoryOption kLogCategories[] = {
    {"All", ""},
    {"Wi-Fi", "/ghostwire/logs/wifi"},
    {"BLE", "/ghostwire/logs/ble"},
    {"Mesh", "/ghostwire/logs/mesh"},
    {"GNSS", "/ghostwire/logs/gnss"},
    {"IMU", "/ghostwire/logs/imu"},
    {"Network", "/ghostwire/logs/network"},
    {"POE", "/ghostwire/logs/poe"},
    {"War Drive", "/ghostwire/logs/wardrive"},
    {"Tools", "/ghostwire/logs/tools"},
    {"System", "/ghostwire/logs/system"},
    {"Familiar", "/ghostwire/logs/familiar"},
    {"Patrol", "/ghostwire/assessments"},
};
inline constexpr size_t kLogCategoryCount =
    sizeof(kLogCategories) / sizeof(kLogCategories[0]);

// Evidence/log session browser screens: a category submenu
// (Screen::LogCategories, kLogCategories above), the unified list of
// ordinary logs and nested Familiar Patrol assessment output scoped to
// whichever category was chosen, session detail, and delete confirmation.
// The category submenu uses the shared listSelection/listOffset cursor
// (every other menu screen's convention); the list itself keeps its own
// separate cursor (logSelection/logOffset) as it always has.
class LogScreens {
public:
    LogScreens(std::vector<LogEntry>& sessions, size_t& logSelection,
              size_t& logOffset, bool& sdAvailable, uint32_t& selectedLogRows,
              Screen& currentScreen, size_t& listSelection, size_t& listOffset)
        : sessions_(sessions),
          logSelection_(logSelection),
          logOffset_(logOffset),
          sdAvailable_(sdAvailable),
          selectedLogRows_(selectedLogRows),
          currentScreen_(currentScreen),
          listSelection_(listSelection),
          listOffset_(listOffset) {}

    void drawCategories();
    void drawSessions();
    void drawDetail();
    void drawDeleteConfirm();

    static String displayName(const LogEntry& entry);

private:
    void normalizePosition();

    std::vector<LogEntry>& sessions_;
    size_t& logSelection_;
    size_t& logOffset_;
    bool& sdAvailable_;
    uint32_t& selectedLogRows_;
    Screen& currentScreen_;
    size_t& listSelection_;
    size_t& listOffset_;
};
