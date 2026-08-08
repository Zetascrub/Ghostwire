#include "file_screens.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "screen_chrome.h"

String FileScreens::formatFileSize(uint64_t bytes) {
    if (bytes >= 1024ULL * 1024ULL) {
        return String(static_cast<double>(bytes) / (1024.0 * 1024.0), 1) +
               " MiB";
    }
    if (bytes >= 1024ULL) {
        return String(static_cast<double>(bytes) / 1024.0, 1) + " KiB";
    }
    return String(bytes) + " bytes";
}

bool FileScreens::isMp3File(const String& name) {
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".mp3");
}

bool FileScreens::isPreviewableFile(const String& name) {
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".csv") || lower.endsWith(".txt") ||
           lower.endsWith(".log");
}

void FileScreens::drawFiles() {
    ScreenChrome::drawHeader("Files");
    if (!sdAvailable_) {
        M5Cardputer.Display.setTextColor(Branding::warning, Branding::background);
        M5Cardputer.Display.setCursor(8, 35);
        M5Cardputer.Display.print("microSD unavailable");
        M5Cardputer.Display.setCursor(8, 52);
        M5Cardputer.Display.print("Press R to retry");
    } else if (files_.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted, Branding::background);
        M5Cardputer.Display.setCursor(8, 35);
        M5Cardputer.Display.print("This folder is empty");
    } else {
        ScreenChrome::normalizeListPosition(files_.size());
        ScreenChrome::drawHeaderPosition(listSelection_ + 1, files_.size());
        for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                             row + listOffset_ < files_.size();
            ++row) {
            const auto& entry = files_[row + listOffset_];
            String label = entry.directory ? "[" + entry.name + "]" : entry.name;
            String suffix;
            if (!entry.directory) {
                suffix = entry.size >= 1024 ? String(entry.size / 1024) + "K"
                                            : String(entry.size) + "B";
            }
            ScreenChrome::drawListRow(row, label, row + listOffset_ == listSelection_,
                                      suffix);
        }
    }
    ScreenChrome::drawFooter("R: remount   Esc/Q: up or back");
}

void FileScreens::drawFileDetail() {
    if (files_.empty() || listSelection_ >= files_.size()) {
        currentScreen_ = Screen::Files;
        drawFiles();
        return;
    }
    const FileEntry& entry = files_[listSelection_];
    ScreenChrome::drawHeader("File Details");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 31);
    display.print(entry.name.substring(0, 36));
    display.setCursor(8, 49);
    display.printf("Type: %s", isMp3File(entry.name)
                                   ? "MP3 audio"
                                   : (isPreviewableFile(entry.name)
                                          ? "Text / CSV"
                                          : "File"));
    display.setCursor(8, 67);
    display.printf("Size: %s", formatFileSize(entry.size).c_str());
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 85);
    display.print(currentPath_.substring(0, 36));
    ScreenChrome::drawFooter(isMp3File(entry.name)
                                 ? "Enter: play   Backspace/Q: files"
                                 : (isPreviewableFile(entry.name)
                                        ? "Enter: preview   Backspace/Q: files"
                                        : "Backspace/Q: files"));
}

void FileScreens::drawTextPreview() {
    ScreenChrome::drawHeader("Text Preview");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    if (previewLines_.empty()) {
        display.setCursor(8, 38);
        display.print("(empty file)");
    } else {
        for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                             previewTopLine_ + row < previewLines_.size();
            ++row) {
            const String& source = previewLines_[previewTopLine_ + row];
            display.setCursor(4, 27 + row * 15);
            if (previewColumn_ < source.length()) {
                display.print(
                    source.substring(previewColumn_, previewColumn_ + 39));
            }
        }
    }
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(125, 7);
    display.printf("%u/%u%s", static_cast<unsigned>(previewTopLine_ + 1),
                   static_cast<unsigned>(previewLines_.size()),
                   previewTruncated_ ? "+" : "");
    ScreenChrome::drawFooter("W/S: lines  A/D: columns  Q: back");
}

String LogScreens::displayName(const LogEntry& entry) {
    if (entry.type != "Patrol") return entry.name;
    const int fileSlash = entry.path.lastIndexOf('/');
    if (fileSlash <= 0) return entry.name;
    const String parentPath = entry.path.substring(0, fileSlash);
    const int parentSlash = parentPath.lastIndexOf('/');
    const String session = parentPath.substring(parentSlash + 1);
    return session + "/" + entry.name;
}

void LogScreens::normalizePosition() {
    if (sessions_.empty()) {
        logSelection_ = 0;
        logOffset_ = 0;
        return;
    }
    if (logSelection_ >= sessions_.size()) {
        logSelection_ = sessions_.size() - 1;
    }
    if (logSelection_ < logOffset_) logOffset_ = logSelection_;
    if (logSelection_ >= logOffset_ + ScreenChrome::kVisibleRows) {
        logOffset_ = logSelection_ - ScreenChrome::kVisibleRows + 1;
    }
}

void LogScreens::drawSessions() {
    ScreenChrome::drawHeader("Evidence");
    normalizePosition();
    ScreenChrome::drawHeaderPosition(logSelection_ + 1, sessions_.size());
    if (!sdAvailable_) {
        M5Cardputer.Display.setTextColor(Branding::warning, Branding::background);
        M5Cardputer.Display.setCursor(8, 38);
        M5Cardputer.Display.print("microSD unavailable");
    } else if (sessions_.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted, Branding::background);
        M5Cardputer.Display.setCursor(8, 38);
        M5Cardputer.Display.print("No evidence saved yet");
    } else {
        for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                             logOffset_ + row < sessions_.size();
            ++row) {
            const auto& entry = sessions_[logOffset_ + row];
            ScreenChrome::drawListRow(row, displayName(entry),
                                      logOffset_ + row == logSelection_,
                                      entry.type);
        }
    }
    ScreenChrome::drawFooter("Enter: inspect   R: refresh   Q: back");
}

void LogScreens::drawDetail() {
    if (sessions_.empty() || logSelection_ >= sessions_.size()) {
        currentScreen_ = Screen::LogSessions;
        drawSessions();
        return;
    }
    const auto& entry = sessions_[logSelection_];
    ScreenChrome::drawHeader("Session Details");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 31);
    display.print(displayName(entry).substring(0, 37));
    display.setCursor(8, 50);
    display.printf("Source: %s", entry.type.c_str());
    display.setCursor(8, 69);
    display.printf("Size: %s", FileScreens::formatFileSize(entry.size).c_str());
    display.setCursor(8, 88);
    String lower = entry.name;
    lower.toLowerCase();
    if (lower.endsWith(".csv")) {
        display.printf("Data rows: %lu", static_cast<unsigned long>(selectedLogRows_));
    } else {
        display.print(FileScreens::isPreviewableFile(entry.name)
                          ? "Readable evidence file"
                          : "Binary capture / asset");
    }
    ScreenChrome::drawFooter(FileScreens::isPreviewableFile(entry.name)
                                 ? "Enter: preview   Tab: actions   Q: evidence"
                                 : "Tab: actions   Q: evidence");
}

void LogScreens::drawDeleteConfirm() {
    ScreenChrome::drawHeader("Delete Session?");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 40);
    display.print(sessions_[logSelection_].name.substring(0, 37));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 65);
    display.print("This cannot be undone.");
    ScreenChrome::drawFooter("Enter: DELETE   Backspace/Q: cancel");
}
