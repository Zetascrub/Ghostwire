#include <Arduino.h>
#include <M5Cardputer.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <algorithm>
#include <climits>
#include <cctype>
#include <cmath>
#include <cstring>
#include <esp_system.h>
#include <esp_wifi.h>
#include <sys/time.h>
#include <time.h>
#include <vector>

#include "branding.h"
#include "app_screen.h"
#include "ble_scanner.h"
#include "ble_keyboard_service.h"
#include "ble_spam_service.h"
#include "biscuit_pro_client.h"
#include "chameleon_ultra_client.h"
#include "cyber_familiar.h"
#include "eapol_parser.h"
#include "ir_service.h"
#include "hid_service.h"
#include "audio_service.h"
#include "ai_service.h"
#include "gnss_service.h"
#include "lora_service.h"
#include "network_host_scan_service.h"
#include "network_port_scan_service.h"
#include "ssh_service.h"
#include "terminal_buffer.h"
#include "pcap_logger.h"
#include "sd_logger.h"
#include "war_drive_service.h"
#include "wifi_sniffer_service.h"

namespace {

constexpr int kBatteryPin = 10;
constexpr int kSdCs = 12;
constexpr int kSdMosi = 14;
constexpr int kSdClock = 40;
constexpr int kSdMiso = 39;
constexpr int kSdCompatibilityPin = 5;
constexpr uint32_t kSdFrequency = 4000000;
constexpr size_t kVisibleRows = 6;

struct FileEntry {
    String name;
    bool directory;
    uint64_t size;
};

struct LogEntry {
    String name;
    String type;
    uint64_t size;
};

// One-off per-screen actions (export, deauth, disconnect, etc.) live in a
// Tab-triggered menu instead of individually memorized letters -- see
// actionsForScreen() and the handleInput() menu-mode block.
struct ActionMenuItem {
    char key;
    String label;
};

const char* const kMenuItems[] = {
    "Wi-Fi", "BLE", "GPS", "Mesh", "War Drive", "Network", "Devices",
    "AI Chat", "Cyber Familiar", "Tools", "Settings",
};
constexpr size_t kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);
const char* const kHidPresetNames[] = {
    "Ghostwire signature",
    "Keyboard layout sample",
    "Slow typing cadence",
};
constexpr size_t kHidPresetCount =
    sizeof(kHidPresetNames) / sizeof(kHidPresetNames[0]);

Screen currentScreen = Screen::MainMenu;
size_t menuSelection = 0;
size_t listSelection = 0;
size_t listOffset = 0;
bool actionMenuOpen = false;
std::vector<ActionMenuItem> actionMenuItems;
size_t actionMenuSelection = 0;
bool sdAvailable = false;
uint8_t sdCardType = CARD_NONE;
uint64_t sdCardSizeMiB = 0;
String currentPath = "/";
Screen audioReturnScreen = Screen::AudioFiles;
Screen textPreviewReturnScreen = Screen::FileDetail;
String nowPlayingName;
String nowPlayingSource;
String qrText;
String placeholderTitle;
String wifiStatus = "Press R to scan";
String bleStatus = "Press R to scan";
String wifiExportStatus;
String wifiDeauthStatus;
String wifiConnectSsid;
String wifiConnectPasswordInput;
String wifiConnectAttemptPassword;
String wifiConnectStatusText = "Not connected";
unsigned long wifiConnectStartMs = 0;
bool wifiConnectAttempting = false;
String wifiConnectSavedSsid;
String wifiConnectSavedPassword;
String bleExportStatus;
String diagnosticExportStatus;
uint32_t bootCount = 0;
uint32_t abnormalResetCount = 0;
bool bootHistorySaved = false;
bool bootChimePending = false;
unsigned long bootChimeDeadlineMs = 0;
unsigned long nextBootChimeAttemptMs = 0;
String bootChimeStatus = "Disabled";
std::vector<FileEntry> files;
std::vector<LogEntry> logSessions;
size_t logSelection = 0;
size_t logOffset = 0;
uint32_t selectedLogRows = 0;
std::vector<String> previewLines;
size_t previewTopLine = 0;
size_t previewColumn = 0;
bool previewTruncated = false;
std::vector<wifi_ap_record_t> accessPoints;
std::vector<BleDeviceInfo> bleDevices;
std::vector<WifiProbeRecord> recentWifiProbes;
BleScanner bleScanner;
BleKeyboardService bleKeyboardService;
BleSpamService bleSpamService;
BiscuitProClient biscuitClient;
String biscuitResultTitle;
std::vector<String> biscuitResultLines;
size_t biscuitResultOffset = 0;
bool biscuitWardriveActive = false;
uint32_t biscuitWardriveApCount = 0;
uint32_t biscuitWardriveBleCount = 0;
std::vector<String> biscuitWardriveBssids;
std::vector<String> biscuitWardriveBleMacs;
String biscuitWardriveParseTail;
unsigned long lastBiscuitWardriveDraw = 0;
ChameleonUltraClient chameleonClient;
bool chameleonHasReadings = false;
uint8_t chameleonAppMajor = 0;
uint8_t chameleonAppMinor = 0;
uint16_t chameleonBatteryMv = 0;
uint8_t chameleonBatteryPct = 0;
bool chameleonScanAttempted = false;
bool chameleonHfFound = false;
ChameleonUltraClient::HfTag chameleonHfTag;
bool chameleonLfFound = false;
uint8_t chameleonLfId[5] = {0, 0, 0, 0, 0};
bool chameleonContinuousScan = false;
unsigned long lastChameleonScanMs = 0;
unsigned long lastChameleonConnectAttemptMs = 0;
uint32_t chameleonConnectAttempts = 0;
constexpr unsigned long kChameleonReconnectIntervalMs = 4000;
String chameleonLastLoggedSignature;
String chameleonWorkflowStatus;
String chameleonSavedPath;
WarDriveService warDriveService;
SdLogger warDriveWifiLogger;
SdLogger warDriveBleLogger;
unsigned long lastWarDriveDraw = 0;
unsigned long lastWifiConnectDraw = 0;
NetworkHostScanService networkHostScanService;
std::vector<NetworkHostResult> networkHostResults;
unsigned long lastNetworkHostScanDraw = 0;
String networkHostScanExportStatus;
IPAddress networkPortScanTarget;
std::vector<uint16_t> networkPortResults;
String networkPortScanExportStatus;
NetworkPortScanService networkPortScanService;
bool networkPortScanIsFull = false;
unsigned long lastNetworkPortScanDraw = 0;
constexpr size_t kMaxTelnetLines = 64;
Screen telnetReturnScreen = Screen::NetworkMenu;
WiFiClient telnetClient;
String telnetHostInput;
String telnetHost;
uint16_t telnetPort = 23;
std::vector<String> telnetLines;
String telnetPendingLine;
String telnetStatus;
unsigned long lastTelnetDraw = 0;
TerminalEscState telnetEscState = TerminalEscState::None;
constexpr size_t kMaxSshLines = 64;
SshService sshService;
String sshHostInput;
String sshUsername;
String sshHost;
uint16_t sshPort = 22;
String sshPasswordInput;
String sshStatus;
std::vector<String> sshLines;
String sshPendingLine;
TerminalEscState sshEscState = TerminalEscState::None;
unsigned long lastSshDraw = 0;
bool sshTrustPending = false;
String sshLocalEchoPending;
String sshHistory[3];
size_t sshHistoryIndex = 0;
IrService irService;
HidService hidService;
AudioService audioService;
AiService aiService;
CyberFamiliar cyberFamiliar;
uint8_t familiarPage = 0;
unsigned long lastFamiliarDraw = 0;
bool familiarIdleActive = false;
Screen lastFamiliarObservedScreen = Screen::MainMenu;
String aiPrompt;
String aiNotice;
size_t aiScrollLines = 0;
String familiarWorkflowStatus;
constexpr char kAiSpeechPath[] = "/ghostwire/audio/ai_reply.mp3";
constexpr char kAiRecordingPath[] = "/ghostwire/ai_voice.wav";
GnssService gnssService;
LoRaService loraService;
WifiSnifferService wifiSnifferService;
SdLogger imuLogger;
SdLogger gnssLogger;
SdLogger loraLogger;
SdLogger wifiSnifferLogger;
SdLogger chameleonLogger;
SdLogger bleCaptureLogger;
PcapLogger wifiPassiveCaptureLogger;
PcapLogger handshakeCaptureLogger;
Preferences preferences;
std::vector<String> audioFiles;
std::vector<String> duckyScripts;
size_t duckyCommandCount = 0;
size_t duckyUnsupportedCount = 0;
uint32_t duckyDeclaredDelayMs = 0;
String duckyRunStatus;
uint16_t microphoneLevel = 0;
int bleCaptureRssiFilter = -100;
unsigned long lastBleCaptureDraw = 0;
bool bleCaptureUiDirty = false;
unsigned long lastMicrophoneDraw = 0;
unsigned long lastUserActivity = 0;
uint8_t speakerVolume = 96;
uint8_t screenBrightness = 128;
uint16_t screenTimeoutSeconds = 30;
bool bootSoundEnabled = true;
bool fastBootEnabled = false;
bool saveWifiCredentials = false;
bool autoConnectWifi = false;
bool cyberdeckIdleEnabled = false;
size_t themeIndex = 0;
uint8_t bootAnimationIndex = 0;
uint8_t bootSoundIndex = 0;
bool screenSleeping = false;
bool cyberdeckIdleActive = false;
constexpr size_t kCyberdeckColumns = 40;
int16_t cyberdeckRainHead[kCyberdeckColumns]{};
uint8_t cyberdeckRainSpeed[kCyberdeckColumns]{};
unsigned long lastCyberdeckDraw = 0;
uint32_t cyberdeckLastWifiCount = 0;
uint32_t cyberdeckLastBleCount = 0;
bool clockSynced = false;
String clockStatus = "Waiting for GNSS UTC";
unsigned long lastClockSyncAttempt = 0;
unsigned long lastHeaderStatusDraw = 0;
unsigned long lastTimeStatusDraw = 0;
float filteredBatteryVoltage = 0.0F;
uint8_t filteredBatteryPercent = 0;
unsigned long lastBatterySample = 0;
unsigned long lastGnssDraw = 0;
unsigned long lastGnssLog = 0;
unsigned long lastLoRaDraw = 0;
uint32_t lastLoggedLoRaPacket = 0;
unsigned long lastWifiSnifferDraw = 0;
unsigned long lastBleSpamDraw = 0;
unsigned long lastBleKeyboardDraw = 0;
constexpr size_t kMaxRecentWifiProbes = 8;
unsigned long lastHandshakeCaptureDraw = 0;
uint32_t handshakeEapolFrameCount = 0;
bool handshakeMessageSeen[5] = {};  // Index 1-4 used; 0 unused.
bool handshakePmkidFound = false;
uint8_t handshakePmkid[16] = {};
unsigned long lastImuDraw = 0;
unsigned long lastImuLog = 0;
m5::imu_data_t imuData{};
bool imuAvailable = false;
bool imuCalibrating = false;
uint16_t imuCalibrationSamples = 0;
float gyroOffsetX = 0.0F;
float gyroOffsetY = 0.0F;
float gyroOffsetZ = 0.0F;
float gyroCalibrationSumX = 0.0F;
float gyroCalibrationSumY = 0.0F;
float gyroCalibrationSumZ = 0.0F;

constexpr uint8_t kDefaultVolume = 96;
constexpr uint8_t kDefaultBrightness = 128;
constexpr uint16_t kDefaultScreenTimeout = 30;
constexpr bool kDefaultBootSound = true;
constexpr bool kDefaultFastBoot = false;
constexpr bool kDefaultSaveWifiCredentials = false;
constexpr bool kDefaultAutoConnectWifi = false;
constexpr bool kDefaultCyberdeckIdle = false;
constexpr uint8_t kDefaultBootAnimation = 0;
constexpr uint8_t kDefaultBootSoundPreset = 0;
constexpr uint16_t kScreenTimeoutOptions[] = {0, 15, 30, 60, 120};
constexpr size_t kSystemDiagnosticCount = 22;
const char* const kBootAnimationNames[] = {
    "System Console", "Cipher Reveal", "Radar Sweep", "Minimal",
    "Neon Breach", "Hacker Terminal", "Silly Bounce", "Synthwave Grid",
};
constexpr size_t kBootAnimationCount =
    sizeof(kBootAnimationNames) / sizeof(kBootAnimationNames[0]);
const char* const kBootSoundNames[] = {
    "Classic Chime", "Hero Signal", "Arcade Ready",
    "Starship", "Mystic Spark",
};
constexpr size_t kBootSoundCount =
    sizeof(kBootSoundNames) / sizeof(kBootSoundNames[0]);

String csvSafePayload(const String& payload);
String utcTimestamp();
void drawCurrentScreen();

void applySettings() {
    audioService.setVolume(speakerVolume);
    M5Cardputer.Display.setBrightness(screenBrightness);
}

void saveSettings() {
    preferences.putUChar("volume", speakerVolume);
    preferences.putUChar("brightness", screenBrightness);
    preferences.putUShort("timeout", screenTimeoutSeconds);
    preferences.putBool("boot_sound", bootSoundEnabled);
    preferences.putBool("fast_boot", fastBootEnabled);
    preferences.putBool("save_wifi", saveWifiCredentials);
    preferences.putBool("auto_wifi", autoConnectWifi);
    preferences.putBool("cyber_idle", cyberdeckIdleEnabled);
    preferences.putUChar("theme", static_cast<uint8_t>(themeIndex));
    preferences.putUChar("boot_anim", bootAnimationIndex);
    preferences.putUChar("boot_tone", bootSoundIndex);
}

void restoreDefaultSettings() {
    speakerVolume = kDefaultVolume;
    screenBrightness = kDefaultBrightness;
    screenTimeoutSeconds = kDefaultScreenTimeout;
    bootSoundEnabled = kDefaultBootSound;
    fastBootEnabled = kDefaultFastBoot;
    saveWifiCredentials = kDefaultSaveWifiCredentials;
    autoConnectWifi = kDefaultAutoConnectWifi;
    cyberdeckIdleEnabled = kDefaultCyberdeckIdle;
    preferences.remove("wifi_ssid");
    preferences.remove("wifi_pass");
    wifiConnectSavedSsid = "";
    wifiConnectSavedPassword = "";
    themeIndex = 0;
    bootAnimationIndex = kDefaultBootAnimation;
    bootSoundIndex = kDefaultBootSoundPreset;
    Branding::applyTheme(themeIndex);
    applySettings();
    saveSettings();
}

void recoverKeyboardAfterBlockingOperation() {
    // Radio scans block the UI while the TCA8418 continues queueing events.
    // Reinitializing flushes the launch-key release and prevents Enter from
    // remaining logically held when navigation resumes.
    M5Cardputer.Keyboard.begin();
    for (int update = 0; update < 4; ++update) {
        M5Cardputer.update();
        delay(10);
    }
}

float sampleBatteryVoltage() {
    return static_cast<float>(analogReadMilliVolts(kBatteryPin)) * 2.0F /
           1000.0F;
}

uint8_t percentageFromVoltage(float voltage) {
    struct Point {
        float voltage;
        uint8_t percent;
    };
    static constexpr Point curve[] = {
        {3.30F, 0},  {3.50F, 5},  {3.65F, 20}, {3.75F, 40},
        {3.85F, 60}, {4.00F, 80}, {4.20F, 100},
    };
    if (voltage <= curve[0].voltage) return 0;
    if (voltage >= curve[6].voltage) return 100;
    for (size_t index = 1; index < 7; ++index) {
        if (voltage <= curve[index].voltage) {
            const Point& low = curve[index - 1];
            const Point& high = curve[index];
            const float ratio =
                (voltage - low.voltage) / (high.voltage - low.voltage);
            return low.percent +
                   static_cast<uint8_t>((high.percent - low.percent) * ratio);
        }
    }
    return 100;
}

void updateBatteryEstimate(bool initialize = false) {
    if (!initialize && millis() - lastBatterySample < 1000) return;
    lastBatterySample = millis();
    float sample = 0.0F;
    constexpr size_t kSamples = 16;
    for (size_t index = 0; index < kSamples; ++index) {
        sample += sampleBatteryVoltage();
    }
    sample /= kSamples;

    if (initialize || filteredBatteryVoltage == 0.0F) {
        filteredBatteryVoltage = sample;
        filteredBatteryPercent = percentageFromVoltage(sample);
        return;
    }
    filteredBatteryVoltage =
        filteredBatteryVoltage * 0.85F + sample * 0.15F;
    const uint8_t target = percentageFromVoltage(filteredBatteryVoltage);
    // Move at most one point per sample. Load changes from audio and radios
    // should not look like sudden changes in stored battery capacity.
    if (target > filteredBatteryPercent) {
        ++filteredBatteryPercent;
    } else if (target < filteredBatteryPercent) {
        --filteredBatteryPercent;
    }
}

float readBatteryVoltage() {
    return filteredBatteryVoltage == 0.0F ? sampleBatteryVoltage()
                                          : filteredBatteryVoltage;
}

uint8_t batteryPercentage() {
    return filteredBatteryPercent;
}

void initSd() {
    SD.end();
    SPI.end();
    sdAvailable = false;
    sdCardType = CARD_NONE;
    sdCardSizeMiB = 0;

    pinMode(kSdCs, OUTPUT);
    digitalWrite(kSdCs, HIGH);
    // The EXT SPI device shares the SD bus. Its CS must never float low.
    pinMode(kSdCompatibilityPin, OUTPUT);
    digitalWrite(kSdCompatibilityPin, HIGH);
    delay(20);

    SPI.begin(kSdClock, kSdMiso, kSdMosi, kSdCs);
    sdAvailable = SD.begin(kSdCs, SPI, kSdFrequency);
    if (sdAvailable) {
        sdCardType = SD.cardType();
        sdAvailable = sdCardType != CARD_NONE;
        sdCardSizeMiB = SD.cardSize() / (1024ULL * 1024ULL);
    }
}

void drawHeaderStatus(bool force = false) {
    auto& display = M5Cardputer.Display;
    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    const bool captureActive = wifiSnifferService.isActive() ||
                               bleSpamService.isActive() ||
                               warDriveService.isActive() ||
                               handshakeCaptureLogger.isActive();
    const uint8_t battery = batteryPercentage();
    const uint32_t clockMinute =
        clockSynced ? static_cast<uint32_t>(time(nullptr) / 60) : 0;
    static bool initialized = false;
    static bool lastWifiConnected = false;
    static bool lastCaptureActive = false;
    static bool lastClockSynced = false;
    static uint8_t lastBattery = 255;
    static uint32_t lastClockMinute = UINT32_MAX;
    if (!force && initialized && wifiConnected == lastWifiConnected &&
        captureActive == lastCaptureActive && clockSynced == lastClockSynced &&
        battery == lastBattery && clockMinute == lastClockMinute) {
        return;
    }
    initialized = true;
    lastWifiConnected = wifiConnected;
    lastCaptureActive = captureActive;
    lastClockSynced = clockSynced;
    lastBattery = battery;
    lastClockMinute = clockMinute;

    display.fillRect(display.width() - 92, 0, 92, 22, Branding::panel);
    display.setTextSize(1);
    if (wifiConnected) {
        // Small connected-status dot, same accent colour used elsewhere in
        // this app to mean "active/good" -- deliberately just a dot, not a
        // label, since there's little room left in this status strip.
        display.fillCircle(display.width() - 86, 11, 3, Branding::accent);
    }
    if (captureActive) {
        display.fillRect(display.width() - 82, 8, 5, 5, Branding::warning);
    }
    if (clockSynced) {
        const time_t now = time(nullptr);
        struct tm local {};
        localtime_r(&now, &local);
        char clockValue[6];
        strftime(clockValue, sizeof(clockValue), "%H:%M", &local);
        display.setTextColor(Branding::muted, Branding::panel);
        display.setCursor(display.width() - 76, 7);
        display.print(clockValue);
    }
    display.setTextColor(battery <= 15 ? Branding::warning : Branding::muted,
                         Branding::panel);
    display.setCursor(display.width() - 35, 7);
    display.printf("%u%%", battery);
}

void drawHeader(const char* title) {
    auto& display = M5Cardputer.Display;
    display.fillScreen(Branding::background);
    display.fillRect(0, 0, display.width(), 22, Branding::panel);
    display.drawFastHLine(0, 21, display.width(), Branding::accent);
    display.setTextSize(1);
    display.setTextColor(Branding::text, Branding::panel);
    display.setCursor(6, 7);
    // Reserve the right side for connection/time/battery status. Long titles
    // used to render underneath that strip and become visually corrupted.
    display.print(String(title).substring(0, 23));
    drawHeaderStatus(true);
}

// Small "n/nn" position readout in the header's top-right, between the
// title and the status icons (wifi dot/clock/battery) -- lets a
// scrollable list say where you are in it instead of items just
// silently disappearing off-screen. Call right after drawHeader(title).
void drawHeaderPosition(size_t oneBasedIndex, size_t total) {
    if (total == 0) return;
    auto& display = M5Cardputer.Display;
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%u/%u",
             static_cast<unsigned>(oneBasedIndex),
             static_cast<unsigned>(total));
    const int width = static_cast<int>(strlen(buffer)) * 6;
    const int x = display.width() - 92 - width - 4;
    // Clear the position slot first so it cannot overlap the tail of a long
    // title or leave wider digits behind when the count shrinks.
    display.fillRect(x - 2, 1, display.width() - 92 - x + 2, 20,
                     Branding::panel);
    display.setTextColor(Branding::muted, Branding::panel);
    display.setCursor(x, 7);
    display.print(buffer);
}

void drawFooter(const char* text) {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, display.height() - 15, display.width(), 15,
                     Branding::panel);
    display.drawFastHLine(0, display.height() - 15, display.width(),
                          Branding::accent);
    display.setTextColor(Branding::muted, Branding::panel);
    display.setCursor(5, display.height() - 11);
    display.print(text);
}

// Live screens repaint only their content pane. Their header and footer are
// separate UI elements and should not flicker just because telemetry changed.
// A full draw is still used when entering a screen or restoring it after an
// overlay; periodic updates pass false and touch only the pane below.
void beginContentUpdate(const char* title, bool fullDraw) {
    if (fullDraw) {
        drawHeader(title);
        return;
    }
    auto& display = M5Cardputer.Display;
    display.fillRect(0, 22, display.width(), display.height() - 37,
                     Branding::background);
    display.setTextSize(1);
}

void normalizeListPosition(size_t count) {
    if (count == 0) {
        listSelection = 0;
        listOffset = 0;
        return;
    }
    if (listSelection >= count) listSelection = count - 1;
    if (listSelection < listOffset) listOffset = listSelection;
    if (listSelection >= listOffset + kVisibleRows) {
        listOffset = listSelection - kVisibleRows + 1;
    }
}

void drawListRow(int row, const String& label, bool selected,
                 const String& suffix = "") {
    auto& display = M5Cardputer.Display;
    const int y = 26 + row * 15;
    const uint16_t background =
        selected ? Branding::accent : Branding::background;
    const uint16_t foreground =
        selected ? Branding::background : Branding::text;
    display.fillRect(4, y, display.width() - 8, 14, background);
    display.setTextColor(foreground, background);
    display.setCursor(8, y + 3);
    size_t labelCharacters = 37;
    int suffixX = display.width() - 8;
    if (!suffix.isEmpty()) {
        suffixX = std::max(8, display.width() -
                                 static_cast<int>(suffix.length()) * 6 - 8);
        labelCharacters = static_cast<size_t>(
            std::max(1, (suffixX - 14) / 6));
    }
    display.print(label.substring(0, labelCharacters));
    if (!suffix.isEmpty()) {
        display.setCursor(suffixX, y + 3);
        display.print(suffix);
    }
}

// Returns the one-off actions available on the current screen, filtered
// by whatever state already gates them today (e.g. "Reconnect saved"
// only appears if a saved network actually exists) -- the underlying
// guard clauses in handleInput() stay as the source of truth, this just
// mirrors them so the menu never shows something that would silently do
// nothing.
std::vector<ActionMenuItem> actionsForScreen(Screen screen) {
    switch (screen) {
        case Screen::WifiRecon:
            return {{'e', "Export CSV"}};
        case Screen::WifiDetail:
            return {{'d', "Deauth"}, {'h', "Capture handshake"}};
        case Screen::WifiHandshakeCapture:
            return {{'d', "Send deauth"}};
        case Screen::WifiConnectSelect:
            if (!wifiConnectSavedSsid.isEmpty()) {
                return {{'c', "Reconnect saved network"}};
            }
            return {};
        case Screen::WifiConnectStatus:
            return {{'d', "Disconnect"}};
        case Screen::Chameleon: {
            std::vector<ActionMenuItem> items;
            if (chameleonClient.isConnected()) {
                items.push_back({'s', "Scan tag"});
                items.push_back(
                    {'c', chameleonContinuousScan ? "Stop continuous scan"
                                                  : "Start continuous scan"});
                items.push_back({'d', "Return to reader mode"});
                if (chameleonHfFound || chameleonLfFound) {
                    items.push_back({'v', "Save captured identity"});
                    items.push_back({'e', "Stage + emulate in slot 8"});
                }
                if (!chameleonSavedPath.isEmpty()) {
                    items.push_back({'o', "Load last saved identity"});
                }
            }
            return items;
        }
        case Screen::BleDiscovery:
            return {{'e', "Export current results"},
                    {'c', bleScanner.isContinuous()
                              ? "Stop continuous capture"
                              : "Start continuous capture"},
                    {'f', "Cycle RSSI filter (" +
                              String(bleCaptureRssiFilter) + " dBm)"}};
        case Screen::CyberFamiliar:
            return {{'p', "Pet familiar"},
                    {'n', "Choose next name"},
                    {'i', cyberFamiliar.idleMode() ? "Disable idle watch"
                                                   : "Enable idle watch"},
                    {'x', "Export familiar record"},
                    {'g', "Import capture logs"},
                    {'z', "Reset familiar progress"}};
        case Screen::System:
            return {{'e', "Export diagnostics"}};
        case Screen::TimeStatus:
            if (WiFi.status() == WL_CONNECTED) {
                return {{'g', "Sync from GNSS"}, {'n', "Sync from NTP"}};
            }
            return {{'g', "Sync from GNSS"}};
        case Screen::NetworkHostScan:
            if (!networkHostResults.empty()) {
                return {{'e', "Export CSV"},
                        {'f', "Full port scan"},
                        {'t', "Telnet"}};
            }
            return {};
        case Screen::NetworkPortScan:
            return {{'e', "Export CSV"}};
        case Screen::Gnss:
            return {{'l', gnssLogger.isActive() ? "Stop logging"
                                                : "Start logging"}};
        case Screen::LoRa:
            return {{'l', loraLogger.isActive() ? "Stop logging"
                                                : "Start logging"},
                    {'p', "Switch profile"}};
        case Screen::WifiSniffer:
            return {{'l', wifiSnifferLogger.isActive() ? "Stop probe CSV"
                                                        : "Start probe CSV"},
                    {'p', wifiPassiveCaptureLogger.isActive()
                              ? "Stop PCAP capture"
                              : "Start PCAP capture"},
                    {'m', "Cycle capture mode"},
                    {'c', wifiSnifferService.channelLocked()
                              ? "Resume channel hopping"
                              : "Lock current channel"}};
        case Screen::Imu: {
            if (!imuAvailable) return {};
            std::vector<ActionMenuItem> items;
            items.push_back({'l', imuLogger.isActive() ? "Stop logging"
                                                        : "Start logging"});
            items.push_back({'c', "Calibrate"});
            return items;
        }
        case Screen::LogDetail:
            return {{'d', "Delete"}};
        default:
            return {};
    }
}

void drawActionMenu() {
    auto& display = M5Cardputer.Display;
    const int itemCount = static_cast<int>(actionMenuItems.size());
    const int rowHeight = 16;
    const int boxHeight = itemCount * rowHeight + 10;
    const int boxWidth = display.width() - 40;
    const int boxX = 20;
    const int boxY = std::max(22, (display.height() - boxHeight) / 2);

    display.fillRect(boxX, boxY, boxWidth, boxHeight, Branding::panel);
    display.drawRect(boxX, boxY, boxWidth, boxHeight, Branding::accent);
    for (int i = 0; i < itemCount; ++i) {
        const bool selected = static_cast<size_t>(i) == actionMenuSelection;
        const int rowY = boxY + 5 + i * rowHeight;
        display.fillRect(boxX + 2, rowY, boxWidth - 4, rowHeight - 2,
                         selected ? Branding::accent : Branding::panel);
        display.setTextColor(selected ? Branding::background : Branding::text,
                             selected ? Branding::accent : Branding::panel);
        display.setCursor(boxX + 8, rowY + 3);
        display.print(actionMenuItems[i].label);
    }
}

void drawMainMenu() {
    drawHeader(Branding::productName);
    drawHeaderPosition(menuSelection + 1, kMenuCount);
    const size_t offset =
        menuSelection >= kVisibleRows ? menuSelection - kVisibleRows + 1 : 0;
    for (size_t row = 0; row < kVisibleRows && row + offset < kMenuCount;
         ++row) {
        drawListRow(row, kMenuItems[row + offset],
                    row + offset == menuSelection);
    }
    drawFooter("Fn+;/.: move   Enter: open");
}

void appendWrappedAiLines(std::vector<String>& lines, const String& prefix,
                          const String& text) {
    constexpr size_t width = 38;
    String remaining = prefix + text;
    remaining.replace("\r", "");
    while (!remaining.isEmpty()) {
        int newline = remaining.indexOf('\n');
        size_t take = std::min(width, remaining.length());
        if (newline >= 0 && static_cast<size_t>(newline) < take) {
            take = static_cast<size_t>(newline);
        } else if (take < remaining.length()) {
            const int space = remaining.substring(0, take + 1).lastIndexOf(' ');
            if (space > 4) take = static_cast<size_t>(space);
        }
        lines.push_back(remaining.substring(0, take));
        size_t remove = take;
        while (remove < remaining.length() &&
               (remaining[remove] == ' ' || remaining[remove] == '\n')) {
            ++remove;
        }
        remaining.remove(0, remove);
        if (take == 0 && remove == 0) break;
    }
}

void drawAiComposer() {
    auto& display = M5Cardputer.Display;
    display.fillRect(4, 86, display.width() - 8, 31,
                     Branding::background);
    display.drawFastHLine(4, 85, display.width() - 8, Branding::panel);
    display.setTextSize(1);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(6, 91);
    const String promptLine = "> " + aiPrompt;
    display.print(promptLine.substring(
        promptLine.length() > 38 ? promptLine.length() - 38 : 0));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(6, 106);
    const String notice = aiNotice.isEmpty() ? aiService.status() : aiNotice;
    display.print(notice.substring(0, 39));
}

void drawAiChat() {
    String title = "AI: " + String(aiService.providerName());
    drawHeader(title.c_str());
    auto& display = M5Cardputer.Display;
    display.setTextSize(1);
    std::vector<String> lines;
    for (const auto& turn : aiService.history()) {
        appendWrappedAiLines(lines, turn.role == "user" ? "> " : "< ",
                             turn.text);
    }
    constexpr size_t shown = 4;
    const size_t maxScroll = lines.size() > shown ? lines.size() - shown : 0;
    if (aiScrollLines > maxScroll) aiScrollLines = maxScroll;
    const size_t end = lines.size() > aiScrollLines
                           ? lines.size() - aiScrollLines
                           : 0;
    const size_t start = end > shown ? end - shown : 0;
    for (size_t index = start; index < end; ++index) {
        display.setTextColor(lines[index].startsWith("> ")
                                 ? Branding::accent
                                 : Branding::text,
                             Branding::background);
        display.setCursor(6, 27 + (index - start) * 14);
        display.print(lines[index].substring(0, 39));
    }
    drawAiComposer();
    drawFooter("Up/Down scroll Enter send Tab provider");
}

const char* authName(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA+2";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2+3";
        default: return "SEC";
    }
}

void drawWifiRecon() {
    drawHeader("Wi-Fi Discovery");
    normalizeListPosition(accessPoints.size());
    drawHeaderPosition(listSelection + 1, accessPoints.size());
    if (accessPoints.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted,
                                        Branding::background);
        M5Cardputer.Display.setCursor(8, 36);
        M5Cardputer.Display.print(wifiStatus);
    } else {
        for (size_t row = 0;
             row < kVisibleRows && row + listOffset < accessPoints.size();
             ++row) {
            const auto& ap = accessPoints[row + listOffset];
            String ssid = reinterpret_cast<const char*>(ap.ssid);
            if (ssid.isEmpty()) ssid = "<hidden>";
            String suffix = String(ap.primary) + "/" + String(ap.rssi);
            drawListRow(row, ssid, row + listOffset == listSelection, suffix);
        }
    }
    drawFooter(wifiExportStatus.isEmpty()
                   ? "R: scan  Enter: details  Tab: actions  Q: back"
                   : wifiExportStatus.c_str());
}

void drawWifiChannelAnalyzer() {
    drawHeader("2.4 GHz Channels");
    auto& display = M5Cardputer.Display;
    uint8_t counts[14]{};
    int strongest[14];
    for (int channel = 0; channel < 14; ++channel) strongest[channel] = -100;
    for (const auto& ap : accessPoints) {
        if (ap.primary < 1 || ap.primary > 13) continue;
        ++counts[ap.primary];
        strongest[ap.primary] = std::max(strongest[ap.primary],
                                         static_cast<int>(ap.rssi));
    }

    int bestChannel = 1;
    int bestScore = INT_MAX;
    static constexpr int candidates[] = {1, 6, 11};
    for (int candidate : candidates) {
        int score = 0;
        for (int channel = 1; channel <= 13; ++channel) {
            const int distance = abs(candidate - channel);
            if (distance > 4) continue;
            const int signal = strongest[channel] <= -100
                                   ? 0
                                   : std::max(1, strongest[channel] + 101);
            score += counts[channel] * signal * (5 - distance);
        }
        if (score < bestScore) {
            bestScore = score;
            bestChannel = candidate;
        }
    }

    const int baseline = 103;
    const int barWidth = 13;
    const int gap = 4;
    const int startX = 9;
    display.drawFastHLine(startX, baseline, 13 * (barWidth + gap) - gap,
                          Branding::muted);
    for (int channel = 1; channel <= 13; ++channel) {
        const int x = startX + (channel - 1) * (barWidth + gap);
        const int height = std::min(62, static_cast<int>(counts[channel]) * 9);
        const uint16_t colour = channel == bestChannel
                                    ? Branding::accent
                                    : (counts[channel] >= 4 ? Branding::warning
                                                            : Branding::text);
        if (height > 0) display.fillRect(x, baseline - height, barWidth, height,
                                         colour);
        display.setTextColor(channel == bestChannel ? Branding::accent
                                                    : Branding::muted,
                             Branding::background);
        display.setCursor(x + (channel < 10 ? 4 : 1), baseline + 4);
        display.print(channel);
    }
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 27);
    if (accessPoints.empty()) {
        display.print("No scan data - press R");
    } else {
        display.printf("%u APs  Suggested channel: %d",
                       static_cast<unsigned>(accessPoints.size()), bestChannel);
    }
    drawFooter("R: rescan   Q: back");
}

