#include "audio_screens.h"

#include <M5Cardputer.h>

#include "branding.h"
#include "familiar_phrases.h"
#include "screen_chrome.h"

void AudioScreens::drawMenu() {
    static const char* const items[] = {
        "Speaker tone test", "Microphone level", "Play MP3 from SD",
        "Familiar phrase lab",
    };
    ScreenChrome::drawHeader("Audio Self-Test");
    ScreenChrome::normalizeListPosition(4);
    for (size_t row = 0; row < 4; ++row) {
        ScreenChrome::drawListRow(row, items[row], row == listSelection_);
    }
    ScreenChrome::drawFooter("Enter: open/run  Backspace/Q: back");
}

void AudioScreens::drawTtsLab() {
    ScreenChrome::drawHeader("Familiar Phrase Lab");
    auto& display = M5Cardputer.Display;
    const auto& phrase = kTtsLabPhrases[ttsLabPhrase_];
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 31);
    display.printf("Phrase %u/%u: %s", ttsLabPhrase_ + 1,
                   static_cast<unsigned>(kTtsLabPhraseCount), phrase.name);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 55);
    display.print("Words: ");
    for (uint8_t i = 0; i < phrase.wordCount; ++i) {
        if (i != 0) display.print(" + ");
        display.print(phrase.words[i]);
    }
    display.setCursor(8, 78);
    display.print(ttsLabStatus_.substring(0, 37));
    display.setCursor(8, 95);
    if (ttsLabPlaybackMs_ > 0) {
        display.printf("Sequence time: %lums",
                       static_cast<unsigned long>(ttsLabPlaybackMs_));
    } else {
        display.print("Streams individual MP3s from SD.");
    }
    ScreenChrome::drawFooter("Tab: phrase  Enter: play  Esc: back");
}

void AudioScreens::drawMicrophone() {
    ScreenChrome::drawHeader("Microphone Level");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 35);
    display.printf("Input level: %u%%", microphoneLevel_);
    display.drawRect(8, 58, display.width() - 16, 22, Branding::muted);
    display.fillRect(10, 60, (display.width() - 20) * microphoneLevel_ / 100, 18,
                     microphoneLevel_ > 75 ? Branding::warning : Branding::accent);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 93);
    display.print("Speak or tap near the microphone.");
    ScreenChrome::drawFooter("Backspace/Q: stop and return");
}

void AudioScreens::updateMicrophoneMeter() {
    auto& display = M5Cardputer.Display;
    display.fillRect(8, 32, display.width() - 16, 18, Branding::background);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 35);
    display.printf("Input level: %u%%", microphoneLevel_);
    display.fillRect(9, 59, display.width() - 18, 20, Branding::background);
    display.fillRect(10, 60, (display.width() - 20) * microphoneLevel_ / 100, 18,
                     microphoneLevel_ > 75 ? Branding::warning : Branding::accent);
}

void AudioScreens::drawAudioFiles() {
    ScreenChrome::drawHeader("MP3 Files");
    ScreenChrome::normalizeListPosition(audioFiles_.size());
    ScreenChrome::drawHeaderPosition(listSelection_ + 1, audioFiles_.size());
    if (audioFiles_.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted, Branding::background);
        M5Cardputer.Display.setCursor(8, 38);
        M5Cardputer.Display.print("No MP3 files in");
        M5Cardputer.Display.setCursor(8, 55);
        M5Cardputer.Display.print("/ghostwire/audio");
    } else {
        for (size_t row = 0; row < ScreenChrome::kVisibleRows &&
                             row + listOffset_ < audioFiles_.size();
            ++row) {
            ScreenChrome::drawListRow(row, audioFiles_[row + listOffset_],
                                      row + listOffset_ == listSelection_);
        }
    }
    ScreenChrome::drawFooter("Enter: play  R: reload  Q: back");
}

void AudioScreens::drawNowPlaying(const String& name, const String& source) {
    ScreenChrome::drawHeader("Now Playing");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 40);
    display.print(name.substring(0, 34));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 64);
    display.print(source.substring(0, 36));
    ScreenChrome::drawFooter("Enter/Backspace/Q: stop");
}