void drawWifiDetail() {
    if (accessPoints.empty() || listSelection >= accessPoints.size()) {
        currentScreen = Screen::WifiRecon;
        drawWifiRecon();
        return;
    }
    const auto& ap = accessPoints[listSelection];
    drawHeader("Access Point");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 32);
    display.printf("SSID: %s",
                   ap.ssid[0] ? reinterpret_cast<const char*>(ap.ssid)
                              : "<hidden>");
    display.setCursor(8, 50);
    display.printf("Channel: %u   RSSI: %d", ap.primary, ap.rssi);
    display.setCursor(8, 68);
    display.printf("Security: %s", authName(ap.authmode));
    display.setCursor(8, 86);
    display.printf("BSSID: %02X:%02X:%02X:%02X:%02X:%02X", ap.bssid[0],
                   ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4],
                   ap.bssid[5]);
    drawFooter(wifiDeauthStatus.isEmpty() ? "Tab: actions   Esc: back"
                                         : wifiDeauthStatus.c_str());
}

void drawWifiDeauthConfirm() {
    if (accessPoints.empty() || listSelection >= accessPoints.size()) {
        currentScreen = Screen::WifiRecon;
        drawWifiRecon();
        return;
    }
    const auto& ap = accessPoints[listSelection];
    String ssid = reinterpret_cast<const char*>(ap.ssid);
    if (ssid.isEmpty()) ssid = "<hidden>";
    drawHeader("Deauth AP?");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 32);
    display.print(ssid.substring(0, 30));
    display.setCursor(8, 50);
    display.printf("BSSID: %02X:%02X:%02X:%02X:%02X:%02X", ap.bssid[0],
                   ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4],
                   ap.bssid[5]);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 72);
    display.print("Sends deauth frames to this AP.");
    display.setCursor(8, 88);
    display.print("Authorized targets only.");
    drawFooter("Enter: DEAUTH   Backspace/Q: cancel");
}

void transmitWifiDeauth(const wifi_ap_record_t& ap) {
    drawHeader("Deauth AP?");
    M5Cardputer.Display.setTextColor(Branding::warning, Branding::background);
    M5Cardputer.Display.setCursor(8, 45);
    M5Cardputer.Display.print("Sending deauth frames...");
    drawFooter("Please wait");

    WiFi.mode(WIFI_STA);
    // The mode switch is asynchronous; every other call site in this file
    // that changes WiFi mode (scanWifiNetworks(), BleScanner::scan()) waits
    // before touching the radio further. This was the one place that
    // didn't, and every low-level radio call below was racing against a
    // driver that hadn't finished switching modes yet.
    delay(150);
    const esp_err_t channelResult =
        esp_wifi_set_channel(ap.primary, WIFI_SECOND_CHAN_NONE);

    // Standard 802.11 deauthentication frame: management/deauth frame
    // control, broadcast destination (deauths every client of this AP,
    // since only the AP's own identity is known here), reason code 7
    // (class 3 frame received from a nonassociated station).
    uint8_t frame[26] = {
        0xC0, 0x00,                          // Frame control
        0x00, 0x00,                          // Duration
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Addr1: destination (broadcast)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Addr2: source (AP BSSID)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Addr3: BSSID
        0x00, 0x00,                          // Sequence/fragment control
        0x07, 0x00,                          // Reason code
    };
    memcpy(frame + 10, ap.bssid, 6);
    memcpy(frame + 16, ap.bssid, 6);

    // en_sys_seq=false: we never associate to an AP (only ever scan), so
    // per esp_wifi_80211_tx's documented contract either value is valid
    // here; false matches the convention used elsewhere for raw frame TX.
    // The real blocker was never en_sys_seq -- it's the frame-type check
    // neutralized by wifi_raw_frame_override.cpp/patch_wifi_lib.py.
    constexpr int kBurstCount = 8;
    esp_err_t lastResult = ESP_FAIL;
    for (int index = 0; index < kBurstCount; ++index) {
        lastResult = esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(frame), false);
        // The driver can transiently run out of TX descriptors; give it a
        // moment to drain and retry rather than counting that as failure.
        for (uint8_t retry = 0; lastResult == ESP_ERR_NO_MEM && retry < 5;
             ++retry) {
            delay(2);
            lastResult =
                esp_wifi_80211_tx(WIFI_IF_STA, frame, sizeof(frame), false);
        }
        delay(70);
    }

    recoverKeyboardAfterBlockingOperation();
    Serial.printf(
        "[wifi] deauth burst=%u bssid=%02X:%02X:%02X:%02X:%02X:%02X "
        "channel=%u channel_result=%d tx_result=%d\n",
        kBurstCount, ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3],
        ap.bssid[4], ap.bssid[5], ap.primary,
        static_cast<int>(channelResult), static_cast<int>(lastResult));
    wifiDeauthStatus =
        lastResult == ESP_OK
            ? "Deauth sent (" + String(kBurstCount) + "x)"
            : "Deauth failed: " + String(static_cast<int>(lastResult)) +
                  " (ch " + String(static_cast<int>(channelResult)) + ")";
}

void drawWifiHandshakeCapture(bool fullDraw = true) {
    if (accessPoints.empty() || listSelection >= accessPoints.size()) {
        currentScreen = Screen::WifiRecon;
        drawWifiRecon();
        return;
    }
    const auto& ap = accessPoints[listSelection];
    String ssid = reinterpret_cast<const char*>(ap.ssid);
    if (ssid.isEmpty()) ssid = "<hidden>";
    beginContentUpdate("Handshake Capture", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(
        wifiSnifferService.isActive() ? Branding::accent : Branding::warning,
        Branding::background);
    display.setCursor(8, 29);
    display.printf("%s  CH %u", ssid.substring(0, 20).c_str(), ap.primary);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf("BSSID: %02X:%02X:%02X:%02X:%02X:%02X", ap.bssid[0],
                   ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4],
                   ap.bssid[5]);
    display.setCursor(8, 63);
    display.printf(
        "EAPOL frames: %lu",
        static_cast<unsigned long>(handshakeEapolFrameCount));

    display.setCursor(8, 80);
    display.setTextColor(Branding::text, Branding::background);
    display.print("Messages: ");
    for (uint8_t message = 1; message <= 4; ++message) {
        display.setTextColor(handshakeMessageSeen[message] ? Branding::accent
                                                           : Branding::muted,
                             Branding::background);
        display.printf("M%u ", message);
    }

    display.setCursor(8, 97);
    if (handshakePmkidFound) {
        String pmkidHex;
        pmkidHex.reserve(32);
        for (uint8_t index = 0; index < 16; ++index) {
            char byteText[3];
            snprintf(byteText, sizeof(byteText), "%02X", handshakePmkid[index]);
            pmkidHex += byteText;
        }
        display.setTextColor(Branding::accent, Branding::background);
        display.printf("PMKID: %s", pmkidHex.c_str());
    } else {
        display.setTextColor(Branding::muted, Branding::background);
        display.print("PMKID: none yet");
    }

    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 114);
    if (handshakeCaptureLogger.isActive()) {
        display.printf(
            "REC %lu frames",
            static_cast<unsigned long>(handshakeCaptureLogger.rowCount()));
    } else {
        display.print("Not recording");
    }
    if (fullDraw) drawFooter("R: restart   Tab: actions   Q: stop");
}

void exportWifiResults() {
    if (!sdAvailable || accessPoints.empty()) {
        wifiExportStatus =
            sdAvailable ? "Nothing to export" : "Export failed: no SD card";
        drawWifiRecon();
        return;
    }
    SdLogger logger;
    if (!logger.begin(
            "wifi",
            "timestamp_utc,ssid,bssid,channel,rssi_dbm,security")) {
        wifiExportStatus = "Export failed: " + logger.status();
        drawWifiRecon();
        return;
    }
    for (const auto& ap : accessPoints) {
        String ssid = reinterpret_cast<const char*>(ap.ssid);
        if (ssid.isEmpty()) ssid = "<hidden>";
        char bssid[18];
        snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
                 ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3],
                 ap.bssid[4], ap.bssid[5]);
        const String row = utcTimestamp() + "," + csvSafePayload(ssid) + "," +
                           bssid + "," + String(ap.primary) + "," +
                           String(ap.rssi) + "," + authName(ap.authmode);
        if (!logger.append(row)) break;
    }
    String name = logger.path();
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    const bool success = logger.isActive();
    logger.stop();
    wifiExportStatus =
        success ? "Saved " + name : "Export failed: " + logger.status();
    drawWifiRecon();
}

void scanWifiNetworks() {
    drawHeader("Wi-Fi Discovery");
    M5Cardputer.Display.setTextColor(Branding::warning,
                                    Branding::background);
    M5Cardputer.Display.setCursor(8, 42);
    M5Cardputer.Display.print("Scanning 2.4 GHz channels...");
    drawFooter("Please wait");

    accessPoints.clear();
    wifiExportStatus = "";
    wifiStatus = "No networks found";

    if (!WiFi.mode(WIFI_STA)) {
        wifiStatus = "Unable to start Wi-Fi radio";
        Serial.println("[wifi] failed to enter station mode");
    } else {
        WiFi.setSleep(false);
        WiFi.disconnect(false, false);
        delay(150);

        const int networkCount = WiFi.scanNetworks(false, true);
        Serial.printf("[wifi] scan result=%d\n", networkCount);
        if (networkCount < 0) {
            wifiStatus = "Scan failed (" + String(networkCount) + ")";
        } else {
            for (int index = 0; index < networkCount; ++index) {
                wifi_ap_record_t record{};
                const String ssid = WiFi.SSID(index);
                ssid.substring(0, sizeof(record.ssid) - 1)
                    .toCharArray(reinterpret_cast<char*>(record.ssid),
                                 sizeof(record.ssid));
                const uint8_t* bssid = WiFi.BSSID(index);
                if (bssid != nullptr) {
                    memcpy(record.bssid, bssid, sizeof(record.bssid));
                }
                record.primary = static_cast<uint8_t>(WiFi.channel(index));
                record.rssi = WiFi.RSSI(index);
                record.authmode = static_cast<wifi_auth_mode_t>(
                    WiFi.encryptionType(index));
                accessPoints.push_back(record);
                cyberFamiliar.observeWifiIdentity(record.bssid);
            }
            wifiStatus = accessPoints.empty()
                             ? "No networks found"
                             : String(accessPoints.size()) + " networks";
        }
        WiFi.scanDelete();
    }

    std::sort(accessPoints.begin(), accessPoints.end(),
              [](const wifi_ap_record_t& left,
                 const wifi_ap_record_t& right) {
                  return left.rssi > right.rssi;
              });
    recoverKeyboardAfterBlockingOperation();
    listSelection = 0;
    listOffset = 0;
    // Redraws whichever screen actually invoked the scan (Wi-Fi Discovery
    // or the Connect picker), not always Wi-Fi Discovery.
    drawCurrentScreen();
}

void drawWifiConnectSelect() {
    drawHeader("Wi-Fi Connect");
    normalizeListPosition(accessPoints.size());
    drawHeaderPosition(listSelection + 1, accessPoints.size());
    if (accessPoints.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted,
                                        Branding::background);
        M5Cardputer.Display.setCursor(8, 36);
        M5Cardputer.Display.print(wifiStatus);
    } else {
        for (size_t row = 0;
             row < kVisibleRows && row + listOffset < accessPoints.size();
             ++row) {
            const auto& ap = accessPoints[row + listOffset];
            String ssid = reinterpret_cast<const char*>(ap.ssid);
            if (ssid.isEmpty()) ssid = "<hidden>";
            String suffix = String(ap.primary) + "/" + String(ap.rssi);
            drawListRow(row, ssid, row + listOffset == listSelection, suffix);
        }
    }
    drawFooter(wifiConnectSavedSsid.isEmpty()
                   ? "R: rescan   Enter: select   Backspace/Q: back"
                   : "R: rescan  Enter: select  Tab: actions  Q: back");
}

void drawTextEntryRow(int y, const char* label, const String& value,
                      bool masked = false) {
    auto& display = M5Cardputer.Display;
    display.fillRect(4, y - 2, display.width() - 8, 15,
                     Branding::background);
    display.setTextSize(1);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, y);
    display.print(label);
    const size_t labelLength = strlen(label);
    const size_t available = labelLength < 38 ? 38 - labelLength : 1;
    if (masked) {
        const size_t shown = std::min(available, value.length());
        for (size_t index = 0; index < shown; ++index) display.print('*');
    } else {
        const size_t start = value.length() > available
                                 ? value.length() - available
                                 : 0;
        display.print(value.substring(start));
    }
}

void drawWifiConnectPassword() {
    drawHeader("Wi-Fi Connect");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 36);
    display.printf("SSID: %s", wifiConnectSsid.c_str());
    drawTextEntryRow(54, "Password: ", wifiConnectPasswordInput, true);
    drawFooter("Enter: connect   Esc: cancel");
}

void drawWifiConnectStatus(bool fullDraw = true) {
    beginContentUpdate("Wi-Fi Connect", fullDraw);
    auto& display = M5Cardputer.Display;
    const bool connected = WiFi.status() == WL_CONNECTED;
    display.setTextColor(connected ? Branding::accent : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.printf("SSID: %s", wifiConnectSsid.c_str());
    display.setCursor(8, 44);
    display.print(wifiConnectStatusText);
    display.setTextColor(Branding::text, Branding::background);
    if (connected) {
        display.setCursor(8, 58);
        display.printf("IP: %s", WiFi.localIP().toString().c_str());
        display.setCursor(8, 72);
        display.printf("Gateway: %s", WiFi.gatewayIP().toString().c_str());
        display.setCursor(8, 86);
        display.printf("RSSI: %d dBm", WiFi.RSSI());
    }
    drawFooter("Tab: actions   Backspace/Q: back");
}

void drawBleDiscovery() {
    drawHeader("BLE Advertisement Sniffer");
    normalizeListPosition(bleDevices.size());
    drawHeaderPosition(listSelection + 1, bleDevices.size());
    if (bleDevices.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted,
                                        Branding::background);
        M5Cardputer.Display.setCursor(8, 36);
        M5Cardputer.Display.print(bleStatus);
    } else {
        for (size_t row = 0;
             row < kVisibleRows && row + listOffset < bleDevices.size();
             ++row) {
            const BleDeviceInfo& device = bleDevices[row + listOffset];
            drawListRow(row, device.name,
                        row + listOffset == listSelection,
                        String(device.rssi));
        }
    }
    if (bleScanner.isContinuous()) {
        drawFooter(("Tab: actions LIVE:" +
                    String(bleScanner.advertisementCount()) + " D:" +
                    String(bleScanner.droppedCount())).c_str());
    } else {
        drawFooter(bleExportStatus.isEmpty()
                       ? "R: scan  Enter: details  Tab: actions  Q: back"
                       : bleExportStatus.c_str());
    }
}

void exportBleResults() {
    if (!sdAvailable || bleDevices.empty()) {
        bleExportStatus =
            sdAvailable ? "Nothing to export" : "Export failed: no SD card";
        drawBleDiscovery();
        return;
    }
    SdLogger logger;
    if (!logger.begin(
            "ble",
            "timestamp_utc,name,address,address_type,rssi_dbm,connectable,"
            "advertisement_type,payload_bytes,service_count,service_uuids,"
            "manufacturer,manufacturer_data_hex,payload_hex")) {
        bleExportStatus = "Export failed: " + logger.status();
        drawBleDiscovery();
        return;
    }
    for (const auto& device : bleDevices) {
        const String row =
            utcTimestamp() + "," + csvSafePayload(device.name) + "," +
            csvSafePayload(device.address) + "," +
            String(device.addressType) + "," + String(device.rssi) + "," +
            (device.connectable ? "1" : "0") + "," +
            String(device.advertisementType) + "," +
            String(device.payloadLength) + "," +
            String(device.serviceCount) + "," + csvSafePayload(device.service) +
            "," + csvSafePayload(device.manufacturer) + "," +
            csvSafePayload(device.manufacturerData) + "," +
            csvSafePayload(device.payloadData);
        if (!logger.append(row)) break;
    }
    String name = logger.path();
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    const bool success = logger.isActive();
    logger.stop();
    bleExportStatus =
        success ? "Saved " + name : "Export failed: " + logger.status();
    drawBleDiscovery();
}

void scanBleDevices() {
    bleCaptureLogger.stop();
    bleScanner.stop();
    drawHeader("BLE Advertisement Sniffer");
    M5Cardputer.Display.setTextColor(Branding::warning,
                                    Branding::background);
    M5Cardputer.Display.setCursor(8, 42);
    M5Cardputer.Display.print("Listening for advertisements...");
    drawFooter("Scanning for 5 seconds");
    bleExportStatus = "";
    bleScanner.scan(bleDevices, bleStatus);
    for (const auto& device : bleDevices) {
        cyberFamiliar.observeBleIdentity(device.address);
    }

    recoverKeyboardAfterBlockingOperation();
    listSelection = 0;
    listOffset = 0;
    drawBleDiscovery();
}

// War Driving intentionally logs every scan result every phase, unlike the
// Chameleon Ultra work's dedup-on-capture -- the same AP/device seen again
// later (from a different position, as the vehicle moves) is useful data,
// not noise. Uniqueness is tracked separately, by WarDriveService, purely
// for the on-screen counters.
void logWarDriveWifiResult(const WarDriveWifiResult& ap) {
    if (!warDriveWifiLogger.isActive()) return;
    const bool fix = gnssService.hasFix();
    String ssid = ap.ssid;
    if (ssid.isEmpty()) ssid = "<hidden>";
    char bssid[18];
    snprintf(bssid, sizeof(bssid), "%02X:%02X:%02X:%02X:%02X:%02X",
             ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4],
             ap.bssid[5]);
    // WiGLE CSV 1.6: directly importable without a desktop conversion step.
    const String row =
        String(bssid) + "," + csvSafePayload(ssid) + "," +
        csvSafePayload(String("[") + authName(ap.authmode) + "]") + "," +
        utcTimestamp() + "," + String(ap.channel) + "," + String(ap.rssi) +
        "," + String(fix ? gnssService.latitude() : 0.0, 6) + "," +
        String(fix ? gnssService.longitude() : 0.0, 6) + "," +
        String(fix ? gnssService.altitudeMetres() : 0.0, 1) + "," +
        String(fix ? gnssService.hdop() : 0.0, 1) + ",WIFI";
    warDriveWifiLogger.append(row);
}

void logWarDriveBleResult(const WarDriveBleResult& device) {
    if (!warDriveBleLogger.isActive()) return;
    const bool fix = gnssService.hasFix();
    const String row =
        utcTimestamp() + "," +
        String(fix ? gnssService.latitude() : 0.0, 6) + "," +
        String(fix ? gnssService.longitude() : 0.0, 6) + "," +
        (fix ? "1" : "0") + "," + csvSafePayload(device.name) + "," +
        csvSafePayload(device.address) + "," + String(device.rssi) + "," +
        (device.connectable ? "1" : "0") + "," +
        csvSafePayload(device.service);
    warDriveBleLogger.append(row);
}

// Redraws only the lines that actually change, via targeted fillRect calls
// instead of drawWarDrive()'s full drawHeader()-triggered screen clear --
// called on every periodic tick, so a full-screen flicker there would be
// very noticeable (and was, before this split).
void drawWarDriveDynamic() {
    auto& display = M5Cardputer.Display;
    const int width = display.width();

    display.fillRect(0, 27, width, 13, Branding::background);
    display.setTextColor(warDriveService.isActive() ? Branding::accent
                                                     : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.print(warDriveService.isActive() ? "ACTIVE" : "STOPPED");

    display.fillRect(0, 42, width, 13, Branding::background);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 44);
    if (gnssService.hasFix()) {
        display.printf("Lat: %.6f  Lon: %.6f", gnssService.latitude(),
                       gnssService.longitude());
    } else {
        display.print("GPS: no fix yet");
    }

    display.fillRect(0, 56, width, 13, Branding::background);
    display.setCursor(8, 58);
    display.printf("Phase: %s", warDriveService.currentPhaseName());

    display.fillRect(0, 70, width, 13, Branding::background);
    display.setCursor(8, 72);
    display.printf(
        "Unique APs: %lu",
        static_cast<unsigned long>(warDriveService.wifiUniqueCount()));

    display.fillRect(0, 84, width, 13, Branding::background);
    display.setCursor(8, 86);
    display.printf(
        "Unique devices: %lu",
        static_cast<unsigned long>(warDriveService.bleUniqueCount()));
}

void drawWarDrive() {
    drawHeader("War Drive");
    drawWarDriveDynamic();
    drawFooter("R: start/stop   Backspace/Q: back");
}

void exportNetworkHostResults() {
    if (!sdAvailable || networkHostResults.empty()) {
        networkHostScanExportStatus =
            sdAvailable ? "Nothing to export" : "Export failed: no SD card";
        return;
    }
    SdLogger logger;
    if (!logger.begin("network_hosts", "timestamp_utc,ip_address")) {
        networkHostScanExportStatus = "Export failed: " + logger.status();
        return;
    }
    for (const auto& host : networkHostResults) {
        logger.append(utcTimestamp() + "," + host.ip.toString());
    }
    String name = logger.path();
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    const bool success = logger.isActive();
    logger.stop();
    networkHostScanExportStatus =
        success ? "Saved " + name : "Export failed: " + logger.status();
}

void drawNetworkHostScan(bool fullDraw = true) {
    const uint32_t signature =
        (static_cast<uint32_t>(WiFi.status()) << 28) ^
        (static_cast<uint32_t>(networkHostScanService.isActive()) << 27) ^
        (static_cast<uint32_t>(networkHostScanService.scannedCount()) << 12) ^
        static_cast<uint32_t>(networkHostResults.size());
    static uint32_t lastSignature = UINT32_MAX;
    if (!fullDraw && signature == lastSignature) return;
    lastSignature = signature;
    beginContentUpdate("Host Discovery", fullDraw);
    auto& display = M5Cardputer.Display;
    if (WiFi.status() != WL_CONNECTED) {
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(8, 36);
        display.print("Connect to Wi-Fi first");
        drawFooter("Wi-Fi > Connect   Backspace/Q: back");
        return;
    }
    normalizeListPosition(networkHostResults.size());
    if (!networkHostScanService.isActive()) {
        drawHeaderPosition(listSelection + 1, networkHostResults.size());
    }
    if (networkHostResults.empty()) {
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 36);
        if (networkHostScanService.isActive()) {
            display.printf(
                "Scanning... %u/%u",
                static_cast<unsigned>(networkHostScanService.scannedCount()),
                static_cast<unsigned>(networkHostScanService.totalCount()));
        } else {
            display.print("No hosts found. Press R to scan.");
        }
    } else {
        for (size_t row = 0; row < kVisibleRows &&
                             row + listOffset < networkHostResults.size();
             ++row) {
            const auto& host = networkHostResults[row + listOffset];
            drawListRow(row, host.ip.toString(),
                       row + listOffset == listSelection);
        }
    }
    String footer;
    if (!networkHostScanExportStatus.isEmpty()) {
        footer = networkHostScanExportStatus;
    } else if (networkHostScanService.isActive()) {
        footer = "Scanning " + String(networkHostScanService.scannedCount()) +
                 "/" + String(networkHostScanService.totalCount()) +
                 "  Found: " + String(networkHostResults.size());
    } else {
        footer = networkHostResults.empty()
                     ? "R: start/stop  Backspace/Q: back"
                     : "Enter: scan  Tab: actions  Q: back";
    }
    drawFooter(footer.c_str());
}

const uint16_t kNetworkPortScanPorts[] = {21,  22,  23,  25,   53,  80,
                                          110, 139, 143, 443, 445, 3389,
                                          8080};
constexpr size_t kNetworkPortScanPortCount =
    sizeof(kNetworkPortScanPorts) / sizeof(kNetworkPortScanPorts[0]);

void scanNetworkPorts(IPAddress target) {
    networkPortScanTarget = target;
    networkPortResults.clear();
    networkPortScanExportStatus = "";
    listSelection = 0;
    listOffset = 0;

    drawHeader("Port Scan");
    M5Cardputer.Display.setTextColor(Branding::warning, Branding::background);
    M5Cardputer.Display.setCursor(8, 36);
    M5Cardputer.Display.printf("Scanning %s...", target.toString().c_str());
    drawFooter("Please wait");

    for (size_t i = 0; i < kNetworkPortScanPortCount; ++i) {
        WiFiClient client;
        if (client.connect(target, kNetworkPortScanPorts[i], 300)) {
            networkPortResults.push_back(kNetworkPortScanPorts[i]);
        }
        client.stop();
    }

    recoverKeyboardAfterBlockingOperation();
    drawCurrentScreen();
}

void exportNetworkPortResults() {
    if (!sdAvailable || networkPortResults.empty()) {
        networkPortScanExportStatus =
            sdAvailable ? "Nothing to export" : "Export failed: no SD card";
        return;
    }
    SdLogger logger;
    if (!logger.begin("network_ports", "timestamp_utc,ip_address,port")) {
        networkPortScanExportStatus = "Export failed: " + logger.status();
        return;
    }
    for (uint16_t port : networkPortResults) {
        logger.append(utcTimestamp() + "," +
                     networkPortScanTarget.toString() + "," + String(port));
    }
    String name = logger.path();
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    const bool success = logger.isActive();
    logger.stop();
    networkPortScanExportStatus =
        success ? "Saved " + name : "Export failed: " + logger.status();
}

void drawNetworkPortScan(bool fullDraw = true) {
    const uint32_t signature =
        (static_cast<uint32_t>(networkPortScanService.isActive()) << 31) ^
        (static_cast<uint32_t>(networkPortScanService.scannedCount()) << 12) ^
        static_cast<uint32_t>(networkPortResults.size());
    static uint32_t lastSignature = UINT32_MAX;
    if (!fullDraw && signature == lastSignature) return;
    lastSignature = signature;
    const String title = "Port Scan " + networkPortScanTarget.toString();
    beginContentUpdate(title.c_str(), fullDraw);
    auto& display = M5Cardputer.Display;
    const bool fullActive =
        networkPortScanIsFull && networkPortScanService.isActive();
    if (networkPortResults.empty()) {
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 36);
        if (fullActive) {
            display.printf(
                "Scanning... %u/%u",
                static_cast<unsigned>(networkPortScanService.scannedCount()),
                static_cast<unsigned>(networkPortScanService.totalCount()));
        } else {
            display.print("No open ports found");
        }
    } else {
        normalizeListPosition(networkPortResults.size());
        for (size_t row = 0; row < kVisibleRows &&
                             row + listOffset < networkPortResults.size();
             ++row) {
            drawListRow(row, String(networkPortResults[row + listOffset]),
                       row + listOffset == listSelection);
        }
    }
    String footer;
    if (!networkPortScanExportStatus.isEmpty()) {
        footer = networkPortScanExportStatus;
    } else if (fullActive) {
        footer = "Scanning " + String(networkPortScanService.scannedCount()) +
                 "/" + String(networkPortScanService.totalCount()) +
                 "  Found: " + String(networkPortResults.size()) +
                 "  Q: stop";
    } else {
        footer = "R: rescan  Tab: actions  Backspace/Q: back";
    }
    drawFooter(footer.c_str());
}

void drawTelnetConnect() {
    drawHeader("Telnet Client");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    drawTextEntryRow(36, "Host: ", telnetHostInput);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 54);
    display.print("Format: host or host:port (default 23)");
    drawFooter(telnetStatus.isEmpty() ? "Enter: connect   Esc: cancel"
                                      : telnetStatus.c_str());
}

// Redraws just the scrolling text area (content between header and
// footer) -- called on every new byte from the remote host, so it must
// not touch drawHeader()'s fillScreen()/drawFooter() or the whole
// screen would flicker on every incoming character, the same lesson
// drawWarDriveDynamic() already established.
void drawTelnetSessionDynamic() {
    auto& display = M5Cardputer.Display;
    const int top = 24;
    const int bottom = display.height() - 15;
    display.fillRect(0, top, display.width(), bottom - top,
                     Branding::background);
    display.setTextColor(Branding::text, Branding::background);

    std::vector<const String*> visible;
    for (const auto& line : telnetLines) visible.push_back(&line);
    visible.push_back(&telnetPendingLine);
    const size_t total = visible.size();
    const size_t shown = std::min(kVisibleRows, total);
    for (size_t row = 0; row < shown; ++row) {
        const String& line = *visible[total - shown + row];
        display.setCursor(4, top + 2 + static_cast<int>(row) * 15);
        display.print(line.substring(0, 39));
    }
}

void drawTelnetSession() {
    drawHeader(("Telnet " + telnetHost + ":" + String(telnetPort)).c_str());
    drawTelnetSessionDynamic();
    drawFooter(telnetClient.connected() ? "Esc: disconnect"
                                        : "Disconnected   Esc: back");
}

// Parses the optional ":port" suffix off telnetHostInput (defaulting to
// 23), then does a single bounded blocking connect -- same
// "Connecting..." status-then-blocking-call convention as
// scanNetworkPorts(), acceptable here for the same reason: one bounded
// attempt, not a scan across many targets.
void connectTelnet() {
    String host = telnetHostInput;
    uint16_t port = 23;
    const int colon = host.lastIndexOf(':');
    if (colon > 0) {
        const String portPart = host.substring(colon + 1);
        bool numeric = !portPart.isEmpty();
        for (size_t i = 0; i < portPart.length() && numeric; ++i) {
            if (!isDigit(portPart[i])) numeric = false;
        }
        if (numeric) {
            port = static_cast<uint16_t>(portPart.toInt());
            host = host.substring(0, colon);
        }
    }
    if (host.isEmpty()) {
        telnetStatus = "Enter a host";
        drawTelnetConnect();
        return;
    }

    telnetStatus = "";
    drawHeader("Telnet Client");
    M5Cardputer.Display.setTextColor(Branding::warning, Branding::background);
    M5Cardputer.Display.setCursor(8, 36);
    M5Cardputer.Display.printf("Connecting to %s:%u...", host.c_str(), port);
    drawFooter("Please wait");

    const bool connected = telnetClient.connect(host.c_str(), port, 5000);
    recoverKeyboardAfterBlockingOperation();

    if (connected) {
        telnetHost = host;
        telnetPort = port;
        telnetLines.clear();
        telnetPendingLine = "";
        telnetEscState = TerminalEscState::None;
        currentScreen = Screen::TelnetSession;
        drawCurrentScreen();
    } else {
        telnetStatus = "Connection failed";
        drawTelnetConnect();
    }
}

void drawSshConnect() {
    drawHeader("SSH Client");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    drawTextEntryRow(36, "Target: ", sshHostInput);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 54);
    display.print("Format: user@host or user@host:port");
    drawFooter(sshStatus.isEmpty() ? "Enter: next   Tab: history"
                                  : sshStatus.c_str());
}

// Parses "user@host" or "user@host:port" (default 22) out of
// sshHostInput. Returns false (and sets sshStatus) on a bad format
// rather than guessing.
bool parseSshTarget() {
    const int at = sshHostInput.indexOf('@');
    if (at <= 0) {
        sshStatus = "Format: user@host[:port]";
        return false;
    }
    sshUsername = sshHostInput.substring(0, at);
    String hostPart = sshHostInput.substring(at + 1);
    sshPort = 22;
    const int colon = hostPart.lastIndexOf(':');
    if (colon > 0) {
        const String portPart = hostPart.substring(colon + 1);
        bool numeric = !portPart.isEmpty();
        for (size_t i = 0; i < portPart.length() && numeric; ++i) {
            if (!isDigit(portPart[i])) numeric = false;
        }
        if (numeric) {
            sshPort = static_cast<uint16_t>(portPart.toInt());
            hostPart = hostPart.substring(0, colon);
        }
    }
    if (hostPart.isEmpty()) {
        sshStatus = "Format: user@host[:port]";
        return false;
    }
    sshHost = hostPart;
    sshTrustPending = false;
    return true;
}

void loadSshHistory() {
    for (size_t i = 0; i < 3; ++i) {
        const String key = "ssh_hist" + String(i);
        sshHistory[i] = preferences.getString(key.c_str(), "");
    }
    sshHistoryIndex = 0;
}

void rememberSshTarget(const String& target) {
    String updated[3] = {target, "", ""};
    size_t destination = 1;
    for (size_t i = 0; i < 3 && destination < 3; ++i) {
        if (!sshHistory[i].isEmpty() && sshHistory[i] != target) {
            updated[destination++] = sshHistory[i];
        }
    }
    for (size_t i = 0; i < 3; ++i) {
        sshHistory[i] = updated[i];
        const String key = "ssh_hist" + String(i);
        if (updated[i].isEmpty()) preferences.remove(key.c_str());
        else preferences.putString(key.c_str(), updated[i]);
    }
    sshHistoryIndex = 0;
}

String sshFingerprintPreferenceKey() {
    uint32_t hash = 2166136261UL;
    const String identity = sshHost + ":" + String(sshPort);
    for (size_t i = 0; i < identity.length(); ++i) {
        hash ^= static_cast<uint8_t>(identity[i]);
        hash *= 16777619UL;
    }
    char key[14];
    snprintf(key, sizeof(key), "ssh_%08lx", static_cast<unsigned long>(hash));
    return String(key);
}

void drawSshPassword() {
    drawHeader("SSH Client");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 36);
    display.printf("%s@%s:%u", sshUsername.c_str(), sshHost.c_str(), sshPort);
    drawTextEntryRow(54, "Password: ", sshPasswordInput, true);
    drawFooter(sshStatus.isEmpty() ? "Enter: connect   Esc: back"
                                  : sshStatus.c_str());
}

// Redraws just the scrolling text area, same split/reasoning as
// drawTelnetSessionDynamic() -- must not touch drawHeader()'s
// fillScreen()/drawFooter() or the screen flickers on every incoming
// byte.
void drawSshSessionDynamic() {
    auto& display = M5Cardputer.Display;
    const int top = 24;
    const int bottom = display.height() - 15;
    display.fillRect(0, top, display.width(), bottom - top,
                     Branding::background);
    display.setTextColor(Branding::text, Branding::background);

    std::vector<const String*> visible;
    for (const auto& line : sshLines) visible.push_back(&line);
    visible.push_back(&sshPendingLine);
    const size_t total = visible.size();
    const size_t shown = std::min(kVisibleRows, total);
    for (size_t row = 0; row < shown; ++row) {
        const String& line = *visible[total - shown + row];
        display.setCursor(4, top + 2 + static_cast<int>(row) * 15);
        display.print(line.substring(0, 39));
    }
}

void drawSshSession() {
    drawHeader(("SSH " + sshUsername + "@" + sshHost).c_str());
    drawSshSessionDynamic();
    drawFooter(sshService.isConnected() ? "Esc: disconnect"
                                        : "Disconnected   Esc: back");
}

void connectSsh() {
    preferences.putUChar("ssh_stage", 4);
    sshService.begin();
    preferences.putUChar("ssh_stage", 5);
    drawHeader("SSH Client");
    M5Cardputer.Display.setTextColor(Branding::warning, Branding::background);
    M5Cardputer.Display.setCursor(8, 36);
    M5Cardputer.Display.printf("Connecting to %s@%s:%u...",
                               sshUsername.c_str(), sshHost.c_str(), sshPort);
    drawFooter("Please wait (this can take several seconds)");

    sshLines.clear();
    sshPendingLine = "";
    sshEscState = TerminalEscState::None;
    const String fingerprintKey = sshFingerprintPreferenceKey();
    String expectedFingerprint = preferences.getString(
        fingerprintKey.c_str(), "");
    if (!expectedFingerprint.isEmpty() && expectedFingerprint.length() != 64) {
        // Discard incomplete values left by a reset during an older write.
        preferences.remove(fingerprintKey.c_str());
        expectedFingerprint = "";
    }
    if (expectedFingerprint.isEmpty() && !sshTrustPending) {
        // LibSSH-ESP32 is unstable when a negotiated, unauthenticated session
        // is retained while control returns to the UI. Confirm TOFU before
        // opening the transport, then connect/authenticate/store the observed
        // fingerprint in one bounded call.
        sshTrustPending = true;
        sshStatus = "Unknown host - Enter to trust/connect";
        preferences.putUChar("ssh_stage", 3);
        currentScreen = Screen::SshPassword;
        drawSshPassword();
        return;
    }
    struct SshConnectWork {
        SshService* service;
        String host;
        uint16_t port;
        String username;
        String password;
        String expectedFingerprint;
        bool trustUnknown;
        volatile bool done;
        bool connected;
    } work{&sshService,
           sshHost,
           sshPort,
           sshUsername,
           sshPasswordInput,
           expectedFingerprint,
           expectedFingerprint.isEmpty() && sshTrustPending,
           false,
           false};

    sshService.setStageCallback([](uint8_t stage) {
        preferences.putUChar("ssh_call", stage);
    });
    const BaseType_t taskCreated = xTaskCreatePinnedToCore(
        [](void* argument) {
            auto* item = static_cast<SshConnectWork*>(argument);
            SshService* service = item->service;
            const bool connected = service->connect(
                item->host, item->port, item->username, item->password,
                item->expectedFingerprint, item->trustUnknown);
            item->connected = connected;
            item->done = true;
            // Do not dereference the stack-backed work item after publishing
            // done; connectSsh() may return and destroy it immediately.
            if (connected) service->runIoLoop();
            vTaskDelete(nullptr);
        },
        "ssh-connect", 24576, &work, 1, nullptr, 1);
    if (taskCreated == pdPASS) {
        while (!work.done) delay(10);
    } else {
        sshStatus = "Unable to allocate SSH task";
    }
    sshService.setStageCallback(nullptr);
    const bool connected = taskCreated == pdPASS && work.connected;
    preferences.putUChar("ssh_stage", 6);
    if (!sshService.needsHostTrust()) {
        for (size_t i = 0; i < sshPasswordInput.length(); ++i) {
            sshPasswordInput.setCharAt(i, '\0');
        }
        sshPasswordInput = "";
    }
    recoverKeyboardAfterBlockingOperation();

    if (connected) {
        preferences.remove("ssh_call");
        if (expectedFingerprint.isEmpty()) {
            preferences.putString(fingerprintKey.c_str(),
                                  sshService.serverFingerprint());
        }
        sshTrustPending = false;
        preferences.remove("ssh_stage");
        currentScreen = Screen::SshSession;
        drawCurrentScreen();
    } else {
        sshStatus = sshService.statusMessage();
        sshTrustPending = sshService.needsHostTrust();
        currentScreen = Screen::SshPassword;
        drawSshPassword();
    }
}

void drawBleDetail() {
    if (bleDevices.empty() || listSelection >= bleDevices.size()) {
        currentScreen = Screen::BleDiscovery;
        drawBleDiscovery();
        return;
    }
    const BleDeviceInfo& device = bleDevices[listSelection];
    drawHeader("BLE Device");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 27);
    display.printf("Name: %s", device.name.substring(0, 31).c_str());
    display.setCursor(8, 42);
    display.printf("%s  %d dBm", device.address.c_str(), device.rssi);
    display.setCursor(8, 57);
    display.printf("Addr type: %u  ADV: %u  %s", device.addressType,
                   device.advertisementType,
                   device.connectable ? "CONNECT" : "BEACON");
    display.setCursor(8, 72);
    display.printf("Mfr: %s", device.manufacturer.c_str());
    display.setCursor(8, 87);
    display.printf("Services (%u): %s", device.serviceCount,
                   device.service.isEmpty()
                       ? "none"
                       : device.service.substring(0, 22).c_str());
    display.setCursor(8, 102);
    display.printf("Payload %uB: %s", device.payloadLength,
                   device.payloadData.isEmpty()
                       ? "not available"
                       : device.payloadData.substring(0, 24).c_str());
    drawFooter("Backspace/Q: results");
}

void drawInfrared() {
    drawHeader("Infrared Self-Test");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 32);
    display.printf("Onboard TX: GPIO %u", IrService::kTransmitPin);
    display.setCursor(8, 49);
    display.printf("Carrier:    %u kHz", IrService::kCarrierKhz);
    display.setCursor(8, 66);
    display.printf("Tests sent: %lu",
                   static_cast<unsigned long>(irService.transmissionCount()));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 86);
    display.print("RX requires an external receiver.");
    display.setCursor(8, 102);
    display.print("View emitter through phone camera.");
    drawFooter("Enter/R: emit test burst  Backspace/Q: back");
}

void transmitInfraredSelfTest() {
    drawHeader("Infrared Self-Test");
    M5Cardputer.Display.setTextColor(Branding::warning,
                                    Branding::background);
    M5Cardputer.Display.setCursor(8, 45);
    M5Cardputer.Display.print("Transmitting test pattern...");
    drawFooter("Point the IR end toward a camera");
    irService.sendSelfTest();
    recoverKeyboardAfterBlockingOperation();
    Serial.printf("[ir] self-test burst=%lu pin=%u carrier=%u kHz\n",
                  static_cast<unsigned long>(irService.transmissionCount()),
                  IrService::kTransmitPin,
                  IrService::kCarrierKhz);
    drawInfrared();
}

void drawUsbHid() {
    drawHeader("USB / HID");
    constexpr size_t kUsbHidItemCount = kHidPresetCount + 1;
    normalizeListPosition(kUsbHidItemCount);
    for (size_t row = 0;
         row < kVisibleRows && row + listOffset < kUsbHidItemCount; ++row) {
        const size_t item = row + listOffset;
        drawListRow(row,
                    item < kHidPresetCount ? kHidPresetNames[item]
                                           : "Run DuckyScript",
                    item == listSelection);
    }
    drawFooter("Enter: review test  Backspace/Q: back");
}

void loadDuckyScripts() {
    duckyScripts.clear();
    if (!sdAvailable) return;
    File directory = SD.open("/ghostwire/scripts");
    if (!directory || !directory.isDirectory()) return;
    File entry = directory.openNextFile();
    while (entry && duckyScripts.size() < 64) {
        if (!entry.isDirectory() && entry.size() <= 65536) {
            String name = entry.name();
            const int slash = name.lastIndexOf('/');
            if (slash >= 0) name = name.substring(slash + 1);
            String lower = name;
            lower.toLowerCase();
            if (lower.endsWith(".txt") || lower.endsWith(".duck")) {
                duckyScripts.push_back(name);
            }
        }
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();
    std::sort(duckyScripts.begin(), duckyScripts.end());
    listSelection = 0;
    listOffset = 0;
}

bool isSupportedDuckyCommand(const String& command) {
    return command == "STRING" || command == "STRINGLN" ||
           command == "DELAY" || command == "DEFAULT_DELAY" ||
           command == "DEFAULTDELAY" || command == "ENTER" ||
           command == "TAB" || command == "BACKSPACE" ||
           command == "SPACE" || command == "REM";
}

void preflightDuckyScript() {
    duckyCommandCount = 0;
    duckyUnsupportedCount = 0;
    duckyDeclaredDelayMs = 0;
    if (duckyScripts.empty() || listSelection >= duckyScripts.size()) return;
    File file = SD.open("/ghostwire/scripts/" + duckyScripts[listSelection],
                        FILE_READ);
    while (file && file.available() && duckyCommandCount < 500) {
        String line = file.readStringUntil('\n');
        line.replace("\r", "");
        String trimmed = line;
        trimmed.trim();
        if (trimmed.isEmpty()) continue;
        ++duckyCommandCount;
        const int space = trimmed.indexOf(' ');
        String command = space < 0 ? trimmed : trimmed.substring(0, space);
        command.toUpperCase();
        if (!isSupportedDuckyCommand(command)) {
            ++duckyUnsupportedCount;
        } else if (command == "DELAY" && space >= 0) {
            duckyDeclaredDelayMs += std::min(10000L,
                std::max(0L, trimmed.substring(space + 1).toInt()));
        }
    }
    if (file) file.close();
}

void drawDuckyScripts() {
    drawHeader("DuckyScript Files");
    normalizeListPosition(duckyScripts.size());
    drawHeaderPosition(listSelection + 1, duckyScripts.size());
    if (duckyScripts.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted,
                                         Branding::background);
        M5Cardputer.Display.setCursor(8, 36);
        M5Cardputer.Display.print("No .txt/.duck scripts in");
        M5Cardputer.Display.setCursor(8, 53);
        M5Cardputer.Display.print("/ghostwire/scripts");
    } else {
        for (size_t row = 0;
             row < kVisibleRows && row + listOffset < duckyScripts.size();
             ++row) {
            drawListRow(row, duckyScripts[row + listOffset],
                        row + listOffset == listSelection);
        }
    }
    drawFooter("Enter: inspect  R: reload  Q: back");
}

void drawDuckyConfirm() {
    drawHeader("Confirm DuckyScript");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 28);
    display.print(duckyScripts[listSelection].substring(0, 35));
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 47);
    display.printf("Commands: %u  Unsupported: %u",
                   static_cast<unsigned>(duckyCommandCount),
                   static_cast<unsigned>(duckyUnsupportedCount));
    display.setCursor(8, 65);
    display.printf("Declared delays: %lus",
                   static_cast<unsigned long>(duckyDeclaredDelayMs / 1000));
    display.setCursor(8, 83);
    display.print("Focus the authorized USB host.");
    display.setCursor(8, 101);
    display.print("3 second cancelable countdown.");
    drawFooter("Enter: RUN   Esc: cancel");
}

bool waitDuckyDelay(uint32_t durationMs) {
    const unsigned long started = millis();
    while (millis() - started < durationMs) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() &&
            M5Cardputer.Keyboard.isPressed() &&
            M5Cardputer.Keyboard.keysState().esc) return false;
        delay(10);
    }
    return true;
}

void runSelectedDuckyScript() {
    const String path = "/ghostwire/scripts/" + duckyScripts[listSelection];
    drawHeader("Running DuckyScript");
    M5Cardputer.Display.setTextColor(Branding::warning,
                                     Branding::background);
    M5Cardputer.Display.setCursor(8, 38);
    M5Cardputer.Display.print("Starting in 3 seconds...");
    drawFooter("Esc: CANCEL");
    if (!waitDuckyDelay(3000)) {
        duckyRunStatus = "Canceled before execution";
        currentScreen = Screen::DuckyResult;
        drawCurrentScreen();
        return;
    }
    File file = SD.open(path, FILE_READ);
    uint32_t defaultDelay = 0;
    uint32_t totalDelay = 0;
    size_t executed = 0;
    bool canceled = false;
    hidService.releaseAll();
    while (file && file.available() && executed < 500 && totalDelay <= 60000) {
        String line = file.readStringUntil('\n');
        line.replace("\r", "");
        String trimmed = line;
        trimmed.trim();
        if (trimmed.isEmpty()) continue;
        const int space = trimmed.indexOf(' ');
        String command = space < 0 ? trimmed : trimmed.substring(0, space);
        command.toUpperCase();
        const String argument = space < 0 ? "" : trimmed.substring(space + 1);
        if (command == "STRING" || command == "STRINGLN") {
            hidService.typeText(argument);
            if (command == "STRINGLN") hidService.tapEnter();
        } else if (command == "ENTER") {
            hidService.tapEnter();
        } else if (command == "TAB") {
            hidService.tapTab();
        } else if (command == "BACKSPACE") {
            hidService.tapBackspace();
        } else if (command == "SPACE") {
            hidService.tapSpace();
        } else if (command == "DEFAULT_DELAY" || command == "DEFAULTDELAY") {
            defaultDelay = std::min(5000L, std::max(0L, argument.toInt()));
        } else if (command == "DELAY") {
            const uint32_t duration =
                std::min(10000L, std::max(0L, argument.toInt()));
            totalDelay += duration;
            if (!waitDuckyDelay(duration)) { canceled = true; break; }
        }
        ++executed;
        if (defaultDelay > 0) {
            totalDelay += defaultDelay;
            if (!waitDuckyDelay(defaultDelay)) { canceled = true; break; }
        }
    }
    if (file) file.close();
    hidService.releaseAll();
    duckyRunStatus = canceled ? "Canceled - keys released"
                              : "Completed " + String(executed) + " commands";
    recoverKeyboardAfterBlockingOperation();
    currentScreen = Screen::DuckyResult;
    drawCurrentScreen();
}

void drawDuckyResult() {
    drawHeader("DuckyScript Result");
    M5Cardputer.Display.setTextColor(Branding::text, Branding::background);
    M5Cardputer.Display.setCursor(8, 42);
    M5Cardputer.Display.print(duckyRunStatus);
    M5Cardputer.Display.setTextColor(Branding::muted, Branding::background);
    M5Cardputer.Display.setCursor(8, 65);
    M5Cardputer.Display.print("All HID keys released.");
    drawFooter("Enter/Esc: scripts");
}

void drawUsbHidConfirmation() {
    drawHeader("Confirm HID Test");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 31);
    display.print("Focus a blank text editor now.");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 51);
    display.printf("Preset: %s", kHidPresetNames[listSelection]);
    display.setCursor(8, 70);
    display.print("This sends text only.");
    display.setCursor(8, 88);
    display.print("No commands or shortcuts.");
    drawFooter("Enter: TYPE NOW  Backspace/Q: cancel");
}

void runUsbHidPreset() {
    drawHeader("USB / HID Self-Test");
    M5Cardputer.Display.setTextColor(Branding::warning,
                                    Branding::background);
    M5Cardputer.Display.setCursor(8, 43);
    M5Cardputer.Display.print("Typing into the USB host...");
    drawFooter("Text-only demonstration");
    delay(350);
    hidService.run(static_cast<HidPreset>(listSelection));
    recoverKeyboardAfterBlockingOperation();
    Serial.printf("[usb] HID preset=%u sent\n",
                  static_cast<unsigned>(listSelection));
    currentScreen = Screen::UsbHid;
    drawUsbHid();
}

void drawAudio() {
    static const char* const items[] = {
        "Speaker tone test", "Microphone level", "Play MP3 from SD",
    };
    drawHeader("Audio Self-Test");
    normalizeListPosition(3);
    for (size_t row = 0; row < 3; ++row) {
        drawListRow(row, items[row], row == listSelection);
    }
    drawFooter("Enter: open/run  Backspace/Q: back");
}

void drawMicrophone() {
    drawHeader("Microphone Level");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 35);
    display.printf("Input level: %u%%", microphoneLevel);
    display.drawRect(8, 58, display.width() - 16, 22, Branding::muted);
    display.fillRect(10, 60,
                     (display.width() - 20) * microphoneLevel / 100, 18,
                     microphoneLevel > 75 ? Branding::warning
                                          : Branding::accent);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 93);
    display.print("Speak or tap near the microphone.");
    drawFooter("Backspace/Q: stop and return");
}

void updateMicrophoneMeter() {
    auto& display = M5Cardputer.Display;
    display.fillRect(8, 32, display.width() - 16, 18,
                     Branding::background);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 35);
    display.printf("Input level: %u%%", microphoneLevel);
    display.fillRect(9, 59, display.width() - 18, 20,
                     Branding::background);
    display.fillRect(10, 60,
                     (display.width() - 20) * microphoneLevel / 100, 18,
                     microphoneLevel > 75 ? Branding::warning
                                          : Branding::accent);
}

void loadAudioFiles() {
    audioFiles.clear();
    if (!sdAvailable) return;
    File directory = SD.open("/ghostwire/audio");
    if (!directory || !directory.isDirectory()) return;
    File entry = directory.openNextFile();
    while (entry && audioFiles.size() < 32) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            if (name.endsWith(".mp3") || name.endsWith(".MP3")) {
                const int slash = name.lastIndexOf('/');
                if (slash >= 0) name = name.substring(slash + 1);
                audioFiles.push_back(name);
            }
        }
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();
    std::sort(audioFiles.begin(), audioFiles.end());
    listSelection = 0;
    listOffset = 0;
}

void drawAudioFiles() {
    drawHeader("MP3 Files");
    normalizeListPosition(audioFiles.size());
    drawHeaderPosition(listSelection + 1, audioFiles.size());
    if (audioFiles.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted,
                                        Branding::background);
        M5Cardputer.Display.setCursor(8, 38);
        M5Cardputer.Display.print("No MP3 files in");
        M5Cardputer.Display.setCursor(8, 55);
        M5Cardputer.Display.print("/ghostwire/audio");
    } else {
        for (size_t row = 0;
             row < kVisibleRows && row + listOffset < audioFiles.size();
             ++row) {
            drawListRow(row, audioFiles[row + listOffset],
                        row + listOffset == listSelection);
        }
    }
    drawFooter("Enter: play  R: reload  Backspace/Q: back");
}

void startSelectedMp3() {
    if (audioFiles.empty() || listSelection >= audioFiles.size()) return;
    const String path = "/ghostwire/audio/" + audioFiles[listSelection];
    recoverKeyboardAfterBlockingOperation();
    if (!audioService.startMp3(path.c_str())) {
        drawHeader("MP3 Playback");
        M5Cardputer.Display.setTextColor(Branding::warning,
                                        Branding::background);
        M5Cardputer.Display.setCursor(8, 44);
        M5Cardputer.Display.print("Unable to decode this file.");
        drawFooter("Press Backspace/Q");
        return;
    }
    audioReturnScreen = Screen::AudioFiles;
    nowPlayingName = audioFiles[listSelection];
    nowPlayingSource = "/ghostwire/audio";
    currentScreen = Screen::AudioPlaying;
    drawHeader("Now Playing");
    M5Cardputer.Display.setTextColor(Branding::text, Branding::background);
    M5Cardputer.Display.setCursor(8, 40);
    M5Cardputer.Display.print(audioFiles[listSelection].substring(0, 34));
    M5Cardputer.Display.setTextColor(Branding::muted,
                                    Branding::background);
    M5Cardputer.Display.setCursor(8, 64);
    M5Cardputer.Display.print("MP3 from /ghostwire/audio");
    drawFooter("Enter/Backspace/Q: stop");
}

String formatFileSize(uint64_t bytes) {
    if (bytes >= 1024ULL * 1024ULL) {
        return String(static_cast<double>(bytes) / (1024.0 * 1024.0), 1) +
               " MiB";
    }
    if (bytes >= 1024ULL) {
        return String(static_cast<double>(bytes) / 1024.0, 1) + " KiB";
    }
    return String(bytes) + " bytes";
}

bool isMp3File(const String& name) {
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".mp3");
}

bool isPreviewableFile(const String& name) {
    String lower = name;
    lower.toLowerCase();
    return lower.endsWith(".csv") || lower.endsWith(".txt") ||
           lower.endsWith(".log");
}

String selectedFilePath() {
    if (files.empty() || listSelection >= files.size()) return "";
    String path = currentPath;
    if (path != "/") path += "/";
    path += files[listSelection].name;
    return path;
}

void loadDirectory() {
    files.clear();
    if (!sdAvailable) return;
    File directory = SD.open(currentPath);
    if (!directory || !directory.isDirectory()) return;
    File entry = directory.openNextFile();
    while (entry && files.size() < 128) {
        String name = entry.name();
        const int slash = name.lastIndexOf('/');
        if (slash >= 0) name = name.substring(slash + 1);
        files.push_back({name, entry.isDirectory(), entry.size()});
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();
    std::sort(files.begin(), files.end(), [](const FileEntry& left,
                                             const FileEntry& right) {
        if (left.directory != right.directory) return left.directory;
        String a = left.name;
        String b = right.name;
        a.toLowerCase();
        b.toLowerCase();
        return a < b;
    });
    listSelection = 0;
    listOffset = 0;
}

void drawFiles() {
    drawHeader("Files");
    if (!sdAvailable) {
        M5Cardputer.Display.setTextColor(Branding::warning,
                                        Branding::background);
        M5Cardputer.Display.setCursor(8, 35);
        M5Cardputer.Display.print("microSD unavailable");
        M5Cardputer.Display.setCursor(8, 52);
        M5Cardputer.Display.print("Press R to retry");
    } else if (files.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted,
                                        Branding::background);
        M5Cardputer.Display.setCursor(8, 35);
        M5Cardputer.Display.print("This folder is empty");
    } else {
        normalizeListPosition(files.size());
        drawHeaderPosition(listSelection + 1, files.size());
        for (size_t row = 0;
             row < kVisibleRows && row + listOffset < files.size(); ++row) {
            const auto& entry = files[row + listOffset];
            String label = entry.directory ? "[" + entry.name + "]" : entry.name;
            String suffix;
            if (!entry.directory) {
                suffix = entry.size >= 1024
                             ? String(entry.size / 1024) + "K"
                             : String(entry.size) + "B";
            }
            drawListRow(row, label, row + listOffset == listSelection, suffix);
        }
    }
    drawFooter((currentPath + "   R: remount  Esc: up/back").c_str());
}

String logTypeFromName(const String& name) {
    String lower = name;
    lower.toLowerCase();
    if (lower.startsWith("imu_")) return "IMU";
    if (lower.startsWith("gnss_")) return "GNSS";
    if (lower.startsWith("lora_")) return "LoRa";
    if (lower.startsWith("wifi_")) return "Wi-Fi";
    if (lower.startsWith("ble_")) return "BLE";
    return "Other";
}

void loadLogSessions() {
    logSessions.clear();
    logSelection = 0;
    logOffset = 0;
    if (!sdAvailable) return;
    File directory = SD.open("/ghostwire/logs");
    if (!directory || !directory.isDirectory()) return;
    File entry = directory.openNextFile();
    while (entry && logSessions.size() < 256) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            const int slash = name.lastIndexOf('/');
            if (slash >= 0) name = name.substring(slash + 1);
            logSessions.push_back(
                {name, logTypeFromName(name), entry.size()});
        }
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();
    std::sort(logSessions.begin(), logSessions.end(),
              [](const LogEntry& left, const LogEntry& right) {
                  String a = left.name;
                  String b = right.name;
                  a.toLowerCase();
                  b.toLowerCase();
                  return a > b;
              });
}

void normalizeLogPosition() {
    if (logSessions.empty()) {
        logSelection = 0;
        logOffset = 0;
        return;
    }
    if (logSelection >= logSessions.size()) {
        logSelection = logSessions.size() - 1;
    }
    if (logSelection < logOffset) logOffset = logSelection;
    if (logSelection >= logOffset + kVisibleRows) {
        logOffset = logSelection - kVisibleRows + 1;
    }
}

void drawLogSessions() {
    drawHeader("Logs / Sessions");
    normalizeLogPosition();
    drawHeaderPosition(logSelection + 1, logSessions.size());
    if (!sdAvailable) {
        M5Cardputer.Display.setTextColor(Branding::warning,
                                        Branding::background);
        M5Cardputer.Display.setCursor(8, 38);
        M5Cardputer.Display.print("microSD unavailable");
    } else if (logSessions.empty()) {
        M5Cardputer.Display.setTextColor(Branding::muted,
                                        Branding::background);
        M5Cardputer.Display.setCursor(8, 38);
        M5Cardputer.Display.print("No saved sessions");
    } else {
        for (size_t row = 0;
             row < kVisibleRows && logOffset + row < logSessions.size();
             ++row) {
            const auto& entry = logSessions[logOffset + row];
            drawListRow(row, entry.name,
                        logOffset + row == logSelection, entry.type);
        }
    }
    drawFooter("Enter: details   R: refresh   Q: back");
}

uint32_t countCsvRows(const String& name) {
    File file = SD.open("/ghostwire/logs/" + name, FILE_READ);
    if (!file) return 0;
    uint32_t lines = 0;
    uint8_t buffer[256];
    while (file.available()) {
        const int count = file.read(buffer, sizeof(buffer));
        if (count <= 0) break;
        for (int index = 0; index < count; ++index) {
            if (buffer[index] == '\n') ++lines;
        }
    }
    file.close();
    return lines > 0 ? lines - 1 : 0;
}

void openLogDetail() {
    if (logSessions.empty() || logSelection >= logSessions.size()) return;
    drawHeader("Session Details");
    M5Cardputer.Display.setTextColor(Branding::warning,
                                    Branding::background);
    M5Cardputer.Display.setCursor(8, 44);
    M5Cardputer.Display.print("Reading session metadata...");
    selectedLogRows = countCsvRows(logSessions[logSelection].name);
    currentScreen = Screen::LogDetail;
}

void drawLogDetail() {
    if (logSessions.empty() || logSelection >= logSessions.size()) {
        currentScreen = Screen::LogSessions;
        drawLogSessions();
        return;
    }
    const auto& entry = logSessions[logSelection];
    drawHeader("Session Details");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 31);
    display.print(entry.name.substring(0, 37));
    display.setCursor(8, 50);
    display.printf("Subsystem: %s", entry.type.c_str());
    display.setCursor(8, 69);
    display.printf("Size: %s", formatFileSize(entry.size).c_str());
    display.setCursor(8, 88);
    display.printf("CSV data rows: %lu",
                   static_cast<unsigned long>(selectedLogRows));
    drawFooter("Enter: preview   Tab: actions   Q: sessions");
}

void drawLogDeleteConfirm() {
    drawHeader("Delete Session?");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 40);
    display.print(logSessions[logSelection].name.substring(0, 37));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 65);
    display.print("This cannot be undone.");
    drawFooter("Enter: DELETE   Backspace/Q: cancel");
}

void drawFileDetail() {
    if (files.empty() || listSelection >= files.size()) {
        currentScreen = Screen::Files;
        drawFiles();
        return;
    }
    const FileEntry& entry = files[listSelection];
    drawHeader("File Details");
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
    display.print(currentPath.substring(0, 36));
    drawFooter(isMp3File(entry.name)
                   ? "Enter: play   Backspace/Q: files"
                   : (isPreviewableFile(entry.name)
                          ? "Enter: preview   Backspace/Q: files"
                          : "Backspace/Q: files"));
}

bool loadTextPreview() {
    previewLines.clear();
    previewTopLine = 0;
    previewColumn = 0;
    previewTruncated = false;
    File file = SD.open(selectedFilePath(), FILE_READ);
    if (!file || file.isDirectory()) return false;

    constexpr size_t kMaxPreviewLines = 128;
    constexpr size_t kMaxLineLength = 160;
    while (file.available() && previewLines.size() < kMaxPreviewLines) {
        String line = file.readStringUntil('\n');
        line.replace("\r", "");
        line.replace("\t", " ");
        for (size_t index = 0; index < line.length(); ++index) {
            const unsigned char value = line[index];
            if (value < 32 || value > 126) line.setCharAt(index, '.');
        }
        if (line.length() > kMaxLineLength) {
            line = line.substring(0, kMaxLineLength);
        }
        previewLines.push_back(line);
    }
    previewTruncated = file.available();
    file.close();
    return true;
}

void drawTextPreview() {
    drawHeader("Text Preview");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    if (previewLines.empty()) {
        display.setCursor(8, 38);
        display.print("(empty file)");
    } else {
        for (size_t row = 0;
             row < kVisibleRows && previewTopLine + row < previewLines.size();
             ++row) {
            const String& source = previewLines[previewTopLine + row];
            display.setCursor(4, 27 + row * 15);
            if (previewColumn < source.length()) {
                display.print(source.substring(previewColumn,
                                               previewColumn + 39));
            }
        }
    }
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(125, 7);
    display.printf("%u/%u%s", static_cast<unsigned>(previewTopLine + 1),
                   static_cast<unsigned>(previewLines.size()),
                   previewTruncated ? "+" : "");
    drawFooter("W/S: lines  A/D: columns  Q: back");
}

void playSelectedBrowserMp3() {
    if (files.empty() || listSelection >= files.size() ||
        !isMp3File(files[listSelection].name)) {
        return;
    }
    String path = currentPath;
    if (path != "/") path += "/";
    path += files[listSelection].name;
    recoverKeyboardAfterBlockingOperation();
    if (!audioService.startMp3(path.c_str())) return;

    audioReturnScreen = Screen::FileDetail;
    nowPlayingName = files[listSelection].name;
    nowPlayingSource = currentPath;
    currentScreen = Screen::AudioPlaying;
    drawHeader("Now Playing");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 40);
    display.print(nowPlayingName.substring(0, 34));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 64);
    display.print(nowPlayingSource.substring(0, 36));
    drawFooter("Enter/Backspace/Q: stop");
}

const char* resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "Power on";
        case ESP_RST_EXT: return "External reset";
        case ESP_RST_SW: return "Software reset";
        case ESP_RST_PANIC: return "Panic/crash";
        case ESP_RST_INT_WDT: return "Interrupt watchdog";
        case ESP_RST_TASK_WDT: return "Task watchdog";
        case ESP_RST_WDT: return "Other watchdog";
        case ESP_RST_DEEPSLEEP: return "Deep sleep wake";
        case ESP_RST_BROWNOUT: return "Brownout";
        case ESP_RST_SDIO: return "SDIO reset";
        default: return "Unknown";
    }
}

bool isAbnormalReset(esp_reset_reason_t reason) {
    return reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT ||
           reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT ||
           reason == ESP_RST_BROWNOUT;
}

void recordBootTelemetry() {
    const esp_reset_reason_t reason = esp_reset_reason();
    bootCount = preferences.getUInt("boot_count", 0) + 1;
    preferences.putUInt("boot_count", bootCount);
    abnormalResetCount = preferences.getUInt("abnormal_count", 0);
    if (isAbnormalReset(reason)) {
        ++abnormalResetCount;
        preferences.putUInt("abnormal_count", abnormalResetCount);
    }

    bootHistorySaved = false;
    if (!sdAvailable) return;
    SD.mkdir("/ghostwire");
    SD.mkdir("/ghostwire/logs");
    static constexpr char path[] = "/ghostwire/logs/boot_history.csv";
    const bool newFile = !SD.exists(path);
    File history = SD.open(path, FILE_APPEND);
    if (!history) return;
    if (newFile) {
        history.println(
            "boot,firmware,reset_code,reset_reason,abnormal_events,"
            "battery_v,battery_percent,free_heap_bytes");
    }
    history.printf("%lu,%s,%u,\"%s\",%lu,%.2f,%u,%u\n",
                   static_cast<unsigned long>(bootCount), Branding::version,
                   static_cast<unsigned>(reason), resetReasonName(reason),
                   static_cast<unsigned long>(abnormalResetCount),
                   readBatteryVoltage(), batteryPercentage(),
                   ESP.getFreeHeap());
    history.close();
    bootHistorySaved = true;
}

String formatUptime() {
    const uint32_t totalSeconds = millis() / 1000;
    const uint32_t days = totalSeconds / 86400;
    const uint8_t hours = (totalSeconds / 3600) % 24;
    const uint8_t minutes = (totalSeconds / 60) % 60;
    const uint8_t seconds = totalSeconds % 60;
    char value[24];
    snprintf(value, sizeof(value), "%lud %02u:%02u:%02u",
             static_cast<unsigned long>(days), hours, minutes, seconds);
    return String(value);
}

enum class DiagnosticState {
    Information,
    Ready,
    Warning,
};

struct SystemDiagnostic {
    String label;
    String value;
    DiagnosticState state;

    SystemDiagnostic(const String& diagnosticLabel,
                     const String& diagnosticValue,
                     DiagnosticState diagnosticState =
                         DiagnosticState::Information)
        : label(diagnosticLabel),
          value(diagnosticValue),
          state(diagnosticState) {}
};

std::vector<SystemDiagnostic> systemDiagnostics() {
    std::vector<SystemDiagnostic> rows;
    rows.reserve(22);
    rows.push_back({"Firmware", Branding::version});
    rows.push_back({"Uptime", formatUptime()});
    const esp_reset_reason_t resetReason = esp_reset_reason();
    rows.push_back({"Last reset", resetReasonName(resetReason),
                    resetReason == ESP_RST_PANIC ||
                            resetReason == ESP_RST_INT_WDT ||
                            resetReason == ESP_RST_TASK_WDT ||
                            resetReason == ESP_RST_WDT ||
                            resetReason == ESP_RST_BROWNOUT
                        ? DiagnosticState::Warning
                        : DiagnosticState::Information});
    uint8_t sshStage = preferences.getUChar("ssh_call", 0);
    if (sshStage == 0) sshStage = sshService.crashStage();
    if (sshStage == 0) sshStage = preferences.getUChar("ssh_stage", 0);
    if (sshStage != 0) {
        const char* stage = "unknown";
        if (sshStage == 1) stage = "target parse";
        else if (sshStage == 2) stage = "password screen";
        else if (sshStage == 3) stage = "password entry";
        else if (sshStage == 4) stage = "libssh init";
        else if (sshStage == 5) stage = "libssh connect";
        else if (sshStage == 6) stage = "libssh returned";
        else if (sshStage == 10) stage = "ssh_new";
        else if (sshStage == 11) stage = "session allocated";
        else if (sshStage == 12) stage = "session options";
        else if (sshStage == 13) stage = "ssh_connect";
        else if (sshStage == 14) stage = "SSH negotiated";
        else if (sshStage == 15) stage = "host key read";
        else if (sshStage == 16) stage = "host key checked";
        else if (sshStage == 17) stage = "authentication";
        else if (sshStage == 18) stage = "channel setup";
        else if (sshStage == 19) stage = "host key compare";
        else if (sshStage == 20) stage = "trust prompt";
        rows.push_back({"SSH reset stage", stage, DiagnosticState::Warning});
    }
    rows.push_back({"Battery", String(batteryPercentage()) + "% / " +
                                   String(readBatteryVoltage(), 2) + " V"});
    rows.push_back(
        {"Heap free", String(ESP.getFreeHeap() / 1024) + " KB"});
    rows.push_back(
        {"Heap minimum", String(ESP.getMinFreeHeap() / 1024) + " KB"});
    rows.push_back(
        {"App image", String(ESP.getSketchSize() / 1024) + " KB"});
    rows.push_back(
        {"App free", String(ESP.getFreeSketchSpace() / 1024) + " KB"});
    rows.push_back(
        {"Flash chip", String(ESP.getFlashChipSize() / 1048576) + " MB"});
    rows.push_back({"CPU", String(ESP.getCpuFreqMHz()) + " MHz"});
    rows.push_back({"Chip rev", String(ESP.getChipRevision())});
    const uint64_t chipId = ESP.getEfuseMac();
    char identity[24];
    snprintf(identity, sizeof(identity), "%04X%08X",
             static_cast<unsigned>((chipId >> 32) & 0xFFFF),
             static_cast<unsigned>(chipId & 0xFFFFFFFF));
    rows.push_back({"Device ID", identity});
    rows.push_back({"microSD",
                    sdAvailable ? "READY / " + String(sdCardSizeMiB) + " MB"
                                : "NOT FOUND",
                    sdAvailable ? DiagnosticState::Ready
                                : DiagnosticState::Warning});
    rows.push_back({"USB HID", hidService.ready() ? "READY" : "FAILED",
                    hidService.ready() ? DiagnosticState::Ready
                                       : DiagnosticState::Warning});
    rows.push_back({"GNSS UART", gnssService.hasData() ? "DATA" : "READY",
                    DiagnosticState::Ready});
    rows.push_back({"LoRa SX1262",
                    loraService.isReady() ? "READY" : "NOT PROBED",
                    loraService.isReady() ? DiagnosticState::Ready
                                          : DiagnosticState::Information});
    rows.push_back({"IMU", M5.Imu.isEnabled() ? "READY" : "NOT PROBED",
                    M5.Imu.isEnabled() ? DiagnosticState::Ready
                                       : DiagnosticState::Information});
    rows.push_back({"Clock", clockSynced ? "GNSS SYNCED" : "NOT SYNCED",
                    clockSynced ? DiagnosticState::Ready
                                : DiagnosticState::Information});
    rows.push_back({"Boot count", String(bootCount)});
    rows.push_back({"Stability events", String(abnormalResetCount),
                    abnormalResetCount ? DiagnosticState::Warning
                                       : DiagnosticState::Ready});
    rows.push_back({"Boot history",
                    bootHistorySaved ? "SAVED" :
                    (sdAvailable ? "WRITE FAILED" : "NO SD"),
                    bootHistorySaved ? DiagnosticState::Ready
                                     : DiagnosticState::Warning});
    rows.push_back({"Boot chime", bootChimeStatus,
                    bootChimePending ? DiagnosticState::Information
                    : bootChimeStatus.startsWith("Gave up")
                          ? DiagnosticState::Warning
                          : DiagnosticState::Ready});
    return rows;
}

bool exportSystemDiagnostics() {
    if (!sdAvailable) {
        diagnosticExportStatus = "Export failed: no SD card";
        return false;
    }
    SD.mkdir("/ghostwire");
    SD.mkdir("/ghostwire/logs");
    String path;
    for (uint16_t index = 1; index < 10000; ++index) {
        char candidate[64];
        snprintf(candidate, sizeof(candidate),
                 "/ghostwire/logs/diagnostics_%04u.txt", index);
        if (!SD.exists(candidate)) {
            path = candidate;
            break;
        }
    }
    if (path.isEmpty()) {
        diagnosticExportStatus = "Export failed: filenames full";
        return false;
    }
    File report = SD.open(path, FILE_WRITE);
    if (!report) {
        diagnosticExportStatus = "Export failed: SD write";
        return false;
    }
    report.printf("%s system diagnostics\n", Branding::productName);
    report.printf("Creator: %s\n", Branding::creatorName);
    report.printf("Generated UTC: %s\n\n",
                  clockSynced ? utcTimestamp().c_str() : "unavailable");
    for (const auto& diagnostic : systemDiagnostics()) {
        report.printf("%-13s%s\n", diagnostic.label.c_str(),
                      diagnostic.value.c_str());
    }
    report.close();
    diagnosticExportStatus = "Saved " + path.substring(path.lastIndexOf('/') + 1);
    return true;
}

void drawSystem() {
    drawHeader("System Diagnostics");
    const std::vector<SystemDiagnostic> diagnostics = systemDiagnostics();
    normalizeListPosition(diagnostics.size());
    drawHeaderPosition(listSelection + 1, diagnostics.size());
    for (size_t row = 0;
         row < kVisibleRows && row + listOffset < diagnostics.size(); ++row) {
        const auto& diagnostic = diagnostics[row + listOffset];
        String label = diagnostic.label;
        while (label.length() < 13) label += ' ';
        label += diagnostic.value;
        drawListRow(row, label, row + listOffset == listSelection);
    }
    drawFooter(diagnosticExportStatus.isEmpty()
                   ? "W/S: browse  Enter: clock  Tab: actions  Q: back"
                   : diagnosticExportStatus.c_str());
}

String utcTimestamp() {
    if (!clockSynced) return "";
    const time_t now = time(nullptr);
    struct tm utc {};
    gmtime_r(&now, &utc);
    char value[25];
    strftime(value, sizeof(value), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return String(value);
}

bool syncClockFromGnss() {
    if (!gnssService.hasUtcDateTime()) {
        clockStatus = "GNSS date/time unavailable";
        return false;
    }
    struct tm utc {};
    utc.tm_year = gnssService.utcYear() - 1900;
    utc.tm_mon = gnssService.utcMonth() - 1;
    utc.tm_mday = gnssService.utcDay();
    utc.tm_hour = gnssService.utcHour();
    utc.tm_min = gnssService.utcMinute();
    utc.tm_sec = gnssService.utcSecond();
    utc.tm_isdst = 0;
    setenv("TZ", "UTC0", 1);
    tzset();
    const time_t epoch = mktime(&utc);
    if (epoch <= 0) {
        clockStatus = "GNSS time conversion failed";
        return false;
    }
    const timeval systemTime{epoch, 0};
    settimeofday(&systemTime, nullptr);
    // UK civil time: GMT in winter and BST from the last Sunday in March
    // until the last Sunday in October. Log formatting continues to use UTC.
    setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0/2", 1);
    tzset();
    clockSynced = true;
    clockStatus = "Synchronized from GNSS";
    return true;
}

bool syncClockFromNtp() {
    if (WiFi.status() != WL_CONNECTED) {
        clockStatus = "Connect Wi-Fi before NTP sync";
        return false;
    }
    clockStatus = "Contacting NTP servers...";
    configTzTime("GMT0BST,M3.5.0/1,M10.5.0/2", "time.cloudflare.com",
                 "pool.ntp.org", "time.google.com");
    struct tm local {};
    if (!getLocalTime(&local, 5000)) {
        clockStatus = "NTP sync timed out";
        return false;
    }
    clockSynced = true;
    clockStatus = "Synchronized from NTP";
    return true;
}

void drawTimeReadouts() {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, 45, display.width(), 40, Branding::background);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 52);
    if (clockSynced) {
        const time_t now = time(nullptr);
        struct tm local {};
        localtime_r(&now, &local);
        char localValue[32];
        strftime(localValue, sizeof(localValue), "%Y-%m-%d %H:%M:%S %Z",
                 &local);
        display.printf("Local: %s", localValue);
        display.setCursor(8, 69);
        display.printf("UTC:   %s", utcTimestamp().c_str());
    } else {
        display.print("Local: ----/--/-- --:--:--");
        display.setCursor(8, 69);
        display.print("UTC:   ----/--/-- --:--:--");
    }
}

void drawTimeStatus() {
    drawHeader("System Clock");
    auto& display = M5Cardputer.Display;
    display.setTextColor(clockSynced ? Branding::accent : Branding::warning,
                         Branding::background);
    display.setCursor(8, 32);
    display.print(clockSynced ? "System clock synchronized"
                              : "System clock not synchronized");
    drawTimeReadouts();
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 87);
    display.print(clockStatus.substring(0, 37));
    display.setCursor(8, 103);
    display.print("Clock resets after full power-off.");
    drawFooter("Tab: actions   Q: back");
}

void drawGnss(bool fullDraw = true) {
    beginContentUpdate("GNSS Foundation", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(gnssService.hasFix() ? Branding::accent
                                              : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    if (!gnssService.hasData()) {
        display.print("UART: waiting for NMEA data");
    } else if (!gnssService.hasFix()) {
        display.print("GNSS: data received, no fix");
    } else {
        display.printf("GNSS: position fix%s",
                       gnssLogger.isActive() ? "  REC" : "");
    }
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf("Sat: %lu  HDOP: %.1f  UTC: %s",
                   static_cast<unsigned long>(gnssService.satellites()),
                   gnssService.hdop(),
                   gnssService.utcTime().c_str());
    display.setCursor(8, 64);
    if (gnssService.hasFix()) {
        display.printf("Lat: %.6f", gnssService.latitude());
        display.setCursor(8, 81);
        display.printf("Lon: %.6f", gnssService.longitude());
        display.setCursor(8, 98);
        display.printf("Altitude: %.1f m", gnssService.altitudeMetres());
        if (gnssLogger.isActive()) {
            display.printf("  REC %lu",
                           static_cast<unsigned long>(gnssLogger.rowCount()));
        }
    } else {
        display.printf(
            "NMEA bytes: %lu",
            static_cast<unsigned long>(gnssService.charactersProcessed()));
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 82);
        display.print("Move outdoors for first fix.");
        if (gnssLogger.isActive()) {
            display.setCursor(8, 98);
            display.printf("REC %lu rows",
                           static_cast<unsigned long>(gnssLogger.rowCount()));
        }
    }
    if (fullDraw) drawFooter("R: restart GNSS   Tab: actions   Q: back");
}

void drawWifiMenu() {
    static const char* const items[] = {
        "Discovery", "Channel Analyzer", "Sniffer", "Connect",
    };
    drawHeader("Wi-Fi");
    normalizeListPosition(4);
    for (size_t row = 0; row < 4; ++row) {
        drawListRow(row, items[row], row == listSelection);
    }
    drawFooter("Enter: open   Backspace/Q: back");
}

void drawBleMenu() {
    static const char* const items[] = {
        "Advertisement Sniffer", "BLE Keyboard", "Spam",
    };
    drawHeader("BLE");
    normalizeListPosition(3);
    for (size_t row = 0; row < 3; ++row) {
        drawListRow(row, items[row], row == listSelection);
    }
    drawFooter("Enter: open   Backspace/Q: back");
}

void drawDevicesMenu() {
    drawHeader("Devices");
    static const char* const items[] = {"Biscuit Pro", "Chameleon Ultra"};
    normalizeListPosition(2);
    for (size_t row = 0; row < 2; ++row) {
        drawListRow(row, items[row], row == listSelection);
    }
    drawFooter("Enter: open   Backspace/Q: back");
}

void drawBiscuit() {
    drawHeader("Biscuit Pro");
    auto& display = M5Cardputer.Display;
    const bool connected = biscuitClient.isConnected();
    display.setTextColor(connected ? Branding::accent : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.print(connected ? "CONNECTED - READY" :
                              biscuitClient.lastStatus());
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 52);
    display.printf("%s  FW %s", biscuitClient.model().c_str(),
                   biscuitClient.firmware().c_str());
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 72);
    display.printf("C5 %s", biscuitClient.c5Firmware().c_str());
    display.setCursor(8, 92);
    display.printf("Device: %s", biscuitClient.deviceStatus().c_str());
    drawFooter(connected ? "Enter: tools   R: reconnect   Q: back"
                         : "Enter/R: connect   Q: back");
}

void drawBiscuitTools() {
    static const char* const items[] = {
        "Device information", "Wi-Fi AP scan", "Station scan",
        "Packet count", "Current channel", "Node list", "Wardrive monitor",
    };
    drawHeader("Biscuit: Read-only tools");
    normalizeListPosition(7);
    for (size_t row = 0; row < kVisibleRows && row + listOffset < 7; ++row) {
        const size_t index = row + listOffset;
        drawListRow(row, items[index], index == listSelection);
    }
    drawFooter("Enter: run   Backspace/Q: back");
}

void appendBiscuitWardriveData(const String& raw) {
    // Both payloads begin "name,MAC,..." (AP uses SSID,BSSID). A record may
    // straddle BLE notifications, so retain an incomplete record from its
    // prefix and finish it on the next update.
    const String combined = biscuitWardriveParseTail + raw;
    size_t cursor = 0;
    const size_t noIndex = static_cast<size_t>(-1);
    size_t incompleteAt = noIndex;
    while (cursor < combined.length()) {
        const int apAt = combined.indexOf("DATA:AP:", cursor);
        const int btAt = combined.indexOf("DATA:BT:", cursor);
        if (apAt < 0 && btAt < 0) break;
        const bool isAp = apAt >= 0 && (btAt < 0 || apAt < btAt);
        const size_t prefixAt = static_cast<size_t>(isAp ? apAt : btAt);
        const size_t payloadAt = prefixAt + 8;
        const int firstComma = combined.indexOf(',', payloadAt);
        const int secondComma = firstComma >= 0
                                    ? combined.indexOf(',', firstComma + 1)
                                    : -1;
        if (firstComma < 0 || secondComma < 0) {
            incompleteAt = prefixAt;
            break;
        }
        String mac = combined.substring(firstComma + 1, secondComma);
        mac.trim();
        mac.toUpperCase();
        const bool validMac = mac.length() == 17 && mac[2] == ':' &&
                              mac[5] == ':' && mac[8] == ':' &&
                              mac[11] == ':' && mac[14] == ':';
        if (validMac) {
            std::vector<String>& identities =
                isAp ? biscuitWardriveBssids : biscuitWardriveBleMacs;
            if (std::find(identities.begin(), identities.end(), mac) ==
                    identities.end() &&
                identities.size() < 512) {
                identities.push_back(mac);
            }
        }
        cursor = static_cast<size_t>(secondComma + 1);
    }
    biscuitWardriveApCount = biscuitWardriveBssids.size();
    biscuitWardriveBleCount = biscuitWardriveBleMacs.size();
    if (incompleteAt != noIndex) {
        biscuitWardriveParseTail = combined.substring(incompleteAt);
        if (biscuitWardriveParseTail.length() > 128) {
            biscuitWardriveParseTail = "";
        }
    } else {
        biscuitWardriveParseTail = combined.length() > 7
                                       ? combined.substring(combined.length() - 7)
                                       : combined;
    }
}

void stopBiscuitWardrive() {
    if (biscuitWardriveActive && biscuitClient.isConnected()) {
        biscuitClient.sendCommandNoWait("CMD:stopscan:");
        delay(100);
        biscuitClient.takeNotifications();
    }
    biscuitWardriveActive = false;
}

void drawBiscuitWardrive(bool fullDraw = true) {
    const uint32_t signature =
        (static_cast<uint32_t>(biscuitWardriveActive) << 31) ^
        (biscuitWardriveApCount * 2654435761UL) ^ biscuitWardriveBleCount;
    static uint32_t lastSignature = UINT32_MAX;
    if (!fullDraw && signature == lastSignature) return;
    lastSignature = signature;
    beginContentUpdate("Biscuit Wardrive", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(biscuitWardriveActive ? Branding::accent
                                               : Branding::warning,
                         Branding::background);
    display.setCursor(8, 28);
    display.print(biscuitWardriveActive ? "MONITORING" : "STOPPED");
    display.setTextColor(Branding::text, Branding::background);
    display.setTextSize(2);
    display.setCursor(18, 54);
    display.printf("AP  %lu", static_cast<unsigned long>(
                                  biscuitWardriveApCount));
    display.setCursor(18, 82);
    display.printf("BLE %lu", static_cast<unsigned long>(
                                  biscuitWardriveBleCount));
    display.setTextSize(1);
    drawFooter(biscuitWardriveActive ? "Enter: stop   Q: back"
                                     : "Enter: start   Q: back");
}

void prepareBiscuitResult(const String& raw) {
    biscuitResultLines.clear();
    biscuitResultOffset = 0;
    String line;
    for (size_t i = 0; i < raw.length(); ++i) {
        char c = raw[i];
        if (c == '\r' || c == '\n') {
            if (!line.isEmpty()) {
                biscuitResultLines.push_back(line);
                line = "";
            }
            continue;
        }
        if (static_cast<uint8_t>(c) < 0x20) c = ' ';
        line += c;
        if (line.length() >= 37) {
            biscuitResultLines.push_back(line);
            line = "";
        }
    }
    if (!line.isEmpty()) biscuitResultLines.push_back(line);
    if (biscuitResultLines.empty()) biscuitResultLines.push_back("No data");
}

void drawBiscuitResult() {
    drawHeader(biscuitResultTitle.c_str());
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    for (size_t row = 0; row < kVisibleRows &&
                         biscuitResultOffset + row < biscuitResultLines.size();
         ++row) {
        display.setCursor(8, 29 + row * 15);
        display.print(biscuitResultLines[biscuitResultOffset + row]);
    }
    drawFooter("Up/Down: scroll   Backspace/Q: tools");
}

void drawBleKeyboard(bool fullDraw = true) {
    const uint32_t signature =
        (static_cast<uint32_t>(bleKeyboardService.isActive()) << 31) ^
        (static_cast<uint32_t>(bleKeyboardService.isConnected()) << 30) ^
        bleKeyboardService.charactersSent();
    static uint32_t lastSignature = UINT32_MAX;
    if (!fullDraw && signature == lastSignature) return;
    lastSignature = signature;
    beginContentUpdate("BLE Keyboard", fullDraw);
    auto& display = M5Cardputer.Display;
    const bool connected = bleKeyboardService.isConnected();
    display.setTextColor(connected ? Branding::accent : Branding::warning,
                         Branding::background);
    display.setCursor(8, 30);
    if (!bleKeyboardService.isActive()) {
        display.print("STOPPED");
    } else if (connected) {
        display.print("CONNECTED - LIVE INPUT");
    } else {
        display.print("ADVERTISING / WAITING");
    }
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 49);
    display.print("Device: Ghostwire Keyboard");
    display.setCursor(8, 67);
    display.printf("Characters sent: %lu",
                   static_cast<unsigned long>(
                       bleKeyboardService.charactersSent()));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 86);
    display.print(bleKeyboardService.isActive()
                      ? "Typed keys go to the paired host."
                      : "Enter starts pairing/advertising.");
    display.setCursor(8, 102);
    display.print("Use only on a device you control.");
    drawFooter(bleKeyboardService.isActive()
                   ? "Esc: stop/disconnect"
                   : "Enter: start   Esc: back");
}

void drawBleSpamSelect() {
    static const char* const items[] = {
        "Apple", "Fast Pair", "Swift Pair", "All",
    };
    drawHeader("BLE Spam");
    normalizeListPosition(4);
    for (size_t row = 0; row < 4; ++row) {
        drawListRow(row, items[row], row == listSelection);
    }
    drawFooter("Enter: start   Backspace/Q: back");
}

void drawBleSpam(bool fullDraw = true) {
    beginContentUpdate("BLE Spam", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(bleSpamService.isActive() ? Branding::accent
                                                    : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.printf("%s", bleSpamService.isActive() ? "SPAMMING" : "STOPPED");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf("Type: %s", bleSpamService.currentTypeName());
    display.setCursor(8, 64);
    display.printf("Sent: %lu",
                   static_cast<unsigned long>(bleSpamService.packetsSent()));
    const uint8_t* mac = bleSpamService.currentAddress();
    display.setCursor(8, 81);
    display.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4],
                   mac[3], mac[2], mac[1], mac[0]);
    if (fullDraw) drawFooter("Q: stop");
}

void drawRfidMenu() {
    static const char* const items[] = {
        "Chameleon Ultra",
    };
    drawHeader("RFID");
    normalizeListPosition(1);
    drawListRow(0, items[0], listSelection == 0);
    drawFooter("Enter: open   Backspace/Q: back");
}

String chameleonHexId(const uint8_t* data, size_t len) {
    String hex;
    for (size_t i = 0; i < len; ++i) {
        if (i > 0) hex += ':';
        if (data[i] < 0x10) hex += '0';
        hex += String(data[i], HEX);
    }
    return hex;
}

bool saveChameleonIdentity() {
    if (!sdAvailable || (!chameleonHfFound && !chameleonLfFound)) {
        chameleonWorkflowStatus = sdAvailable ? "No captured identity"
                                              : "SD card unavailable";
        return false;
    }
    SD.mkdir("/ghostwire");
    SD.mkdir("/ghostwire/tags");
    String path;
    for (uint16_t index = 1; index < 10000; ++index) {
        char candidate[56];
        snprintf(candidate, sizeof(candidate),
                 "/ghostwire/tags/chameleon_%04u.txt", index);
        if (!SD.exists(candidate)) {
            path = candidate;
            break;
        }
    }
    if (path.isEmpty()) {
        chameleonWorkflowStatus = "No free identity filename";
        return false;
    }
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        chameleonWorkflowStatus = "Unable to create identity file";
        return false;
    }
    if (chameleonHfFound) {
        file.printf("HF14A,%s,%04X,%02X\n",
                    chameleonHexId(chameleonHfTag.uid,
                                   chameleonHfTag.uidLen).c_str(),
                    chameleonHfTag.atqa, chameleonHfTag.sak);
    } else {
        file.printf("EM410X,%s,,\n",
                    chameleonHexId(chameleonLfId, 5).c_str());
    }
    file.close();
    chameleonSavedPath = path;
    preferences.putString("cham_last", path);
    chameleonWorkflowStatus = "Saved " + path.substring(path.lastIndexOf('/') + 1);
    return true;
}

bool parseChameleonHex(const String& text, uint8_t* output,
                       size_t capacity, size_t& length) {
    length = 0;
    int start = 0;
    while (start < static_cast<int>(text.length()) && length < capacity) {
        const int separator = text.indexOf(':', start);
        const int end = separator < 0 ? text.length() : separator;
        if (end - start != 2) return false;
        char* parsedEnd = nullptr;
        const String byteText = text.substring(start, end);
        const unsigned long value = strtoul(byteText.c_str(), &parsedEnd, 16);
        if (parsedEnd == byteText.c_str() || *parsedEnd != '\0' || value > 255) {
            return false;
        }
        output[length++] = static_cast<uint8_t>(value);
        if (separator < 0) break;
        start = separator + 1;
    }
    return length > 0;
}

bool loadChameleonIdentity() {
    if (!sdAvailable || chameleonSavedPath.isEmpty()) {
        chameleonWorkflowStatus = "No saved identity";
        return false;
    }
    File file = SD.open(chameleonSavedPath, FILE_READ);
    if (!file) {
        chameleonWorkflowStatus = "Saved identity is unavailable";
        return false;
    }
    String line = file.readStringUntil('\n');
    file.close();
    line.trim();
    const int first = line.indexOf(',');
    const int second = line.indexOf(',', first + 1);
    const int third = line.indexOf(',', second + 1);
    if (first < 0 || second < 0 || third < 0) {
        chameleonWorkflowStatus = "Saved identity is invalid";
        return false;
    }
    const String type = line.substring(0, first);
    const String id = line.substring(first + 1, second);
    size_t length = 0;
    if (type == "HF14A") {
        if (!parseChameleonHex(id, chameleonHfTag.uid,
                               sizeof(chameleonHfTag.uid), length)) return false;
        chameleonHfTag.uidLen = length;
        chameleonHfTag.atqa = strtoul(
            line.substring(second + 1, third).c_str(), nullptr, 16);
        chameleonHfTag.sak = strtoul(
            line.substring(third + 1).c_str(), nullptr, 16);
        chameleonHfFound = true;
        chameleonLfFound = false;
    } else if (type == "EM410X") {
        if (!parseChameleonHex(id, chameleonLfId,
                               sizeof(chameleonLfId), length) || length != 5) {
            return false;
        }
        chameleonLfFound = true;
        chameleonHfFound = false;
    } else {
        chameleonWorkflowStatus = "Unsupported saved identity";
        return false;
    }
    chameleonScanAttempted = true;
    chameleonWorkflowStatus = "Loaded saved identity";
    return true;
}

void drawChameleonEmulateConfirm() {
    drawHeader("Confirm Identity Emulation");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 32);
    display.print("This changes Chameleon slot 8.");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 50);
    display.print(chameleonHfFound ? "HF identity only; not card data."
                                   : "EM410x ID will be staged.");
    display.setCursor(8, 68);
    display.print("Use only with an authorised tag.");
    display.setCursor(8, 88);
    display.print("Enter: stage + emulate");
    drawFooter("Enter: confirm   Esc: cancel");
}

void drawChameleon(bool fullDraw = true) {
    beginContentUpdate("Chameleon Ultra", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(chameleonClient.isConnected() ? Branding::accent
                                                        : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.print(chameleonClient.lastStatus());
    display.setTextColor(Branding::text, Branding::background);
    if (chameleonHasReadings) {
        display.setCursor(8, 44);
        display.printf("App version: %u.%u", chameleonAppMajor,
                       chameleonAppMinor);
        display.setCursor(8, 58);
        display.printf("Battery: %u%%  (%u mV)", chameleonBatteryPct,
                       chameleonBatteryMv);
    }
    if (chameleonScanAttempted) {
        display.setCursor(8, 72);
        if (chameleonHfFound) {
            display.printf(
                "UID: %s",
                chameleonHexId(chameleonHfTag.uid, chameleonHfTag.uidLen)
                    .c_str());
            display.setCursor(8, 86);
            display.printf("ATQA: 0x%04X  SAK: 0x%02X", chameleonHfTag.atqa,
                           chameleonHfTag.sak);
        } else if (chameleonLfFound) {
            display.printf("EM410x ID: %s",
                           chameleonHexId(chameleonLfId, 5).c_str());
        } else {
            display.print("No tag found");
        }
    }
    display.setCursor(8, 100);
    if (!chameleonWorkflowStatus.isEmpty()) {
        display.setTextColor(Branding::muted, Branding::background);
        display.print(chameleonWorkflowStatus.substring(0, 37));
    } else {
        display.printf("Continuous: %s", chameleonContinuousScan ? "ON" : "OFF");
        if (chameleonLogger.isActive()) {
            display.printf("  Logged: %lu",
                           static_cast<unsigned long>(chameleonLogger.rowCount()));
        }
    }
    if (fullDraw) drawFooter("R: reconnect  Tab: actions  Q: back");
}

void attemptChameleonConnection() {
    if (chameleonClient.isConnected()) return;

    lastChameleonConnectAttemptMs = millis();
    ++chameleonConnectAttempts;
    chameleonWorkflowStatus = "Auto-connect attempt " +
                              String(chameleonConnectAttempts);
    drawChameleon(false);

    // A bounded scan keeps automatic retries responsive enough that the
    // user can still leave the screen between attempts.
    if (chameleonClient.connect(3000)) {
        const bool gotVersion = chameleonClient.getAppVersion(
            chameleonAppMajor, chameleonAppMinor);
        const bool gotBattery = chameleonClient.getBatteryInfo(
            chameleonBatteryMv, chameleonBatteryPct);
        chameleonHasReadings = gotVersion && gotBattery;
        chameleonWorkflowStatus = "Connected on attempt " +
                                  String(chameleonConnectAttempts);
        if (sdAvailable && !chameleonLogger.isActive()) {
            chameleonLogger.begin(
                "chameleon_tags", "timestamp_utc,tag_type,id,atqa,sak");
        }
    } else {
        chameleonWorkflowStatus = "Retrying automatically...";
    }
    recoverKeyboardAfterBlockingOperation();
    drawChameleon(false);
}

// Runs one HF-then-LF scan attempt, updates the on-screen result state, and
// appends a CSV row the first time a given tag is seen -- shared by the
// manual S key and the continuous-scan tick in loop() so both paths log
// identically.
void performChameleonScan() {
    chameleonHfFound = chameleonClient.scanHf14a(chameleonHfTag);
    chameleonLfFound =
        !chameleonHfFound && chameleonClient.scanEm410x(chameleonLfId);
    chameleonScanAttempted = true;

    String signature, csvType, csvId, csvAtqa, csvSak;
    if (chameleonHfFound) {
        csvId = chameleonHexId(chameleonHfTag.uid, chameleonHfTag.uidLen);
        signature = "HF:" + csvId;
        csvType = "HF14A";
        csvAtqa = String(chameleonHfTag.atqa, HEX);
        csvSak = String(chameleonHfTag.sak, HEX);
    } else if (chameleonLfFound) {
        csvId = chameleonHexId(chameleonLfId, 5);
        signature = "LF:" + csvId;
        csvType = "EM410x";
    }

    if (signature.isEmpty()) {
        // No tag present right now -- the next capture, even of the same
        // card, should log again once it reappears.
        chameleonLastLoggedSignature = "";
        return;
    }
    if (signature == chameleonLastLoggedSignature) return;
    chameleonLastLoggedSignature = signature;

    if (chameleonLogger.isActive()) {
        chameleonLogger.append(utcTimestamp() + "," + csvType + "," + csvId +
                               "," + csvAtqa + "," + csvSak);
    }
}

void drawGpsMenu() {
    static const char* const items[] = {
        "GNSS Monitor",
    };
    drawHeader("GPS");
    normalizeListPosition(1);
    drawListRow(0, items[0], listSelection == 0);
    drawFooter("Enter: open   Backspace/Q: back");
}

void drawMeshMenu() {
    static const char* const items[] = {
        "LoRa / Meshtastic",
    };
    drawHeader("Mesh");
    normalizeListPosition(1);
    drawListRow(0, items[0], listSelection == 0);
    drawFooter("Enter: open   Backspace/Q: back");
}

void drawNetworkMenu() {
    static const char* const items[] = {
        "Network Dashboard",
        "Host Discovery",
        "Telnet Client",
        "SSH Client",
    };
    constexpr size_t kNetworkMenuCount = sizeof(items) / sizeof(items[0]);
    drawHeader("Network");
    normalizeListPosition(kNetworkMenuCount);
    for (size_t row = 0; row < kNetworkMenuCount; ++row) {
        drawListRow(row, items[row], row == listSelection);
    }
    drawFooter("Enter: open   Backspace/Q: back");
}

void drawNetworkDashboard() {
    drawHeader("Network Dashboard");
    auto& display = M5Cardputer.Display;
    display.setTextSize(1);
    if (WiFi.status() != WL_CONNECTED) {
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(8, 36);
        display.print("Wi-Fi is not connected");
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 55);
        display.print("Use Wi-Fi > Connect first.");
        drawFooter("R: refresh   Q: back");
        return;
    }
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 28);
    display.printf("SSID: %s  %d dBm", WiFi.SSID().c_str(), WiFi.RSSI());
    display.setCursor(8, 44);
    display.printf("IP:   %s", WiFi.localIP().toString().c_str());
    display.setCursor(8, 60);
    display.printf("GW:   %s", WiFi.gatewayIP().toString().c_str());
    display.setCursor(8, 76);
    display.printf("Mask: %s", WiFi.subnetMask().toString().c_str());
    display.setCursor(8, 92);
    display.printf("DNS:  %s", WiFi.dnsIP().toString().c_str());
    display.setCursor(8, 108);
    display.printf("MAC:  %s", WiFi.macAddress().c_str());
    drawFooter("R: refresh   Q: back");
}

void drawToolsMenu() {
    static const char* const items[] = {
        "Infrared",     "USB / HID", "Audio",  "Logs / Sessions",
        "Motion / IMU", "Files",     "QR Generator", "System", "About",
    };
    constexpr size_t kToolsCount = sizeof(items) / sizeof(items[0]);
    drawHeader("Tools");
    normalizeListPosition(kToolsCount);
    drawHeaderPosition(listSelection + 1, kToolsCount);
    for (size_t row = 0; row < kVisibleRows && row + listOffset < kToolsCount;
         ++row) {
        drawListRow(row, items[row + listOffset],
                    row + listOffset == listSelection);
    }
    drawFooter("Enter: open   Backspace/Q: back");
}

void drawQrEntry() {
    drawHeader("QR Generator");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 34);
    display.print("Enter text, URL, or short note:");
    drawTextEntryRow(56, "> ", qrText);
    display.setCursor(8, 78);
    display.printf("%u / 100 characters", static_cast<unsigned>(qrText.length()));
    display.setCursor(8, 96);
    display.print("Generated entirely offline.");
    drawFooter("Enter: generate   Esc: back");
}

void drawQrDisplay() {
    drawHeader("QR Code");
    auto& display = M5Cardputer.Display;
    display.fillRect(0, 22, display.width(), display.height() - 37,
                     TFT_WHITE);
    display.qrcode(qrText, 74, 25, 92, 6);
    drawFooter("Enter: edit   Esc: back");
}

void drawLoRa(bool fullDraw = true) {
    const uint32_t signature =
        (static_cast<uint32_t>(loraService.isReady()) << 31) ^
        (static_cast<uint32_t>(loraLogger.isActive()) << 30) ^
        (static_cast<uint32_t>(loraService.profile()) << 28) ^
        loraService.packetCount() ^ (loraLogger.rowCount() << 8);
    static uint32_t lastSignature = UINT32_MAX;
    if (!fullDraw && signature == lastSignature) return;
    lastSignature = signature;
    beginContentUpdate("LoRa Receive", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(loraService.isReady() ? Branding::accent
                                               : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    if (loraService.isReady()) {
        display.printf("SX1262 ready %.3f MHz",
                       loraService.frequencyMhz());
    } else {
        display.printf("Radio init failed: %d", loraService.status());
    }
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 47);
    display.printf("Packets: %lu",
                   static_cast<unsigned long>(loraService.packetCount()));
    if (loraLogger.isActive()) {
        display.printf("  REC %lu",
                       static_cast<unsigned long>(loraLogger.rowCount()));
    }
    display.setCursor(8, 64);
    if (loraService.packetCount() > 0) {
        display.printf("RSSI %.1f dBm  SNR %.1f dB", loraService.lastRssi(),
                       loraService.lastSnr());
        display.setCursor(8, 82);
        const auto& decoded = loraService.lastDecoded();
        if (decoded.valid) {
            display.printf("Mesh %08lX  %s",
                           static_cast<unsigned long>(decoded.from),
                           MeshtasticDecoder::portName(decoded.port));
            display.setCursor(8, 99);
            display.print(decoded.summary.substring(0, 37));
        } else {
            display.print(loraService.lastPacket().substring(0, 36));
            display.setCursor(8, 99);
            display.setTextColor(Branding::muted, Branding::background);
            display.print("Encrypted / unsupported payload");
        }
    } else {
        display.setTextColor(Branding::muted, Branding::background);
        display.print(loraService.profileName());
        display.setCursor(8, 82);
        if (loraService.profile() ==
            LoRaService::Profile::MeshtasticEuLongFast) {
            display.print("BW250 SF11 CR4/5 sync 0x2B");
        } else {
            display.print("BW125 SF12 CR4/5 sync 0x34");
        }
    }
    if (fullDraw) drawFooter("R: restart   Tab: actions   Q: back");
}

void drawWifiSniffer(bool fullDraw = true) {
    beginContentUpdate("Wi-Fi Sniffer", fullDraw);
    auto& display = M5Cardputer.Display;
    display.setTextColor(wifiSnifferService.isActive() ? Branding::accent
                                                       : Branding::warning,
                         Branding::background);
    display.setCursor(8, 29);
    display.printf("RF %s  CH %-2u%s  %-9s",
                   wifiSnifferService.isActive() ? "ON" : "OFF",
                   wifiSnifferService.currentChannel(),
                   wifiSnifferService.channelLocked() ? "L" : "H",
                   wifiSnifferService.captureModeName());
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf(
        "Probes: %lu  Unique: %u",
        static_cast<unsigned long>(wifiSnifferService.probeCount()),
        static_cast<unsigned>(wifiSnifferService.uniqueDeviceCount()));
    const uint32_t dropped = wifiSnifferService.droppedProbeCount();
    if (dropped > 0) {
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(184, 46);
        display.printf("D:%lu", static_cast<unsigned long>(dropped));
    }
    if (wifiPassiveCaptureLogger.isActive()) {
        display.setTextColor(Branding::accent, Branding::background);
        display.setCursor(8, 58);
        display.printf("PCAP %lu frames  %llu KiB  drop %lu",
                       static_cast<unsigned long>(
                           wifiPassiveCaptureLogger.rowCount()),
                       wifiPassiveCaptureLogger.byteCount() / 1024ULL,
                       static_cast<unsigned long>(
                           wifiSnifferService.droppedRawFrameCount()));
    } else if (wifiPassiveCaptureLogger.rowCount() > 0) {
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 58);
        display.printf("PCAP SAVED  %lu frames  %llu KiB",
                       static_cast<unsigned long>(
                           wifiPassiveCaptureLogger.rowCount()),
                       wifiPassiveCaptureLogger.byteCount() / 1024ULL);
    }

    if (recentWifiProbes.empty()) {
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 72);
        display.print("Waiting for probe requests...");
    } else {
        constexpr int kLineHeight = 13;
        constexpr int kFirstLineY = 72;
        constexpr size_t kVisibleLines = 4;
        const size_t total = recentWifiProbes.size();
        const size_t shown = std::min(kVisibleLines, total);
        display.setTextColor(Branding::text, Branding::background);
        for (size_t row = 0; row < shown; ++row) {
            const WifiProbeRecord& probe = recentWifiProbes[total - 1 - row];
            String ssid = String(probe.ssid);
            if (ssid.isEmpty()) ssid = "<wildcard>";
            display.setCursor(8,
                              kFirstLineY + static_cast<int>(row) * kLineHeight);
            display.printf("%02X:%02X:%02X %-12s %d", probe.mac[3],
                           probe.mac[4], probe.mac[5],
                           ssid.substring(0, 12).c_str(), probe.rssi);
        }
    }
    if (fullDraw) drawFooter("R: restart   Tab: actions   Q: back");
}

String csvSafePayload(const String& payload) {
    String safe = "\"";
    safe.reserve(std::min(static_cast<size_t>(payload.length()), size_t{96}) +
                 4);
    const size_t limit =
        std::min(static_cast<size_t>(payload.length()), size_t{96});
    for (size_t index = 0; index < limit; ++index) {
        const unsigned char value = payload[index];
        if (value == '"') {
            safe += "\"\"";
        } else if (value >= 32 && value <= 126) {
            safe += static_cast<char>(value);
        } else {
            safe += '.';
        }
    }
    safe += '"';
    return safe;
}

const char* familiarFace() {
    const bool blink = (millis() / 2600U) % 9U == 0;
    if (blink) return "(-_-)";
    switch (cyberFamiliar.mood()) {
        case FamiliarMood::Curious: return "(o_o)?";
        case FamiliarMood::Excited: return "(^o^)";
        case FamiliarMood::Sleepy: return "(-.-)z";
        case FamiliarMood::Proud: return "(^_^)7";
        case FamiliarMood::Worried: return "(O_O)!";
        case FamiliarMood::Dizzy: return "(@_@)";
        default: return "(._.)";
    }
}

String familiarAgeText() {
    const uint32_t seconds = cyberFamiliar.ageSeconds();
    const uint32_t days = seconds / 86400U;
    const uint8_t hours = (seconds / 3600U) % 24U;
    char value[24];
    snprintf(value, sizeof(value), "%lud %uh",
             static_cast<unsigned long>(days), hours);
    return String(value);
}

bool looksLikeMac(const String& value) {
    if (value.length() != 17) return false;
    for (size_t index = 0; index < value.length(); ++index) {
        if (index % 3 == 2) {
            if (value[index] != ':') return false;
        } else if (!isxdigit(static_cast<unsigned char>(value[index]))) {
            return false;
        }
    }
    return true;
}

bool exportFamiliarRecord() {
    if (!sdAvailable) {
        familiarWorkflowStatus = "SD card unavailable";
        return false;
    }
    SD.mkdir("/ghostwire");
    SD.mkdir("/ghostwire/logs");
    String path;
    for (uint16_t index = 1; index < 10000; ++index) {
        char candidate[56];
        snprintf(candidate, sizeof(candidate),
                 "/ghostwire/logs/familiar_%04u.txt", index);
        if (!SD.exists(candidate)) {
            path = candidate;
            break;
        }
    }
    if (path.isEmpty()) return false;
    File file = SD.open(path, FILE_WRITE);
    if (!file) return false;
    file.printf("name=%s\nlevel=%u\nxp=%lu\nbond=%u\nage_seconds=%lu\n",
                cyberFamiliar.name().c_str(), cyberFamiliar.level(),
                static_cast<unsigned long>(cyberFamiliar.xp()),
                cyberFamiliar.bond(),
                static_cast<unsigned long>(cyberFamiliar.ageSeconds()));
    file.printf("evolution=%s\nmood=%s\nwifi_discoveries=%lu\n",
                cyberFamiliar.evolutionName(), cyberFamiliar.moodName(),
                static_cast<unsigned long>(
                    cyberFamiliar.wifiDiscoveries()));
    file.printf("ble_discoveries=%lu\ntools_known=%lu\n",
                static_cast<unsigned long>(
                    cyberFamiliar.bleDiscoveries()),
                static_cast<unsigned long>(cyberFamiliar.toolCount()));
    file.println("journal:");
    for (const auto& entry : cyberFamiliar.journal()) file.println(entry);
    file.close();
    familiarWorkflowStatus = "Exported " + path.substring(path.lastIndexOf('/') + 1);
    return true;
}

void importFamiliarCaptureLogs() {
    if (!sdAvailable) {
        familiarWorkflowStatus = "SD card unavailable";
        return;
    }
    File directory = SD.open("/ghostwire/logs");
    if (!directory || !directory.isDirectory()) {
        familiarWorkflowStatus = "No capture log directory";
        return;
    }
    uint32_t rows = 0;
    uint32_t imported = 0;
    File file;
    while (rows < 12000 && (file = directory.openNextFile())) {
        if (file.isDirectory()) {
            file.close();
            continue;
        }
        String name = file.name();
        name.toLowerCase();
        const bool isBle = name.indexOf("ble") >= 0;
        const bool isWifi = name.indexOf("wifi") >= 0 ||
                            name.indexOf("probe") >= 0;
        if ((!isBle && !isWifi) || !name.endsWith(".csv")) {
            file.close();
            continue;
        }
        while (file.available() && rows++ < 12000) {
            const String line = file.readStringUntil('\n');
            for (size_t start = 0; start + 17 <= line.length(); ++start) {
                const String candidate = line.substring(start, start + 17);
                if (!looksLikeMac(candidate)) continue;
                if (isBle) {
                    if (cyberFamiliar.observeBleIdentity(candidate)) ++imported;
                } else {
                    uint8_t mac[6];
                    for (size_t byte = 0; byte < 6; ++byte) {
                        mac[byte] = strtoul(
                            candidate.substring(byte * 3, byte * 3 + 2).c_str(),
                            nullptr, 16);
                    }
                    if (cyberFamiliar.observeWifiIdentity(mac)) ++imported;
                }
                break;
            }
        }
        file.close();
    }
    directory.close();
    familiarWorkflowStatus = "Imported " + String(imported) +
                             " new / " + String(rows) + " rows";
}

void drawCyberFamiliarResetConfirm() {
    drawHeader("Reset Cyber Familiar");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 34);
    display.print("This clears progression, bond,");
    display.setCursor(8, 50);
    display.print("discoveries, journal and tools.");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 76);
    display.print("Name and idle preference remain.");
    drawFooter("Enter: reset   Esc: cancel");
}

void drawCyberFamiliar(bool fullDraw = true) {
    beginContentUpdate(cyberFamiliar.name().c_str(), fullDraw);
    auto& display = M5Cardputer.Display;
    if (familiarPage == 0) {
        display.setTextSize(3);
        display.setTextColor(Branding::accent, Branding::background);
        display.setCursor(57, 30);
        display.print(familiarFace());
        display.setTextSize(1);
        display.setTextColor(Branding::text, Branding::background);
        display.setCursor(8, 70);
        display.printf("Lv %u  %s  Bond %u",
                       static_cast<unsigned>(cyberFamiliar.level()),
                       cyberFamiliar.evolutionName(),
                       static_cast<unsigned>(cyberFamiliar.bond()));
        const uint32_t levelStart =
            static_cast<uint32_t>(cyberFamiliar.level() - 1U) * 100U;
        const uint32_t progress = cyberFamiliar.xp() - levelStart;
        display.drawRect(8, 84, 224, 8, Branding::muted);
        display.fillRect(10, 86, std::min<uint32_t>(220, progress * 220 / 100),
                         4, Branding::accent);
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 98);
        display.print((familiarWorkflowStatus.isEmpty()
                           ? cyberFamiliar.lastMessage()
                           : familiarWorkflowStatus).substring(0, 37));
    } else if (familiarPage == 1) {
        display.setTextColor(Branding::text, Branding::background);
        display.setCursor(8, 30);
        display.printf("Mood:        %s", cyberFamiliar.moodName());
        display.setCursor(8, 47);
        display.printf("Age:         %s", familiarAgeText().c_str());
        display.setCursor(8, 64);
        display.printf("Wi-Fi seen:  %lu", static_cast<unsigned long>(
                                              cyberFamiliar.wifiDiscoveries()));
        display.setCursor(8, 81);
        display.printf("BLE seen:    %lu", static_cast<unsigned long>(
                                              cyberFamiliar.bleDiscoveries()));
        display.setCursor(8, 98);
        display.printf("Tools known: %lu", static_cast<unsigned long>(
                                              cyberFamiliar.toolCount()));
    } else {
        const auto& journal = cyberFamiliar.journal();
        display.setTextColor(Branding::text, Branding::background);
        const size_t shown = std::min<size_t>(6, journal.size());
        const size_t start = journal.size() - shown;
        for (size_t row = 0; row < shown; ++row) {
            display.setCursor(6, 27 + static_cast<int>(row) * 15);
            display.print(("> " + journal[start + row]).substring(0, 39));
        }
    }
    if (fullDraw) {
        drawFooter("Up/Down: pages   Tab: menu");
    }
}

void drawCyberFamiliarIdle() {
    auto& display = M5Cardputer.Display;
    display.fillRect(0, 0, display.width(), display.height(),
                     Branding::background);
    display.setTextSize(3);
    display.setTextColor(Branding::accent, Branding::background);
    display.setCursor(57, 31);
    display.print(familiarFace());
    display.setTextSize(1);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 82);
    display.printf("%s  Lv%u  %s", cyberFamiliar.name().c_str(),
                   static_cast<unsigned>(cyberFamiliar.level()),
                   cyberFamiliar.moodName());
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 101);
    display.print(cyberFamiliar.lastMessage().substring(0, 37));
    display.setCursor(8, 119);
    display.printf("WiFi:%s  Battery:%u%%",
                   WiFi.status() == WL_CONNECTED ? "UP" : "--",
                   batteryPercentage());
}

void drawSettings() {
    drawHeader("Settings");
    static const char* const items[] = {
        "Display & Audio", "Boot Experience", "Connectivity",
        "Restore Defaults",
    };
    constexpr size_t count = sizeof(items) / sizeof(items[0]);
    normalizeListPosition(count);
    for (size_t row = 0; row < count; ++row) {
        drawListRow(row, items[row], row == listSelection);
    }
    drawFooter("Enter: open   Backspace/Q: back");
}

void drawSettingsDisplay() {
    drawHeader("Settings: Display");
    constexpr size_t count = 5;
    normalizeListPosition(count);
    const String timeout =
        screenTimeoutSeconds == 0 ? "Off"
                                  : String(screenTimeoutSeconds) + "s";
    const char* const labels[count] = {
        "Speaker volume", "Screen brightness", "Screen timeout",
        "Cyberdeck idle", "Theme",
    };
    const String values[count] = {
        String((speakerVolume * 100U) / 255U) + "%",
        String((screenBrightness * 100U) / 255U) + "%",
        timeout, cyberdeckIdleEnabled ? "On" : "Off",
        Branding::kThemes[themeIndex].name,
    };
    for (size_t row = 0; row < count; ++row) {
        drawListRow(row, labels[row], row == listSelection, values[row]);
    }
    drawFooter("Up/Down select   -/=: adjust   Q: back");
}

void drawSettingsBoot() {
    drawHeader("Settings: Boot");
    constexpr size_t count = 6;
    normalizeListPosition(count);
    const char* const labels[count] = {
        "Boot sound", "Sound style", "Animation style", "Fast boot",
        "Preview sound", "Preview animation",
    };
    const String values[count] = {
        bootSoundEnabled ? "On" : "Off",
        kBootSoundNames[bootSoundIndex],
        kBootAnimationNames[bootAnimationIndex],
        fastBootEnabled ? "On" : "Off", "Enter", "Enter",
    };
    for (size_t row = 0; row < count; ++row) {
        drawListRow(row, labels[row], row == listSelection, values[row]);
    }
    drawFooter("Enter: change / preview   Q: back");
}

void drawSettingsConnectivity() {
    drawHeader("Settings: Connectivity");
    constexpr size_t count = 2;
    normalizeListPosition(count);
    drawListRow(0, "Save Wi-Fi login", listSelection == 0,
                saveWifiCredentials ? "On" : "Off");
    drawListRow(1, "Auto-connect Wi-Fi", listSelection == 1,
                autoConnectWifi ? "On" : "Off");
    drawFooter("-/= or Enter: toggle   Q: back");
}

void drawSettingsReset() {
    drawHeader("Restore Settings?");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 42);
    display.print("Reset all preferences?");
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 64);
    display.print("This does not erase SD files.");
    drawFooter("Enter: restore   Backspace/Q: cancel");
}

void drawPlaceholder() {
    drawHeader(placeholderTitle.c_str());
    M5Cardputer.Display.setTextColor(Branding::muted,
                                    Branding::background);
    M5Cardputer.Display.setCursor(8, 42);
    M5Cardputer.Display.print("Module slots ready.");
    M5Cardputer.Display.setCursor(8, 59);
    M5Cardputer.Display.print("Tools arrive in later builds.");
    drawFooter("Esc: back");
}

void drawAbout() {
    drawHeader("About");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 34);
    display.printf("%s %s", Branding::productName, Branding::version);
    display.setTextColor(Branding::accent, Branding::background);
    display.setCursor(8, 54);
    display.printf("by %s", Branding::creatorName);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 76);
    display.print("Authorized security field toolkit");
    display.setCursor(8, 92);
    display.print("Build: Cardputer-Adv / ESP32-S3");
    drawFooter("Esc: back");
}

const char* imuTypeName() {
    switch (M5.Imu.getType()) {
        case m5::imu_bmi270: return "BMI270";
        case m5::imu_mpu6050: return "MPU6050";
        case m5::imu_mpu6886: return "MPU6886";
        case m5::imu_mpu9250: return "MPU9250";
        case m5::imu_sh200q: return "SH200Q";
        case m5::imu_none: return "not detected";
        default: return "unknown";
    }
}

void drawImu(bool fullDraw = true) {
    beginContentUpdate("Motion / IMU", fullDraw);
    auto& display = M5Cardputer.Display;
    if (!imuAvailable) {
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(8, 38);
        display.printf("Sensor: %s", imuTypeName());
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 58);
        display.print("IMU is unavailable.");
        drawFooter("R: retry   Backspace/Q: back");
        return;
    }

    const float gx = imuData.gyro.x - gyroOffsetX;
    const float gy = imuData.gyro.y - gyroOffsetY;
    const float gz = imuData.gyro.z - gyroOffsetZ;
    const float accelMagnitude =
        sqrtf(imuData.accel.x * imuData.accel.x +
              imuData.accel.y * imuData.accel.y +
              imuData.accel.z * imuData.accel.z);
    const float gyroMagnitude = sqrtf(gx * gx + gy * gy + gz * gz);
    // The sensor's native XY frame is rotated relative to the landscape
    // screen/keyboard. Rotate it 90 degrees clockwise for human-facing
    // pitch and roll while retaining native axes in the diagnostic rows.
    const float screenAccelX = imuData.accel.y;
    const float screenAccelY = -imuData.accel.x;
    const float roll =
        atan2f(screenAccelY, imuData.accel.z) * 180.0F / PI;
    const float pitch =
        atan2f(screenAccelX,
               sqrtf(screenAccelY * screenAccelY +
                     imuData.accel.z * imuData.accel.z)) *
        180.0F / PI;
    const bool stationary =
        accelMagnitude > 0.85F && accelMagnitude < 1.15F &&
        gyroMagnitude < 2.0F;

    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 29);
    display.printf("%s  %s%s", imuTypeName(),
                   imuCalibrating ? "CALIBRATING" :
                   (stationary ? "STILL" : "MOVING"),
                   imuLogger.isActive() ? "  REC" : "");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 46);
    display.printf("Accel g  X%6.2f Y%6.2f Z%6.2f",
                   imuData.accel.x, imuData.accel.y, imuData.accel.z);
    display.setCursor(8, 63);
    display.printf("Gyro d/s X%6.1f Y%6.1f Z%6.1f", gx, gy, gz);
    display.setCursor(8, 80);
    display.printf("Pitch %6.1f   Roll %6.1f", pitch, roll);
    display.setCursor(8, 97);
    if (imuCalibrating) {
        display.printf("Keep still: %u%%", imuCalibrationSamples);
    } else if (imuLogger.isActive()) {
        String logName = imuLogger.path();
        const int slash = logName.lastIndexOf('/');
        if (slash >= 0) logName = logName.substring(slash + 1);
        display.printf("%s  %lu rows", logName.c_str(),
                       static_cast<unsigned long>(imuLogger.rowCount()));
    } else {
        display.printf("|a| %.2fg   |w| %.1f d/s", accelMagnitude,
                       gyroMagnitude);
    }
    drawFooter("Tab: actions   Q: back");
}

void beginImuCalibration() {
    if (!imuAvailable) return;
    imuCalibrating = true;
    imuCalibrationSamples = 0;
    gyroCalibrationSumX = 0.0F;
    gyroCalibrationSumY = 0.0F;
    gyroCalibrationSumZ = 0.0F;
}

void updateImu() {
    if (!imuAvailable) return;
    if (M5.Imu.update() == m5::IMU_Class::sensor_mask_none) return;
    imuData = M5.Imu.getImuData();
    if (imuLogger.isActive() && millis() - lastImuLog >= 100) {
        lastImuLog = millis();
        char row[224];
        snprintf(row, sizeof(row),
                 "%lu,%s,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f",
                 static_cast<unsigned long>(millis()), utcTimestamp().c_str(),
                 imuData.accel.x, imuData.accel.y, imuData.accel.z,
                 imuData.gyro.x - gyroOffsetX, imuData.gyro.y - gyroOffsetY,
                 imuData.gyro.z - gyroOffsetZ);
        imuLogger.append(row);
    }
    if (imuCalibrating) {
        gyroCalibrationSumX += imuData.gyro.x;
        gyroCalibrationSumY += imuData.gyro.y;
        gyroCalibrationSumZ += imuData.gyro.z;
        ++imuCalibrationSamples;
        if (imuCalibrationSamples >= 100) {
            gyroOffsetX = gyroCalibrationSumX / imuCalibrationSamples;
            gyroOffsetY = gyroCalibrationSumY / imuCalibrationSamples;
            gyroOffsetZ = gyroCalibrationSumZ / imuCalibrationSamples;
            imuCalibrating = false;
        }
    }
}

void drawCurrentScreen() {
    switch (currentScreen) {
        case Screen::MainMenu: drawMainMenu(); break;
        case Screen::WifiMenu: drawWifiMenu(); break;
        case Screen::WifiRecon: drawWifiRecon(); break;
        case Screen::WifiChannelAnalyzer: drawWifiChannelAnalyzer(); break;
        case Screen::WifiDetail: drawWifiDetail(); break;
        case Screen::WifiDeauthConfirm: drawWifiDeauthConfirm(); break;
        case Screen::WifiHandshakeCapture: drawWifiHandshakeCapture(); break;
        case Screen::WifiConnectSelect: drawWifiConnectSelect(); break;
        case Screen::WifiConnectPassword: drawWifiConnectPassword(); break;
        case Screen::WifiConnectStatus: drawWifiConnectStatus(); break;
        case Screen::BleMenu: drawBleMenu(); break;
        case Screen::DevicesMenu: drawDevicesMenu(); break;
        case Screen::AiChat: drawAiChat(); break;
        case Screen::CyberFamiliar: drawCyberFamiliar(); break;
        case Screen::CyberFamiliarResetConfirm:
            drawCyberFamiliarResetConfirm();
            break;
        case Screen::BleDiscovery: drawBleDiscovery(); break;
        case Screen::BleDetail: drawBleDetail(); break;
        case Screen::BleKeyboard: drawBleKeyboard(); break;
        case Screen::Biscuit: drawBiscuit(); break;
        case Screen::BiscuitTools: drawBiscuitTools(); break;
        case Screen::BiscuitResult: drawBiscuitResult(); break;
        case Screen::BiscuitWardrive: drawBiscuitWardrive(); break;
        case Screen::BleSpamSelect: drawBleSpamSelect(); break;
        case Screen::BleSpam: drawBleSpam(); break;
        case Screen::RfidMenu: drawRfidMenu(); break;
        case Screen::Chameleon: drawChameleon(); break;
        case Screen::ChameleonEmulateConfirm:
            drawChameleonEmulateConfirm();
            break;
        case Screen::ToolsMenu: drawToolsMenu(); break;
        case Screen::Infrared: drawInfrared(); break;
        case Screen::UsbHid: drawUsbHid(); break;
        case Screen::UsbHidConfirm: drawUsbHidConfirmation(); break;
        case Screen::DuckyScripts: drawDuckyScripts(); break;
        case Screen::DuckyConfirm: drawDuckyConfirm(); break;
        case Screen::DuckyResult: drawDuckyResult(); break;
        case Screen::Audio: drawAudio(); break;
        case Screen::AudioMic: drawMicrophone(); break;
        case Screen::AudioFiles: drawAudioFiles(); break;
        case Screen::AudioPlaying: break;
        case Screen::QrEntry: drawQrEntry(); break;
        case Screen::QrDisplay: drawQrDisplay(); break;
        case Screen::Files: drawFiles(); break;
        case Screen::FileDetail: drawFileDetail(); break;
        case Screen::TextPreview: drawTextPreview(); break;
        case Screen::LogSessions: drawLogSessions(); break;
        case Screen::LogDetail: drawLogDetail(); break;
        case Screen::LogDeleteConfirm: drawLogDeleteConfirm(); break;
        case Screen::System: drawSystem(); break;
        case Screen::TimeStatus: drawTimeStatus(); break;
        case Screen::GpsMenu: drawGpsMenu(); break;
        case Screen::MeshMenu: drawMeshMenu(); break;
        case Screen::WarDrive: drawWarDrive(); break;
        case Screen::NetworkMenu: drawNetworkMenu(); break;
        case Screen::NetworkDashboard: drawNetworkDashboard(); break;
        case Screen::NetworkHostScan: drawNetworkHostScan(); break;
        case Screen::NetworkPortScan: drawNetworkPortScan(); break;
        case Screen::TelnetConnect: drawTelnetConnect(); break;
        case Screen::TelnetSession: drawTelnetSession(); break;
        case Screen::SshConnect: drawSshConnect(); break;
        case Screen::SshPassword: drawSshPassword(); break;
        case Screen::SshSession: drawSshSession(); break;
        case Screen::Gnss: drawGnss(); break;
        case Screen::LoRa: drawLoRa(); break;
        case Screen::WifiSniffer: drawWifiSniffer(); break;
        case Screen::Imu: drawImu(); break;
        case Screen::Settings: drawSettings(); break;
        case Screen::SettingsDisplay: drawSettingsDisplay(); break;
        case Screen::SettingsBoot: drawSettingsBoot(); break;
        case Screen::SettingsConnectivity: drawSettingsConnectivity(); break;
        case Screen::SettingsReset: drawSettingsReset(); break;
        case Screen::Placeholder: drawPlaceholder(); break;
        case Screen::About: drawAbout(); break;
    }
}

void drawBootConsole(unsigned long elapsed,
                     const std::vector<SystemDiagnostic>& diagnostics,
                     M5Canvas& console, size_t& drawnEntries,
                     int& drawnSpinner) {
    auto& display = M5Cardputer.Display;
    static constexpr char spinner[] = {'|', '/', '-', '\\'};
    static constexpr size_t entryIndexes[] = {
        0, 18, 2, 19, 9, 4, 7, 3, 12, 13, 14, 15, 16, 17,
    };
    constexpr size_t entryCount =
        sizeof(entryIndexes) / sizeof(entryIndexes[0]);
    if (drawnEntries == 0 && drawnSpinner < 0) {
        display.setTextSize(1);
        display.setTextColor(Branding::accent, Branding::background);
        display.setCursor(5, 5);
        display.print(Branding::productName);
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(171, 5);
        display.printf("v%s", Branding::version);
        display.setTextColor(Branding::text, Branding::background);
        display.setCursor(55, 35);
        display.print("SYSTEM INITIALIZATION");
        display.drawFastHLine(0, 68, display.width(), Branding::accent);
    }
    const int spinnerFrame = (elapsed / 120) % 4;
    if (spinnerFrame != drawnSpinner) {
        display.fillRect(114, 49, 12, 10, Branding::background);
        display.setTextColor(Branding::accent, Branding::background);
        display.setCursor(116, 51);
        display.print(spinner[spinnerFrame]);
        drawnSpinner = spinnerFrame;
    }

    const size_t visibleEntries =
        std::min<size_t>(entryCount, 1 + elapsed / 280);
    if (visibleEntries == drawnEntries) return;
    drawnEntries = visibleEntries;
    console.fillSprite(Branding::background);
    console.setTextSize(1);
    const size_t first = visibleEntries > 5 ? visibleEntries - 5 : 0;
    for (size_t index = first; index < visibleEntries; ++index) {
        const auto& entry = diagnostics[entryIndexes[index]];
        const int y = 3 + static_cast<int>(index - first) * 11;
        console.setTextColor(Branding::muted, Branding::background);
        console.setCursor(3, y);
        console.printf("> %-11s", entry.label.c_str());
        console.setTextColor(entry.state == DiagnosticState::Ready
                                 ? Branding::accent
                                 : entry.state == DiagnosticState::Warning
                                       ? Branding::warning
                                       : Branding::text,
                             Branding::background);
        console.setCursor(151, y);
        console.print(entry.value.substring(0, 14));
    }
    console.pushSprite(0, 70);
    const int progress =
        static_cast<int>(visibleEntries * display.width() / entryCount);
    display.fillRect(0, 132, progress, 3, Branding::accent);
}

void playBootChimeTones() {
    M5Cardputer.Speaker.setVolume(speakerVolume);
    const uint16_t* notes = nullptr;
    const uint16_t* durations = nullptr;
    size_t count = 0;
    static constexpr uint16_t classicNotes[] = {988, 1319};
    static constexpr uint16_t classicDurations[] = {65, 90};
    static constexpr uint16_t heroNotes[] = {740, 988, 740, 1480};
    static constexpr uint16_t heroDurations[] = {70, 70, 70, 150};
    static constexpr uint16_t arcadeNotes[] = {523, 659, 784, 1047};
    static constexpr uint16_t arcadeDurations[] = {55, 55, 55, 130};
    static constexpr uint16_t starshipNotes[] = {1320, 990, 660, 990};
    static constexpr uint16_t starshipDurations[] = {45, 60, 90, 120};
    static constexpr uint16_t mysticNotes[] = {880, 1175, 1568};
    static constexpr uint16_t mysticDurations[] = {90, 110, 180};
    switch (bootSoundIndex) {
        case 1:
            notes = heroNotes; durations = heroDurations;
            count = sizeof(heroNotes) / sizeof(heroNotes[0]); break;
        case 2:
            notes = arcadeNotes; durations = arcadeDurations;
            count = sizeof(arcadeNotes) / sizeof(arcadeNotes[0]); break;
        case 3:
            notes = starshipNotes; durations = starshipDurations;
            count = sizeof(starshipNotes) / sizeof(starshipNotes[0]); break;
        case 4:
            notes = mysticNotes; durations = mysticDurations;
            count = sizeof(mysticNotes) / sizeof(mysticNotes[0]); break;
        default:
            notes = classicNotes; durations = classicDurations;
            count = sizeof(classicNotes) / sizeof(classicNotes[0]); break;
    }
    for (size_t index = 0; index < count; ++index) {
        M5Cardputer.Speaker.tone(notes[index], durations[index]);
        delay(durations[index] + 35);
    }
}

void playBootSound() {
    playBootChimeTones();
}

void previewBootSound() {
    audioService.stopPlayback();
    M5Cardputer.Speaker.begin();
    playBootSound();
    recoverKeyboardAfterBlockingOperation();
}

void showBootSummary(const std::vector<SystemDiagnostic>& diagnostics) {
    size_t ready = 0;
    size_t warnings = 0;
    size_t deferred = 0;
    for (size_t index : {12U, 13U, 14U, 15U, 16U, 17U}) {
        if (diagnostics[index].state == DiagnosticState::Ready) {
            ++ready;
        } else if (diagnostics[index].state == DiagnosticState::Warning) {
            ++warnings;
        } else {
            ++deferred;
        }
    }
    auto& display = M5Cardputer.Display;
    display.fillScreen(Branding::background);
    display.drawRoundRect(18, 25, display.width() - 36, 76, 5,
                          warnings ? Branding::warning : Branding::accent);
    display.setTextSize(2);
    display.setTextColor(warnings ? Branding::warning : Branding::accent,
                         Branding::background);
    display.setCursor(warnings ? 43 : 50, 39);
    display.print(warnings ? "CHECK SYSTEM" : "SYSTEM READY");
    display.setTextSize(1);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(47, 72);
    display.printf("%u READY  %u DEFERRED  %u WARN",
                   static_cast<unsigned>(ready),
                   static_cast<unsigned>(deferred),
                   static_cast<unsigned>(warnings));
    if (bootSoundEnabled) {
        // Speaker.begin() issues a single, non-retried I2C write to enable the
        // codec, and M5Unified never retries it internally. On a cold boot
        // the codec can take several seconds longer than this boot screen's
        // budget to become I2C-ready (a warm reset, where the rail never
        // dropped, succeeds immediately). Rather than block the boot
        // sequence for that long, try once here and, if it's not ready yet,
        // hand off to loop() to keep retrying in the background and play the
        // chime whenever the speaker actually comes up.
        if (M5Cardputer.Speaker.begin()) {
            playBootSound();
            bootChimeStatus = "Played immediately";
        } else {
            bootChimePending = true;
            bootChimeDeadlineMs = millis() + 20000;
            nextBootChimeAttemptMs = millis() + 250;
            bootChimeStatus = "Pending (deferred)";
            if (!fastBootEnabled) delay(1000);
        }
    } else {
        if (!fastBootEnabled) delay(1000);
    }
}

void drawCornerBrackets(int x, int y, int w, int h, int armLength,
                        uint16_t color) {
    auto& display = M5Cardputer.Display;
    display.drawFastHLine(x, y, armLength, color);
    display.drawFastVLine(x, y, armLength, color);
    display.drawFastHLine(x + w - armLength, y, armLength, color);
    display.drawFastVLine(x + w - 1, y, armLength, color);
    display.drawFastHLine(x, y + h - 1, armLength, color);
    display.drawFastVLine(x, y + h - armLength, armLength, color);
    display.drawFastHLine(x + w - armLength, y + h - 1, armLength, color);
    display.drawFastVLine(x + w - 1, y + h - armLength, armLength, color);
}

bool bootTitleSkipRequested() {
    M5Cardputer.update();
    return M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed();
}

void showBootTitle() {
    auto& display = M5Cardputer.Display;
    display.fillScreen(Branding::background);

    constexpr int kBoxX = 15;
    constexpr int kBoxY = 24;
    const int boxW = display.width() - 30;
    constexpr int kBoxH = 76;
    constexpr int kArmLength = 14;
    drawCornerBrackets(kBoxX, kBoxY, boxW, kBoxH, kArmLength, Branding::accent);
    display.drawFastHLine(kBoxX + kArmLength, kBoxY, boxW - kArmLength * 2,
                          Branding::panel);
    display.drawFastHLine(kBoxX + kArmLength, kBoxY + kBoxH - 1,
                          boxW - kArmLength * 2, Branding::panel);

    // Decrypt-style reveal: the product name resolves out of scrambled
    // characters left-to-right, like a terminal brute-forcing a cipher.
    // Skipped entirely under fast boot.
    display.setTextSize(2);
    const int titleY = 39;
    const int titleWidth = display.textWidth(Branding::productName);
    const int titleX = (display.width() - titleWidth) / 2;
    const size_t nameLength = strlen(Branding::productName);
    bool skipped = false;
    if (!fastBootEnabled) {
        static constexpr char kScrambleChars[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#$%&@?";
        constexpr size_t kScrambleCharCount = sizeof(kScrambleChars) - 1;
        constexpr unsigned long kRevealDurationMs = 650;
        const unsigned long revealStarted = millis();
        size_t locked = 0;
        while (locked < nameLength && !skipped) {
            const unsigned long elapsed = millis() - revealStarted;
            locked = std::min(nameLength,
                              static_cast<size_t>((elapsed * nameLength) /
                                                  kRevealDurationMs));
            display.fillRect(titleX - 2, titleY - 2, titleWidth + 4, 18,
                             Branding::background);
            display.setCursor(titleX, titleY);
            display.setTextColor(Branding::accent, Branding::background);
            for (size_t index = 0; index < nameLength; ++index) {
                const char actual = Branding::productName[index];
                display.print(index < locked || actual == ' '
                                 ? actual
                                 : kScrambleChars[random(kScrambleCharCount)]);
            }
            skipped = bootTitleSkipRequested();
            delay(30);
        }
    }
    display.fillRect(titleX - 2, titleY - 2, titleWidth + 4, 18,
                     Branding::background);
    display.setCursor(titleX, titleY);
    display.setTextColor(Branding::accent, Branding::background);
    display.print(Branding::productName);

    display.setTextSize(1);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(76, 67);
    display.printf("by %s", Branding::creatorName);
    display.setTextColor(Branding::muted, Branding::background);
    const int versionX = 91;
    const int versionY = 108;
    display.setCursor(versionX, versionY);
    display.printf("v%s", Branding::version);

    if (fastBootEnabled) {
        delay(150);
        return;
    }

    // Blinking terminal cursor for the remaining hold time.
    const int cursorX = versionX + display.textWidth(String("v") +
                                                      Branding::version) +
                        2;
    const unsigned long holdStarted = millis();
    bool cursorOn = true;
    while (!skipped && millis() - holdStarted < 2200) {
        display.fillRect(cursorX, versionY, 6, 8,
                         cursorOn ? Branding::muted : Branding::background);
        cursorOn = !cursorOn;
        skipped = bootTitleSkipRequested();
        delay(300);
    }
}

void showRadarBoot(unsigned long durationMs = 1500) {
    auto& display = M5Cardputer.Display;
    display.fillScreen(Branding::background);
    const int cx = display.width() / 2;
    const int cy = display.height() / 2;
    const int radius = 48;
    display.drawCircle(cx, cy, radius, Branding::muted);
    display.drawCircle(cx, cy, 31, Branding::panel);
    display.drawCircle(cx, cy, 15, Branding::panel);
    display.drawFastHLine(cx - radius, cy, radius * 2, Branding::panel);
    display.drawFastVLine(cx, cy - radius, radius * 2, Branding::panel);
    const unsigned long started = millis();
    int previousX = cx + radius;
    int previousY = cy;
    while (millis() - started < durationMs) {
        const float angle = static_cast<float>(millis() - started) * 0.012F;
        const int x = cx + static_cast<int>(cosf(angle) * radius);
        const int y = cy + static_cast<int>(sinf(angle) * radius);
        display.drawLine(cx, cy, previousX, previousY, Branding::background);
        display.drawLine(cx, cy, x, y, Branding::accent);
        display.fillCircle(cx + 24, cy - 18, 2, Branding::warning);
        previousX = x;
        previousY = y;
        if (bootTitleSkipRequested()) break;
        delay(28);
    }
}

void showMinimalBoot(unsigned long durationMs = 700) {
    auto& display = M5Cardputer.Display;
    display.fillScreen(Branding::background);
    display.setTextSize(2);
    display.setTextColor(Branding::accent, Branding::background);
    const int x = (display.width() - display.textWidth(Branding::productName)) / 2;
    display.setCursor(x, 52);
    display.print(Branding::productName);
    display.setTextSize(1);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(92, 76);
    display.printf("v%s", Branding::version);
    const unsigned long started = millis();
    while (millis() - started < durationMs) {
        if (bootTitleSkipRequested()) break;
        delay(25);
    }
}

void showNeonBreachBoot(unsigned long durationMs = 1500) {
    auto& display = M5Cardputer.Display;
    display.fillScreen(0x0000);
    const unsigned long started = millis();
    while (millis() - started < durationMs) {
        const int progress = static_cast<int>(
            std::min<unsigned long>(100, (millis() - started) * 100 /
                                             std::max(1UL, durationMs)));
        for (int line = 0; line < 5; ++line) {
            const int y = random(8, display.height() - 8);
            display.drawFastHLine(random(0, 70), y, random(30, 170),
                                  line & 1 ? 0xF81F : 0x07FF);
        }
        display.fillRect(18, 37, display.width() - 36, 54, 0x0000);
        display.drawRect(18, 37, display.width() - 36, 54, 0xF81F);
        display.setTextSize(2);
        display.setTextColor(0x07FF, 0x0000);
        display.setCursor(38 + random(-2, 3), 48);
        display.print("NEON BREACH");
        display.setTextSize(1);
        display.setTextColor(0xFFFF, 0x0000);
        display.setCursor(67, 74);
        display.printf("LINK %03d%%", progress);
        display.fillRect(24, 96, (display.width() - 48) * progress / 100, 3,
                         0x07FF);
        if (bootTitleSkipRequested()) break;
        delay(65);
    }
}

void showHackerBoot(unsigned long durationMs = 1600) {
    auto& display = M5Cardputer.Display;
    display.fillScreen(0x0000);
    display.setTextSize(1);
    static const char* const lines[] = {
        "$ mount /dev/ghost", "[ok] entropy seeded", "[ok] radio isolated",
        "[ok] keys verified", "root@ghostwire:~#", "ACCESS GRANTED",
    };
    const unsigned long started = millis();
    size_t shown = 0;
    while (millis() - started < durationMs) {
        const size_t target = std::min<size_t>(
            sizeof(lines) / sizeof(lines[0]), 1 + (millis() - started) / 210);
        while (shown < target) {
            display.setTextColor(shown == 5 ? Branding::warning : 0x07E0,
                                 0x0000);
            display.setCursor(7, 10 + static_cast<int>(shown) * 18);
            display.print(lines[shown]);
            ++shown;
        }
        display.fillRect(7, 120, 7, 8,
                         ((millis() / 180) & 1) ? 0x07E0 : 0x0000);
        if (bootTitleSkipRequested()) break;
        delay(35);
    }
}

void showSillyBoot(unsigned long durationMs = 1500) {
    auto& display = M5Cardputer.Display;
    display.fillScreen(Branding::background);
    const unsigned long started = millis();
    while (millis() - started < durationMs) {
        const int frame = static_cast<int>((millis() - started) / 55);
        const int x = 18 + (frame * 7) % (display.width() - 52);
        const int y = 54 + static_cast<int>(sinf(frame * 0.55F) * 22);
        display.fillRect(0, 24, display.width(), 88, Branding::background);
        display.fillCircle(x + 14, y, 13, 0xFFE0);
        display.fillCircle(x + 10, y - 4, 2, 0x0000);
        display.fillCircle(x + 18, y - 4, 2, 0x0000);
        display.fillTriangle(x + 27, y, x + 36, y + 4, x + 27, y + 8,
                             0xFD20);
        display.setTextSize(1);
        display.setTextColor(Branding::accent, Branding::background);
        display.setCursor(67, 12);
        display.print("SERIOUS BOOTING...");
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(65, 118);
        display.print("please hold your ducks");
        if (bootTitleSkipRequested()) break;
        delay(55);
    }
}

void showSynthwaveBoot(unsigned long durationMs = 1500) {
    auto& display = M5Cardputer.Display;
    display.fillScreen(0x0008);
    const int horizon = 72;
    display.fillCircle(display.width() / 2, horizon - 12, 24, 0xFBE0);
    display.fillRect(0, horizon, display.width(), display.height() - horizon,
                     0x0008);
    for (int y = horizon; y < display.height(); y += 11) {
        display.drawFastHLine(0, y, display.width(), 0xF81F);
    }
    for (int x = -120; x <= 360; x += 24) {
        display.drawLine(display.width() / 2, horizon, x, display.height() - 1,
                         0x07FF);
    }
    display.setTextSize(2);
    display.setTextColor(0x07FF, 0x0008);
    display.setCursor(57, 22);
    display.print("GHOSTWIRE");
    const unsigned long started = millis();
    while (millis() - started < durationMs) {
        const int y = horizon + static_cast<int>((millis() - started) / 12) %
                                    (display.height() - horizon);
        display.drawFastHLine(0, y, display.width(), 0xFFFF);
        if (bootTitleSkipRequested()) break;
        delay(35);
    }
}

void showSelectedStyledBoot(unsigned long durationMs) {
    switch (bootAnimationIndex) {
        case 4: showNeonBreachBoot(durationMs); break;
        case 5: showHackerBoot(durationMs); break;
        case 6: showSillyBoot(durationMs); break;
        case 7: showSynthwaveBoot(durationMs); break;
        default: break;
    }
}

void previewBootAnimation() {
    if (bootAnimationIndex == 2) {
        showRadarBoot(1200);
    } else if (bootAnimationIndex == 3) {
        showMinimalBoot(900);
    } else if (bootAnimationIndex >= 4) {
        showSelectedStyledBoot(1300);
    } else {
        // A compact preview of the console/cipher styles without replaying
        // the full multi-second boot diagnostics sequence.
        auto& display = M5Cardputer.Display;
        display.fillScreen(Branding::background);
        display.setTextSize(1);
        display.setTextColor(Branding::accent, Branding::background);
        display.setCursor(8, 18);
        display.print(bootAnimationIndex == 0 ? "SYSTEM INITIALIZATION"
                                              : "DECRYPTING GHOSTWIRE");
        for (int step = 0; step < 6; ++step) {
            display.fillRect(8, 42 + step * 12, 224, 9,
                             Branding::background);
            display.setCursor(8, 42 + step * 12);
            display.setTextColor(step < 4 ? Branding::text : Branding::muted,
                                 Branding::background);
            display.printf("> %-12s %s", step % 2 ? "RADIO" : "SUBSYSTEM",
                           step < 4 ? "READY" : "WAIT");
            delay(100);
        }
        delay(350);
    }
    recoverKeyboardAfterBlockingOperation();
}

void showBootSequence() {
    M5Cardputer.Display.fillScreen(Branding::background);
    const std::vector<SystemDiagnostic> diagnostics = systemDiagnostics();
    if (!fastBootEnabled && bootAnimationIndex == 0) {
        M5Canvas console(&M5Cardputer.Display);
        if (!console.createSprite(M5Cardputer.Display.width(), 61)) {
            showBootSummary(diagnostics);
            showBootTitle();
            return;
        }
        size_t drawnEntries = 0;
        int drawnSpinner = -1;
        const unsigned long started = millis();
        while (millis() - started < 3920) {
            const unsigned long elapsed = millis() - started;
            drawBootConsole(elapsed, diagnostics, console, drawnEntries,
                            drawnSpinner);
            M5Cardputer.update();
            if (M5Cardputer.Keyboard.isChange() &&
                M5Cardputer.Keyboard.isPressed()) {
                break;
            }
            delay(40);
        }
        console.deleteSprite();
    }
    if (!fastBootEnabled && bootAnimationIndex == 2) {
        showRadarBoot();
    }
    if (!fastBootEnabled && bootAnimationIndex >= 4) {
        showSelectedStyledBoot(1700);
    }
    if (bootAnimationIndex == 3 || fastBootEnabled) {
        showMinimalBoot(fastBootEnabled ? 180 : 700);
        return;
    }
    showBootSummary(diagnostics);
    showBootTitle();
}

void showSplash() {
    showBootSequence();
}

bool pressedLetter(const Keyboard_Class::KeysState& keys, char target) {
    for (char value : keys.word) {
        if (tolower(static_cast<unsigned char>(value)) == target) return true;
    }
    return false;
}

void stopAllActiveOperations() {
    wifiSnifferService.end();
    bleSpamService.end();
    bleKeyboardService.end();
    stopBiscuitWardrive();
    biscuitClient.disconnect();
    warDriveService.stop();
    networkHostScanService.stop();
    networkPortScanService.stop();
    chameleonContinuousScan = false;
    chameleonClient.disconnect();
    telnetClient.stop();
    sshService.stop();
    audioService.stopPlayback();

    imuLogger.stop();
    gnssLogger.stop();
    loraLogger.stop();
    wifiSnifferLogger.stop();
    bleCaptureLogger.stop();
    wifiPassiveCaptureLogger.stop();
    chameleonLogger.stop();
    warDriveWifiLogger.stop();
    warDriveBleLogger.stop();
    handshakeCaptureLogger.stop();

    wifiConnectAttempting = false;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    actionMenuOpen = false;
    currentScreen = Screen::MainMenu;
    menuSelection = 0;
    listSelection = 0;
    listOffset = 0;
    Serial.println("[safety] all active operations stopped");
    drawMainMenu();
    drawFooter("Emergency stop complete");
}

void startWifiConnection(const String& ssid, const String& password,
                         bool showStatusScreen = true) {
    // Association and promiscuous capture cannot safely share the radio.
    // Normalize all entry paths (typed, saved, and open networks) through the
    // same cleanup sequence rather than relying on whichever tool ran last.
    wifiSnifferService.end();
    esp_wifi_set_promiscuous(false);
    WiFi.scanDelete();
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(false, false);
    delay(150);

    wifiConnectSsid = ssid;
    wifiConnectAttemptPassword = password;
    WiFi.begin(wifiConnectSsid.c_str(), wifiConnectAttemptPassword.c_str());
    wifiConnectAttempting = true;
    wifiConnectStartMs = millis();
    wifiConnectStatusText = "Connecting...";
    if (showStatusScreen) currentScreen = Screen::WifiConnectStatus;
}

void moveSelection(int direction, size_t count) {
    if (count == 0) return;
    if (direction < 0) {
        listSelection = listSelection == 0 ? count - 1 : listSelection - 1;
    } else {
        listSelection = (listSelection + 1) % count;
    }
}

void enterMenuItem() {
    listSelection = 0;
    listOffset = 0;
    switch (menuSelection) {
        case 0: currentScreen = Screen::WifiMenu; break;
        case 1: currentScreen = Screen::BleMenu; break;
        case 2: currentScreen = Screen::GpsMenu; break;
        case 3: currentScreen = Screen::MeshMenu; break;
        case 4: currentScreen = Screen::WarDrive; break;
        case 5: currentScreen = Screen::NetworkMenu; break;
        case 6: currentScreen = Screen::DevicesMenu; break;
        case 7:
            currentScreen = Screen::AiChat;
            aiNotice = "";
            aiService.loadConfig();
            break;
        case 8:
            currentScreen = Screen::CyberFamiliar;
            familiarPage = 0;
            break;
        case 9: currentScreen = Screen::ToolsMenu; break;
        case 10: currentScreen = Screen::Settings; break;
    }
    drawCurrentScreen();
}

void goBack() {
    if (currentScreen == Screen::AiChat) {
        currentScreen = Screen::MainMenu;
        menuSelection = 7;
        drawMainMenu();
        return;
    }
    if (currentScreen == Screen::CyberFamiliar) {
        currentScreen = Screen::MainMenu;
        menuSelection = 8;
        drawMainMenu();
        return;
    }
    if (currentScreen == Screen::CyberFamiliarResetConfirm) {
        currentScreen = Screen::CyberFamiliar;
        drawCyberFamiliar();
        return;
    }
    if (currentScreen == Screen::Imu) {
        imuLogger.stop();
    }
    if (currentScreen == Screen::Gnss) {
        gnssLogger.stop();
    }
    if (currentScreen == Screen::LoRa) {
        loraLogger.stop();
    }
    if (currentScreen == Screen::WifiSniffer) {
        wifiSnifferLogger.stop();
        wifiPassiveCaptureLogger.stop();
        wifiSnifferService.end();
    }
    if (currentScreen == Screen::BleDiscovery) {
        bleCaptureLogger.stop();
        bleScanner.stop();
    }
    if (currentScreen == Screen::Gnss) {
        currentScreen = Screen::GpsMenu;
        listSelection = 0;
        drawGpsMenu();
        return;
    }
    if (currentScreen == Screen::LoRa) {
        currentScreen = Screen::MeshMenu;
        listSelection = 0;
        drawMeshMenu();
        return;
    }
    if (currentScreen == Screen::NetworkHostScan) {
        networkHostScanService.stop();
        currentScreen = Screen::NetworkMenu;
        listSelection = 0;
        drawNetworkMenu();
        return;
    }
    if (currentScreen == Screen::NetworkDashboard) {
        currentScreen = Screen::NetworkMenu;
        listSelection = 0;
        drawNetworkMenu();
        return;
    }
    if (currentScreen == Screen::NetworkPortScan) {
        if (networkPortScanService.isActive()) networkPortScanService.stop();
        currentScreen = Screen::NetworkHostScan;
        listSelection = 0;
        listOffset = 0;
        drawNetworkHostScan();
        return;
    }
    if (currentScreen == Screen::SettingsDisplay ||
        currentScreen == Screen::SettingsBoot ||
        currentScreen == Screen::SettingsConnectivity ||
        currentScreen == Screen::SettingsReset) {
        currentScreen = Screen::Settings;
        listSelection = 0;
        listOffset = 0;
        drawSettings();
        return;
    }
    if (currentScreen == Screen::AudioMic) {
        audioService.endMicrophone();
        M5Cardputer.Speaker.begin();
        currentScreen = Screen::Audio;
        drawAudio();
        return;
    }
    if (currentScreen == Screen::AudioFiles) {
        currentScreen = Screen::Audio;
        listSelection = 2;
        drawAudio();
        return;
    }
    if (currentScreen == Screen::AudioPlaying) {
        audioService.stopPlayback();
        currentScreen = audioReturnScreen;
        drawCurrentScreen();
        return;
    }
    if (currentScreen == Screen::QrDisplay) {
        currentScreen = Screen::QrEntry;
        drawQrEntry();
        return;
    }
    if (currentScreen == Screen::QrEntry) {
        currentScreen = Screen::ToolsMenu;
        listSelection = 6;
        listOffset = 1;
        drawToolsMenu();
        return;
    }
    if (currentScreen == Screen::FileDetail) {
        currentScreen = Screen::Files;
        drawFiles();
        return;
    }
    if (currentScreen == Screen::TextPreview) {
        currentScreen = textPreviewReturnScreen;
        drawCurrentScreen();
        return;
    }
    if (currentScreen == Screen::LogDetail) {
        currentScreen = Screen::LogSessions;
        drawLogSessions();
        return;
    }
    if (currentScreen == Screen::LogDeleteConfirm) {
        currentScreen = Screen::LogDetail;
        drawLogDetail();
        return;
    }
    if (currentScreen == Screen::TimeStatus) {
        currentScreen = Screen::System;
        drawSystem();
        return;
    }
    if (currentScreen == Screen::WifiDetail) {
        currentScreen = Screen::WifiRecon;
        drawWifiRecon();
        return;
    }
    if (currentScreen == Screen::WifiDeauthConfirm) {
        currentScreen = Screen::WifiDetail;
        drawWifiDetail();
        return;
    }
    if (currentScreen == Screen::WifiHandshakeCapture) {
        handshakeCaptureLogger.stop();
        wifiSnifferService.clearHandshakeTarget();
        currentScreen = Screen::WifiDetail;
        drawWifiDetail();
        return;
    }
    if (currentScreen == Screen::BleDetail) {
        currentScreen = Screen::BleDiscovery;
        drawBleDiscovery();
        return;
    }
    if (currentScreen == Screen::UsbHidConfirm) {
        currentScreen = Screen::UsbHid;
        drawUsbHid();
        return;
    }
    if (currentScreen == Screen::DuckyScripts) {
        currentScreen = Screen::UsbHid;
        listSelection = kHidPresetCount;
        drawUsbHid();
        return;
    }
    if (currentScreen == Screen::DuckyConfirm ||
        currentScreen == Screen::DuckyResult) {
        currentScreen = Screen::DuckyScripts;
        drawDuckyScripts();
        return;
    }
    if (currentScreen == Screen::Files && currentPath != "/") {
        const int slash = currentPath.lastIndexOf('/');
        currentPath = slash <= 0 ? "/" : currentPath.substring(0, slash);
        loadDirectory();
        drawFiles();
        return;
    }
    if (currentScreen == Screen::WifiRecon ||
        currentScreen == Screen::WifiChannelAnalyzer ||
        currentScreen == Screen::WifiSniffer) {
        currentScreen = Screen::WifiMenu;
        listSelection = 0;
        drawWifiMenu();
        return;
    }
    if (currentScreen == Screen::WifiConnectSelect) {
        currentScreen = Screen::WifiMenu;
        listSelection = 2;
        drawWifiMenu();
        return;
    }
    if (currentScreen == Screen::WifiConnectPassword) {
        currentScreen = Screen::WifiConnectSelect;
        drawWifiConnectSelect();
        return;
    }
    if (currentScreen == Screen::WifiConnectStatus) {
        // Deliberately does not call WiFi.disconnect() -- unlike every
        // other Wi-Fi screen, this connection is meant to persist in the
        // background for future network tools to use. Disconnecting is
        // the explicit D key on this screen, not a side effect of leaving.
        currentScreen = Screen::WifiConnectSelect;
        drawWifiConnectSelect();
        return;
    }
    if (currentScreen == Screen::BleDiscovery) {
        currentScreen = Screen::BleMenu;
        listSelection = 0;
        drawBleMenu();
        return;
    }
    if (currentScreen == Screen::BleKeyboard) {
        bleKeyboardService.end();
        currentScreen = Screen::BleMenu;
        listSelection = 1;
        drawBleMenu();
        return;
    }
    if (currentScreen == Screen::BiscuitResult) {
        currentScreen = Screen::BiscuitTools;
        listSelection = 0;
        drawBiscuitTools();
        return;
    }
    if (currentScreen == Screen::BiscuitWardrive) {
        stopBiscuitWardrive();
        currentScreen = Screen::BiscuitTools;
        listSelection = 6;
        drawBiscuitTools();
        return;
    }
    if (currentScreen == Screen::BiscuitTools) {
        currentScreen = Screen::Biscuit;
        drawBiscuit();
        return;
    }
    if (currentScreen == Screen::Biscuit) {
        biscuitClient.disconnect();
        currentScreen = Screen::DevicesMenu;
        listSelection = 0;
        drawDevicesMenu();
        return;
    }
    if (currentScreen == Screen::BleSpam) {
        bleSpamService.end();
        currentScreen = Screen::BleSpamSelect;
        listSelection = 0;
        drawBleSpamSelect();
        return;
    }
    if (currentScreen == Screen::BleSpamSelect) {
        currentScreen = Screen::BleMenu;
        listSelection = 2;
        drawBleMenu();
        return;
    }
    if (currentScreen == Screen::Chameleon) {
        chameleonClient.disconnect();
        if (chameleonLogger.isActive()) chameleonLogger.stop();
        chameleonHasReadings = false;
        chameleonScanAttempted = false;
        chameleonContinuousScan = false;
        chameleonLastLoggedSignature = "";
        currentScreen = Screen::DevicesMenu;
        listSelection = 1;
        drawDevicesMenu();
        return;
    }
    if (currentScreen == Screen::ChameleonEmulateConfirm) {
        currentScreen = Screen::Chameleon;
        drawChameleon();
        return;
    }
    if (currentScreen == Screen::Infrared || currentScreen == Screen::UsbHid ||
        currentScreen == Screen::Audio ||
        currentScreen == Screen::LogSessions || currentScreen == Screen::Imu ||
        currentScreen == Screen::System || currentScreen == Screen::About ||
        currentScreen == Screen::Files) {
        // Files only reaches here when at "/" -- the case above already
        // returns for "go up a directory" when currentPath isn't root.
        currentScreen = Screen::ToolsMenu;
        listSelection = 0;
        drawToolsMenu();
        return;
    }
    if (currentScreen == Screen::DevicesMenu) {
        currentScreen = Screen::MainMenu;
        menuSelection = 6;
        drawMainMenu();
        return;
    }
    if (currentScreen == Screen::WarDrive) {
        warDriveService.stop();
        if (warDriveWifiLogger.isActive()) warDriveWifiLogger.stop();
        if (warDriveBleLogger.isActive()) warDriveBleLogger.stop();
    }
    currentScreen = Screen::MainMenu;
    drawMainMenu();
}

void handleInput(const Keyboard_Class::KeysState& keys) {
    // Global emergency stop: available even inside terminal/password screens.
    // It disconnects radios/sockets, flushes loggers, and returns home.
    if (keys.ctrl && keys.alt && keys.backspace) {
        stopAllActiveOperations();
        return;
    }
    // QR composition owns printable keys before global letter shortcuts.
    if (currentScreen == Screen::QrEntry) {
        if (keys.esc) {
            goBack();
            return;
        }
        if (keys.enter && !qrText.isEmpty()) {
            currentScreen = Screen::QrDisplay;
            drawQrDisplay();
            return;
        }
        if (keys.backspace && !qrText.isEmpty()) {
            qrText.remove(qrText.length() - 1);
        }
        for (char value : keys.word) {
            if (!keys.ctrl && qrText.length() < 100) qrText += value;
        }
        drawTextEntryRow(56, "> ", qrText);
        return;
    }
    if (currentScreen == Screen::QrDisplay) {
        if (keys.esc) {
            goBack();
        } else if (keys.enter) {
            currentScreen = Screen::QrEntry;
            drawQrEntry();
        }
        return;
    }
    // AI chat owns ordinary text entry. Ctrl+R records a short voice turn;
    // Ctrl+S speaks the newest answer; Tab changes text provider.
    if (currentScreen == Screen::AiChat) {
        if (keys.esc) {
            goBack();
            return;
        }
        if (keys.up) {
            ++aiScrollLines;
            drawAiChat();
            return;
        }
        if (keys.down) {
            if (aiScrollLines > 0) --aiScrollLines;
            drawAiChat();
            return;
        }
        if (keys.ctrl && pressedLetter(keys, 'r')) {
            if (!sdAvailable || WiFi.status() != WL_CONNECTED) {
                aiNotice = !sdAvailable ? "SD card required"
                                        : "Connect Wi-Fi first";
                drawAiChat();
                return;
            }
            drawHeader("AI: Recording 6 seconds");
            M5Cardputer.Display.setTextColor(Branding::warning,
                                             Branding::background);
            M5Cardputer.Display.setCursor(8, 48);
            M5Cardputer.Display.print("Speak now...");
            drawFooter("Esc: cancel recording");
            String error;
            SD.remove(kAiRecordingPath);
            if (audioService.recordWav(kAiRecordingPath, 6000, error)) {
                drawHeader("AI: Transcribing");
                M5Cardputer.Display.setCursor(8, 48);
                M5Cardputer.Display.print("Uploading voice clip...");
                String transcript;
                if (aiService.transcribe(kAiRecordingPath, transcript)) {
                    aiPrompt = transcript;
                    aiNotice = "Voice ready - Enter to send";
                } else {
                    aiNotice = aiService.status();
                }
            } else {
                aiNotice = error;
            }
            SD.remove(kAiRecordingPath);
            recoverKeyboardAfterBlockingOperation();
            drawAiChat();
            return;
        }
        if (keys.ctrl && pressedLetter(keys, 's')) {
            const auto& history = aiService.history();
            if (history.empty() || history.back().role != "assistant") {
                aiNotice = "No assistant reply to speak";
            } else if (!sdAvailable) {
                aiNotice = "SD card required";
            } else {
                drawHeader("AI: Creating speech");
                M5Cardputer.Display.setCursor(8, 48);
                M5Cardputer.Display.print("Downloading audio...");
                if (aiService.synthesize(history.back().text,
                                         kAiSpeechPath) &&
                    audioService.startMp3(kAiSpeechPath)) {
                    audioReturnScreen = Screen::AiChat;
                    currentScreen = Screen::AudioPlaying;
                    nowPlayingName = "AI reply";
                    nowPlayingSource = kAiSpeechPath;
                    drawHeader("Now Playing");
                    M5Cardputer.Display.setTextColor(
                        Branding::text, Branding::background);
                    M5Cardputer.Display.setCursor(8, 40);
                    M5Cardputer.Display.print("AI reply");
                    M5Cardputer.Display.setTextColor(
                        Branding::muted, Branding::background);
                    M5Cardputer.Display.setCursor(8, 64);
                    M5Cardputer.Display.print("OpenAI text-to-speech");
                    drawFooter("Enter/Backspace/Q: stop");
                    recoverKeyboardAfterBlockingOperation();
                    return;
                }
                aiNotice = aiService.status();
                recoverKeyboardAfterBlockingOperation();
            }
            drawAiChat();
            return;
        }
        if (keys.ctrl && pressedLetter(keys, 'n')) {
            aiService.clearHistory();
            aiNotice = "Conversation cleared";
            drawAiChat();
            return;
        }
        if (keys.tab) {
            aiService.toggleProvider();
            aiNotice = aiService.isConfigured() ? "Provider changed"
                                                 : "Provider key missing";
            drawAiChat();
            return;
        }
        if (keys.enter && !aiPrompt.isEmpty()) {
            const String prompt = aiPrompt;
            aiPrompt = "";
            aiNotice = "Waiting for " + String(aiService.providerName()) + "...";
            drawAiChat();
            String answer;
            if (!aiService.send(prompt, answer)) {
                aiPrompt = prompt;
                aiNotice = aiService.status();
            } else {
                aiNotice = "Ready";
                aiScrollLines = 0;
            }
            recoverKeyboardAfterBlockingOperation();
            drawAiChat();
            return;
        }
        if (keys.backspace && !aiPrompt.isEmpty()) {
            aiPrompt.remove(aiPrompt.length() - 1);
        }
        for (char value : keys.word) {
            if (!keys.ctrl && aiPrompt.length() < 500) aiPrompt += value;
        }
        drawAiComposer();
        return;
    }
    // This screen forwards ordinary keys, so process it before Ghostwire's
    // letter-based navigation shortcuts. Escape is retained as the local,
    // immediate disconnect control and is never sent to the peer.
    if (currentScreen == Screen::BleKeyboard) {
        if (keys.esc) {
            bleKeyboardService.end();
            currentScreen = Screen::BleMenu;
            listSelection = 1;
            drawBleMenu();
            return;
        }
        if (!bleKeyboardService.isActive()) {
            if (keys.enter) {
                bleKeyboardService.begin(batteryPercentage());
            }
            drawBleKeyboard();
            return;
        }
        if (bleKeyboardService.isConnected()) {
            if (keys.enter) bleKeyboardService.sendAscii('\n');
            if (keys.backspace) bleKeyboardService.sendAscii('\b');
            if (keys.tab) bleKeyboardService.sendAscii('\t');
            for (char value : keys.word) bleKeyboardService.sendAscii(value);
        }
        drawBleKeyboard();
        return;
    }
    // Free-text password entry: every other screen treats individual
    // letters as navigation shortcuts (q/b/r/w/s/k/j/;/.,-/= etc.), which
    // would corrupt anything typed here, so this screen is handled
    // entirely separately, before any of those shortcuts are computed.
    if (currentScreen == Screen::WifiConnectPassword) {
        if (keys.esc) {
            goBack();
            return;
        }
        if (keys.enter) {
            startWifiConnection(wifiConnectSsid, wifiConnectPasswordInput);
            drawCurrentScreen();
            return;
        }
        if (keys.backspace && !wifiConnectPasswordInput.isEmpty()) {
            wifiConnectPasswordInput.remove(
                wifiConnectPasswordInput.length() - 1);
        }
        for (char value : keys.word) {
            if (wifiConnectPasswordInput.length() < 63) {
                wifiConnectPasswordInput += value;
            }
        }
        drawTextEntryRow(54, "Password: ", wifiConnectPasswordInput, true);
        return;
    }

    // Telnet host entry: same free-text shape as Wi-Fi Connect's
    // password screen above.
    if (currentScreen == Screen::TelnetConnect) {
        if (keys.esc) {
            currentScreen = telnetReturnScreen;
            listSelection = 0;
            listOffset = 0;
            drawCurrentScreen();
            return;
        }
        if (keys.enter) {
            connectTelnet();
            return;
        }
        if (keys.backspace && !telnetHostInput.isEmpty()) {
            telnetHostInput.remove(telnetHostInput.length() - 1);
        }
        for (char value : keys.word) {
            if (telnetHostInput.length() < 63) {
                telnetHostInput += value;
            }
        }
        drawTextEntryRow(36, "Host: ", telnetHostInput);
        return;
    }

    // Telnet live session: every key must reach the remote host, so this
    // screen has no navigation shortcuts and no action menu at all --
    // even Tab is forwarded literally rather than opening the action
    // menu, since a real remote shell may use it for completion. Esc is
    // the only way out.
    if (currentScreen == Screen::TelnetSession) {
        if (keys.esc) {
            telnetClient.stop();
            currentScreen = Screen::TelnetConnect;
            drawCurrentScreen();
            return;
        }
        if (telnetClient.connected()) {
            if (keys.enter) {
                telnetClient.write((const uint8_t*)"\r\n", 2);
            }
            if (keys.backspace) {
                telnetClient.write(static_cast<uint8_t>(0x7F));
            }
            if (keys.tab) {
                telnetClient.write(static_cast<uint8_t>(0x09));
            }
            for (char value : keys.word) {
                telnetClient.write(static_cast<uint8_t>(value));
            }
        }
        return;
    }

    // SSH target entry: same free-text shape as the Telnet host field.
    if (currentScreen == Screen::SshConnect) {
        if (keys.esc) {
            currentScreen = Screen::NetworkMenu;
            listSelection = 0;
            listOffset = 0;
            drawCurrentScreen();
            return;
        }
        if (keys.enter) {
            preferences.putUChar("ssh_stage", 1);
            sshStatus = "";
            if (parseSshTarget()) {
                rememberSshTarget(sshHostInput);
                preferences.putUChar("ssh_stage", 2);
                sshPasswordInput = "";
                currentScreen = Screen::SshPassword;
                drawCurrentScreen();
                preferences.putUChar("ssh_stage", 3);
                return;
            }
            preferences.remove("ssh_stage");
            drawSshConnect();
            return;
        }
        if (keys.tab) {
            for (size_t attempts = 0; attempts < 3; ++attempts) {
                sshHistoryIndex = (sshHistoryIndex + 1) % 3;
                if (!sshHistory[sshHistoryIndex].isEmpty()) {
                    sshHostInput = sshHistory[sshHistoryIndex];
                    break;
                }
            }
            drawTextEntryRow(36, "Target: ", sshHostInput);
            return;
        }
        if (keys.backspace && !sshHostInput.isEmpty()) {
            sshHostInput.remove(sshHostInput.length() - 1);
        }
        for (char value : keys.word) {
            if (sshHostInput.length() < 63) sshHostInput += value;
        }
        drawTextEntryRow(36, "Target: ", sshHostInput);
        return;
    }

    // SSH password entry: identical mechanic to WifiConnectPassword.
    if (currentScreen == Screen::SshPassword) {
        if (keys.esc) {
            for (size_t i = 0; i < sshPasswordInput.length(); ++i) {
                sshPasswordInput.setCharAt(i, '\0');
            }
            sshPasswordInput = "";
            sshTrustPending = false;
            sshService.stop();
            preferences.remove("ssh_stage");
            currentScreen = Screen::SshConnect;
            drawCurrentScreen();
            return;
        }
        if (keys.enter) {
            connectSsh();
            return;
        }
        if (keys.backspace && !sshPasswordInput.isEmpty()) {
            sshPasswordInput.remove(sshPasswordInput.length() - 1);
        }
        for (char value : keys.word) {
            if (sshPasswordInput.length() < 63) sshPasswordInput += value;
        }
        drawTextEntryRow(54, "Password: ", sshPasswordInput, true);
        return;
    }

    // SSH live shell: every key must reach the remote host, same
    // all-keys-forwarded design as Screen::TelnetSession -- no action
    // menu, no navigation shortcuts, Tab sent literally, Esc is the
    // only way out.
    if (currentScreen == Screen::SshSession) {
        if (keys.esc) {
            sshService.stop();
            currentScreen = Screen::SshConnect;
            drawCurrentScreen();
            return;
        }
        if (sshService.isConnected()) {
            uint8_t outgoing[72];
            size_t outgoingLength = 0;
            if (keys.enter) {
                outgoing[outgoingLength++] = '\r';
                outgoing[outgoingLength++] = '\n';
                appendTerminalByte('\n', sshLines, sshPendingLine,
                                   sshEscState, kMaxSshLines);
                sshLocalEchoPending += "\r\n";
            }
            if (keys.backspace) {
                outgoing[outgoingLength++] = 0x7F;
                appendTerminalByte(static_cast<char>(0x7F), sshLines,
                                   sshPendingLine, sshEscState, kMaxSshLines);
                sshLocalEchoPending += static_cast<char>(0x7F);
            }
            if (keys.tab) {
                outgoing[outgoingLength++] = 0x09;
            }
            for (char value : keys.word) {
                if (outgoingLength < sizeof(outgoing)) {
                    outgoing[outgoingLength++] = static_cast<uint8_t>(value);
                    appendTerminalByte(value, sshLines, sshPendingLine,
                                       sshEscState, kMaxSshLines);
                    sshLocalEchoPending += value;
                }
            }
            if (outgoingLength > 0) {
                sshService.write(outgoing, outgoingLength);
            }
            if (!keys.word.empty() || keys.backspace || keys.enter) {
                drawSshSessionDynamic();
            }
        }
        return;
    }

    // Contextual action menu: everything that used to be a one-off
    // letter (export, deauth, disconnect, etc.) now lives here instead,
    // reached via Tab. While open, this block owns all input; closing it
    // (Esc/Backspace/Tab again) never triggers screen-level "back"
    // navigation, only leaving the menu.
    if (actionMenuOpen) {
        const bool menuUp = keys.up || pressedLetter(keys, 'w') ||
                            pressedLetter(keys, 'k') ||
                            pressedLetter(keys, ';');
        const bool menuDown = keys.down || pressedLetter(keys, 's') ||
                              pressedLetter(keys, 'j') ||
                              pressedLetter(keys, '.');
        const bool menuClose = keys.esc || keys.backspace || keys.tab ||
                               keys.left || pressedLetter(keys, 'q') ||
                               pressedLetter(keys, 'b');
        if (!actionMenuItems.empty()) {
            if (menuUp) {
                actionMenuSelection = actionMenuSelection == 0
                                          ? actionMenuItems.size() - 1
                                          : actionMenuSelection - 1;
            }
            if (menuDown) {
                actionMenuSelection =
                    (actionMenuSelection + 1) % actionMenuItems.size();
            }
        }
        if (menuClose) {
            actionMenuOpen = false;
            drawCurrentScreen();
            return;
        }
        if (keys.enter && !actionMenuItems.empty()) {
            const char chosen = actionMenuItems[actionMenuSelection].key;
            actionMenuOpen = false;
            Keyboard_Class::KeysState synthetic{};
            synthetic.word.push_back(chosen);
            handleInput(synthetic);
            return;
        }
        drawActionMenu();
        return;
    }

    const bool up = keys.up || pressedLetter(keys, 'w') ||
                    pressedLetter(keys, 'k') || pressedLetter(keys, ';');
    const bool down = keys.down || pressedLetter(keys, 's') ||
                      pressedLetter(keys, 'j') || pressedLetter(keys, '.');
    const bool back = keys.esc || keys.backspace;
    const bool alternateBack =
        keys.left || pressedLetter(keys, 'q') || pressedLetter(keys, 'b');
    const bool refresh = pressedLetter(keys, 'r');
    const bool decrease = pressedLetter(keys, '-');
    const bool increase = pressedLetter(keys, '=');

    if ((back || alternateBack) && currentScreen != Screen::MainMenu) {
        goBack();
        return;
    }

    if (keys.tab) {
        actionMenuItems = actionsForScreen(currentScreen);
        if (!actionMenuItems.empty()) {
            actionMenuOpen = true;
            actionMenuSelection = 0;
            drawActionMenu();
        }
        return;
    }

    switch (currentScreen) {
        case Screen::MainMenu:
            if (up) {
                menuSelection =
                    menuSelection == 0 ? kMenuCount - 1 : menuSelection - 1;
            } else if (down) {
                menuSelection = (menuSelection + 1) % kMenuCount;
            } else if (keys.enter) {
                enterMenuItem();
                return;
            }
            drawMainMenu();
            break;

        case Screen::AiChat:
            // Handled before global navigation so printable letters remain
            // prompt input rather than menu shortcuts.
            break;

        case Screen::WifiMenu:
            if (up) moveSelection(-1, 4);
            if (down) moveSelection(1, 4);
            if (keys.enter) {
                if (listSelection == 0) {
                    currentScreen = Screen::WifiRecon;
                    drawCurrentScreen();
                    scanWifiNetworks();
                } else if (listSelection == 1) {
                    currentScreen = Screen::WifiChannelAnalyzer;
                    drawCurrentScreen();
                    scanWifiNetworks();
                } else if (listSelection == 2) {
                    currentScreen = Screen::WifiSniffer;
                    recentWifiProbes.clear();
                    wifiSnifferService.begin();
                    drawCurrentScreen();
                } else {
                    currentScreen = Screen::WifiConnectSelect;
                    drawCurrentScreen();
                    scanWifiNetworks();
                }
                return;
            }
            drawWifiMenu();
            break;

        case Screen::WifiRecon:
            if (refresh) {
                scanWifiNetworks();
                return;
            }
            if (pressedLetter(keys, 'e')) {
                exportWifiResults();
                return;
            }
            if (up) moveSelection(-1, accessPoints.size());
            if (down) moveSelection(1, accessPoints.size());
            if (keys.enter && !accessPoints.empty()) {
                wifiDeauthStatus = "";
                currentScreen = Screen::WifiDetail;
                drawWifiDetail();
                return;
            }
            drawWifiRecon();
            break;

        case Screen::WifiChannelAnalyzer:
            if (refresh) {
                scanWifiNetworks();
                return;
            }
            drawWifiChannelAnalyzer();
            break;

        case Screen::WifiDetail:
            if (pressedLetter(keys, 'd') && !accessPoints.empty()) {
                currentScreen = Screen::WifiDeauthConfirm;
                drawWifiDeauthConfirm();
                return;
            }
            if (pressedLetter(keys, 'h') && !accessPoints.empty()) {
                handshakeEapolFrameCount = 0;
                for (bool& seen : handshakeMessageSeen) seen = false;
                handshakePmkidFound = false;
                wifiSnifferService.begin();
                wifiSnifferService.setHandshakeTarget(
                    accessPoints[listSelection].bssid);
                handshakeCaptureLogger.begin("handshake");
                currentScreen = Screen::WifiHandshakeCapture;
                drawWifiHandshakeCapture();
                return;
            }
            break;

        case Screen::WifiDeauthConfirm:
            if (keys.enter && !accessPoints.empty()) {
                transmitWifiDeauth(accessPoints[listSelection]);
                currentScreen = Screen::WifiDetail;
                drawWifiDetail();
                return;
            }
            break;

        case Screen::WifiHandshakeCapture:
            if (pressedLetter(keys, 'd') && !accessPoints.empty()) {
                transmitWifiDeauth(accessPoints[listSelection]);
                drawWifiHandshakeCapture();
                return;
            }
            if (refresh && !accessPoints.empty()) {
                handshakeEapolFrameCount = 0;
                for (bool& seen : handshakeMessageSeen) seen = false;
                handshakePmkidFound = false;
                wifiSnifferService.setHandshakeTarget(
                    accessPoints[listSelection].bssid);
                handshakeCaptureLogger.begin("handshake");
            }
            drawWifiHandshakeCapture();
            break;

        case Screen::WifiConnectSelect: {
            if (refresh) {
                scanWifiNetworks();
                return;
            }
            if (pressedLetter(keys, 'c') && !wifiConnectSavedSsid.isEmpty()) {
                startWifiConnection(wifiConnectSavedSsid,
                                    wifiConnectSavedPassword);
                drawCurrentScreen();
                return;
            }
            if (up) moveSelection(-1, accessPoints.size());
            if (down) moveSelection(1, accessPoints.size());
            if (keys.enter && !accessPoints.empty()) {
                const auto& ap = accessPoints[listSelection];
                wifiConnectSsid = reinterpret_cast<const char*>(ap.ssid);
                if (ap.authmode == WIFI_AUTH_OPEN) {
                    startWifiConnection(wifiConnectSsid, "");
                } else {
                    wifiConnectPasswordInput = "";
                    currentScreen = Screen::WifiConnectPassword;
                }
                drawCurrentScreen();
                return;
            }
            drawWifiConnectSelect();
            break;
        }

        case Screen::WifiConnectStatus:
            if (pressedLetter(keys, 'd')) {
                WiFi.disconnect(true);
                wifiConnectAttempting = false;
                wifiConnectStatusText = "Disconnected";
                drawWifiConnectStatus();
                return;
            }
            drawWifiConnectStatus();
            break;

        case Screen::BleMenu:
            if (up) moveSelection(-1, 3);
            if (down) moveSelection(1, 3);
            if (keys.enter) {
                if (listSelection == 0) {
                    currentScreen = Screen::BleDiscovery;
                    drawCurrentScreen();
                    scanBleDevices();
                } else if (listSelection == 1) {
                    currentScreen = Screen::BleKeyboard;
                    drawCurrentScreen();
                } else {
                    listSelection = 0;
                    listOffset = 0;
                    currentScreen = Screen::BleSpamSelect;
                    drawCurrentScreen();
                }
                return;
            }
            drawBleMenu();
            break;

        case Screen::DevicesMenu:
            if (up) moveSelection(-1, 2);
            if (down) moveSelection(1, 2);
            if (keys.enter) {
                currentScreen = listSelection == 0 ? Screen::Biscuit
                                                   : Screen::Chameleon;
                if (currentScreen == Screen::Chameleon) {
                    chameleonConnectAttempts = 0;
                    lastChameleonConnectAttemptMs =
                        millis() - kChameleonReconnectIntervalMs;
                    chameleonWorkflowStatus = "Automatic connection enabled";
                }
                drawCurrentScreen();
                return;
            }
            drawDevicesMenu();
            break;

        case Screen::Biscuit:
            if (refresh || (keys.enter && !biscuitClient.isConnected())) {
                drawHeader("Biscuit Pro");
                M5Cardputer.Display.setTextColor(Branding::warning,
                                                 Branding::background);
                M5Cardputer.Display.setCursor(8, 44);
                M5Cardputer.Display.print("Scanning and connecting...");
                drawFooter("Please wait");
                biscuitClient.connect();
                recoverKeyboardAfterBlockingOperation();
                drawBiscuit();
                return;
            }
            if (keys.enter && biscuitClient.isConnected()) {
                listSelection = 0;
                currentScreen = Screen::BiscuitTools;
                drawBiscuitTools();
                return;
            }
            drawBiscuit();
            break;

        case Screen::BiscuitTools: {
            static const char* const commands[] = {
                "CMD:info:", "CMD:scanap:", "CMD:scansta:",
                "CMD:packetcount:", "CMD:channel:", "CMD:getnodelist:",
            };
            static const char* const titles[] = {
                "Biscuit: Information", "Biscuit: AP scan",
                "Biscuit: Stations", "Biscuit: Packets",
                "Biscuit: Channel", "Biscuit: Nodes",
            };
            if (up) moveSelection(-1, 7);
            if (down) moveSelection(1, 7);
            if (keys.enter) {
                const size_t commandIndex = listSelection;
                if (commandIndex == 6) {
                    biscuitWardriveApCount = 0;
                    biscuitWardriveBleCount = 0;
                    biscuitWardriveBssids.clear();
                    biscuitWardriveBleMacs.clear();
                    biscuitWardriveParseTail = "";
                    biscuitWardriveActive = false;
                    currentScreen = Screen::BiscuitWardrive;
                    drawBiscuitWardrive();
                    return;
                }
                drawHeader(titles[commandIndex]);
                M5Cardputer.Display.setTextColor(Branding::warning,
                                                 Branding::background);
                M5Cardputer.Display.setCursor(8, 44);
                M5Cardputer.Display.print("Waiting for response...");
                drawFooter("Read-only command");
                String response;
                if (!biscuitClient.sendReadOnlyCommand(
                        commands[commandIndex], response)) {
                    response = biscuitClient.lastStatus();
                }
                biscuitResultTitle = titles[commandIndex];
                prepareBiscuitResult(response);
                currentScreen = Screen::BiscuitResult;
                recoverKeyboardAfterBlockingOperation();
                drawBiscuitResult();
                return;
            }
            drawBiscuitTools();
            break;
        }

        case Screen::BiscuitWardrive:
            if (keys.enter) {
                if (biscuitWardriveActive) {
                    stopBiscuitWardrive();
                } else {
                    biscuitWardriveApCount = 0;
                    biscuitWardriveBleCount = 0;
                    biscuitWardriveBssids.clear();
                    biscuitWardriveBleMacs.clear();
                    biscuitWardriveParseTail = "";
                    biscuitClient.takeNotifications();
                    biscuitWardriveActive = biscuitClient.sendCommandNoWait(
                        "CMD:wardrive:");
                    if (!biscuitWardriveActive) {
                        biscuitWardriveParseTail = biscuitClient.lastStatus();
                    }
                }
                drawBiscuitWardrive();
                return;
            }
            drawBiscuitWardrive();
            break;

        case Screen::BiscuitResult:
            if (up && biscuitResultOffset > 0) --biscuitResultOffset;
            if (down && biscuitResultOffset + kVisibleRows <
                            biscuitResultLines.size()) {
                ++biscuitResultOffset;
            }
            drawBiscuitResult();
            break;

        case Screen::BleSpamSelect: {
            if (up) moveSelection(-1, 4);
            if (down) moveSelection(1, 4);
            if (keys.enter) {
                BleSpamMode mode;
                switch (listSelection) {
                    case 0: mode = BleSpamMode::Apple; break;
                    case 1: mode = BleSpamMode::FastPair; break;
                    case 2: mode = BleSpamMode::SwiftPair; break;
                    default: mode = BleSpamMode::All; break;
                }
                bleSpamService.begin(mode);
                currentScreen = Screen::BleSpam;
                drawBleSpam();
                return;
            }
            drawBleSpamSelect();
            break;
        }

        case Screen::BleSpam:
            drawBleSpam();
            break;

        case Screen::RfidMenu:
            if (keys.enter) {
                listSelection = 0;
                currentScreen = Screen::Chameleon;
                chameleonConnectAttempts = 0;
                lastChameleonConnectAttemptMs =
                    millis() - kChameleonReconnectIntervalMs;
                chameleonWorkflowStatus = "Automatic connection enabled";
                drawCurrentScreen();
                return;
            }
            drawRfidMenu();
            break;

        case Screen::Chameleon:
            if (refresh) {
                chameleonClient.disconnect();
                chameleonHasReadings = false;
                chameleonScanAttempted = false;
                chameleonLastLoggedSignature = "";
                if (chameleonLogger.isActive()) chameleonLogger.stop();
                chameleonConnectAttempts = 0;
                lastChameleonConnectAttemptMs =
                    millis() - kChameleonReconnectIntervalMs;
                chameleonWorkflowStatus = "Automatic connection enabled";
                drawChameleon();
                return;
            }
            if (pressedLetter(keys, 's') && chameleonClient.isConnected()) {
                chameleonWorkflowStatus = "";
                performChameleonScan();
                drawChameleon();
                return;
            }
            if (pressedLetter(keys, 'c') && chameleonClient.isConnected()) {
                chameleonWorkflowStatus = "";
                chameleonContinuousScan = !chameleonContinuousScan;
                drawChameleon();
                return;
            }
            if (pressedLetter(keys, 'v') && chameleonClient.isConnected()) {
                saveChameleonIdentity();
                drawChameleon();
                return;
            }
            if (pressedLetter(keys, 'o') && chameleonClient.isConnected()) {
                loadChameleonIdentity();
                drawChameleon();
                return;
            }
            if (pressedLetter(keys, 'd') && chameleonClient.isConnected()) {
                chameleonClient.setReaderMode();
                chameleonWorkflowStatus = chameleonClient.lastStatus();
                drawChameleon();
                return;
            }
            if (pressedLetter(keys, 'e') && chameleonClient.isConnected() &&
                (chameleonHfFound || chameleonLfFound)) {
                chameleonContinuousScan = false;
                currentScreen = Screen::ChameleonEmulateConfirm;
                drawChameleonEmulateConfirm();
                return;
            }
            drawChameleon();
            break;

        case Screen::ChameleonEmulateConfirm:
            if (keys.enter) {
                const bool success = chameleonHfFound
                    ? chameleonClient.stageHfIdentity(chameleonHfTag, 7)
                    : chameleonClient.stageEm410xIdentity(chameleonLfId, 7);
                chameleonWorkflowStatus = success
                    ? chameleonClient.lastStatus()
                    : "Stage failed: " + chameleonClient.lastStatus();
                currentScreen = Screen::Chameleon;
                drawChameleon();
            }
            break;

        case Screen::BleDiscovery:
            if (refresh) {
                scanBleDevices();
                return;
            }
            if (pressedLetter(keys, 'e')) {
                exportBleResults();
                return;
            }
            if (pressedLetter(keys, 'c')) {
                if (bleScanner.isContinuous()) {
                    bleScanner.stop();
                    bleCaptureLogger.stop();
                    bleStatus = "Continuous capture stopped";
                } else {
                    bleDevices.clear();
                    bleExportStatus = "";
                    if (bleScanner.beginContinuous(bleStatus) && sdAvailable) {
                        bleCaptureLogger.begin(
                            "ble_capture",
                            "timestamp_utc,name,address,address_type,rssi_dbm,"
                            "connectable,advertisement_type,payload_bytes,"
                            "service_count,service_uuids,manufacturer,"
                            "manufacturer_data_hex,payload_hex");
                    }
                    recoverKeyboardAfterBlockingOperation();
                }
                listSelection = 0;
                listOffset = 0;
                drawBleDiscovery();
                return;
            }
            if (pressedLetter(keys, 'f')) {
                static constexpr int filters[] = {-100, -80, -65, -50};
                size_t selected = 0;
                for (size_t index = 0;
                     index < sizeof(filters) / sizeof(filters[0]); ++index) {
                    if (filters[index] == bleCaptureRssiFilter) selected = index;
                }
                bleCaptureRssiFilter =
                    filters[(selected + 1) %
                            (sizeof(filters) / sizeof(filters[0]))];
                drawBleDiscovery();
                return;
            }
            if (up) moveSelection(-1, bleDevices.size());
            if (down) moveSelection(1, bleDevices.size());
            if (keys.enter && !bleDevices.empty()) {
                currentScreen = Screen::BleDetail;
                drawBleDetail();
                return;
            }
            drawBleDiscovery();
            break;

        case Screen::CyberFamiliar:
            if (up) familiarPage = (familiarPage + 2) % 3;
            if (down) familiarPage = (familiarPage + 1) % 3;
            if (pressedLetter(keys, 'p')) cyberFamiliar.interact();
            if (pressedLetter(keys, 'n')) cyberFamiliar.cycleName();
            if (pressedLetter(keys, 'i')) cyberFamiliar.toggleIdleMode();
            if (pressedLetter(keys, 'o')) familiarPage = 0;
            if (pressedLetter(keys, 't')) familiarPage = 1;
            if (pressedLetter(keys, 'j')) familiarPage = 2;
            if (pressedLetter(keys, 'x')) {
                exportFamiliarRecord();
                familiarPage = 0;
            }
            if (pressedLetter(keys, 'g')) {
                importFamiliarCaptureLogs();
                familiarPage = 0;
            }
            if (pressedLetter(keys, 'z')) {
                currentScreen = Screen::CyberFamiliarResetConfirm;
                drawCyberFamiliarResetConfirm();
                return;
            }
            drawCyberFamiliar();
            break;

        case Screen::CyberFamiliarResetConfirm:
            if (keys.enter) {
                cyberFamiliar.resetProgress();
                familiarWorkflowStatus = "Familiar progress reset";
                familiarPage = 0;
                currentScreen = Screen::CyberFamiliar;
                drawCyberFamiliar();
            }
            break;

        case Screen::BleDetail:
            break;

        case Screen::ToolsMenu:
            if (up) moveSelection(-1, 9);
            if (down) moveSelection(1, 9);
            if (keys.enter) {
                const size_t toolIndex = listSelection;
                listSelection = 0;
                listOffset = 0;
                switch (toolIndex) {
                    case 0: currentScreen = Screen::Infrared; break;
                    case 1: currentScreen = Screen::UsbHid; break;
                    case 2: currentScreen = Screen::Audio; break;
                    case 3:
                        currentScreen = Screen::LogSessions;
                        loadLogSessions();
                        break;
                    case 4:
                        imuAvailable = M5.Imu.isEnabled();
                        currentScreen = Screen::Imu;
                        break;
                    case 5:
                        currentScreen = Screen::Files;
                        currentPath = "/";
                        loadDirectory();
                        break;
                    case 6:
                        qrText = "";
                        currentScreen = Screen::QrEntry;
                        break;
                    case 7: currentScreen = Screen::System; break;
                    case 8: currentScreen = Screen::About; break;
                }
                drawCurrentScreen();
                return;
            }
            drawToolsMenu();
            break;

        case Screen::Infrared:
            if (keys.enter || refresh) {
                transmitInfraredSelfTest();
                return;
            }
            drawInfrared();
            break;

        case Screen::UsbHid:
            if (up) moveSelection(-1, kHidPresetCount + 1);
            if (down) moveSelection(1, kHidPresetCount + 1);
            if (keys.enter) {
                if (listSelection == kHidPresetCount) {
                    loadDuckyScripts();
                    currentScreen = Screen::DuckyScripts;
                    drawDuckyScripts();
                } else {
                    currentScreen = Screen::UsbHidConfirm;
                    drawUsbHidConfirmation();
                }
                return;
            }
            drawUsbHid();
            break;

        case Screen::UsbHidConfirm:
            if (keys.enter) {
                runUsbHidPreset();
                return;
            }
            break;

        case Screen::DuckyScripts:
            if (refresh) loadDuckyScripts();
            if (up) moveSelection(-1, duckyScripts.size());
            if (down) moveSelection(1, duckyScripts.size());
            if (keys.enter && !duckyScripts.empty()) {
                preflightDuckyScript();
                currentScreen = Screen::DuckyConfirm;
                drawDuckyConfirm();
                return;
            }
            drawDuckyScripts();
            break;

        case Screen::DuckyConfirm:
            if (keys.enter) {
                runSelectedDuckyScript();
                return;
            }
            break;

        case Screen::DuckyResult:
            if (keys.enter) {
                currentScreen = Screen::DuckyScripts;
                drawDuckyScripts();
            }
            break;

        case Screen::Audio:
            if (up) moveSelection(-1, 3);
            if (down) moveSelection(1, 3);
            if (keys.enter) {
                if (listSelection == 0) {
                    audioService.playToneTest();
                    recoverKeyboardAfterBlockingOperation();
                    drawAudio();
                } else if (listSelection == 1) {
                    microphoneLevel = 0;
                    if (audioService.beginMicrophone()) {
                        currentScreen = Screen::AudioMic;
                        drawMicrophone();
                    }
                } else {
                    loadAudioFiles();
                    currentScreen = Screen::AudioFiles;
                    drawAudioFiles();
                }
                return;
            }
            drawAudio();
            break;

        case Screen::AudioMic:
            break;

        case Screen::AudioFiles:
            if (refresh) loadAudioFiles();
            if (up) moveSelection(-1, audioFiles.size());
            if (down) moveSelection(1, audioFiles.size());
            if (keys.enter) {
                startSelectedMp3();
                return;
            }
            drawAudioFiles();
            break;

        case Screen::AudioPlaying:
            if (keys.enter) {
                audioService.stopPlayback();
                currentScreen = audioReturnScreen;
                drawCurrentScreen();
            }
            break;

        case Screen::Files:
            if (refresh) {
                initSd();
                loadDirectory();
            } else if (up) {
                moveSelection(-1, files.size());
            } else if (down) {
                moveSelection(1, files.size());
            } else if (keys.enter && !files.empty()) {
                if (files[listSelection].directory) {
                    if (currentPath != "/") currentPath += "/";
                    currentPath += files[listSelection].name;
                    loadDirectory();
                } else {
                    currentScreen = Screen::FileDetail;
                    drawFileDetail();
                    return;
                }
            }
            drawFiles();
            break;

        case Screen::FileDetail:
            if (keys.enter) {
                if (isMp3File(files[listSelection].name)) {
                    playSelectedBrowserMp3();
                } else if (isPreviewableFile(files[listSelection].name) &&
                           loadTextPreview()) {
                    textPreviewReturnScreen = Screen::FileDetail;
                    currentScreen = Screen::TextPreview;
                    drawTextPreview();
                }
            }
            break;

        case Screen::TextPreview:
            if (up && previewTopLine > 0) {
                --previewTopLine;
            } else if (down &&
                       previewTopLine + kVisibleRows < previewLines.size()) {
                ++previewTopLine;
            }
            if (pressedLetter(keys, 'a') && previewColumn >= 8) {
                previewColumn -= 8;
            } else if (pressedLetter(keys, 'd') && previewColumn < 120) {
                previewColumn += 8;
            }
            drawTextPreview();
            break;

        case Screen::LogSessions:
            if (refresh) {
                initSd();
                loadLogSessions();
            } else if (up && !logSessions.empty()) {
                logSelection = logSelection == 0
                                   ? logSessions.size() - 1
                                   : logSelection - 1;
            } else if (down && !logSessions.empty()) {
                logSelection = (logSelection + 1) % logSessions.size();
            } else if (keys.enter && !logSessions.empty()) {
                openLogDetail();
                drawLogDetail();
                return;
            }
            drawLogSessions();
            break;

        case Screen::LogDetail:
            if (pressedLetter(keys, 'd')) {
                currentScreen = Screen::LogDeleteConfirm;
                drawLogDeleteConfirm();
            } else if (keys.enter) {
                currentPath = "/ghostwire/logs";
                files.clear();
                const auto& log = logSessions[logSelection];
                files.push_back({log.name, false, log.size});
                listSelection = 0;
                if (loadTextPreview()) {
                    textPreviewReturnScreen = Screen::LogDetail;
                    currentScreen = Screen::TextPreview;
                    drawTextPreview();
                }
            }
            break;

        case Screen::LogDeleteConfirm:
            if (keys.enter && !logSessions.empty()) {
                const String path =
                    "/ghostwire/logs/" + logSessions[logSelection].name;
                SD.remove(path);
                loadLogSessions();
                currentScreen = Screen::LogSessions;
                drawLogSessions();
            }
            break;

        case Screen::System:
            if (refresh) {
                initSd();
                diagnosticExportStatus = "";
            }
            if (up) moveSelection(-1, kSystemDiagnosticCount);
            if (down) moveSelection(1, kSystemDiagnosticCount);
            if (pressedLetter(keys, 'e')) exportSystemDiagnostics();
            if (keys.enter) {
                currentScreen = Screen::TimeStatus;
                drawTimeStatus();
                return;
            }
            drawSystem();
            break;

        case Screen::TimeStatus:
            if (pressedLetter(keys, 'g')) syncClockFromGnss();
            if (pressedLetter(keys, 'n')) syncClockFromNtp();
            drawTimeStatus();
            break;

        case Screen::GpsMenu:
            if (keys.enter) {
                gnssService.begin();
                currentScreen = Screen::Gnss;
                drawGnss();
                return;
            }
            drawGpsMenu();
            break;

        case Screen::MeshMenu:
            if (keys.enter) {
                currentScreen = Screen::LoRa;
                drawHeader("LoRa Receive");
                M5Cardputer.Display.setTextColor(Branding::warning,
                                                Branding::background);
                M5Cardputer.Display.setCursor(8, 42);
                M5Cardputer.Display.print("Initialising SX1262...");
                loraService.begin();
                drawLoRa();
                return;
            }
            drawMeshMenu();
            break;

        case Screen::WarDrive:
            if (refresh) {
                if (!warDriveService.isActive()) {
                    warDriveService.start();
                    if (sdAvailable) {
                        const String wigleHeader =
                            String("WigleWifi-1.6,appRelease=Ghostwire,model=") +
                            "Cardputer-ADV,release=" + Branding::version +
                            ",device=ESP32-S3,display=1\nMAC,SSID,AuthMode,"
                            "FirstSeen,Channel,RSSI,CurrentLatitude,"
                            "CurrentLongitude,AltitudeMeters,AccuracyMeters,Type";
                        warDriveWifiLogger.begin(
                            "wardrive_wigle", wigleHeader.c_str());
                        warDriveBleLogger.begin(
                            "wardrive_ble",
                            "timestamp_utc,lat,lon,fix,name,address,"
                            "rssi_dbm,connectable,service_uuid");
                    }
                } else {
                    warDriveService.stop();
                    if (warDriveWifiLogger.isActive()) warDriveWifiLogger.stop();
                    if (warDriveBleLogger.isActive()) warDriveBleLogger.stop();
                }
                drawWarDriveDynamic();
                return;
            }
            drawWarDriveDynamic();
            break;

        case Screen::NetworkMenu:
            if (up) moveSelection(-1, 4);
            if (down) moveSelection(1, 4);
            if (keys.enter) {
                if (listSelection == 0) {
                    currentScreen = Screen::NetworkDashboard;
                } else if (listSelection == 1) {
                    currentScreen = Screen::NetworkHostScan;
                } else if (listSelection == 2) {
                    telnetReturnScreen = Screen::NetworkMenu;
                    telnetHostInput = "";
                    telnetStatus = "";
                    currentScreen = Screen::TelnetConnect;
                } else {
                    loadSshHistory();
                    sshHostInput = sshHistory[0];
                    currentScreen = Screen::SshConnect;
                }
                listSelection = 0;
                listOffset = 0;
                drawCurrentScreen();
                return;
            }
            drawNetworkMenu();
            break;

        case Screen::NetworkDashboard:
            if (refresh) drawNetworkDashboard();
            break;

        case Screen::NetworkHostScan:
            if (WiFi.status() != WL_CONNECTED) {
                drawNetworkHostScan();
                break;
            }
            if (refresh) {
                if (!networkHostScanService.isActive()) {
                    networkHostResults.clear();
                    networkHostScanExportStatus = "";
                    listSelection = 0;
                    listOffset = 0;
                    networkHostScanService.start();
                } else {
                    networkHostScanService.stop();
                }
                drawNetworkHostScan();
                return;
            }
            if (pressedLetter(keys, 'e')) {
                exportNetworkHostResults();
                drawNetworkHostScan();
                return;
            }
            if (up) moveSelection(-1, networkHostResults.size());
            if (down) moveSelection(1, networkHostResults.size());
            if (keys.enter && !networkHostResults.empty()) {
                currentScreen = Screen::NetworkPortScan;
                networkPortScanIsFull = false;
                scanNetworkPorts(networkHostResults[listSelection].ip);
                return;
            }
            if (pressedLetter(keys, 'f') && !networkHostResults.empty()) {
                currentScreen = Screen::NetworkPortScan;
                networkPortScanIsFull = true;
                networkPortScanTarget = networkHostResults[listSelection].ip;
                networkPortResults.clear();
                networkPortScanExportStatus = "";
                listSelection = 0;
                listOffset = 0;
                networkPortScanService.start(networkPortScanTarget, 1, 65535);
                drawCurrentScreen();
                return;
            }
            if (pressedLetter(keys, 't') && !networkHostResults.empty()) {
                telnetReturnScreen = Screen::NetworkHostScan;
                telnetHostInput = networkHostResults[listSelection].ip.toString();
                telnetStatus = "";
                currentScreen = Screen::TelnetConnect;
                drawCurrentScreen();
                return;
            }
            drawNetworkHostScan();
            break;

        case Screen::NetworkPortScan:
            if (refresh) {
                if (networkPortScanIsFull) {
                    if (networkPortScanService.isActive()) {
                        networkPortScanService.stop();
                    } else {
                        networkPortResults.clear();
                        networkPortScanExportStatus = "";
                        listSelection = 0;
                        listOffset = 0;
                        networkPortScanService.start(networkPortScanTarget, 1,
                                                     65535);
                    }
                    drawNetworkPortScan();
                } else {
                    scanNetworkPorts(networkPortScanTarget);
                }
                return;
            }
            if (pressedLetter(keys, 'e')) {
                exportNetworkPortResults();
                drawNetworkPortScan();
                return;
            }
            if (up) moveSelection(-1, networkPortResults.size());
            if (down) moveSelection(1, networkPortResults.size());
            drawNetworkPortScan();
            break;

        case Screen::Gnss:
            if (pressedLetter(keys, 'l')) {
                if (gnssLogger.isActive()) {
                    gnssLogger.stop();
                } else if (sdAvailable) {
                    lastGnssLog = 0;
                    gnssLogger.begin(
                        "gnss",
                        "elapsed_ms,timestamp_utc,fix,latitude,longitude,altitude_m,"
                        "satellites,hdop");
                }
            } else if (refresh) {
                gnssService.restart();
            }
            drawGnss();
            break;

        case Screen::LoRa:
            if (pressedLetter(keys, 'l')) {
                if (loraLogger.isActive()) {
                    loraLogger.stop();
                } else if (sdAvailable && loraService.isReady()) {
                    lastLoggedLoRaPacket = loraService.packetCount();
                    loraLogger.begin(
                        "lora",
                        "elapsed_ms,timestamp_utc,packet,profile,frequency_mhz,rssi_dbm,"
                        "snr_db,mesh_from,mesh_to,mesh_id,mesh_port,"
                        "decoded_preview");
                }
            } else if (pressedLetter(keys, 'p')) {
                loraService.toggleProfile();
            } else if (refresh) {
                loraService.restartReceive();
            }
            drawLoRa();
            break;

        case Screen::WifiSniffer:
            if (pressedLetter(keys, 'l')) {
                if (wifiSnifferLogger.isActive()) {
                    wifiSnifferLogger.stop();
                } else if (sdAvailable) {
                    wifiSnifferLogger.begin(
                        "wifi_probes",
                        "timestamp_utc,mac_address,ssid,rssi_dbm,channel");
                }
            } else if (pressedLetter(keys, 'p')) {
                if (wifiPassiveCaptureLogger.isActive()) {
                    wifiPassiveCaptureLogger.stop();
                } else if (sdAvailable) {
                    if (wifiSnifferService.captureMode() ==
                        WifiCaptureMode::Probes) {
                        wifiSnifferService.cycleCaptureMode();
                    }
                    wifiPassiveCaptureLogger.begin(
                        wifiSnifferService.captureMode() ==
                                WifiCaptureMode::Full
                            ? "wifi_full"
                            : "wifi_management");
                }
            } else if (pressedLetter(keys, 'm')) {
                wifiPassiveCaptureLogger.stop();
                wifiSnifferService.cycleCaptureMode();
            } else if (pressedLetter(keys, 'c')) {
                wifiSnifferService.toggleChannelLock();
            } else if (refresh) {
                wifiPassiveCaptureLogger.stop();
                wifiSnifferService.end();
                recentWifiProbes.clear();
                wifiSnifferService.begin();
            }
            drawWifiSniffer();
            break;

        case Screen::Imu:
            if (pressedLetter(keys, 'l')) {
                if (imuLogger.isActive()) {
                    imuLogger.stop();
                } else if (sdAvailable) {
                    lastImuLog = 0;
                    imuLogger.begin(
                        "imu",
                        "elapsed_ms,timestamp_utc,accel_x_g,accel_y_g,accel_z_g,"
                        "gyro_x_dps,gyro_y_dps,gyro_z_dps");
                }
            } else if (pressedLetter(keys, 'c')) {
                beginImuCalibration();
            } else if (refresh) {
                imuAvailable = M5.Imu.isEnabled() ||
                               M5.Imu.begin(nullptr, M5.getBoard());
            }
            drawImu();
            break;

        case Screen::Settings:
            if (up) moveSelection(-1, 4);
            if (down) moveSelection(1, 4);
            if (keys.enter) {
                if (listSelection == 0) currentScreen = Screen::SettingsDisplay;
                else if (listSelection == 1) currentScreen = Screen::SettingsBoot;
                else if (listSelection == 2) currentScreen = Screen::SettingsConnectivity;
                else currentScreen = Screen::SettingsReset;
                listSelection = 0;
                listOffset = 0;
                drawCurrentScreen();
                return;
            }
            drawSettings();
            break;

        case Screen::SettingsDisplay:
            if (up) moveSelection(-1, 5);
            if (down) moveSelection(1, 5);
            if (decrease || increase) {
                const int direction = increase ? 1 : -1;
                if (listSelection == 0) {
                    speakerVolume = static_cast<uint8_t>(std::max(
                        0, std::min(255, static_cast<int>(speakerVolume) +
                                             direction * 16)));
                } else if (listSelection == 1) {
                    screenBrightness = static_cast<uint8_t>(std::max(
                        16, std::min(255, static_cast<int>(screenBrightness) +
                                              direction * 16)));
                } else if (listSelection == 2) {
                    size_t option = 0;
                    while (option + 1 <
                               sizeof(kScreenTimeoutOptions) /
                                   sizeof(kScreenTimeoutOptions[0]) &&
                           kScreenTimeoutOptions[option] !=
                               screenTimeoutSeconds) {
                        ++option;
                    }
                    const size_t count =
                        sizeof(kScreenTimeoutOptions) /
                        sizeof(kScreenTimeoutOptions[0]);
                    option = increase ? (option + 1) % count
                                      : (option + count - 1) % count;
                    screenTimeoutSeconds = kScreenTimeoutOptions[option];
                } else if (listSelection == 3) {
                    cyberdeckIdleEnabled = !cyberdeckIdleEnabled;
                } else if (listSelection == 4) {
                    themeIndex = increase
                                     ? (themeIndex + 1) % Branding::kThemeCount
                                     : (themeIndex + Branding::kThemeCount -
                                        1) %
                                           Branding::kThemeCount;
                    Branding::applyTheme(themeIndex);
                }
                applySettings();
                saveSettings();
                lastUserActivity = millis();
            }
            if (keys.enter && listSelection == 3) {
                cyberdeckIdleEnabled = !cyberdeckIdleEnabled;
                saveSettings();
            }
            drawSettingsDisplay();
            break;

        case Screen::SettingsBoot:
            if (up) moveSelection(-1, 6);
            if (down) moveSelection(1, 6);
            if (decrease || increase) {
                if (listSelection == 0) {
                    bootSoundEnabled = !bootSoundEnabled;
                } else if (listSelection == 1) {
                    bootSoundIndex = static_cast<uint8_t>(increase
                        ? (bootSoundIndex + 1) % kBootSoundCount
                        : (bootSoundIndex + kBootSoundCount - 1) %
                              kBootSoundCount);
                } else if (listSelection == 2) {
                    bootAnimationIndex = static_cast<uint8_t>(increase
                        ? (bootAnimationIndex + 1) % kBootAnimationCount
                        : (bootAnimationIndex + kBootAnimationCount - 1) %
                              kBootAnimationCount);
                } else if (listSelection == 3) {
                    fastBootEnabled = !fastBootEnabled;
                }
                saveSettings();
            }
            if (keys.enter && listSelection == 0) {
                bootSoundEnabled = !bootSoundEnabled;
                saveSettings();
            }
            if (keys.enter && listSelection == 1) {
                bootSoundIndex = static_cast<uint8_t>(
                    (bootSoundIndex + 1) % kBootSoundCount);
                saveSettings();
            }
            if (keys.enter && listSelection == 2) {
                bootAnimationIndex = static_cast<uint8_t>(
                    (bootAnimationIndex + 1) % kBootAnimationCount);
                saveSettings();
            }
            if (keys.enter && listSelection == 3) {
                fastBootEnabled = !fastBootEnabled;
                saveSettings();
            }
            if (keys.enter && listSelection == 4) {
                previewBootSound();
            }
            if (keys.enter && listSelection == 5) {
                previewBootAnimation();
            }
            drawSettingsBoot();
            break;

        case Screen::SettingsConnectivity:
            if (up) moveSelection(-1, 2);
            if (down) moveSelection(1, 2);
            if ((decrease || increase || keys.enter) && listSelection == 0) {
                saveWifiCredentials = !saveWifiCredentials;
                if (!saveWifiCredentials) {
                    autoConnectWifi = false;
                    preferences.remove("wifi_ssid");
                    preferences.remove("wifi_pass");
                    wifiConnectSavedSsid = "";
                    wifiConnectSavedPassword = "";
                }
                saveSettings();
            }
            if ((decrease || increase || keys.enter) && listSelection == 1) {
                if (saveWifiCredentials) autoConnectWifi = !autoConnectWifi;
                saveSettings();
            }
            drawSettingsConnectivity();
            break;

        case Screen::SettingsReset:
            if (keys.enter) {
                restoreDefaultSettings();
                currentScreen = Screen::Settings;
                listSelection = 0;
                drawSettings();
            }
            break;

        case Screen::Placeholder:
        case Screen::About: drawCurrentScreen(); break;

        // These text-entry/session screens return from their dedicated input
        // handlers above before this navigation switch is reached.
        case Screen::QrEntry:
        case Screen::QrDisplay:
        case Screen::WifiConnectPassword:
        case Screen::BleKeyboard:
        case Screen::TelnetConnect:
        case Screen::TelnetSession:
        case Screen::SshConnect:
        case Screen::SshPassword:
        case Screen::SshSession: break;
    }
}

void beginCyberdeckIdle() {
    cyberdeckIdleActive = true;
    lastCyberdeckDraw = 0;
    cyberdeckLastWifiCount = wifiSnifferService.probeCount() +
                             warDriveService.wifiUniqueCount();
    cyberdeckLastBleCount = warDriveService.bleUniqueCount();
    const int rows = std::max(1, (M5Cardputer.Display.height() - 18) / 8);
    for (size_t column = 0; column < kCyberdeckColumns; ++column) {
        cyberdeckRainHead[column] = random(-rows, rows);
        cyberdeckRainSpeed[column] = static_cast<uint8_t>(random(1, 4));
    }
    M5Cardputer.Display.fillScreen(Branding::background);
}

void drawCyberdeckIdle() {
    const unsigned long now = millis();
    if (now - lastCyberdeckDraw < 75) return;
    lastCyberdeckDraw = now;

    auto& display = M5Cardputer.Display;
    const int width = display.width();
    const int height = display.height();
    const int rows = std::max(1, (height - 18) / 8);
    const int columnWidth = std::max(1, width / static_cast<int>(kCyberdeckColumns));
    const uint32_t wifiCount = wifiSnifferService.probeCount() +
                               warDriveService.wifiUniqueCount();
    const uint32_t bleCount = warDriveService.bleUniqueCount();
    const bool radioPulse = wifiCount != cyberdeckLastWifiCount ||
                            bleCount != cyberdeckLastBleCount;
    cyberdeckLastWifiCount = wifiCount;
    cyberdeckLastBleCount = bleCount;

    display.fillRect(0, 0, width, height - 18, Branding::background);
    display.setTextSize(1);
    static constexpr char kGlyphs[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ<>/\\[]{}#@";
    static constexpr size_t kGlyphCount = sizeof(kGlyphs) - 1;
    for (size_t column = 0; column < kCyberdeckColumns; ++column) {
        if ((now / 75U) % cyberdeckRainSpeed[column] == 0) {
            ++cyberdeckRainHead[column];
        }
        if (cyberdeckRainHead[column] - 5 > rows) {
            cyberdeckRainHead[column] = random(-rows, 1);
            cyberdeckRainSpeed[column] = static_cast<uint8_t>(random(1, 4));
        }
        for (int tail = 5; tail >= 0; --tail) {
            const int row = cyberdeckRainHead[column] - tail;
            if (row < 0 || row >= rows) continue;
            const uint16_t colour =
                tail == 0 ? (radioPulse ? Branding::warning : Branding::text)
                          : (tail <= 2 ? Branding::accent : Branding::muted);
            display.setTextColor(colour, Branding::background);
            display.setCursor(static_cast<int>(column) * columnWidth, row * 8);
            display.print(kGlyphs[random(kGlyphCount)]);
        }
    }

    display.fillRect(0, height - 18, width, 18, Branding::panel);
    display.drawFastHLine(0, height - 18, width,
                         radioPulse ? Branding::warning : Branding::accent);
    display.setTextColor(Branding::text, Branding::panel);
    display.setCursor(4, height - 13);
    display.print("GHOSTWIRE");
    display.setTextColor(Branding::muted, Branding::panel);
    display.setCursor(70, height - 13);
    if (WiFi.status() == WL_CONNECTED) {
        display.printf("WiFi %ddBm", WiFi.RSSI());
    } else if (wifiSnifferService.isActive() || warDriveService.isActive()) {
        display.printf("W:%lu B:%lu", static_cast<unsigned long>(wifiCount),
                       static_cast<unsigned long>(bleCount));
    } else {
        display.print("RADIO STANDBY");
    }
    display.setCursor(width - 27, height - 13);
    display.printf("%u%%", batteryPercentage());
}

void observeFamiliarToolScreen() {
    if (currentScreen == lastFamiliarObservedScreen) return;
    lastFamiliarObservedScreen = currentScreen;
    switch (currentScreen) {
        case Screen::WifiRecon: cyberFamiliar.observeTool(0, "Wi-Fi survey"); break;
        case Screen::WifiChannelAnalyzer: cyberFamiliar.observeTool(1, "channel map"); break;
        case Screen::WifiSniffer: cyberFamiliar.observeTool(2, "sniffer"); break;
        case Screen::WarDrive: cyberFamiliar.observeTool(3, "wardrive"); break;
        case Screen::NetworkHostScan: cyberFamiliar.observeTool(4, "host scan"); break;
        case Screen::NetworkPortScan: cyberFamiliar.observeTool(5, "port scan"); break;
        case Screen::TelnetSession: cyberFamiliar.observeTool(6, "Telnet"); break;
        case Screen::SshSession: cyberFamiliar.observeTool(7, "SSH"); break;
        case Screen::BleDiscovery: cyberFamiliar.observeTool(8, "BLE survey"); break;
        case Screen::Chameleon: cyberFamiliar.observeTool(9, "Chameleon"); break;
        case Screen::Gnss: cyberFamiliar.observeTool(10, "GNSS"); break;
        case Screen::LoRa: cyberFamiliar.observeTool(11, "LoRa"); break;
        case Screen::AiChat: cyberFamiliar.observeTool(12, "AI chat"); break;
        case Screen::Files: cyberFamiliar.observeTool(13, "file browser"); break;
        case Screen::Imu: cyberFamiliar.observeTool(14, "IMU"); break;
        case Screen::DuckyScripts: cyberFamiliar.observeTool(15, "script runner"); break;
        case Screen::QrEntry: cyberFamiliar.observeTool(16, "QR generator"); break;
        default: break;
    }
}

}  // namespace

void setup() {
    auto config = M5.config();
    config.output_power = true;
    M5Cardputer.begin(config, true);
    Serial.begin(115200);
    preferences.begin("ghostwire", false);
    speakerVolume = preferences.getUChar("volume", kDefaultVolume);
    screenBrightness =
        preferences.getUChar("brightness", kDefaultBrightness);
    screenTimeoutSeconds =
        preferences.getUShort("timeout", kDefaultScreenTimeout);
    bootSoundEnabled =
        preferences.getBool("boot_sound", kDefaultBootSound);
    fastBootEnabled =
        preferences.getBool("fast_boot", kDefaultFastBoot);
    saveWifiCredentials =
        preferences.getBool("save_wifi", kDefaultSaveWifiCredentials);
    autoConnectWifi =
        preferences.getBool("auto_wifi", kDefaultAutoConnectWifi);
    cyberdeckIdleEnabled =
        preferences.getBool("cyber_idle", kDefaultCyberdeckIdle);
    themeIndex = preferences.getUChar("theme", 0);
    if (themeIndex >= Branding::kThemeCount) themeIndex = 0;
    bootAnimationIndex = preferences.getUChar(
        "boot_anim", kDefaultBootAnimation);
    if (bootAnimationIndex >= kBootAnimationCount) {
        bootAnimationIndex = kDefaultBootAnimation;
    }
    bootSoundIndex = preferences.getUChar(
        "boot_tone", kDefaultBootSoundPreset);
    if (bootSoundIndex >= kBootSoundCount) {
        bootSoundIndex = kDefaultBootSoundPreset;
    }
    Branding::applyTheme(themeIndex);
    if (saveWifiCredentials) {
        wifiConnectSavedSsid = preferences.getString("wifi_ssid", "");
        wifiConnectSavedPassword = preferences.getString("wifi_pass", "");
    } else {
        autoConnectWifi = false;
        preferences.remove("wifi_ssid");
        preferences.remove("wifi_pass");
    }
    applySettings();
    lastUserActivity = millis();

    analogReadResolution(12);
    analogSetPinAttenuation(kBatteryPin, ADC_11db);
    updateBatteryEstimate(true);
    hidService.begin();
    gnssService.begin();
    initSd();
    recordBootTelemetry();
    cyberFamiliar.begin(preferences);
    chameleonSavedPath = preferences.getString("cham_last", "");
    if (isAbnormalReset(esp_reset_reason())) cyberFamiliar.noteRecovery();
    showSplash();
    drawMainMenu();

    if (autoConnectWifi && !wifiConnectSavedSsid.isEmpty()) {
        Serial.printf("[wifi] auto-connecting to %s\n",
                      wifiConnectSavedSsid.c_str());
        startWifiConnection(wifiConnectSavedSsid, wifiConnectSavedPassword,
                            false);
    }

    Serial.printf("%s %s by %s\n", Branding::productName, Branding::version,
                  Branding::creatorName);
    Serial.printf("microSD=%s size=%llu MiB battery=%.2f V\n",
                  sdAvailable ? "ready" : "failed", sdCardSizeMiB,
                  readBatteryVoltage());
}

void loop() {
    M5Cardputer.update();
    observeFamiliarToolScreen();
    static unsigned long lastFamiliarUpdate = 0;
    if (millis() - lastFamiliarUpdate >= 1000) {
        lastFamiliarUpdate = millis();
        FamiliarActivity activity;
        activity.wifiSeen = wifiSnifferService.probeCount() +
                            warDriveService.wifiUniqueCount() +
                            accessPoints.size();
        activity.bleSeen = warDriveService.bleUniqueCount() + bleDevices.size();
        activity.loraPackets = loraService.packetCount();
        activity.gnssFix = gnssService.hasFix();
        activity.wifiConnected = WiFi.status() == WL_CONNECTED;
        activity.batteryPercent = batteryPercentage();
        cyberFamiliar.update(activity);
    }
    if (bootChimePending) {
        const unsigned long now = millis();
        if (now >= bootChimeDeadlineMs) {
            bootChimePending = false;
            bootChimeStatus = "Gave up after 20000ms";
            Serial.println("[boot] chime gave up: speaker never became ready");
        } else if (now >= nextBootChimeAttemptMs) {
            nextBootChimeAttemptMs = now + 250;
            if (M5Cardputer.Speaker.begin()) {
                Serial.printf("[boot] chime ready after %lums\n",
                              static_cast<unsigned long>(now));
                playBootSound();
                bootChimeStatus = "Played after " + String(now) + "ms";
                bootChimePending = false;
            }
        }
    }
    updateBatteryEstimate();
    gnssService.update();
    loraService.update();
    wifiSnifferService.update();
    bleSpamService.update();
    if (!clockSynced && millis() - lastClockSyncAttempt >= 1000) {
        lastClockSyncAttempt = millis();
        syncClockFromGnss();
    }
    updateImu();
    imuLogger.update();
    if (gnssLogger.isActive() && millis() - lastGnssLog >= 1000) {
        lastGnssLog = millis();
        char row[192];
        snprintf(row, sizeof(row), "%lu,%s,%u,%.6f,%.6f,%.1f,%lu,%.1f",
                 static_cast<unsigned long>(millis()),
                 utcTimestamp().c_str(), gnssService.hasFix() ? 1 : 0,
                 gnssService.latitude(), gnssService.longitude(),
                 gnssService.altitudeMetres(),
                 static_cast<unsigned long>(gnssService.satellites()),
                 gnssService.hdop());
        gnssLogger.append(row);
    }
    gnssLogger.update();
    if (loraLogger.isActive() &&
        loraService.packetCount() != lastLoggedLoRaPacket) {
        lastLoggedLoRaPacket = loraService.packetCount();
        const auto& decoded = loraService.lastDecoded();
        String row = String(millis()) + "," + utcTimestamp() + "," +
                     String(loraService.packetCount()) + ",\"" +
                     String(loraService.profileName()) + "\"," +
                     String(loraService.frequencyMhz(), 3) + "," +
                     String(loraService.lastRssi(), 1) + "," +
                     String(loraService.lastSnr(), 1) + ",";
        if (decoded.valid) {
            row += String(decoded.from) + "," + String(decoded.to) + "," +
                   String(decoded.id) + "," + String(decoded.port) + "," +
                   csvSafePayload(decoded.summary);
        } else {
            row += ",,,,\"\"";
        }
        loraLogger.append(row);
    }
    loraLogger.update();

    WifiProbeRecord probeRecord;
    while (wifiSnifferService.nextRecord(probeRecord)) {
        cyberFamiliar.observeWifiIdentity(probeRecord.mac);
        recentWifiProbes.push_back(probeRecord);
        if (recentWifiProbes.size() > kMaxRecentWifiProbes) {
            recentWifiProbes.erase(recentWifiProbes.begin());
        }
        if (wifiSnifferLogger.isActive()) {
            char macText[18];
            snprintf(macText, sizeof(macText), "%02X:%02X:%02X:%02X:%02X:%02X",
                     probeRecord.mac[0], probeRecord.mac[1], probeRecord.mac[2],
                     probeRecord.mac[3], probeRecord.mac[4], probeRecord.mac[5]);
            const String row = utcTimestamp() + "," + macText + "," +
                               csvSafePayload(String(probeRecord.ssid)) + "," +
                               String(probeRecord.rssi) + "," +
                               String(probeRecord.channel);
            wifiSnifferLogger.append(row);
        }
    }
    wifiSnifferLogger.update();
    chameleonLogger.update();

    BleDeviceInfo liveBle;
    while (bleScanner.nextResult(liveBle)) {
        if (liveBle.rssi < bleCaptureRssiFilter) continue;
        cyberFamiliar.observeBleIdentity(liveBle.address);
        auto existing = std::find_if(
            bleDevices.begin(), bleDevices.end(),
            [&liveBle](const BleDeviceInfo& device) {
                return device.address == liveBle.address;
            });
        if (existing == bleDevices.end()) {
            if (bleDevices.size() < 256) {
                bleDevices.push_back(liveBle);
                bleCaptureUiDirty = true;
            }
        } else {
            *existing = liveBle;
        }
        if (bleCaptureLogger.isActive()) {
            const String row =
                utcTimestamp() + "," + csvSafePayload(liveBle.name) + "," +
                csvSafePayload(liveBle.address) + "," +
                String(liveBle.addressType) + "," + String(liveBle.rssi) +
                "," + (liveBle.connectable ? "1" : "0") + "," +
                String(liveBle.advertisementType) + "," +
                String(liveBle.payloadLength) + "," +
                String(liveBle.serviceCount) + "," +
                csvSafePayload(liveBle.service) + "," +
                csvSafePayload(liveBle.manufacturer) + "," +
                csvSafePayload(liveBle.manufacturerData) + "," +
                csvSafePayload(liveBle.payloadData);
            bleCaptureLogger.append(row);
        }
    }
    bleCaptureLogger.update();

    WifiRawFrameRecord rawFrame;
    while (wifiSnifferService.nextRawFrame(rawFrame)) {
        if (wifiPassiveCaptureLogger.isActive()) {
            wifiPassiveCaptureLogger.append(
                rawFrame.data, rawFrame.length,
                static_cast<uint32_t>(time(nullptr)),
                static_cast<uint32_t>(micros() % 1000000UL));
        }
        if (rawFrame.isEapol) {
            ++handshakeEapolFrameCount;
            EapolInfo eapolInfo;
            if (EapolParser::parse(rawFrame.data + rawFrame.eapolOffset,
                                   rawFrame.length - rawFrame.eapolOffset,
                                   eapolInfo)) {
                if (eapolInfo.messageNumber >= 1 &&
                    eapolInfo.messageNumber <= 4) {
                    handshakeMessageSeen[eapolInfo.messageNumber] = true;
                }
                if (eapolInfo.hasPmkid) {
                    handshakePmkidFound = true;
                    memcpy(handshakePmkid, eapolInfo.pmkid,
                           sizeof(handshakePmkid));
                }
            }
            if (handshakeCaptureLogger.isActive()) {
                handshakeCaptureLogger.append(
                    rawFrame.data, rawFrame.length,
                    static_cast<uint32_t>(time(nullptr)),
                    static_cast<uint32_t>(micros() % 1000000UL));
            }
        }
    }
    wifiPassiveCaptureLogger.update();
    handshakeCaptureLogger.update();

    if (screenSleeping) {
        if (M5Cardputer.Keyboard.isChange() &&
            M5Cardputer.Keyboard.isPressed()) {
            screenSleeping = false;
            cyberdeckIdleActive = false;
            familiarIdleActive = false;
            M5Cardputer.Display.setBrightness(screenBrightness);
            lastUserActivity = millis();
            drawCurrentScreen();
        } else if (familiarIdleActive &&
                   millis() - lastFamiliarDraw >= 500) {
            lastFamiliarDraw = millis();
            drawCyberFamiliarIdle();
        } else if (cyberdeckIdleActive) {
            drawCyberdeckIdle();
        }
        delay(10);
        return;
    }

    if (M5Cardputer.Keyboard.isChange() &&
        M5Cardputer.Keyboard.isPressed()) {
        lastUserActivity = millis();
        handleInput(M5Cardputer.Keyboard.keysState());
    }

    if (currentScreen == Screen::AudioPlaying && !audioService.isPlaying()) {
        currentScreen = audioReturnScreen;
        drawCurrentScreen();
    }

    if (currentScreen == Screen::CyberFamiliar && !actionMenuOpen &&
        millis() - lastFamiliarDraw >= 500) {
        lastFamiliarDraw = millis();
        drawCyberFamiliar(false);
    }

    if (currentScreen == Screen::BleDiscovery && !actionMenuOpen &&
        bleScanner.isContinuous() && bleCaptureUiDirty &&
        millis() - lastBleCaptureDraw >= 500) {
        lastBleCaptureDraw = millis();
        bleCaptureUiDirty = false;
        drawBleDiscovery();
    }

    if (currentScreen == Screen::AudioMic) {
        uint16_t level = 0;
        if (audioService.updateMicrophone(level)) {
            microphoneLevel = level;
            if (!screenSleeping && millis() - lastMicrophoneDraw >= 80) {
                lastMicrophoneDraw = millis();
                updateMicrophoneMeter();
            }
        }
    }

    if (currentScreen == Screen::Gnss && !actionMenuOpen &&
        millis() - lastGnssDraw >= 500) {
        lastGnssDraw = millis();
        drawGnss(false);
    }

    if (currentScreen == Screen::LoRa && !actionMenuOpen &&
        millis() - lastLoRaDraw >= 500) {
        lastLoRaDraw = millis();
        drawLoRa(false);
    }

    if (currentScreen == Screen::WifiSniffer && !actionMenuOpen &&
        millis() - lastWifiSnifferDraw >= 500) {
        lastWifiSnifferDraw = millis();
        drawWifiSniffer(false);
    }

    if (currentScreen == Screen::BleSpam && !actionMenuOpen &&
        millis() - lastBleSpamDraw >= 300) {
        lastBleSpamDraw = millis();
        drawBleSpam(false);
    }

    if (currentScreen == Screen::BleKeyboard && !actionMenuOpen &&
        millis() - lastBleKeyboardDraw >= 500) {
        lastBleKeyboardDraw = millis();
        drawBleKeyboard(false);
    }

    if (currentScreen == Screen::Chameleon && chameleonContinuousScan &&
        chameleonClient.isConnected() &&
        millis() - lastChameleonScanMs >= 500) {
        lastChameleonScanMs = millis();
        performChameleonScan();
        if (!actionMenuOpen) drawChameleon(false);
    }

    if (currentScreen == Screen::Chameleon && !actionMenuOpen &&
        !chameleonClient.isConnected() &&
        (chameleonConnectAttempts == 0 ||
         millis() - lastChameleonConnectAttemptMs >=
             kChameleonReconnectIntervalMs)) {
        attemptChameleonConnection();
    }

    if (currentScreen == Screen::BiscuitWardrive &&
        biscuitWardriveActive) {
        const String incoming = biscuitClient.takeNotifications();
        if (!incoming.isEmpty()) appendBiscuitWardriveData(incoming);
        if (!biscuitClient.isConnected()) {
            biscuitWardriveActive = false;
            biscuitWardriveParseTail = "Disconnected";
        }
        if (!actionMenuOpen && millis() - lastBiscuitWardriveDraw >= 250) {
            lastBiscuitWardriveDraw = millis();
            drawBiscuitWardrive(false);
        }
    }

    warDriveService.update();
    WarDriveWifiResult warDriveWifi;
    while (warDriveService.nextWifiResult(warDriveWifi)) {
        cyberFamiliar.observeWifiIdentity(warDriveWifi.bssid);
        logWarDriveWifiResult(warDriveWifi);
    }
    WarDriveBleResult warDriveBle;
    while (warDriveService.nextBleResult(warDriveBle)) {
        cyberFamiliar.observeBleIdentity(warDriveBle.address);
        logWarDriveBleResult(warDriveBle);
    }

    if (currentScreen == Screen::WarDrive && !actionMenuOpen &&
        millis() - lastWarDriveDraw >= 500) {
        lastWarDriveDraw = millis();
        drawWarDriveDynamic();
    }

    networkHostScanService.update();
    NetworkHostResult networkHostResult;
    while (networkHostScanService.nextHostResult(networkHostResult)) {
        networkHostResults.push_back(networkHostResult);
    }
    if (currentScreen == Screen::NetworkHostScan && !actionMenuOpen &&
        millis() - lastNetworkHostScanDraw >= 500) {
        lastNetworkHostScanDraw = millis();
        drawNetworkHostScan(false);
    }

    networkPortScanService.update();
    NetworkPortResult networkPortResult;
    while (networkPortScanService.nextPortResult(networkPortResult)) {
        networkPortResults.push_back(networkPortResult.port);
    }
    if (currentScreen == Screen::NetworkPortScan && !actionMenuOpen &&
        millis() - lastNetworkPortScanDraw >= 500) {
        lastNetworkPortScanDraw = millis();
        drawNetworkPortScan(false);
    }

    if (currentScreen == Screen::TelnetSession) {
        bool newData = false;
        while (telnetClient.connected() && telnetClient.available()) {
            const int value = telnetClient.read();
            if (value < 0) break;
            appendTerminalByte(static_cast<char>(value), telnetLines,
                              telnetPendingLine, telnetEscState,
                              kMaxTelnetLines);
            newData = true;
        }
        if (millis() - lastTelnetDraw >= 150) {
            lastTelnetDraw = millis();
            if (newData) drawTelnetSessionDynamic();
            drawFooter(telnetClient.connected() ? "Esc: disconnect"
                                                : "Disconnected   Esc: back");
        }
    }

    if (currentScreen == Screen::SshSession) {
        bool newData = false;
        uint8_t buffer[256];
        while (sshService.isConnected()) {
            const int got = sshService.read(buffer, sizeof(buffer));
            if (got <= 0) break;
            for (int i = 0; i < got; ++i) {
                if (!sshLocalEchoPending.isEmpty()) {
                    const char incoming = static_cast<char>(buffer[i]);
                    const char expected = sshLocalEchoPending[0];
                    const bool equivalentBackspace =
                        (incoming == '\b' ||
                         static_cast<uint8_t>(incoming) == 0x7F) &&
                        (expected == '\b' ||
                         static_cast<uint8_t>(expected) == 0x7F);
                    if (incoming == expected || equivalentBackspace) {
                        sshLocalEchoPending.remove(0, 1);
                        continue;
                    }
                    // The remote used a different echo/control sequence.
                    // Stop suppressing rather than swallowing later output
                    // that merely happens to match stale typed text.
                    sshLocalEchoPending = "";
                }
                appendTerminalByte(static_cast<char>(buffer[i]), sshLines,
                                  sshPendingLine, sshEscState, kMaxSshLines);
            }
            newData = true;
        }
        if (millis() - lastSshDraw >= 150) {
            lastSshDraw = millis();
            if (newData) drawSshSessionDynamic();
            drawFooter(sshService.isConnected() ? "Esc: disconnect"
                                                : "Disconnected   Esc: back");
        }
    }

    if (wifiConnectAttempting) {
        const wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED) {
            wifiConnectAttempting = false;
            wifiConnectStatusText = "Connected";
            if (saveWifiCredentials) {
                preferences.putString("wifi_ssid", wifiConnectSsid);
                preferences.putString("wifi_pass", wifiConnectAttemptPassword);
                wifiConnectSavedSsid = wifiConnectSsid;
                wifiConnectSavedPassword = wifiConnectAttemptPassword;
            }
            for (size_t i = 0; i < wifiConnectPasswordInput.length(); ++i) {
                wifiConnectPasswordInput.setCharAt(i, '\0');
            }
            wifiConnectPasswordInput = "";
            for (size_t i = 0; i < wifiConnectAttemptPassword.length(); ++i) {
                wifiConnectAttemptPassword.setCharAt(i, '\0');
            }
            wifiConnectAttemptPassword = "";
        } else if (status == WL_CONNECT_FAILED) {
            wifiConnectAttempting = false;
            wifiConnectStatusText = "Failed: wrong password?";
        } else if (status == WL_NO_SSID_AVAIL) {
            wifiConnectAttempting = false;
            wifiConnectStatusText = "Failed: network not found";
        } else if (millis() - wifiConnectStartMs > 15000) {
            wifiConnectAttempting = false;
            wifiConnectStatusText = "Failed: timed out";
        }
        if (!wifiConnectAttempting && WiFi.status() != WL_CONNECTED) {
            for (size_t i = 0; i < wifiConnectPasswordInput.length(); ++i) {
                wifiConnectPasswordInput.setCharAt(i, '\0');
            }
            wifiConnectPasswordInput = "";
            for (size_t i = 0; i < wifiConnectAttemptPassword.length(); ++i) {
                wifiConnectAttemptPassword.setCharAt(i, '\0');
            }
            wifiConnectAttemptPassword = "";
        }
    }

    if (currentScreen == Screen::WifiConnectStatus && !actionMenuOpen &&
        millis() - lastWifiConnectDraw >= 1000) {
        lastWifiConnectDraw = millis();
        drawWifiConnectStatus(false);
    }

    if (currentScreen == Screen::WifiHandshakeCapture && !actionMenuOpen &&
        millis() - lastHandshakeCaptureDraw >= 500) {
        lastHandshakeCaptureDraw = millis();
        drawWifiHandshakeCapture(false);
    }

    if (currentScreen == Screen::Imu && !actionMenuOpen &&
        millis() - lastImuDraw >= 100) {
        lastImuDraw = millis();
        drawImu(false);
    }

    if (millis() - lastHeaderStatusDraw >= 1000) {
        lastHeaderStatusDraw = millis();
        drawHeaderStatus();
    }

    if (currentScreen == Screen::TimeStatus && !actionMenuOpen &&
        millis() - lastTimeStatusDraw >= 1000) {
        lastTimeStatusDraw = millis();
        drawTimeReadouts();
    }

    if (!screenSleeping && screenTimeoutSeconds > 0 &&
        millis() - lastUserActivity >= screenTimeoutSeconds * 1000UL) {
        screenSleeping = true;
        if (cyberFamiliar.idleMode()) {
            familiarIdleActive = true;
            cyberdeckIdleActive = false;
            lastFamiliarDraw = 0;
            drawCyberFamiliarIdle();
        } else if (cyberdeckIdleEnabled) {
            familiarIdleActive = false;
            beginCyberdeckIdle();
            drawCyberdeckIdle();
        } else {
            cyberdeckIdleActive = false;
            familiarIdleActive = false;
            M5Cardputer.Display.setBrightness(0);
        }
    }
    delay(10);
}
