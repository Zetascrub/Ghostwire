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
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <sys/time.h>
#include <time.h>
#include <vector>

#include "branding.h"
#include "app_screen.h"
#include "file_screens.h"
#include "ble_scanner.h"
#include "ble_keyboard_service.h"
#include "ble_screens.h"
#include "ble_spam_service.h"
#include "ble_spam_screen.h"
#include "biscuit_pro_client.h"
#include "chameleon_ultra_client.h"
#include "device_screens.h"
#include "cyber_familiar.h"
#include "familiar_screens.h"
#include "menu_screens.h"
#include "settings_names.h"
#include "settings_screens.h"
#include "ota_service.h"
#include "ota_screens.h"
#include "system_screens.h"
#include "eapol_parser.h"
#include "familiar_patrol_service.h"
#include "ir_service.h"
#include "ir_screen.h"
#include "screen_chrome.h"
#include "hid_service.h"
#include "hid_presets.h"
#include "usb_hid_screens.h"
#include "audio_screens.h"
#include "familiar_phrases.h"
#include "qr_screens.h"
#include "audio_service.h"
#include "ai_service.h"
#include "gnss_service.h"
#include "gnss_screen.h"
#include "lora_service.h"
#include "lora_screen.h"
#include "network_host_scan_service.h"
#include "network_port_scan_service.h"
#include "network_scan_screens.h"
#include "ssh_service.h"
#include "ssh_screens.h"
#include "telnet_screens.h"
#include "terminal_buffer.h"
#include "pcap_logger.h"
#include "sd_logger.h"
#include "imu_screen.h"
#include "war_drive_service.h"
#include "wifi_sniffer_service.h"
#include "wifi_sniffer_screen.h"
#include "wifi_screens.h"
#include "wifi_guardian_service.h"
#include "wifi_guardian_screen.h"

namespace {

constexpr int kBatteryPin = 10;
constexpr int kSdCs = 12;
constexpr int kSdMosi = 14;
constexpr int kSdClock = 40;
constexpr int kSdMiso = 39;
constexpr int kSdCompatibilityPin = 5;
constexpr uint32_t kSdFrequency = 4000000;
constexpr size_t kVisibleRows = 6;

// FileEntry / LogEntry: see include/file_screens.h.

// One-off per-screen actions (export, deauth, disconnect, etc.) live in a
// Tab-triggered menu instead of individually memorized letters -- see
// actionsForScreen() and the handleInput() menu-mode block.
struct ActionMenuItem {
    char key;
    String label;
};

// Main menu items: see MenuScreens::kMenuItems/kMenuCount.
// HID preset labels: see include/hid_presets.h.

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
Screen evidenceReturnScreen = Screen::MainMenu;
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
// True when the running OTA partition is still in the bootloader's
// ESP_OTA_IMG_PENDING_VERIFY state (see verifyOtaBootOrRollback()/
// markBootHealthy() in setup()).
bool otaPendingVerify = false;
constexpr uint8_t kMaxBootAttemptsBeforeRollback = 3;
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
FileScreens fileScreens(files, listSelection, listOffset, currentPath,
                        sdAvailable, previewLines, previewTopLine,
                        previewColumn, previewTruncated, currentScreen);
LogScreens logScreens(logSessions, logSelection, logOffset, sdAvailable,
                      selectedLogRows, currentScreen);
std::vector<wifi_ap_record_t> accessPoints;
std::vector<BleDeviceInfo> bleDevices;
std::vector<WifiProbeRecord> recentWifiProbes;
BleScanner bleScanner;
BleKeyboardService bleKeyboardService;
BleScreens bleScreens(bleDevices, listSelection, listOffset, currentScreen,
                      bleStatus, bleExportStatus, bleScanner,
                      bleKeyboardService);
BleSpamService bleSpamService;
BleSpamScreen bleSpamScreen(bleSpamService);
BiscuitProClient biscuitClient;
String biscuitResultTitle;
std::vector<String> biscuitResultLines;
size_t biscuitResultOffset = 0;
bool biscuitWardriveActive = false;
uint32_t biscuitWardriveApCount = 0;
uint32_t biscuitWardriveBleCount = 0;
BiscuitScreens biscuitScreens(biscuitClient, listSelection, listOffset,
                              biscuitWardriveActive, biscuitWardriveApCount,
                              biscuitWardriveBleCount, biscuitResultTitle,
                              biscuitResultLines, biscuitResultOffset);
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
TelnetScreens telnetScreens(telnetHostInput, telnetStatus, telnetClient,
                            telnetHost, telnetPort, telnetLines,
                            telnetPendingLine);
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
SshScreens sshScreens(sshHostInput, sshStatus, sshUsername, sshHost, sshPort,
                      sshPasswordInput, sshService, sshLines, sshPendingLine);
TerminalEscState sshEscState = TerminalEscState::None;
unsigned long lastSshDraw = 0;
bool sshTrustPending = false;
String sshLocalEchoPending;
String sshHistory[3];
size_t sshHistoryIndex = 0;
IrService irService;
IrScreen irScreen(irService);
HidService hidService;
AudioService audioService;
AiService aiService;
CyberFamiliar cyberFamiliar;
FamiliarPatrolService familiarPatrolService;
FamiliarPatrolState lastFamiliarPatrolState = FamiliarPatrolState::Idle;
unsigned long lastFamiliarPatrolDraw = 0;
bool familiarPatrolContinuousChoice = false;
uint8_t familiarPatrolIntervalIndex = 1;
// kFamiliarPatrolIntervals / FamiliarReaction: see include/familiar_screens.h.
uint8_t familiarPage = 0;
unsigned long lastFamiliarDraw = 0;
bool familiarIdleActive = false;
bool familiarIdleDrawn = false;
FamiliarReaction familiarReaction = FamiliarReaction::None;
unsigned long familiarReactionUntil = 0;
uint32_t familiarObservedPatrolHosts = 0;
uint32_t familiarObservedOpenPorts = 0;
String familiarSpeechBubble;
unsigned long familiarSpeechBubbleUntil = 0;
unsigned long lastFamiliarCueMs = 0;
void triggerFamiliarReaction(FamiliarReaction reaction, uint32_t durationMs);
void showFamiliarSpeech(const String& message, uint32_t durationMs);
Screen lastFamiliarObservedScreen = Screen::MainMenu;
String aiPrompt;
String ttsLabStatus = "Word bank ready";
uint8_t ttsLabPhrase = 0;
uint32_t ttsLabPlaybackMs = 0;
String aiNotice;
size_t aiScrollLines = 0;
String familiarWorkflowStatus;
AiChatScreen aiChatScreen(aiService, aiPrompt, aiNotice, aiScrollLines);
FamiliarScreens familiarScreens(
    cyberFamiliar, familiarPatrolService, familiarPage, familiarWorkflowStatus,
    familiarReaction, familiarReactionUntil, familiarSpeechBubble,
    familiarSpeechBubbleUntil, familiarPatrolContinuousChoice,
    familiarPatrolIntervalIndex, sdAvailable);
constexpr char kAiSpeechPath[] = "/ghostwire/audio/ai_reply.mp3";
// Familiar phrase word bank: see include/familiar_phrases.h.
constexpr char kFamiliarAudioPath[] = "/ghostwire/audio/Familiar/";
String familiarWordPath(const char* word) {
    return String(kFamiliarAudioPath) + word + ".mp3";
}
int8_t familiarVoicePhrase = -1;
int8_t familiarVoicePending = -1;
uint8_t familiarVoiceWord = 0;
unsigned long familiarVoiceReadyMs = 0;
constexpr char kAiRecordingPath[] = "/ghostwire/ai_voice.wav";
GnssService gnssService;
SdLogger gnssLogger;
GnssScreen gnssScreen(gnssService, gnssLogger);
NetworkScanScreens networkScanScreens(
    warDriveService, gnssService, listSelection, listOffset,
    networkHostScanService, networkHostResults, networkHostScanExportStatus,
    networkPortScanService, networkPortResults, networkPortScanExportStatus,
    networkPortScanTarget, networkPortScanIsFull);
LoRaService loraService;
WifiSnifferService wifiSnifferService;
WifiGuardianService wifiGuardianService;
SdLogger imuLogger;
SdLogger loraLogger;
LoRaScreen loraScreen(loraService, loraLogger);
SdLogger wifiSnifferLogger;
SdLogger guardianEventLogger;
SdLogger chameleonLogger;
ChameleonScreen chameleonScreen(chameleonClient, chameleonHasReadings,
                                chameleonAppMajor, chameleonAppMinor,
                                chameleonBatteryMv, chameleonBatteryPct,
                                chameleonScanAttempted, chameleonHfFound,
                                chameleonHfTag, chameleonLfFound, chameleonLfId,
                                chameleonWorkflowStatus, chameleonContinuousScan,
                                chameleonLogger);
SdLogger bleCaptureLogger;
PcapLogger wifiPassiveCaptureLogger;
WifiSnifferScreen wifiSnifferScreen(wifiSnifferService,
                                    wifiPassiveCaptureLogger,
                                    recentWifiProbes);
PcapLogger guardianEvidenceLogger;
PcapLogger handshakeCaptureLogger;
Preferences preferences;
std::vector<String> audioFiles;
std::vector<String> duckyScripts;
size_t duckyCommandCount = 0;
size_t duckyUnsupportedCount = 0;
uint32_t duckyDeclaredDelayMs = 0;
String duckyRunStatus;
uint16_t microphoneLevel = 0;
UsbHidScreens usbHidScreens(listSelection, listOffset, duckyScripts,
                           duckyCommandCount, duckyUnsupportedCount,
                           duckyDeclaredDelayMs, duckyRunStatus);
AudioScreens audioScreens(listSelection, listOffset, ttsLabPhrase, ttsLabStatus,
                          ttsLabPlaybackMs, microphoneLevel, audioFiles);
QrScreens qrScreens(qrText);
int bleCaptureRssiFilter = -100;
unsigned long lastBleCaptureDraw = 0;
bool bleCaptureUiDirty = false;
unsigned long lastMicrophoneDraw = 0;
unsigned long lastUserActivity = 0;
uint8_t speakerVolume = 96;
uint8_t screenBrightness = 128;
uint16_t screenTimeoutSeconds = 30;
bool bootSoundEnabled = true;
uint8_t bootSpeedIndex = 1;
bool saveWifiCredentials = false;
bool autoConnectWifi = false;
bool cyberdeckIdleEnabled = false;
uint8_t cyberdeckIdleStyle = 0;
bool cardNavigationEnabled = false;
MenuScreens menuScreens(listSelection, listOffset, menuSelection,
                        cardNavigationEnabled, cyberFamiliar,
                        wifiGuardianService, familiarPatrolService,
                        warDriveService, sdAvailable);
size_t themeIndex = 0;
uint8_t bootAnimationIndex = 0;
uint8_t bootSoundIndex = 0;
uint8_t familiarCueIndex = 0;
SettingsScreens settingsScreens(
    listSelection, listOffset, speakerVolume, screenBrightness,
    screenTimeoutSeconds, cyberdeckIdleEnabled, themeIndex, familiarCueIndex,
    cardNavigationEnabled, cyberdeckIdleStyle, bootSoundEnabled,
    bootSoundIndex, bootAnimationIndex, bootSpeedIndex, saveWifiCredentials,
    autoConnectWifi, placeholderTitle);
bool screenSleeping = false;
bool cyberdeckIdleActive = false;
constexpr size_t kCyberdeckColumns = 40;
int16_t cyberdeckRainHead[kCyberdeckColumns]{};
uint8_t cyberdeckRainSpeed[kCyberdeckColumns]{};
unsigned long lastCyberdeckDraw = 0;
uint32_t cyberdeckLastWifiCount = 0;
uint32_t cyberdeckLastBleCount = 0;
M5Canvas cyberdeckIdleCanvas(&M5Cardputer.Display);
bool cyberdeckIdleCanvasReady = false;
constexpr size_t kIdleNodeCount = 12;
int16_t idleNodeX[kIdleNodeCount]{};
int16_t idleNodeY[kIdleNodeCount]{};
int8_t idleNodeDx[kIdleNodeCount]{};
int8_t idleNodeDy[kIdleNodeCount]{};
bool clockSynced = false;
String clockStatus = "Waiting for GNSS UTC";
SystemScreens systemScreens(listSelection, listOffset, diagnosticExportStatus,
                            clockSynced, clockStatus);
OtaService otaService;
OtaScreens otaScreens(otaService);
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
unsigned long lastGuardianDraw = 0;
String guardianLastEvent = "No alerts observed";
WifiGuardianScreen wifiGuardianScreen(wifiGuardianService, wifiSnifferService,
                                      guardianEvidenceLogger,
                                      guardianLastEvent);
uint8_t guardianLastChannel = 0;
int8_t guardianLastRssi = 0;
unsigned long lastBleSpamDraw = 0;
unsigned long lastBleKeyboardDraw = 0;
constexpr size_t kMaxRecentWifiProbes = 8;
unsigned long lastHandshakeCaptureDraw = 0;
uint32_t handshakeEapolFrameCount = 0;
bool handshakeMessageSeen[5] = {};  // Index 1-4 used; 0 unused.
bool handshakePmkidFound = false;
uint8_t handshakePmkid[16] = {};
WifiScreens wifiScreens(accessPoints, listSelection, listOffset, currentScreen,
                        wifiStatus, wifiExportStatus, wifiDeauthStatus,
                        wifiSnifferService, handshakeEapolFrameCount,
                        handshakeMessageSeen, handshakePmkidFound,
                        handshakePmkid, handshakeCaptureLogger,
                        wifiConnectSavedSsid, wifiConnectSsid,
                        wifiConnectPasswordInput, wifiConnectStatusText);
unsigned long lastImuDraw = 0;
unsigned long lastImuLog = 0;
m5::imu_data_t imuData{};
bool imuAvailable = false;
bool imuCalibrating = false;
uint16_t imuCalibrationSamples = 0;
float gyroOffsetX = 0.0F;
float gyroOffsetY = 0.0F;
float gyroOffsetZ = 0.0F;
ImuScreen imuScreen(imuAvailable, imuData, imuCalibrating,
                    imuCalibrationSamples, gyroOffsetX, gyroOffsetY,
                    gyroOffsetZ, imuLogger);
float gyroCalibrationSumX = 0.0F;
float gyroCalibrationSumY = 0.0F;
float gyroCalibrationSumZ = 0.0F;

constexpr uint8_t kDefaultVolume = 96;
constexpr uint8_t kDefaultBrightness = 128;
constexpr uint16_t kDefaultScreenTimeout = 30;
constexpr bool kDefaultBootSound = true;
constexpr uint8_t kDefaultBootSpeed = 1;
constexpr bool kDefaultSaveWifiCredentials = false;
constexpr bool kDefaultAutoConnectWifi = false;
constexpr bool kDefaultCyberdeckIdle = false;
constexpr uint8_t kDefaultCyberdeckIdleStyle = 0;
constexpr bool kDefaultCardNavigation = false;
constexpr uint8_t kDefaultBootAnimation = 0;
constexpr uint8_t kDefaultBootSoundPreset = 0;
constexpr uint8_t kDefaultFamiliarCue = 0;
constexpr uint16_t kScreenTimeoutOptions[] = {0, 15, 30, 60, 120};
constexpr size_t kSystemDiagnosticCount = 22;
// Boot/settings name tables: see include/settings_names.h.

String csvSafePayload(const String& payload);
String utcTimestamp();
void drawCurrentScreen();
void beginCyberdeckIdle();
void drawCyberdeckIdle();

void applySettings() {
    audioService.setVolume(speakerVolume);
    M5Cardputer.Display.setBrightness(screenBrightness);
}

void saveSettings() {
    preferences.putUChar("volume", speakerVolume);
    preferences.putUChar("brightness", screenBrightness);
    preferences.putUShort("timeout", screenTimeoutSeconds);
    preferences.putBool("boot_sound", bootSoundEnabled);
    preferences.putUChar("boot_speed", bootSpeedIndex);
    preferences.remove("fast_boot");
    preferences.putBool("save_wifi", saveWifiCredentials);
    preferences.putBool("auto_wifi", autoConnectWifi);
    preferences.putBool("cyber_idle", cyberdeckIdleEnabled);
    preferences.putUChar("idle_style", cyberdeckIdleStyle);
    preferences.putBool("nav_cards", cardNavigationEnabled);
    preferences.putUChar("theme", static_cast<uint8_t>(themeIndex));
    preferences.putUChar("boot_anim", bootAnimationIndex);
    preferences.putUChar("boot_tone", bootSoundIndex);
    preferences.putUChar("fam_cue", familiarCueIndex);
}

void restoreDefaultSettings() {
    speakerVolume = kDefaultVolume;
    screenBrightness = kDefaultBrightness;
    screenTimeoutSeconds = kDefaultScreenTimeout;
    bootSoundEnabled = kDefaultBootSound;
    bootSpeedIndex = kDefaultBootSpeed;
    saveWifiCredentials = kDefaultSaveWifiCredentials;
    autoConnectWifi = kDefaultAutoConnectWifi;
    cyberdeckIdleEnabled = kDefaultCyberdeckIdle;
    cyberdeckIdleStyle = kDefaultCyberdeckIdleStyle;
    cardNavigationEnabled = kDefaultCardNavigation;
    preferences.remove("wifi_ssid");
    preferences.remove("wifi_pass");
    wifiConnectSavedSsid = "";
    wifiConnectSavedPassword = "";
    themeIndex = 0;
    bootAnimationIndex = kDefaultBootAnimation;
    bootSoundIndex = kDefaultBootSoundPreset;
    familiarCueIndex = kDefaultFamiliarCue;
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

// Queries the public repo's latest release (see OtaService::checkForUpdate)
// and lands on Screen::OtaCheck either way -- same "please wait" blocking
// convention as scanWifiNetworks()/connectSsh().
void checkForFirmwareUpdate() {
    currentScreen = Screen::OtaCheck;
    ScreenChrome::drawHeader("Firmware Update");
    M5Cardputer.Display.setTextColor(Branding::warning, Branding::background);
    M5Cardputer.Display.setCursor(8, 36);
    M5Cardputer.Display.print("Checking for updates...");
    ScreenChrome::drawFooter("Please wait");
    otaService.checkForUpdate(Branding::version);
    recoverKeyboardAfterBlockingOperation();
    drawCurrentScreen();
}

// Downloads, verifies, and flashes the release found by a prior
// checkForFirmwareUpdate(). Blocking, with periodic progress redraws and
// Esc-to-cancel polling inside the progress callback -- same pattern as
// waitDuckyDelay(). Reboots on success; otherwise returns to Screen::OtaCheck
// with the failure reason so the operator can retry or back out.
void installFirmwareUpdate() {
    currentScreen = Screen::OtaInstalling;
    otaScreens.drawInstalling();
    unsigned long lastRedrawMs = 0;
    const OtaService::InstallResult result = otaService.downloadAndInstall(
        [&](size_t /*downloaded*/, size_t /*total*/) -> bool {
            M5Cardputer.update();
            if (M5Cardputer.Keyboard.isChange() &&
                M5Cardputer.Keyboard.isPressed() &&
                M5Cardputer.Keyboard.keysState().esc) {
                return false;
            }
            const unsigned long now = millis();
            if (now - lastRedrawMs >= 150) {
                lastRedrawMs = now;
                otaScreens.drawInstalling(false);
            }
            return true;
        });
    recoverKeyboardAfterBlockingOperation();
    if (result == OtaService::InstallResult::Success) {
        ScreenChrome::drawHeader("Firmware Update");
        M5Cardputer.Display.setTextColor(Branding::accent, Branding::background);
        M5Cardputer.Display.setCursor(8, 40);
        M5Cardputer.Display.print("Update installed.");
        M5Cardputer.Display.setCursor(8, 58);
        M5Cardputer.Display.print("Restarting...");
        delay(1500);
        ESP.restart();
        return;
    }
    currentScreen = Screen::OtaCheck;
    drawCurrentScreen();
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
                               wifiGuardianService.isActive() ||
                               bleSpamService.isActive() ||
                               warDriveService.isActive() ||
                               handshakeCaptureLogger.isActive() ||
                               bleCaptureLogger.isActive() ||
                               gnssLogger.isActive() ||
                               loraLogger.isActive() ||
                               imuLogger.isActive() ||
                               familiarPatrolService.isActive();
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
            return {{'a', familiarPatrolService.isActive()
                              ? "Open active patrol"
                              : "Start Familiar Patrol"},
                    {'p', "Pet familiar"},
                    {'n', "Choose next name"},
                    {'i', cyberFamiliar.idleMode() ? "Disable idle watch"
                                                   : "Enable idle watch"},
                    {'w', "Start idle watch now"},
                    {'x', "Export familiar record"},
                    {'g', "Import capture logs"},
                    {'z', "Reset familiar progress"}};
        case Screen::FamiliarPatrol:
            if (familiarPatrolService.isActive()) {
                return {{'x', "Stop patrol"}};
            }
            return {};
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
    const int visibleCount = std::min(itemCount, 6);
    const int firstVisible = std::max(
        0, std::min(static_cast<int>(actionMenuSelection) - visibleCount + 1,
                    itemCount - visibleCount));
    const int rowHeight = 16;
    const int boxHeight = visibleCount * rowHeight + 10;
    const int boxWidth = display.width() - 40;
    const int boxX = 20;
    const int boxY = std::max(22, (display.height() - boxHeight) / 2);

    display.fillRect(boxX, boxY, boxWidth, boxHeight, Branding::panel);
    display.drawRect(boxX, boxY, boxWidth, boxHeight, Branding::accent);
    for (int row = 0; row < visibleCount; ++row) {
        const int i = firstVisible + row;
        const bool selected = static_cast<size_t>(i) == actionMenuSelection;
        const int rowY = boxY + 5 + row * rowHeight;
        display.fillRect(boxX + 2, rowY, boxWidth - 4, rowHeight - 2,
                         selected ? Branding::accent : Branding::panel);
        display.setTextColor(selected ? Branding::background : Branding::text,
                             selected ? Branding::accent : Branding::panel);
        display.setCursor(boxX + 8, rowY + 3);
        display.print(actionMenuItems[i].label);
    }
}

void drawNavigationIcon(uint8_t icon, int cx, int cy) {
    auto& display = M5Cardputer.Display;
    const uint16_t color = Branding::accent;
    display.drawRoundRect(cx - 26, cy - 26, 52, 52, 9, Branding::muted);
    switch (icon) {
        case 0:  // Familiar face
            display.fillTriangle(cx - 18, cy - 10, cx - 11, cy - 23,
                                 cx - 4, cy - 9, color);
            display.fillTriangle(cx + 4, cy - 9, cx + 11, cy - 23,
                                 cx + 18, cy - 10, color);
            display.drawRoundRect(cx - 19, cy - 12, 38, 30, 8, color);
            display.fillCircle(cx - 8, cy, 2, color);
            display.fillCircle(cx + 8, cy, 2, color);
            display.drawFastHLine(cx - 5, cy + 9, 10, color);
            break;
        case 1:  // Radio waves
            display.fillCircle(cx, cy + 6, 3, color);
            display.drawCircle(cx, cy + 6, 10, color);
            display.drawCircle(cx, cy + 6, 17, color);
            display.drawFastVLine(cx, cy - 17, 23, color);
            break;
        case 2:  // Network nodes
            display.fillCircle(cx, cy - 15, 5, color);
            display.fillCircle(cx - 17, cy + 14, 5, color);
            display.fillCircle(cx + 17, cy + 14, 5, color);
            display.drawLine(cx, cy - 10, cx - 14, cy + 10, color);
            display.drawLine(cx, cy - 10, cx + 14, cy + 10, color);
            display.drawFastHLine(cx - 12, cy + 14, 24, color);
            break;
        case 3:  // Evidence folder
            display.fillRect(cx - 20, cy - 10, 40, 27, color);
            display.fillRect(cx - 17, cy - 16, 17, 7, color);
            display.drawFastHLine(cx - 13, cy - 2, 26,
                                  Branding::background);
            display.drawFastHLine(cx - 13, cy + 5, 20,
                                  Branding::background);
            break;
        case 4:  // Field kit
            display.drawRoundRect(cx - 20, cy - 11, 40, 29, 4, color);
            display.drawRect(cx - 9, cy - 18, 18, 8, color);
            display.drawFastHLine(cx - 19, cy, 38, color);
            display.fillRect(cx - 4, cy - 3, 8, 7, color);
            break;
        case 5:  // Settings cog
            display.drawCircle(cx, cy, 17, color);
            display.drawCircle(cx, cy, 7, color);
            display.drawFastHLine(cx - 24, cy, 48, color);
            display.drawFastVLine(cx, cy - 24, 48, color);
            display.drawLine(cx - 17, cy - 17, cx + 17, cy + 17, color);
            display.drawLine(cx + 17, cy - 17, cx - 17, cy + 17, color);
            break;
        case 6:  // Bluetooth
            display.drawFastVLine(cx, cy - 22, 44, color);
            display.drawLine(cx, cy - 22, cx + 14, cy - 9, color);
            display.drawLine(cx + 14, cy - 9, cx - 10, cy + 12, color);
            display.drawLine(cx - 10, cy - 12, cx + 14, cy + 9, color);
            display.drawLine(cx + 14, cy + 9, cx, cy + 22, color);
            break;
        case 7:  // Position marker
            display.drawCircle(cx, cy - 5, 16, color);
            display.fillCircle(cx, cy - 5, 5, color);
            display.fillTriangle(cx - 12, cy + 5, cx + 12, cy + 5,
                                 cx, cy + 23, color);
            break;
        case 8:  // AI notes
            display.drawRoundRect(cx - 20, cy - 19, 40, 37, 5, color);
            display.fillCircle(cx - 8, cy - 4, 2, color);
            display.fillCircle(cx + 8, cy - 4, 2, color);
            display.drawFastHLine(cx - 10, cy + 7, 20, color);
            break;
        default:  // Utilities
            display.drawLine(cx - 17, cy + 17, cx + 17, cy - 17, color);
            display.drawCircle(cx - 13, cy + 13, 7, color);
            display.drawCircle(cx + 13, cy - 13, 7, color);
            break;
    }
}

void drawNavigationCard(const char* header, const String& label,
                        const String& description, size_t selected,
                        size_t count, uint8_t icon,
                        const String& badge = "") {
    drawHeader(header);
    drawHeaderPosition(selected + 1, count);
    auto& display = M5Cardputer.Display;
    drawNavigationIcon(icon, 39, 65);
    display.setTextSize(2);
    display.setTextColor(Branding::text, Branding::background);
    String titleFirst = label;
    String titleSecond;
    int descriptionY = 57;
    if (titleFirst.length() > 13) {
        int split = titleFirst.substring(0, 14).lastIndexOf(' ');
        if (split < 5) split = 13;
        titleSecond = titleFirst.substring(split + 1);
        titleFirst = titleFirst.substring(0, split);
        descriptionY = 68;
    }
    display.setCursor(76, titleSecond.isEmpty() ? 31 : 25);
    display.print(titleFirst.substring(0, 13));
    if (!titleSecond.isEmpty()) {
        display.setCursor(76, 43);
        display.print(titleSecond.substring(0, 13));
    }
    display.setTextSize(1);
    display.setTextColor(Branding::muted, Branding::background);
    String firstLine = description;
    String secondLine;
    if (firstLine.length() > 25) {
        int split = firstLine.substring(0, 26).lastIndexOf(' ');
        if (split < 8) split = 25;
        secondLine = firstLine.substring(split + 1);
        firstLine = firstLine.substring(0, split);
    }
    if (!titleSecond.isEmpty() && !badge.isEmpty()) secondLine = "";
    display.setCursor(77, descriptionY);
    display.print(firstLine.substring(0, 25));
    display.setCursor(77, descriptionY + 13);
    display.print(secondLine.substring(0, 25));
    if (!badge.isEmpty()) {
        const int width = static_cast<int>(badge.length()) * 6 + 10;
        display.fillRoundRect(77, 88, width, 15, 4, Branding::accent);
        display.setTextColor(Branding::background, Branding::accent);
        display.setCursor(82, 92);
        display.print(badge);
    }
    for (size_t i = 0; i < count; ++i) {
        const int x = 120 - static_cast<int>(count * 5) +
                      static_cast<int>(i * 10);
        if (i == selected) display.fillCircle(x, 108, 3, Branding::accent);
        else display.drawCircle(x, 108, 2, Branding::muted);
    }
    drawFooter("Left/Right: browse   Enter: open");
}

// Main/Observe/Field kit menus: see include/menu_screens.h/src/menu_screens.cpp.

// AI Chat screen: see include/familiar_screens.h/src/familiar_screens.cpp.

// Wi-Fi auth-mode label: see WifiScreens::authName.

// Wi-Fi Discovery / Channel Analyzer screens: see
// include/wifi_screens.h/src/wifi_screens.cpp.

// Wi-Fi Detail / Deauth Confirm screens: see
// include/wifi_screens.h/src/wifi_screens.cpp.

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

// Handshake Capture screen: see include/wifi_screens.h/src/wifi_screens.cpp.

void exportWifiResults() {
    if (!sdAvailable || accessPoints.empty()) {
        wifiExportStatus =
            sdAvailable ? "Nothing to export" : "Export failed: no SD card";
        wifiScreens.drawRecon();
        return;
    }
    SdLogger logger;
    if (!logger.begin(
            "wifi",
            "timestamp_utc,ssid,bssid,channel,rssi_dbm,security")) {
        wifiExportStatus = "Export failed: " + logger.status();
        wifiScreens.drawRecon();
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
                           String(ap.rssi) + "," + WifiScreens::authName(ap.authmode);
        if (!logger.append(row)) break;
    }
    String name = logger.path();
    const int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    const bool success = logger.isActive();
    logger.stop();
    wifiExportStatus =
        success ? "Saved " + name : "Export failed: " + logger.status();
    wifiScreens.drawRecon();
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

// Wi-Fi Connect picker: see include/wifi_screens.h/src/wifi_screens.cpp.

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

// Wi-Fi Connect password screen: see
// include/wifi_screens.h/src/wifi_screens.cpp.

// Wi-Fi Connect status screen: see
// include/wifi_screens.h/src/wifi_screens.cpp.

// BLE Discovery screen: see include/ble_screens.h/src/ble_screens.cpp.

void exportBleResults() {
    if (!sdAvailable || bleDevices.empty()) {
        bleExportStatus =
            sdAvailable ? "Nothing to export" : "Export failed: no SD card";
        bleScreens.drawDiscovery();
        return;
    }
    SdLogger logger;
    if (!logger.begin(
            "ble",
            "timestamp_utc,name,address,address_type,rssi_dbm,connectable,"
            "advertisement_type,payload_bytes,service_count,service_uuids,"
            "manufacturer,manufacturer_data_hex,payload_hex")) {
        bleExportStatus = "Export failed: " + logger.status();
        bleScreens.drawDiscovery();
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
    bleScreens.drawDiscovery();
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
    bleScreens.drawDiscovery();
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
        csvSafePayload(String("[") + WifiScreens::authName(ap.authmode) + "]") + "," +
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
// instead of networkScanScreens.drawWarDrive()'s full drawHeader()-triggered screen clear --
// called on every periodic tick, so a full-screen flicker there would be
// very noticeable (and was, before this split).
// War Drive screen: see
// include/network_scan_screens.h/src/network_scan_screens.cpp.

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

// Host Discovery screen: see
// include/network_scan_screens.h/src/network_scan_screens.cpp.

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

// Port Scan screen: see
// include/network_scan_screens.h/src/network_scan_screens.cpp.

// Telnet Client screens: see include/telnet_screens.h/src/telnet_screens.cpp.

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
        telnetScreens.drawConnect();
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
        telnetScreens.drawConnect();
    }
}

// SSH Client screens: see include/ssh_screens.h/src/ssh_screens.cpp.

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

// SSH Password/Session screens: see include/ssh_screens.h/src/ssh_screens.cpp.

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
        sshScreens.drawPassword();
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
        sshScreens.drawPassword();
    }
}

// BLE Device detail screen: see include/ble_screens.h/src/ble_screens.cpp.

// Infrared self-test screen: see include/ir_screen.h/src/ir_screen.cpp.
// (First screen extracted out of this file -- see docs/screen-extraction.md.)

// USB/HID and DuckyScript screens: see
// include/usb_hid_screens.h/src/usb_hid_screens.cpp.

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

// DuckyScript Files/Confirm screens: see
// include/usb_hid_screens.h/src/usb_hid_screens.cpp.

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

// DuckyScript Result / USB HID Confirm screens: see
// include/usb_hid_screens.h/src/usb_hid_screens.cpp.

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
    usbHidScreens.drawUsbHid();
}

// Audio Self-Test / Familiar Phrase Lab screens: see
// include/audio_screens.h/src/audio_screens.cpp.

// Microphone Level screen: see include/audio_screens.h/src/audio_screens.cpp.

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

// MP3 Files screen: see include/audio_screens.h/src/audio_screens.cpp.

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
    audioScreens.drawNowPlaying(audioFiles[listSelection],
                                "MP3 from /ghostwire/audio");
}

// formatFileSize/isMp3File/isPreviewableFile: see
// FileScreens::formatFileSize/isMp3File/isPreviewableFile.

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

// Files browser screen: see include/file_screens.h/src/file_screens.cpp.

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

String evidenceTypeFromPath(const String& path, const String& name) {
    if (path.startsWith("/ghostwire/assessments/")) return "Patrol";
    return logTypeFromName(name);
}

void collectEvidenceFiles(const String& directoryPath, uint8_t depth) {
    if (depth > 3 || logSessions.size() >= 256) return;
    File directory = SD.open(directoryPath);
    if (!directory || !directory.isDirectory()) return;
    File entry = directory.openNextFile();
    while (entry && logSessions.size() < 256) {
        String name = entry.name();
        const int slash = name.lastIndexOf('/');
        if (slash >= 0) name = name.substring(slash + 1);
        String path = directoryPath;
        if (!path.endsWith("/")) path += "/";
        path += name;
        if (entry.isDirectory()) {
            entry.close();
            collectEvidenceFiles(path, depth + 1);
        } else {
            // Temporary checkpoints are recovery state, not operator evidence.
            if (!name.endsWith(".tmp") && !name.endsWith(".bak") &&
                name != "active.json") {
                logSessions.push_back(
                    {name, evidenceTypeFromPath(path, name), path,
                     entry.size()});
            }
            entry.close();
        }
        entry = directory.openNextFile();
    }
    directory.close();
}

void loadLogSessions() {
    logSessions.clear();
    logSelection = 0;
    logOffset = 0;
    if (!sdAvailable) return;
    collectEvidenceFiles("/ghostwire/logs", 0);
    collectEvidenceFiles("/ghostwire/assessments", 0);
    std::sort(logSessions.begin(), logSessions.end(),
              [](const LogEntry& left, const LogEntry& right) {
                  String a = left.name;
                  String b = right.name;
                  a.toLowerCase();
                  b.toLowerCase();
                  return a > b;
              });
}

// Evidence/log sessions screen: see
// include/file_screens.h/src/file_screens.cpp.

uint32_t countCsvRows(const String& path) {
    File file = SD.open(path, FILE_READ);
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
    String lower = logSessions[logSelection].name;
    lower.toLowerCase();
    selectedLogRows = lower.endsWith(".csv")
                          ? countCsvRows(logSessions[logSelection].path)
                          : 0;
    currentScreen = Screen::LogDetail;
}

// Session Details screen: see include/file_screens.h/src/file_screens.cpp.

// Log Delete Confirm / File Details screens: see
// include/file_screens.h/src/file_screens.cpp.

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

// Text Preview screen: see include/file_screens.h/src/file_screens.cpp.

void playSelectedBrowserMp3() {
    if (files.empty() || listSelection >= files.size() ||
        !FileScreens::isMp3File(files[listSelection].name)) {
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
    audioScreens.drawNowPlaying(nowPlayingName, nowPlayingSource);
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

// Call as early in setup() as preferences.begin() allows (before anything
// that could itself crash/hang). Two independent, layered safety nets for
// a bad OTA update:
//
// 1. ESP-IDF's own pending-verify mechanism, if the bootloader has app
//    rollback enabled -- harmless no-op read if it isn't. Catches a crash
//    at literally any point after boot, including before this function
//    returns, since the bootloader itself reboots into the previous
//    partition after enough failed boot attempts. Confirmed valid (this
//    boot counts as healthy) by markBootHealthy() once setup() finishes.
// 2. An app-level boot-attempt counter in NVS, independent of whether (1)
//    is actually enabled on this board. Catches the more common real-world
//    case: the new firmware boots far enough to run setup() but hangs,
//    watchdog-resets, or panics before reaching a healthy checkpoint.
//    Falls back to the other OTA partition after kMaxBootAttemptsBeforeRollback
//    consecutive attempts that never reached markBootHealthy().
void verifyOtaBootOrRollback() {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t otaState;
    if (running &&
        esp_ota_get_state_partition(running, &otaState) == ESP_OK &&
        otaState == ESP_OTA_IMG_PENDING_VERIFY) {
        otaPendingVerify = true;
    }

    const uint8_t attempts = preferences.getUChar("boot_attempts", 0);
    if (attempts >= kMaxBootAttemptsBeforeRollback) {
        preferences.putUChar("boot_attempts", 0);
        const esp_partition_t* other =
            esp_ota_get_next_update_partition(nullptr);
        if (other && other != running) {
            Serial.println(
                "[ota] repeated failed boots after an update -- "
                "rolling back to the previous firmware");
            esp_ota_set_boot_partition(other);
            esp_restart();
        }
        // No other OTA partition to fall back to (e.g. very first boot
        // before any update has ever happened) -- nothing more we can do
        // here; the counter is already cleared above.
    } else {
        preferences.putUChar("boot_attempts", attempts + 1);
    }
}

// Call once setup() has reached a point that's a meaningful sign of a
// healthy boot (SD/display/core services initialized, main menu drawn).
// Clears the boot-attempt counter and, if this boot was pending bootloader
// verification, confirms the partition so the bootloader stops treating it
// as provisional.
void markBootHealthy() {
    preferences.putUChar("boot_attempts", 0);
    if (otaPendingVerify) {
        esp_ota_mark_app_valid_cancel_rollback();
        otaPendingVerify = false;
    }
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

// DiagnosticState / SystemDiagnostic: see include/system_screens.h.

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

// System Diagnostics screen: see include/system_screens.h/src/system_screens.cpp.

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

// System Clock screen: see include/system_screens.h/src/system_screens.cpp.

// GNSS screen: see include/gnss_screen.h/src/gnss_screen.cpp.

bool startWifiGuardian() {
    guardianLastEvent = "Starting passive watch...";
    if (!sdAvailable) {
        guardianLastEvent = "microSD required for evidence";
        return false;
    }
    wifiPassiveCaptureLogger.stop();
    wifiSnifferLogger.stop();
    wifiSnifferService.end();
    WiFi.disconnect(false, false);
    delay(50);
    wifiSnifferService.setCaptureMode(WifiCaptureMode::Management);
    if (!guardianEventLogger.begin(
            "guardian_events",
            "timestamp_utc,event,channel,rssi,deauth_total,disassoc_total")) {
        guardianLastEvent = guardianEventLogger.status();
        return false;
    }
    if (!guardianEvidenceLogger.begin("guardian_evidence")) {
        guardianEventLogger.stop();
        guardianLastEvent = guardianEvidenceLogger.status();
        return false;
    }
    if (!wifiSnifferService.begin()) {
        guardianEventLogger.stop();
        guardianEvidenceLogger.stop();
        guardianLastEvent = "Unable to start Wi-Fi monitor";
        return false;
    }
    wifiGuardianService.begin(wifiGuardianService.sensitivity());
    guardianLastEvent = "Watching; observations are not proof";
    cyberFamiliar.notePatrol("Guardian watch started.", 5,
                             FamiliarMood::Curious);
    triggerFamiliarReaction(FamiliarReaction::Searching, 2200);
    showFamiliarSpeech("I'll keep an eye on things.", 2800);
    return true;
}

void stopWifiGuardian() {
    const bool wasActive = wifiGuardianService.isActive();
    wifiGuardianService.stop();
    wifiSnifferService.end();
    guardianEventLogger.stop();
    guardianEvidenceLogger.stop();
    if (wasActive) {
        cyberFamiliar.notePatrol("Guardian watch ended. Evidence saved.", 3,
                                 FamiliarMood::Content);
    }
}

// Familiar Guardian screen: see
// include/wifi_guardian_screen.h/src/wifi_guardian_screen.cpp.

// Wi-Fi/BLE/Devices menus: see include/menu_screens.h/src/menu_screens.cpp.

// Biscuit Pro screens: see include/device_screens.h/src/device_screens.cpp.

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

// Biscuit Wardrive screen: see include/device_screens.h/src/device_screens.cpp.

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

// Biscuit Result screen: see include/device_screens.h/src/device_screens.cpp.

// BLE Keyboard screen: see include/ble_screens.h/src/ble_screens.cpp.

// BLE Spam screens: see include/ble_spam_screen.h/src/ble_spam_screen.cpp.

// RFID menu: see include/menu_screens.h/src/menu_screens.cpp.

// Chameleon hex ID formatting: see ChameleonScreen::hexId.

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
                    ChameleonScreen::hexId(chameleonHfTag.uid,
                                   chameleonHfTag.uidLen).c_str(),
                    chameleonHfTag.atqa, chameleonHfTag.sak);
    } else {
        file.printf("EM410X,%s,,\n",
                    ChameleonScreen::hexId(chameleonLfId, 5).c_str());
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

// Chameleon Ultra screens: see include/device_screens.h/src/device_screens.cpp.

void attemptChameleonConnection() {
    if (chameleonClient.isConnected()) return;

    lastChameleonConnectAttemptMs = millis();
    ++chameleonConnectAttempts;
    chameleonWorkflowStatus = "Auto-connect attempt " +
                              String(chameleonConnectAttempts);
    chameleonScreen.draw(false);

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
    chameleonScreen.draw(false);
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
        csvId = ChameleonScreen::hexId(chameleonHfTag.uid, chameleonHfTag.uidLen);
        signature = "HF:" + csvId;
        csvType = "HF14A";
        csvAtqa = String(chameleonHfTag.atqa, HEX);
        csvSak = String(chameleonHfTag.sak, HEX);
    } else if (chameleonLfFound) {
        csvId = ChameleonScreen::hexId(chameleonLfId, 5);
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

// GPS/Mesh/Network menus: see include/menu_screens.h/src/menu_screens.cpp.

// Network Dashboard screen: see
// include/network_scan_screens.h/src/network_scan_screens.cpp.

// Tools menu: see include/menu_screens.h/src/menu_screens.cpp.

// QR Generator screens: see include/qr_screens.h/src/qr_screens.cpp.

// LoRa screen: see include/lora_screen.h/src/lora_screen.cpp.

// Wi-Fi Sniffer screen: see
// include/wifi_sniffer_screen.h/src/wifi_sniffer_screen.cpp.

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

// familiarFace(): see FamiliarScreens::face (private).

void triggerFamiliarReaction(FamiliarReaction reaction,
                             uint32_t durationMs = 1800) {
    familiarReaction = reaction;
    familiarReactionUntil = millis() + durationMs;
}

enum class FamiliarCue : uint8_t {
    Started, Host, Service, Warning, Complete, Error,
};

uint8_t familiarVoicePhraseForCue(FamiliarCue cue) {
    switch (cue) {
        case FamiliarCue::Started: return 3;
        case FamiliarCue::Host: return 0;
        case FamiliarCue::Service: return 2;
        case FamiliarCue::Warning: return 5;
        case FamiliarCue::Complete: return 4;
        case FamiliarCue::Error: return 6;
    }
    return 6;
}

uint8_t familiarVoicePriority(uint8_t phrase) {
    if (phrase == 6) return 3;
    if (phrase == 5) return 2;
    return 1;
}

void queueFamiliarVoice(FamiliarCue cue) {
    const uint8_t phrase = familiarVoicePhraseForCue(cue);
    if (familiarVoicePhrase < 0 && millis() >= familiarVoiceReadyMs) {
        familiarVoicePhrase = phrase;
        familiarVoiceWord = 0;
        return;
    }
    // Retain only one pending announcement. Warnings and errors may replace
    // routine news, preventing a busy host from creating stale narration.
    if (familiarVoicePending < 0 ||
        familiarVoicePriority(phrase) >
            familiarVoicePriority(static_cast<uint8_t>(familiarVoicePending))) {
        familiarVoicePending = phrase;
    }
}

void updateFamiliarVoice() {
    if (familiarVoicePhrase < 0) {
        if (familiarVoicePending < 0 || millis() < familiarVoiceReadyMs) return;
        familiarVoicePhrase = familiarVoicePending;
        familiarVoicePending = -1;
        familiarVoiceWord = 0;
    }
    if (audioService.isPlaying()) return;
    const auto& phrase =
        kTtsLabPhrases[static_cast<uint8_t>(familiarVoicePhrase)];
    if (familiarVoiceWord >= phrase.wordCount) {
        familiarVoicePhrase = -1;
        familiarVoiceWord = 0;
        familiarVoiceReadyMs = millis() + 900;
        return;
    }
    const String path = familiarWordPath(phrase.words[familiarVoiceWord]);
    if (!audioService.startMp3(path.c_str(), 25)) {
        familiarVoicePhrase = -1;
        familiarVoicePending = -1;
        familiarVoiceReadyMs = millis() + 3000;
        return;
    }
    ++familiarVoiceWord;
}

void showFamiliarSpeech(const String& message, uint32_t durationMs = 2600) {
    familiarSpeechBubble = message;
    familiarSpeechBubbleUntil = millis() + durationMs;
}

String familiarHostLabel(const IPAddress& ip) {
    // Always-available compact fallback. A future non-blocking name resolver
    // can replace this without changing reaction or rendering code.
    return "." + String(ip[3]);
}

const char* familiarServiceName(uint16_t port) {
    switch (port) {
        case 21: return "FTP"; case 22: return "SSH";
        case 23: return "Telnet"; case 53: return "DNS";
        case 80: case 8080: case 8081: case 8000: return "Web";
        case 139: case 445: return "SMB";
        case 443: case 8443: return "Secure web";
        case 1883: return "MQTT"; case 2049: return "NFS";
        case 2375: case 2376: return "Docker";
        case 3306: return "MySQL"; case 3389: return "RDP";
        case 5432: return "Postgres"; case 5900: return "VNC";
        case 6379: return "Redis"; case 6443: return "Kubernetes";
        case 9100: return "Printer"; case 9200: return "Elastic";
        case 27017: return "MongoDB";
        default: return "service";
    }
}

void playFamiliarCue(FamiliarCue cue) {
    if (familiarCueIndex == 0) return;
    if (familiarCueIndex == kFamiliarCueCount - 1) {
        if (millis() - lastFamiliarCueMs >= 1800 ||
            cue == FamiliarCue::Warning || cue == FamiliarCue::Error) {
            lastFamiliarCueMs = millis();
            queueFamiliarVoice(cue);
        }
        return;
    }
    if (audioService.isPlaying() || millis() - lastFamiliarCueMs < 700) {
        return;
    }
    lastFamiliarCueMs = millis();
    uint16_t first = 700;
    uint16_t second = 0;
    uint16_t duration = 55;
    if (familiarCueIndex == 1) {
        first = cue == FamiliarCue::Warning || cue == FamiliarCue::Error
                    ? 440 : 880;
        duration = 45;
    } else if (familiarCueIndex == 2) {
        first = cue == FamiliarCue::Warning || cue == FamiliarCue::Error
                    ? 520 : 1050;
        second = cue == FamiliarCue::Complete ? 1400 : first + 180;
    } else if (familiarCueIndex == 3) {
        first = cue == FamiliarCue::Warning || cue == FamiliarCue::Error
                    ? 220 : 660;
        second = cue == FamiliarCue::Complete ? 1320 : first * 2;
        duration = 60;
    } else {
        first = cue == FamiliarCue::Warning || cue == FamiliarCue::Error
                    ? 330 : 784;
        second = cue == FamiliarCue::Complete ? 1568 : 1175;
        duration = 85;
    }
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(speakerVolume);
    M5Cardputer.Speaker.tone(first, duration);
    if (second > 0) M5Cardputer.Speaker.tone(second, duration + 20);
}

// drawFamiliarSpeechBubble(): see FamiliarScreens::drawSpeechBubble.

bool familiarSensitivePort(uint16_t port) {
    switch (port) {
        case 23: case 111: case 135: case 139: case 445:
        case 1433: case 2049: case 3306: case 3389: case 5432:
        case 5900: case 6379: case 9200: case 27017:
            return true;
        default: return false;
    }
}

// drawFamiliarCreature(): see FamiliarScreens::drawCreature.
// familiarAgeText(): inlined into FamiliarScreens::drawFamiliar (its only
// caller).

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

// Cyber Familiar / Familiar Patrol screens: see
// include/familiar_screens.h/src/familiar_screens.cpp.

void drawCyberFamiliarIdle() {
    auto& display = M5Cardputer.Display;
    if (!familiarIdleDrawn) {
        display.fillRect(0, 0, display.width(), display.height(),
                         Branding::background);
        familiarIdleDrawn = true;
    } else {
        // The creature and bubble are the only animated idle elements.
        display.fillRect(0, 0, display.width(), 80, Branding::background);
    }
    familiarScreens.drawCreature(120, 75, true);
    familiarScreens.drawSpeechBubble(15, 4, 210);
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

// Settings root menu: see MenuScreens::drawSettings.
// Settings sub-screens, About, Placeholder: see
// include/settings_screens.h/src/settings_screens.cpp.

// IMU screen: see include/imu_screen.h/src/imu_screen.cpp.

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
        case Screen::MainMenu: menuScreens.drawMain(); break;
        case Screen::ObserveMenu: menuScreens.drawObserve(); break;
        case Screen::FieldKitMenu: menuScreens.drawFieldKit(); break;
        case Screen::WifiMenu: menuScreens.drawWifi(); break;
        case Screen::WifiRecon: wifiScreens.drawRecon(); break;
        case Screen::WifiChannelAnalyzer: wifiScreens.drawChannelAnalyzer(); break;
        case Screen::WifiDetail: wifiScreens.drawDetail(); break;
        case Screen::WifiDeauthConfirm: wifiScreens.drawDeauthConfirm(); break;
        case Screen::WifiHandshakeCapture: wifiScreens.drawHandshakeCapture(); break;
        case Screen::WifiConnectSelect: wifiScreens.drawConnectSelect(); break;
        case Screen::WifiConnectPassword: wifiScreens.drawConnectPassword(); break;
        case Screen::WifiConnectStatus: wifiScreens.drawConnectStatus(); break;
        case Screen::BleMenu: menuScreens.drawBle(); break;
        case Screen::DevicesMenu: menuScreens.drawDevices(); break;
        case Screen::AiChat: aiChatScreen.draw(); break;
        case Screen::CyberFamiliar: familiarScreens.drawFamiliar(); break;
        case Screen::FamiliarPatrol: familiarScreens.drawPatrol(); break;
        case Screen::FamiliarPatrolConfirm: familiarScreens.drawPatrolConfirm(); break;
        case Screen::CyberFamiliarResetConfirm:
            familiarScreens.drawResetConfirm();
            break;
        case Screen::BleDiscovery: bleScreens.drawDiscovery(); break;
        case Screen::BleDetail: bleScreens.drawDetail(); break;
        case Screen::BleKeyboard: bleScreens.drawKeyboard(); break;
        case Screen::Biscuit: biscuitScreens.drawMain(); break;
        case Screen::BiscuitTools: biscuitScreens.drawTools(); break;
        case Screen::BiscuitResult: biscuitScreens.drawResult(); break;
        case Screen::BiscuitWardrive: biscuitScreens.drawWardrive(); break;
        case Screen::BleSpamSelect:
            normalizeListPosition(BleSpamScreen::kModeCount);
            bleSpamScreen.drawSelect(listSelection);
            break;
        case Screen::BleSpam: bleSpamScreen.drawActive(); break;
        case Screen::RfidMenu: menuScreens.drawRfid(); break;
        case Screen::Chameleon: chameleonScreen.draw(); break;
        case Screen::ChameleonEmulateConfirm:
            chameleonScreen.drawEmulateConfirm();
            break;
        case Screen::ToolsMenu: menuScreens.drawTools(); break;
        case Screen::Infrared: irScreen.draw(); break;
        case Screen::UsbHid: usbHidScreens.drawUsbHid(); break;
        case Screen::UsbHidConfirm: usbHidScreens.drawUsbHidConfirm(); break;
        case Screen::DuckyScripts: usbHidScreens.drawDuckyScripts(); break;
        case Screen::DuckyConfirm: usbHidScreens.drawDuckyConfirm(); break;
        case Screen::DuckyResult: usbHidScreens.drawDuckyResult(); break;
        case Screen::Audio: audioScreens.drawMenu(); break;
        case Screen::TtsLab: audioScreens.drawTtsLab(); break;
        case Screen::AudioMic: audioScreens.drawMicrophone(); break;
        case Screen::AudioFiles: audioScreens.drawAudioFiles(); break;
        case Screen::AudioPlaying: break;
        case Screen::QrEntry: qrScreens.drawEntry(); break;
        case Screen::QrDisplay: qrScreens.drawDisplay(); break;
        case Screen::Files: fileScreens.drawFiles(); break;
        case Screen::FileDetail: fileScreens.drawFileDetail(); break;
        case Screen::TextPreview: fileScreens.drawTextPreview(); break;
        case Screen::LogSessions: logScreens.drawSessions(); break;
        case Screen::LogDetail: logScreens.drawDetail(); break;
        case Screen::LogDeleteConfirm: logScreens.drawDeleteConfirm(); break;
        case Screen::System: systemScreens.drawSystem(systemDiagnostics()); break;
        case Screen::TimeStatus: systemScreens.drawTimeStatus(); break;
        case Screen::GpsMenu: menuScreens.drawGps(); break;
        case Screen::MeshMenu: menuScreens.drawMesh(); break;
        case Screen::WarDrive: networkScanScreens.drawWarDrive(); break;
        case Screen::NetworkMenu: menuScreens.drawNetwork(); break;
        case Screen::NetworkDashboard: networkScanScreens.drawNetworkDashboard(); break;
        case Screen::NetworkHostScan: networkScanScreens.drawNetworkHostScan(); break;
        case Screen::NetworkPortScan: networkScanScreens.drawNetworkPortScan(); break;
        case Screen::TelnetConnect: telnetScreens.drawConnect(); break;
        case Screen::TelnetSession: telnetScreens.drawSession(); break;
        case Screen::SshConnect: sshScreens.drawConnect(); break;
        case Screen::SshPassword: sshScreens.drawPassword(); break;
        case Screen::SshSession: sshScreens.drawSession(); break;
        case Screen::Gnss: gnssScreen.draw(); break;
        case Screen::LoRa: loraScreen.draw(); break;
        case Screen::WifiSniffer: wifiSnifferScreen.draw(); break;
        case Screen::WifiGuardian: wifiGuardianScreen.draw(); break;
        case Screen::Imu: imuScreen.draw(); break;
        case Screen::Settings: menuScreens.drawSettings(); break;
        case Screen::SettingsDisplay: settingsScreens.drawDisplay(); break;
        case Screen::SettingsBoot: settingsScreens.drawBoot(); break;
        case Screen::SettingsConnectivity: settingsScreens.drawConnectivity(); break;
        case Screen::SettingsReset: settingsScreens.drawResetConfirm(); break;
        case Screen::Placeholder: settingsScreens.drawPlaceholder(); break;
        case Screen::About: settingsScreens.drawAbout(); break;
        case Screen::OtaCheck: otaScreens.drawCheck(); break;
        case Screen::OtaInstalling: otaScreens.drawInstalling(); break;
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

unsigned long bootDuration(unsigned long normalMs) {
    if (bootSpeedIndex == 0) return normalMs * 3UL / 2UL;
    if (bootSpeedIndex == 2) return std::max(100UL, normalMs / 2UL);
    return normalMs;
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
            delay(bootDuration(1000));
        }
    } else {
        delay(bootDuration(1000));
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
    // Reveal and hold durations follow the selected boot speed.
    display.setTextSize(2);
    const int titleY = 39;
    const int titleWidth = display.textWidth(Branding::productName);
    const int titleX = (display.width() - titleWidth) / 2;
    const size_t nameLength = strlen(Branding::productName);
    bool skipped = false;
    {
        static constexpr char kScrambleChars[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789#$%&@?";
        constexpr size_t kScrambleCharCount = sizeof(kScrambleChars) - 1;
        const unsigned long revealDurationMs = bootDuration(650);
        const unsigned long revealStarted = millis();
        size_t locked = 0;
        while (locked < nameLength && !skipped) {
            const unsigned long elapsed = millis() - revealStarted;
            locked = std::min(nameLength,
                              static_cast<size_t>((elapsed * nameLength) /
                                                  revealDurationMs));
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

    // Blinking terminal cursor for the remaining hold time.
    const int cursorX = versionX + display.textWidth(String("v") +
                                                      Branding::version) +
                        2;
    const unsigned long holdStarted = millis();
    bool cursorOn = true;
    while (!skipped && millis() - holdStarted < bootDuration(2200)) {
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

void showCipherHackBoot(unsigned long durationMs = 1800) {
    auto& display = M5Cardputer.Display;
    display.fillScreen(Branding::background);
    display.setTextSize(1);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(7, 7);
    display.print("BREACH SEQUENCE // GHOSTWIRE");
    display.drawFastHLine(7, 19, 226, Branding::panel);
    static constexpr char kHex[] = "0123456789ABCDEF";
    static constexpr char kTarget[] = "B7A4";
    const unsigned long started = millis();
    int lastFrame = -1;
    while (millis() - started < durationMs) {
        const unsigned long elapsed = millis() - started;
        const int locked = std::min(4, static_cast<int>(elapsed * 5 /
                                                        std::max(1UL, durationMs)));
        const int frame = static_cast<int>(elapsed / 70);
        if (frame != lastFrame) {
            lastFrame = frame;
            for (int column = 0; column < 4; ++column) {
                const int x = 24 + column * 54;
                display.fillRoundRect(x, 32, 42, 48, 4, Branding::panel);
                display.drawRoundRect(x, 32, 42, 48, 4,
                                      column < locked ? Branding::accent
                                                      : Branding::muted);
                const char value = column < locked
                                       ? kTarget[column]
                                       : kHex[(frame + column * 5) & 0x0F];
                display.setTextSize(2);
                display.setTextColor(column < locked ? Branding::accent
                                                     : Branding::text,
                                     Branding::panel);
                display.setCursor(x + 14, 48);
                display.print(value);
            }
            display.setTextSize(1);
            display.fillRect(7, 90, 226, 31, Branding::background);
            display.setTextColor(locked == 4 ? Branding::accent
                                              : Branding::warning,
                                 Branding::background);
            display.setCursor(7, 92);
            display.printf("ICE: %s", locked == 4 ? "BREACHED" : "DECRYPTING");
            display.setTextColor(Branding::muted, Branding::background);
            display.setCursor(7, 108);
            for (int index = 0; index < 20; ++index) {
                display.print(index < locked * 5 ? '#' : '-');
            }
        }
        if (bootTitleSkipRequested()) break;
        delay(25);
    }
    display.setTextSize(1);
}

void showNeonBreachBoot(unsigned long durationMs = 1500) {
    auto& display = M5Cardputer.Display;
    display.fillScreen(Branding::background);
    const unsigned long started = millis();
    while (millis() - started < durationMs) {
        const int progress = static_cast<int>(
            std::min<unsigned long>(100, (millis() - started) * 100 /
                                             std::max(1UL, durationMs)));
        for (int line = 0; line < 5; ++line) {
            const int y = random(8, display.height() - 8);
            display.drawFastHLine(random(0, 70), y, random(30, 170),
                                  line & 1 ? Branding::warning
                                           : Branding::accent);
        }
        display.fillRect(18, 37, display.width() - 36, 54,
                         Branding::background);
        display.drawRect(18, 37, display.width() - 36, 54,
                         Branding::warning);
        display.setTextSize(2);
        display.setTextColor(Branding::accent, Branding::background);
        display.setCursor(38 + random(-2, 3), 48);
        display.print("NEON BREACH");
        display.setTextSize(1);
        display.setTextColor(Branding::text, Branding::background);
        display.setCursor(67, 74);
        display.printf("LINK %03d%%", progress);
        display.fillRect(24, 96, (display.width() - 48) * progress / 100, 3,
                         Branding::accent);
        if (bootTitleSkipRequested()) break;
        delay(65);
    }
}

void showHackerBoot(unsigned long durationMs = 1600) {
    auto& display = M5Cardputer.Display;
    display.fillScreen(Branding::background);
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
            display.setTextColor(shown == 5 ? Branding::warning
                                             : Branding::accent,
                                 Branding::background);
            display.setCursor(7, 10 + static_cast<int>(shown) * 18);
            display.print(lines[shown]);
            ++shown;
        }
        display.fillRect(7, 120, 7, 8,
                         ((millis() / 180) & 1) ? Branding::accent
                                                : Branding::background);
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
        display.fillCircle(x + 14, y, 13, Branding::accent);
        display.fillCircle(x + 10, y - 4, 2, Branding::background);
        display.fillCircle(x + 18, y - 4, 2, Branding::background);
        display.fillTriangle(x + 27, y, x + 36, y + 4, x + 27, y + 8,
                             Branding::warning);
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
    display.fillScreen(Branding::background);
    const int horizon = 72;
    display.fillCircle(display.width() / 2, horizon - 12, 24,
                       Branding::warning);
    display.fillRect(0, horizon, display.width(), display.height() - horizon,
                     Branding::background);
    for (int y = horizon; y < display.height(); y += 11) {
        display.drawFastHLine(0, y, display.width(), Branding::warning);
    }
    for (int x = -120; x <= 360; x += 24) {
        display.drawLine(display.width() / 2, horizon, x, display.height() - 1,
                         Branding::accent);
    }
    display.setTextSize(2);
    display.setTextColor(Branding::accent, Branding::background);
    display.setCursor(57, 22);
    display.print("GHOSTWIRE");
    const unsigned long started = millis();
    while (millis() - started < durationMs) {
        const int y = horizon + static_cast<int>((millis() - started) / 12) %
                                    (display.height() - horizon);
        display.drawFastHLine(0, y, display.width(), Branding::text);
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
    if (bootAnimationIndex == 1) {
        showCipherHackBoot(bootDuration(1800));
    } else if (bootAnimationIndex == 2) {
        showRadarBoot(bootDuration(1200));
    } else if (bootAnimationIndex == 3) {
        showMinimalBoot(bootDuration(900));
    } else if (bootAnimationIndex >= 4) {
        showSelectedStyledBoot(bootDuration(1300));
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
        delay(bootDuration(350));
    }
    recoverKeyboardAfterBlockingOperation();
}

void showBootSequence() {
    M5Cardputer.Display.fillScreen(Branding::background);
    const std::vector<SystemDiagnostic> diagnostics = systemDiagnostics();
    if (bootAnimationIndex == 0) {
        M5Canvas console(&M5Cardputer.Display);
        if (!console.createSprite(M5Cardputer.Display.width(), 61)) {
            showBootSummary(diagnostics);
            showBootTitle();
            return;
        }
        size_t drawnEntries = 0;
        int drawnSpinner = -1;
        const unsigned long started = millis();
        const unsigned long consoleDuration = bootDuration(3920);
        while (millis() - started < consoleDuration) {
            const unsigned long elapsed = millis() - started;
            const unsigned long normalized = elapsed * 3920UL /
                                             std::max(1UL, consoleDuration);
            drawBootConsole(normalized, diagnostics, console, drawnEntries,
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
    if (bootAnimationIndex == 1) {
        showCipherHackBoot(bootDuration(1800));
    }
    if (bootAnimationIndex == 2) {
        showRadarBoot(bootDuration(1500));
    }
    if (bootAnimationIndex >= 4) {
        showSelectedStyledBoot(bootDuration(1700));
    }
    if (bootAnimationIndex == 3) {
        showMinimalBoot(bootDuration(700));
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
    wifiGuardianService.stop();
    wifiSnifferService.end();
    bleSpamService.end();
    bleKeyboardService.end();
    stopBiscuitWardrive();
    biscuitClient.disconnect();
    warDriveService.stop();
    networkHostScanService.stop();
    networkPortScanService.stop();
    familiarPatrolService.stop();
    chameleonContinuousScan = false;
    chameleonClient.disconnect();
    telnetClient.stop();
    sshService.stop();
    audioService.stopPlayback();

    imuLogger.stop();
    gnssLogger.stop();
    loraLogger.stop();
    wifiSnifferLogger.stop();
    guardianEventLogger.stop();
    bleCaptureLogger.stop();
    wifiPassiveCaptureLogger.stop();
    guardianEvidenceLogger.stop();
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
    menuScreens.drawMain();
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
        case 0:
            currentScreen = Screen::CyberFamiliar;
            familiarPage = 0;
            break;
        case 1: currentScreen = Screen::ObserveMenu; break;
        case 2: currentScreen = Screen::NetworkMenu; break;
        case 3:
            currentScreen = Screen::LogSessions;
            evidenceReturnScreen = Screen::MainMenu;
            loadLogSessions();
            break;
        case 4: currentScreen = Screen::FieldKitMenu; break;
        case 5: currentScreen = Screen::Settings; break;
    }
    drawCurrentScreen();
}

void goBack() {
    if (currentScreen == Screen::ObserveMenu ||
        currentScreen == Screen::FieldKitMenu) {
        const bool wasObserve = currentScreen == Screen::ObserveMenu;
        currentScreen = Screen::MainMenu;
        menuSelection = wasObserve ? 1 : 4;
        menuScreens.drawMain();
        return;
    }
    if (currentScreen == Screen::AiChat) {
        currentScreen = Screen::FieldKitMenu;
        listSelection = 1;
        menuScreens.drawFieldKit();
        return;
    }
    if (currentScreen == Screen::CyberFamiliar) {
        currentScreen = Screen::MainMenu;
        menuSelection = 0;
        menuScreens.drawMain();
        return;
    }
    if (currentScreen == Screen::CyberFamiliarResetConfirm) {
        currentScreen = Screen::CyberFamiliar;
        familiarScreens.drawFamiliar();
        return;
    }
    if (currentScreen == Screen::FamiliarPatrol ||
        currentScreen == Screen::FamiliarPatrolConfirm) {
        currentScreen = Screen::CyberFamiliar;
        familiarPage = 0;
        familiarScreens.drawFamiliar();
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
    if (currentScreen == Screen::WifiGuardian) stopWifiGuardian();
    if (currentScreen == Screen::BleDiscovery) {
        bleCaptureLogger.stop();
        bleScanner.stop();
    }
    if (currentScreen == Screen::Gnss) {
        currentScreen = Screen::GpsMenu;
        listSelection = 0;
        menuScreens.drawGps();
        return;
    }
    if (currentScreen == Screen::LoRa) {
        currentScreen = Screen::MeshMenu;
        listSelection = 0;
        menuScreens.drawMesh();
        return;
    }
    if (currentScreen == Screen::NetworkHostScan) {
        networkHostScanService.stop();
        currentScreen = Screen::NetworkMenu;
        listSelection = 0;
        menuScreens.drawNetwork();
        return;
    }
    if (currentScreen == Screen::NetworkDashboard) {
        currentScreen = Screen::NetworkMenu;
        listSelection = 0;
        menuScreens.drawNetwork();
        return;
    }
    if (currentScreen == Screen::NetworkPortScan) {
        if (networkPortScanService.isActive()) networkPortScanService.stop();
        currentScreen = Screen::NetworkHostScan;
        listSelection = 0;
        listOffset = 0;
        networkScanScreens.drawNetworkHostScan();
        return;
    }
    if (currentScreen == Screen::SettingsDisplay ||
        currentScreen == Screen::SettingsBoot ||
        currentScreen == Screen::SettingsConnectivity ||
        currentScreen == Screen::SettingsReset) {
        currentScreen = Screen::Settings;
        listSelection = 0;
        listOffset = 0;
        menuScreens.drawSettings();
        return;
    }
    if (currentScreen == Screen::AudioMic) {
        audioService.endMicrophone();
        M5Cardputer.Speaker.begin();
        currentScreen = Screen::Audio;
        audioScreens.drawMenu();
        return;
    }
    if (currentScreen == Screen::AudioFiles) {
        currentScreen = Screen::Audio;
        listSelection = 2;
        audioScreens.drawMenu();
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
        qrScreens.drawEntry();
        return;
    }
    if (currentScreen == Screen::TtsLab) {
        currentScreen = Screen::Audio;
        listSelection = 3;
        audioScreens.drawMenu();
        return;
    }
    if (currentScreen == Screen::QrEntry) {
        currentScreen = Screen::ToolsMenu;
        listSelection = 6;
        listOffset = 1;
        menuScreens.drawTools();
        return;
    }
    if (currentScreen == Screen::FileDetail) {
        currentScreen = Screen::Files;
        fileScreens.drawFiles();
        return;
    }
    if (currentScreen == Screen::TextPreview) {
        currentScreen = textPreviewReturnScreen;
        drawCurrentScreen();
        return;
    }
    if (currentScreen == Screen::LogDetail) {
        currentScreen = Screen::LogSessions;
        logScreens.drawSessions();
        return;
    }
    if (currentScreen == Screen::LogSessions) {
        currentScreen = evidenceReturnScreen;
        if (currentScreen == Screen::MainMenu) {
            menuSelection = 3;
            menuScreens.drawMain();
        } else {
            listSelection = 3;
            menuScreens.drawTools();
        }
        return;
    }
    if (currentScreen == Screen::LogDeleteConfirm) {
        currentScreen = Screen::LogDetail;
        logScreens.drawDetail();
        return;
    }
    if (currentScreen == Screen::TimeStatus) {
        currentScreen = Screen::System;
        systemScreens.drawSystem(systemDiagnostics());
        return;
    }
    if (currentScreen == Screen::WifiDetail) {
        currentScreen = Screen::WifiRecon;
        wifiScreens.drawRecon();
        return;
    }
    if (currentScreen == Screen::WifiDeauthConfirm) {
        currentScreen = Screen::WifiDetail;
        wifiScreens.drawDetail();
        return;
    }
    if (currentScreen == Screen::WifiHandshakeCapture) {
        handshakeCaptureLogger.stop();
        wifiSnifferService.clearHandshakeTarget();
        currentScreen = Screen::WifiDetail;
        wifiScreens.drawDetail();
        return;
    }
    if (currentScreen == Screen::BleDetail) {
        currentScreen = Screen::BleDiscovery;
        bleScreens.drawDiscovery();
        return;
    }
    if (currentScreen == Screen::UsbHidConfirm) {
        currentScreen = Screen::UsbHid;
        usbHidScreens.drawUsbHid();
        return;
    }
    if (currentScreen == Screen::DuckyScripts) {
        currentScreen = Screen::UsbHid;
        listSelection = kHidPresetCount;
        usbHidScreens.drawUsbHid();
        return;
    }
    if (currentScreen == Screen::DuckyConfirm ||
        currentScreen == Screen::DuckyResult) {
        currentScreen = Screen::DuckyScripts;
        usbHidScreens.drawDuckyScripts();
        return;
    }
    if (currentScreen == Screen::Files && currentPath != "/") {
        const int slash = currentPath.lastIndexOf('/');
        currentPath = slash <= 0 ? "/" : currentPath.substring(0, slash);
        loadDirectory();
        fileScreens.drawFiles();
        return;
    }
    if (currentScreen == Screen::WifiRecon ||
        currentScreen == Screen::WifiChannelAnalyzer ||
        currentScreen == Screen::WifiSniffer ||
        currentScreen == Screen::WifiGuardian) {
        currentScreen = Screen::WifiMenu;
        listSelection = 0;
        menuScreens.drawWifi();
        return;
    }
    if (currentScreen == Screen::WifiConnectSelect) {
        currentScreen = Screen::WifiMenu;
        listSelection = 4;
        menuScreens.drawWifi();
        return;
    }
    if (currentScreen == Screen::WifiConnectPassword) {
        currentScreen = Screen::WifiConnectSelect;
        wifiScreens.drawConnectSelect();
        return;
    }
    if (currentScreen == Screen::WifiConnectStatus) {
        // Deliberately does not call WiFi.disconnect() -- unlike every
        // other Wi-Fi screen, this connection is meant to persist in the
        // background for future network tools to use. Disconnecting is
        // the explicit D key on this screen, not a side effect of leaving.
        currentScreen = Screen::WifiConnectSelect;
        wifiScreens.drawConnectSelect();
        return;
    }
    if (currentScreen == Screen::BleDiscovery) {
        currentScreen = Screen::BleMenu;
        listSelection = 0;
        menuScreens.drawBle();
        return;
    }
    if (currentScreen == Screen::BleKeyboard) {
        bleKeyboardService.end();
        currentScreen = Screen::BleMenu;
        listSelection = 1;
        menuScreens.drawBle();
        return;
    }
    if (currentScreen == Screen::BiscuitResult) {
        currentScreen = Screen::BiscuitTools;
        listSelection = 0;
        biscuitScreens.drawTools();
        return;
    }
    if (currentScreen == Screen::BiscuitWardrive) {
        stopBiscuitWardrive();
        currentScreen = Screen::BiscuitTools;
        listSelection = 6;
        biscuitScreens.drawTools();
        return;
    }
    if (currentScreen == Screen::BiscuitTools) {
        currentScreen = Screen::Biscuit;
        biscuitScreens.drawMain();
        return;
    }
    if (currentScreen == Screen::Biscuit) {
        biscuitClient.disconnect();
        currentScreen = Screen::DevicesMenu;
        listSelection = 0;
        menuScreens.drawDevices();
        return;
    }
    if (currentScreen == Screen::BleSpam) {
        bleSpamService.end();
        currentScreen = Screen::BleSpamSelect;
        listSelection = 0;
        normalizeListPosition(BleSpamScreen::kModeCount);
        bleSpamScreen.drawSelect(listSelection);
        return;
    }
    if (currentScreen == Screen::BleSpamSelect) {
        currentScreen = Screen::BleMenu;
        listSelection = 2;
        menuScreens.drawBle();
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
        menuScreens.drawDevices();
        return;
    }
    if (currentScreen == Screen::ChameleonEmulateConfirm) {
        currentScreen = Screen::Chameleon;
        chameleonScreen.draw();
        return;
    }
    if (currentScreen == Screen::Infrared || currentScreen == Screen::UsbHid ||
        currentScreen == Screen::Audio ||
        currentScreen == Screen::Imu ||
        currentScreen == Screen::System || currentScreen == Screen::About ||
        currentScreen == Screen::Files) {
        // Files only reaches here when at "/" -- the case above already
        // returns for "go up a directory" when currentPath isn't root.
        currentScreen = Screen::ToolsMenu;
        listSelection = 0;
        menuScreens.drawTools();
        return;
    }
    if (currentScreen == Screen::DevicesMenu) {
        currentScreen = Screen::FieldKitMenu;
        listSelection = 0;
        menuScreens.drawFieldKit();
        return;
    }
    if (currentScreen == Screen::WarDrive) {
        warDriveService.stop();
        if (warDriveWifiLogger.isActive()) warDriveWifiLogger.stop();
        if (warDriveBleLogger.isActive()) warDriveBleLogger.stop();
        currentScreen = Screen::ObserveMenu;
        listSelection = 4;
        menuScreens.drawObserve();
        return;
    }
    if (currentScreen == Screen::WifiMenu || currentScreen == Screen::BleMenu ||
        currentScreen == Screen::GpsMenu || currentScreen == Screen::MeshMenu) {
        const Screen previous = currentScreen;
        currentScreen = Screen::ObserveMenu;
        listSelection = previous == Screen::WifiMenu ? 0
                        : previous == Screen::BleMenu ? 1
                        : previous == Screen::GpsMenu ? 2 : 3;
        menuScreens.drawObserve();
        return;
    }
    if (currentScreen == Screen::ToolsMenu) {
        currentScreen = Screen::FieldKitMenu;
        listSelection = 2;
        menuScreens.drawFieldKit();
        return;
    }
    if (currentScreen == Screen::NetworkMenu) {
        currentScreen = Screen::MainMenu;
        menuSelection = 2;
        menuScreens.drawMain();
        return;
    }
    if (currentScreen == Screen::Settings) {
        currentScreen = Screen::MainMenu;
        menuSelection = 5;
        menuScreens.drawMain();
        return;
    }
    currentScreen = Screen::MainMenu;
    menuScreens.drawMain();
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
            qrScreens.drawDisplay();
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
            qrScreens.drawEntry();
        }
        return;
    }
    if (currentScreen == Screen::TtsLab) {
        if (keys.esc) { goBack(); return; }
        if (keys.tab) {
            ttsLabPhrase = (ttsLabPhrase + 1) % kTtsLabPhraseCount;
            audioScreens.drawTtsLab();
            return;
        }
        if (keys.enter) {
            const auto& phrase = kTtsLabPhrases[ttsLabPhrase];
            drawHeader("Familiar: speaking");
            M5Cardputer.Display.setCursor(8, 48);
            M5Cardputer.Display.print(phrase.name);
            const uint32_t started = millis();
            bool success = true;
            for (uint8_t i = 0; i < phrase.wordCount; ++i) {
                const String path = familiarWordPath(phrase.words[i]);
                // The normal player uses a conservative 150 ms decoder
                // settling delay. A short gap makes adjacent word clips
                // sound like a phrase while retaining an I2S handoff margin.
                if (!audioService.startMp3(path.c_str(), 25)) {
                    success = false;
                    break;
                }
                while (audioService.isPlaying()) delay(2);
            }
            ttsLabPlaybackMs = millis() - started;
            ttsLabStatus = success ? "Complete" : "Missing or invalid word MP3";
            recoverKeyboardAfterBlockingOperation();
            audioScreens.drawTtsLab();
            return;
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
            aiChatScreen.draw();
            return;
        }
        if (keys.down) {
            if (aiScrollLines > 0) --aiScrollLines;
            aiChatScreen.draw();
            return;
        }
        if (keys.ctrl && pressedLetter(keys, 'r')) {
            if (!sdAvailable || WiFi.status() != WL_CONNECTED) {
                aiNotice = !sdAvailable ? "SD card required"
                                        : "Connect Wi-Fi first";
                aiChatScreen.draw();
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
            aiChatScreen.draw();
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
                    audioScreens.drawNowPlaying("AI reply",
                                                "OpenAI text-to-speech");
                    recoverKeyboardAfterBlockingOperation();
                    return;
                }
                aiNotice = aiService.status();
                recoverKeyboardAfterBlockingOperation();
            }
            aiChatScreen.draw();
            return;
        }
        if (keys.ctrl && pressedLetter(keys, 'n')) {
            aiService.clearHistory();
            aiNotice = "Conversation cleared";
            aiChatScreen.draw();
            return;
        }
        if (keys.tab) {
            aiService.toggleProvider();
            aiNotice = aiService.isConfigured() ? "Provider changed"
                                                 : "Provider key missing";
            aiChatScreen.draw();
            return;
        }
        if (keys.enter && !aiPrompt.isEmpty()) {
            const String prompt = aiPrompt;
            aiPrompt = "";
            aiNotice = "Waiting for " + String(aiService.providerName()) + "...";
            aiChatScreen.draw();
            String answer;
            if (!aiService.send(prompt, answer)) {
                aiPrompt = prompt;
                aiNotice = aiService.status();
            } else {
                aiNotice = "Ready";
                aiScrollLines = 0;
            }
            recoverKeyboardAfterBlockingOperation();
            aiChatScreen.draw();
            return;
        }
        if (keys.backspace && !aiPrompt.isEmpty()) {
            aiPrompt.remove(aiPrompt.length() - 1);
        }
        for (char value : keys.word) {
            if (!keys.ctrl && aiPrompt.length() < 500) aiPrompt += value;
        }
        aiChatScreen.drawComposer();
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
            menuScreens.drawBle();
            return;
        }
        if (!bleKeyboardService.isActive()) {
            if (keys.enter) {
                bleKeyboardService.begin(batteryPercentage());
            }
            bleScreens.drawKeyboard();
            return;
        }
        if (bleKeyboardService.isConnected()) {
            if (keys.enter) bleKeyboardService.sendAscii('\n');
            if (keys.backspace) bleKeyboardService.sendAscii('\b');
            if (keys.tab) bleKeyboardService.sendAscii('\t');
            for (char value : keys.word) bleKeyboardService.sendAscii(value);
        }
        bleScreens.drawKeyboard();
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
            sshScreens.drawConnect();
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
                sshScreens.drawSessionDynamic();
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
                               keys.left || pressedLetter(keys, ',') ||
                               pressedLetter(keys, 'q') ||
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

    bool up = keys.up || pressedLetter(keys, 'w') ||
                    pressedLetter(keys, 'k') || pressedLetter(keys, ';');
    bool down = keys.down || pressedLetter(keys, 's') ||
                      pressedLetter(keys, 'j') || pressedLetter(keys, '.');
    const bool navigationLeft = keys.left || pressedLetter(keys, ',');
    const bool navigationRight = keys.right || pressedLetter(keys, '/');
    const bool back = keys.esc || keys.backspace;
    const bool cardMenuScreen = cardNavigationEnabled &&
        (currentScreen == Screen::MainMenu ||
         currentScreen == Screen::ObserveMenu ||
         currentScreen == Screen::FieldKitMenu ||
         currentScreen == Screen::WifiMenu ||
         currentScreen == Screen::BleMenu ||
         currentScreen == Screen::GpsMenu ||
         currentScreen == Screen::MeshMenu ||
         currentScreen == Screen::NetworkMenu ||
         currentScreen == Screen::DevicesMenu ||
         currentScreen == Screen::ToolsMenu ||
         currentScreen == Screen::Settings);
    if (cardMenuScreen) {
        up = up || navigationLeft;
        down = down || navigationRight;
    }
    const bool settingsAdjustmentScreen =
        currentScreen == Screen::SettingsDisplay ||
        currentScreen == Screen::SettingsBoot ||
        currentScreen == Screen::SettingsConnectivity;
    const bool alternateBack =
        (navigationLeft && !cardMenuScreen && !settingsAdjustmentScreen) ||
        pressedLetter(keys, 'q') ||
        pressedLetter(keys, 'b');
    const bool refresh = pressedLetter(keys, 'r');
    const bool decrease = pressedLetter(keys, '-') ||
                          (settingsAdjustmentScreen && navigationLeft);
    const bool increase = pressedLetter(keys, '=') ||
                          (settingsAdjustmentScreen && navigationRight);

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
                menuSelection = menuSelection == 0 ? MenuScreens::kMenuCount - 1
                                                    : menuSelection - 1;
            } else if (down) {
                menuSelection = (menuSelection + 1) % MenuScreens::kMenuCount;
            } else if (keys.enter) {
                enterMenuItem();
                return;
            }
            menuScreens.drawMain();
            break;

        case Screen::ObserveMenu:
            if (up) moveSelection(-1, 5);
            if (down) moveSelection(1, 5);
            if (keys.enter) {
                const size_t mission = listSelection;
                listSelection = 0;
                listOffset = 0;
                switch (mission) {
                    case 0: currentScreen = Screen::WifiMenu; break;
                    case 1: currentScreen = Screen::BleMenu; break;
                    case 2: currentScreen = Screen::GpsMenu; break;
                    case 3: currentScreen = Screen::MeshMenu; break;
                    case 4: currentScreen = Screen::WarDrive; break;
                }
                drawCurrentScreen();
                return;
            }
            menuScreens.drawObserve();
            break;

        case Screen::FieldKitMenu:
            if (up) moveSelection(-1, 3);
            if (down) moveSelection(1, 3);
            if (keys.enter) {
                const size_t tool = listSelection;
                listSelection = 0;
                listOffset = 0;
                if (tool == 0) {
                    currentScreen = Screen::DevicesMenu;
                } else if (tool == 1) {
                    currentScreen = Screen::AiChat;
                    aiNotice = "";
                    aiService.loadConfig();
                } else {
                    currentScreen = Screen::ToolsMenu;
                }
                drawCurrentScreen();
                return;
            }
            menuScreens.drawFieldKit();
            break;

        case Screen::AiChat:
            // Handled before global navigation so printable letters remain
            // prompt input rather than menu shortcuts.
            break;

        case Screen::WifiMenu:
            if (up) moveSelection(-1, 5);
            if (down) moveSelection(1, 5);
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
                } else if (listSelection == 3) {
                    currentScreen = Screen::WifiGuardian;
                    startWifiGuardian();
                    drawCurrentScreen();
                } else {
                    currentScreen = Screen::WifiConnectSelect;
                    drawCurrentScreen();
                    scanWifiNetworks();
                }
                return;
            }
            menuScreens.drawWifi();
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
                wifiScreens.drawDetail();
                return;
            }
            wifiScreens.drawRecon();
            break;

        case Screen::WifiChannelAnalyzer:
            if (refresh) {
                scanWifiNetworks();
                return;
            }
            wifiScreens.drawChannelAnalyzer();
            break;

        case Screen::WifiDetail:
            if (pressedLetter(keys, 'd') && !accessPoints.empty()) {
                currentScreen = Screen::WifiDeauthConfirm;
                wifiScreens.drawDeauthConfirm();
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
                wifiScreens.drawHandshakeCapture();
                return;
            }
            break;

        case Screen::WifiDeauthConfirm:
            if (keys.enter && !accessPoints.empty()) {
                transmitWifiDeauth(accessPoints[listSelection]);
                currentScreen = Screen::WifiDetail;
                wifiScreens.drawDetail();
                return;
            }
            break;

        case Screen::WifiHandshakeCapture:
            if (pressedLetter(keys, 'd') && !accessPoints.empty()) {
                transmitWifiDeauth(accessPoints[listSelection]);
                wifiScreens.drawHandshakeCapture();
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
            wifiScreens.drawHandshakeCapture();
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
            wifiScreens.drawConnectSelect();
            break;
        }

        case Screen::WifiConnectStatus:
            if (pressedLetter(keys, 'd')) {
                WiFi.disconnect(true);
                wifiConnectAttempting = false;
                wifiConnectStatusText = "Disconnected";
                wifiScreens.drawConnectStatus();
                return;
            }
            wifiScreens.drawConnectStatus();
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
            menuScreens.drawBle();
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
            menuScreens.drawDevices();
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
                biscuitScreens.drawMain();
                return;
            }
            if (keys.enter && biscuitClient.isConnected()) {
                listSelection = 0;
                currentScreen = Screen::BiscuitTools;
                biscuitScreens.drawTools();
                return;
            }
            biscuitScreens.drawMain();
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
                    biscuitScreens.drawWardrive();
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
                biscuitScreens.drawResult();
                return;
            }
            biscuitScreens.drawTools();
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
                biscuitScreens.drawWardrive();
                return;
            }
            biscuitScreens.drawWardrive();
            break;

        case Screen::BiscuitResult:
            if (up && biscuitResultOffset > 0) --biscuitResultOffset;
            if (down && biscuitResultOffset + kVisibleRows <
                            biscuitResultLines.size()) {
                ++biscuitResultOffset;
            }
            biscuitScreens.drawResult();
            break;

        case Screen::BleSpamSelect: {
            if (up) moveSelection(-1, BleSpamScreen::kModeCount);
            if (down) moveSelection(1, BleSpamScreen::kModeCount);
            if (keys.enter) {
                bleSpamService.begin(
                    BleSpamScreen::modeForSelection(listSelection));
                currentScreen = Screen::BleSpam;
                bleSpamScreen.drawActive();
                return;
            }
            normalizeListPosition(BleSpamScreen::kModeCount);
            bleSpamScreen.drawSelect(listSelection);
            break;
        }

        case Screen::BleSpam:
            bleSpamScreen.drawActive();
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
            menuScreens.drawRfid();
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
                chameleonScreen.draw();
                return;
            }
            if (pressedLetter(keys, 's') && chameleonClient.isConnected()) {
                chameleonWorkflowStatus = "";
                performChameleonScan();
                chameleonScreen.draw();
                return;
            }
            if (pressedLetter(keys, 'c') && chameleonClient.isConnected()) {
                chameleonWorkflowStatus = "";
                chameleonContinuousScan = !chameleonContinuousScan;
                chameleonScreen.draw();
                return;
            }
            if (pressedLetter(keys, 'v') && chameleonClient.isConnected()) {
                saveChameleonIdentity();
                chameleonScreen.draw();
                return;
            }
            if (pressedLetter(keys, 'o') && chameleonClient.isConnected()) {
                loadChameleonIdentity();
                chameleonScreen.draw();
                return;
            }
            if (pressedLetter(keys, 'd') && chameleonClient.isConnected()) {
                chameleonClient.setReaderMode();
                chameleonWorkflowStatus = chameleonClient.lastStatus();
                chameleonScreen.draw();
                return;
            }
            if (pressedLetter(keys, 'e') && chameleonClient.isConnected() &&
                (chameleonHfFound || chameleonLfFound)) {
                chameleonContinuousScan = false;
                currentScreen = Screen::ChameleonEmulateConfirm;
                chameleonScreen.drawEmulateConfirm();
                return;
            }
            chameleonScreen.draw();
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
                chameleonScreen.draw();
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
                bleScreens.drawDiscovery();
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
                bleScreens.drawDiscovery();
                return;
            }
            if (up) moveSelection(-1, bleDevices.size());
            if (down) moveSelection(1, bleDevices.size());
            if (keys.enter && !bleDevices.empty()) {
                currentScreen = Screen::BleDetail;
                bleScreens.drawDetail();
                return;
            }
            bleScreens.drawDiscovery();
            break;

        case Screen::CyberFamiliar:
            if (up) familiarPage = (familiarPage + 2) % 3;
            if (down) familiarPage = (familiarPage + 1) % 3;
            if (pressedLetter(keys, 'p')) cyberFamiliar.interact();
            if (pressedLetter(keys, 'n')) cyberFamiliar.cycleName();
            if (pressedLetter(keys, 'i')) cyberFamiliar.toggleIdleMode();
            if (pressedLetter(keys, 'w')) {
                screenSleeping = true;
                familiarIdleActive = true;
                familiarIdleDrawn = false;
                cyberdeckIdleActive = false;
                lastFamiliarDraw = 0;
                drawCyberFamiliarIdle();
                return;
            }
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
                familiarScreens.drawResetConfirm();
                return;
            }
            if (pressedLetter(keys, 'a')) {
                currentScreen = familiarPatrolService.isActive()
                                    ? Screen::FamiliarPatrol
                                    : Screen::FamiliarPatrolConfirm;
                drawCurrentScreen();
                return;
            }
            familiarScreens.drawFamiliar();
            break;

        case Screen::FamiliarPatrolConfirm:
            if (pressedLetter(keys, 'c')) {
                familiarPatrolContinuousChoice =
                    !familiarPatrolContinuousChoice;
                familiarScreens.drawPatrolConfirm();
                return;
            }
            if (pressedLetter(keys, 'v')) {
                familiarPatrolIntervalIndex = static_cast<uint8_t>(
                    (familiarPatrolIntervalIndex + 1) %
                    (sizeof(kFamiliarPatrolIntervals) /
                     sizeof(kFamiliarPatrolIntervals[0])));
                familiarScreens.drawPatrolConfirm();
                return;
            }
            if (keys.enter && sdAvailable && WiFi.status() == WL_CONNECTED) {
                if (familiarPatrolService.start(WiFi.localIP(),
                        WiFi.subnetMask(), familiarPatrolContinuousChoice,
                        kFamiliarPatrolIntervals[familiarPatrolIntervalIndex])) {
                    cyberFamiliar.notePatrol("Patrol started. Mapping the deck.",
                                             10, FamiliarMood::Excited);
                    triggerFamiliarReaction(FamiliarReaction::Searching, 2200);
                    showFamiliarSpeech("Let's see who's here!", 2600);
                    playFamiliarCue(FamiliarCue::Started);
                    lastFamiliarPatrolState = familiarPatrolService.state();
                    currentScreen = Screen::FamiliarPatrol;
                }
                drawCurrentScreen();
            }
            break;

        case Screen::FamiliarPatrol:
            if (pressedLetter(keys, 'x')) {
                familiarPatrolService.stop();
                cyberFamiliar.notePatrol("Patrol stopped by operator.", 0,
                                         FamiliarMood::Content);
            }
            familiarScreens.drawPatrol();
            break;

        case Screen::CyberFamiliarResetConfirm:
            if (keys.enter) {
                cyberFamiliar.resetProgress();
                familiarWorkflowStatus = "Familiar progress reset";
                familiarPage = 0;
                currentScreen = Screen::CyberFamiliar;
                familiarScreens.drawFamiliar();
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
                        evidenceReturnScreen = Screen::ToolsMenu;
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
            menuScreens.drawTools();
            break;

        case Screen::Infrared:
            if (keys.enter || refresh) {
                irScreen.transmitSelfTest();
                return;
            }
            irScreen.draw();
            break;

        case Screen::UsbHid:
            if (up) moveSelection(-1, kHidPresetCount + 1);
            if (down) moveSelection(1, kHidPresetCount + 1);
            if (keys.enter) {
                if (listSelection == kHidPresetCount) {
                    loadDuckyScripts();
                    currentScreen = Screen::DuckyScripts;
                    usbHidScreens.drawDuckyScripts();
                } else {
                    currentScreen = Screen::UsbHidConfirm;
                    usbHidScreens.drawUsbHidConfirm();
                }
                return;
            }
            usbHidScreens.drawUsbHid();
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
                usbHidScreens.drawDuckyConfirm();
                return;
            }
            usbHidScreens.drawDuckyScripts();
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
                usbHidScreens.drawDuckyScripts();
            }
            break;

        case Screen::Audio:
            if (up) moveSelection(-1, 4);
            if (down) moveSelection(1, 4);
            if (keys.enter) {
                if (listSelection == 0) {
                    audioService.playToneTest();
                    recoverKeyboardAfterBlockingOperation();
                    audioScreens.drawMenu();
                } else if (listSelection == 1) {
                    microphoneLevel = 0;
                    if (audioService.beginMicrophone()) {
                        currentScreen = Screen::AudioMic;
                        audioScreens.drawMicrophone();
                    }
                } else if (listSelection == 2) {
                    loadAudioFiles();
                    currentScreen = Screen::AudioFiles;
                    audioScreens.drawAudioFiles();
                } else {
                    currentScreen = Screen::TtsLab;
                    audioScreens.drawTtsLab();
                }
                return;
            }
            audioScreens.drawMenu();
            break;

        case Screen::AudioMic:
            break;

        case Screen::TtsLab:
            // Text entry is handled before the general screen switch.
            break;

        case Screen::AudioFiles:
            if (refresh) loadAudioFiles();
            if (up) moveSelection(-1, audioFiles.size());
            if (down) moveSelection(1, audioFiles.size());
            if (keys.enter) {
                startSelectedMp3();
                return;
            }
            audioScreens.drawAudioFiles();
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
                    fileScreens.drawFileDetail();
                    return;
                }
            }
            fileScreens.drawFiles();
            break;

        case Screen::FileDetail:
            if (keys.enter) {
                if (FileScreens::isMp3File(files[listSelection].name)) {
                    playSelectedBrowserMp3();
                } else if (FileScreens::isPreviewableFile(files[listSelection].name) &&
                           loadTextPreview()) {
                    textPreviewReturnScreen = Screen::FileDetail;
                    currentScreen = Screen::TextPreview;
                    fileScreens.drawTextPreview();
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
            fileScreens.drawTextPreview();
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
                logScreens.drawDetail();
                return;
            }
            logScreens.drawSessions();
            break;

        case Screen::LogDetail:
            if (pressedLetter(keys, 'd')) {
                currentScreen = Screen::LogDeleteConfirm;
                logScreens.drawDeleteConfirm();
            } else if (keys.enter &&
                       FileScreens::isPreviewableFile(logSessions[logSelection].name)) {
                const auto& log = logSessions[logSelection];
                const int slash = log.path.lastIndexOf('/');
                currentPath = slash <= 0 ? "/" : log.path.substring(0, slash);
                files.clear();
                files.push_back({log.name, false, log.size});
                listSelection = 0;
                if (loadTextPreview()) {
                    textPreviewReturnScreen = Screen::LogDetail;
                    currentScreen = Screen::TextPreview;
                    fileScreens.drawTextPreview();
                }
            }
            break;

        case Screen::LogDeleteConfirm:
            if (keys.enter && !logSessions.empty()) {
                SD.remove(logSessions[logSelection].path);
                loadLogSessions();
                currentScreen = Screen::LogSessions;
                logScreens.drawSessions();
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
                systemScreens.drawTimeStatus();
                return;
            }
            systemScreens.drawSystem(systemDiagnostics());
            break;

        case Screen::TimeStatus:
            if (pressedLetter(keys, 'g')) syncClockFromGnss();
            if (pressedLetter(keys, 'n')) syncClockFromNtp();
            systemScreens.drawTimeStatus();
            break;

        case Screen::GpsMenu:
            if (keys.enter) {
                gnssService.begin();
                currentScreen = Screen::Gnss;
                gnssScreen.draw();
                return;
            }
            menuScreens.drawGps();
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
                loraScreen.draw();
                return;
            }
            menuScreens.drawMesh();
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
                networkScanScreens.drawWarDriveDynamic();
                return;
            }
            networkScanScreens.drawWarDriveDynamic();
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
            menuScreens.drawNetwork();
            break;

        case Screen::NetworkDashboard:
            if (refresh) networkScanScreens.drawNetworkDashboard();
            break;

        case Screen::NetworkHostScan:
            if (WiFi.status() != WL_CONNECTED) {
                networkScanScreens.drawNetworkHostScan();
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
                networkScanScreens.drawNetworkHostScan();
                return;
            }
            if (pressedLetter(keys, 'e')) {
                exportNetworkHostResults();
                networkScanScreens.drawNetworkHostScan();
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
            networkScanScreens.drawNetworkHostScan();
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
                    networkScanScreens.drawNetworkPortScan();
                } else {
                    scanNetworkPorts(networkPortScanTarget);
                }
                return;
            }
            if (pressedLetter(keys, 'e')) {
                exportNetworkPortResults();
                networkScanScreens.drawNetworkPortScan();
                return;
            }
            if (up) moveSelection(-1, networkPortResults.size());
            if (down) moveSelection(1, networkPortResults.size());
            networkScanScreens.drawNetworkPortScan();
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
            gnssScreen.draw();
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
            loraScreen.draw();
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
            wifiSnifferScreen.draw();
            break;

        case Screen::WifiGuardian:
            if (pressedLetter(keys, 's')) {
                wifiGuardianService.cycleSensitivity();
                guardianLastEvent = String("Sensitivity: ") +
                                    wifiGuardianService.sensitivityName();
            } else if (refresh) {
                stopWifiGuardian();
                startWifiGuardian();
            }
            wifiGuardianScreen.draw();
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
            imuScreen.draw();
            break;

        case Screen::Settings:
            if (up) moveSelection(-1, 5);
            if (down) moveSelection(1, 5);
            if (keys.enter) {
                if (listSelection == 0) currentScreen = Screen::SettingsDisplay;
                else if (listSelection == 1) currentScreen = Screen::SettingsBoot;
                else if (listSelection == 2) currentScreen = Screen::SettingsConnectivity;
                else if (listSelection == 3) currentScreen = Screen::SettingsReset;
                else {
                    listSelection = 0;
                    listOffset = 0;
                    checkForFirmwareUpdate();
                    return;
                }
                listSelection = 0;
                listOffset = 0;
                drawCurrentScreen();
                return;
            }
            menuScreens.drawSettings();
            break;

        case Screen::SettingsDisplay:
            if (up) moveSelection(-1, 8);
            if (down) moveSelection(1, 8);
            normalizeListPosition(8);
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
                } else if (listSelection == 5) {
                    familiarCueIndex = static_cast<uint8_t>(
                        increase
                            ? (familiarCueIndex + 1) % kFamiliarCueCount
                            : (familiarCueIndex + kFamiliarCueCount - 1) %
                                  kFamiliarCueCount);
                    playFamiliarCue(FamiliarCue::Host);
                } else if (listSelection == 6) {
                    cardNavigationEnabled = !cardNavigationEnabled;
                } else if (listSelection == 7) {
                    cyberdeckIdleStyle = static_cast<uint8_t>(
                        increase
                            ? (cyberdeckIdleStyle + 1) %
                                  kCyberdeckIdleStyleCount
                            : (cyberdeckIdleStyle +
                               kCyberdeckIdleStyleCount - 1) %
                                  kCyberdeckIdleStyleCount);
                }
                applySettings();
                saveSettings();
                lastUserActivity = millis();
            }
            if (keys.enter && listSelection == 7) {
                screenSleeping = true;
                familiarIdleActive = false;
                beginCyberdeckIdle();
                drawCyberdeckIdle();
                return;
            }
            settingsScreens.drawDisplay();
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
                    bootSpeedIndex = static_cast<uint8_t>(
                        increase ? (bootSpeedIndex + 1) % kBootSpeedCount
                                 : (bootSpeedIndex + kBootSpeedCount - 1) %
                                       kBootSpeedCount);
                }
                saveSettings();
            }
            if (keys.enter && listSelection == 1) {
                previewBootSound();
            }
            if (keys.enter && listSelection == 2) {
                previewBootAnimation();
            }
            if (keys.enter && listSelection == 4) {
                previewBootSound();
            }
            if (keys.enter && listSelection == 5) {
                previewBootAnimation();
            }
            settingsScreens.drawBoot();
            break;

        case Screen::SettingsConnectivity:
            if (up) moveSelection(-1, 2);
            if (down) moveSelection(1, 2);
            if ((decrease || increase) && listSelection == 0) {
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
            if ((decrease || increase) && listSelection == 1) {
                if (saveWifiCredentials) autoConnectWifi = !autoConnectWifi;
                saveSettings();
            }
            settingsScreens.drawConnectivity();
            break;

        case Screen::SettingsReset:
            if (keys.enter) {
                restoreDefaultSettings();
                currentScreen = Screen::Settings;
                listSelection = 0;
                menuScreens.drawSettings();
            }
            break;

        case Screen::Placeholder:
        case Screen::About: drawCurrentScreen(); break;

        case Screen::OtaCheck:
            if (keys.enter && otaService.hasVerifiedUpdate()) {
                installFirmwareUpdate();
                return;
            }
            if (refresh) {
                checkForFirmwareUpdate();
                return;
            }
            otaScreens.drawCheck();
            break;

        case Screen::OtaInstalling:
            // downloadAndInstall() polls for its own cancel key and blocks
            // until it returns; this screen is never live in the normal
            // input loop, but the switch stays exhaustive for consistency
            // with every other screen here.
            otaScreens.drawInstalling();
            break;

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
    for (size_t node = 0; node < kIdleNodeCount; ++node) {
        idleNodeX[node] = random(8, M5Cardputer.Display.width() - 8);
        idleNodeY[node] = random(8, M5Cardputer.Display.height() - 26);
        idleNodeDx[node] = random(0, 2) == 0 ? -1 : 1;
        idleNodeDy[node] = random(0, 2) == 0 ? -1 : 1;
    }
    cyberdeckIdleCanvasReady = cyberdeckIdleCanvas.createSprite(
        M5Cardputer.Display.width(), M5Cardputer.Display.height() - 18);
    M5Cardputer.Display.fillScreen(Branding::background);
}

void drawCyberdeckIdle() {
    const unsigned long now = millis();
    if (now - lastCyberdeckDraw < 50) return;
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

    if (!cyberdeckIdleCanvasReady) return;
    auto& canvas = cyberdeckIdleCanvas;
    const int contentHeight = height - 18;
    canvas.fillSprite(Branding::background);
    canvas.setTextSize(1);
    static constexpr char kGlyphs[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ<>/\\[]{}#@";
    static constexpr size_t kGlyphCount = sizeof(kGlyphs) - 1;
    if (cyberdeckIdleStyle == 0) {
        for (size_t column = 0; column < kCyberdeckColumns; ++column) {
            if ((now / 50U) % cyberdeckRainSpeed[column] == 0) {
                ++cyberdeckRainHead[column];
            }
            if (cyberdeckRainHead[column] - 6 > rows) {
                cyberdeckRainHead[column] = random(-rows, 1);
                cyberdeckRainSpeed[column] = static_cast<uint8_t>(random(1, 4));
            }
            for (int tail = 6; tail >= 0; --tail) {
                const int row = cyberdeckRainHead[column] - tail;
                if (row < 0 || row >= rows) continue;
                const uint16_t colour = tail == 0
                    ? (radioPulse ? Branding::warning : Branding::text)
                    : (tail <= 2 ? Branding::accent : Branding::muted);
                canvas.setTextColor(colour, Branding::background);
                canvas.setCursor(static_cast<int>(column) * columnWidth,
                                 row * 8);
                canvas.print(kGlyphs[random(kGlyphCount)]);
            }
        }
    } else if (cyberdeckIdleStyle == 1) {
        const int cx = width / 2;
        const int cy = contentHeight / 2;
        const int radius = 43;
        canvas.drawCircle(cx, cy, radius, Branding::muted);
        canvas.drawCircle(cx, cy, 28, Branding::panel);
        canvas.drawCircle(cx, cy, 14, Branding::panel);
        const float angle = static_cast<float>(now % 4000UL) *
                            6.2831853F / 4000.0F;
        canvas.drawLine(cx, cy, cx + cosf(angle) * radius,
                        cy + sinf(angle) * radius, Branding::accent);
        for (size_t node = 0; node < 5; ++node) {
            const float nodeAngle = node * 1.19F + 0.4F;
            const int nodeRadius = 12 + static_cast<int>(node * 7);
            canvas.fillCircle(cx + cosf(nodeAngle) * nodeRadius,
                              cy + sinf(nodeAngle) * nodeRadius,
                              radioPulse && node == 0 ? 3 : 2,
                              radioPulse && node == 0 ? Branding::warning
                                                       : Branding::text);
        }
    } else {
        for (size_t node = 0; node < kIdleNodeCount; ++node) {
            idleNodeX[node] += idleNodeDx[node];
            idleNodeY[node] += idleNodeDy[node];
            if (idleNodeX[node] <= 4 || idleNodeX[node] >= width - 4)
                idleNodeDx[node] = -idleNodeDx[node];
            if (idleNodeY[node] <= 4 || idleNodeY[node] >= contentHeight - 4)
                idleNodeDy[node] = -idleNodeDy[node];
            for (size_t other = node + 1; other < kIdleNodeCount; ++other) {
                const int dx = idleNodeX[node] - idleNodeX[other];
                const int dy = idleNodeY[node] - idleNodeY[other];
                if (dx * dx + dy * dy < 1600) {
                    canvas.drawLine(idleNodeX[node], idleNodeY[node],
                                    idleNodeX[other], idleNodeY[other],
                                    Branding::panel);
                }
            }
            canvas.fillCircle(idleNodeX[node], idleNodeY[node],
                              radioPulse && node == 0 ? 3 : 2,
                              radioPulse && node == 0 ? Branding::warning
                                                       : Branding::accent);
        }
    }
    canvas.pushSprite(0, 0);

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
        case Screen::WifiGuardian: cyberFamiliar.observeTool(17, "guardian"); break;
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

// Forwarders for extracted screen modules (see include/screen_chrome.h).
// These call straight into the internal-linkage implementations above --
// extracted .cpp files live in a different translation unit and cannot see
// symbols inside this file's anonymous namespace directly.
namespace ScreenChrome {
void drawHeader(const char* title) { ::drawHeader(title); }
void drawFooter(const char* text) { ::drawFooter(text); }
void recoverKeyboard() { ::recoverKeyboardAfterBlockingOperation(); }
void beginContentUpdate(const char* title, bool fullDraw) {
    ::beginContentUpdate(title, fullDraw);
}
void drawListRow(int row, const String& label, bool selected,
                 const String& suffix) {
    ::drawListRow(row, label, selected, suffix);
}
void normalizeListPosition(size_t count) { ::normalizeListPosition(count); }
void drawHeaderPosition(size_t oneBasedIndex, size_t total) {
    ::drawHeaderPosition(oneBasedIndex, total);
}
void drawNavigationCard(const char* header, const String& label,
                        const String& description, size_t selected,
                        size_t count, uint8_t icon, const String& badge) {
    ::drawNavigationCard(header, label, description, selected, count, icon,
                         badge);
}
void drawTextEntryRow(int y, const char* label, const String& value,
                      bool masked) {
    ::drawTextEntryRow(y, label, value, masked);
}
}  // namespace ScreenChrome

void setup() {
    auto config = M5.config();
    config.output_power = true;
    M5Cardputer.begin(config, true);
    Serial.begin(115200);
    preferences.begin("ghostwire", false);
    verifyOtaBootOrRollback();
    speakerVolume = preferences.getUChar("volume", kDefaultVolume);
    screenBrightness =
        preferences.getUChar("brightness", kDefaultBrightness);
    screenTimeoutSeconds =
        preferences.getUShort("timeout", kDefaultScreenTimeout);
    bootSoundEnabled =
        preferences.getBool("boot_sound", kDefaultBootSound);
    if (preferences.isKey("boot_speed")) {
        bootSpeedIndex = preferences.getUChar("boot_speed", kDefaultBootSpeed);
    } else {
        // Migrate the former binary Fast boot setting without changing the
        // user's established startup preference.
        bootSpeedIndex = preferences.getBool("fast_boot", false) ? 2 : 1;
    }
    if (bootSpeedIndex >= kBootSpeedCount) bootSpeedIndex = kDefaultBootSpeed;
    saveWifiCredentials =
        preferences.getBool("save_wifi", kDefaultSaveWifiCredentials);
    autoConnectWifi =
        preferences.getBool("auto_wifi", kDefaultAutoConnectWifi);
    cyberdeckIdleEnabled =
        preferences.getBool("cyber_idle", kDefaultCyberdeckIdle);
    cyberdeckIdleStyle =
        preferences.getUChar("idle_style", kDefaultCyberdeckIdleStyle);
    if (cyberdeckIdleStyle >= kCyberdeckIdleStyleCount) {
        cyberdeckIdleStyle = kDefaultCyberdeckIdleStyle;
    }
    cardNavigationEnabled =
        preferences.getBool("nav_cards", kDefaultCardNavigation);
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
    familiarCueIndex = preferences.getUChar(
        "fam_cue", kDefaultFamiliarCue);
    if (familiarCueIndex >= kFamiliarCueCount) {
        familiarCueIndex = kDefaultFamiliarCue;
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
    if (sdAvailable) familiarPatrolService.begin();
    cyberFamiliar.begin(preferences);
    chameleonSavedPath = preferences.getString("cham_last", "");
    if (isAbnormalReset(esp_reset_reason())) cyberFamiliar.noteRecovery();
    showSplash();
    menuScreens.drawMain();
    markBootHealthy();

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
    familiarPatrolService.update();
    updateFamiliarVoice();
    const uint32_t patrolHosts = familiarPatrolService.hostsFound();
    const uint32_t patrolOpenPorts = familiarPatrolService.openPortsFound();
    if (patrolHosts < familiarObservedPatrolHosts) {
        familiarObservedPatrolHosts = patrolHosts;
    } else if (patrolHosts > familiarObservedPatrolHosts) {
        familiarObservedPatrolHosts = patrolHosts;
        triggerFamiliarReaction(FamiliarReaction::HostFound, 1400);
        showFamiliarSpeech("Oh! Found " + familiarHostLabel(
                               familiarPatrolService.lastSeenHost()) + "!");
        playFamiliarCue(FamiliarCue::Host);
    }
    if (patrolOpenPorts < familiarObservedOpenPorts) {
        familiarObservedOpenPorts = patrolOpenPorts;
    } else if (patrolOpenPorts > familiarObservedOpenPorts) {
        familiarObservedOpenPorts = patrolOpenPorts;
        triggerFamiliarReaction(
            familiarSensitivePort(familiarPatrolService.lastOpenPort())
                ? FamiliarReaction::Warning
                : FamiliarReaction::ServiceFound,
            1900);
        const uint16_t foundPort = familiarPatrolService.lastOpenPort();
        const String endpoint = familiarHostLabel(
            familiarPatrolService.lastOpenHost());
        if (familiarSensitivePort(foundPort)) {
            showFamiliarSpeech(String("Careful: ") +
                               familiarServiceName(foundPort) + " on " +
                               endpoint + "!");
            playFamiliarCue(FamiliarCue::Warning);
        } else {
            showFamiliarSpeech(String("Interesting: ") +
                               familiarServiceName(foundPort) + " on " +
                               endpoint + "!");
            playFamiliarCue(FamiliarCue::Service);
        }
    }
    const FamiliarPatrolState patrolState = familiarPatrolService.state();
    if (patrolState != lastFamiliarPatrolState) {
        if (patrolState == FamiliarPatrolState::CommonPorts) {
            const uint32_t fresh = familiarPatrolService.cycleHostsFound();
            const String message =
                fresh == 0 ? "Quiet map. No new hosts this pass."
                           : "Map ready. " + String(fresh) +
                                 (fresh == 1 ? " new host." : " new hosts.");
            cyberFamiliar.notePatrol(
                message, fresh == 0 ? 0 : 10,
                fresh == 0 ? FamiliarMood::Content : FamiliarMood::Curious);
        } else if (patrolState == FamiliarPatrolState::Complete) {
            const uint32_t fresh = familiarPatrolService.cycleHostsFound();
            cyberFamiliar.notePatrol(
                fresh == 0 ? "Quiet patrol. No new hosts."
                           : "Patrol complete. New leads saved.",
                fresh == 0 ? 5 : 30,
                fresh == 0 ? FamiliarMood::Content : FamiliarMood::Proud);
            triggerFamiliarReaction(FamiliarReaction::Complete, 4000);
            showFamiliarSpeech(
                fresh == 0 ? "All quiet. Nothing new."
                           : "All done! " + String(fresh) +
                                 (fresh == 1 ? " new host." : " new hosts."),
                4000);
            playFamiliarCue(FamiliarCue::Complete);
        } else if (patrolState == FamiliarPatrolState::WatchWait) {
            const uint32_t fresh = familiarPatrolService.cycleHostsFound();
            cyberFamiliar.notePatrol(
                fresh == 0 ? "Quiet watch pass. Nothing changed."
                           : "Scout pass found new company. Keeping watch.",
                fresh == 0 ? 3 : 10,
                fresh == 0 ? FamiliarMood::Sleepy : FamiliarMood::Proud);
            triggerFamiliarReaction(FamiliarReaction::Complete, 3000);
            showFamiliarSpeech(
                fresh == 0 ? "Still quiet. I'll keep watch!"
                           : "I found " + String(fresh) +
                                 (fresh == 1 ? " new host!" : " new hosts!"),
                3400);
            playFamiliarCue(FamiliarCue::Complete);
        } else if (patrolState == FamiliarPatrolState::Error) {
            cyberFamiliar.notePatrol("Patrol needs operator attention.", 0,
                                     FamiliarMood::Worried);
            showFamiliarSpeech("I need some help!", 3500);
            playFamiliarCue(FamiliarCue::Error);
        }
        lastFamiliarPatrolState = patrolState;
    }
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
        if (wifiGuardianService.isActive()) {
            wifiGuardianService.ingest(rawFrame);
            if (WifiGuardianService::isDisruptionFrame(rawFrame)) {
                guardianLastChannel = rawFrame.channel;
                guardianLastRssi = rawFrame.rssi;
                if (guardianEvidenceLogger.isActive()) {
                    guardianEvidenceLogger.append(
                        rawFrame.data, rawFrame.length,
                        static_cast<uint32_t>(time(nullptr)),
                        static_cast<uint32_t>(micros() % 1000000UL));
                }
            }
        }
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
    guardianEvidenceLogger.update();
    wifiGuardianService.update();
    String guardianAlert;
    if (wifiGuardianService.takeAlert(guardianAlert)) {
        guardianLastEvent = guardianAlert;
        if (guardianEventLogger.isActive()) {
            guardianEventLogger.append(
                utcTimestamp() + "," + csvSafePayload(guardianAlert) + "," +
                String(guardianLastChannel) + "," + String(guardianLastRssi) +
                "," + String(wifiGuardianService.deauthFrames()) + "," +
                String(wifiGuardianService.disassocFrames()));
        }
        cyberFamiliar.notePatrol(
            "Guardian observed unusual disconnect traffic.", 8,
            FamiliarMood::Worried);
        triggerFamiliarReaction(FamiliarReaction::Warning, 4500);
        showFamiliarSpeech("Warning! Disconnect burst observed.", 4500);
        playFamiliarCue(FamiliarCue::Warning);
    }
    guardianEventLogger.update();

    if (screenSleeping) {
        if (M5Cardputer.Keyboard.isChange() &&
            M5Cardputer.Keyboard.isPressed()) {
            screenSleeping = false;
            if (cyberdeckIdleCanvasReady) {
                cyberdeckIdleCanvas.deleteSprite();
                cyberdeckIdleCanvasReady = false;
            }
            cyberdeckIdleActive = false;
            familiarIdleActive = false;
            familiarIdleDrawn = false;
            M5Cardputer.Display.setBrightness(screenBrightness);
            lastUserActivity = millis();
            drawCurrentScreen();
        } else if (familiarIdleActive &&
                   millis() - lastFamiliarDraw >= 180) {
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
        millis() - lastFamiliarDraw >= 180) {
        lastFamiliarDraw = millis();
        familiarScreens.drawFamiliar(false);
    }

    if (currentScreen == Screen::BleDiscovery && !actionMenuOpen &&
        bleScanner.isContinuous() && bleCaptureUiDirty &&
        millis() - lastBleCaptureDraw >= 500) {
        lastBleCaptureDraw = millis();
        bleCaptureUiDirty = false;
        bleScreens.drawDiscovery();
    }

    if (currentScreen == Screen::AudioMic) {
        uint16_t level = 0;
        if (audioService.updateMicrophone(level)) {
            microphoneLevel = level;
            if (!screenSleeping && millis() - lastMicrophoneDraw >= 80) {
                lastMicrophoneDraw = millis();
                audioScreens.updateMicrophoneMeter();
            }
        }
    }

    if (currentScreen == Screen::Gnss && !actionMenuOpen &&
        millis() - lastGnssDraw >= 500) {
        lastGnssDraw = millis();
        gnssScreen.draw(false);
    }

    if (currentScreen == Screen::LoRa && !actionMenuOpen &&
        millis() - lastLoRaDraw >= 500) {
        lastLoRaDraw = millis();
        loraScreen.draw(false);
    }

    if (currentScreen == Screen::WifiSniffer && !actionMenuOpen &&
        millis() - lastWifiSnifferDraw >= 500) {
        lastWifiSnifferDraw = millis();
        wifiSnifferScreen.draw(false);
    }

    if (currentScreen == Screen::WifiGuardian && !actionMenuOpen &&
        millis() - lastGuardianDraw >= 500) {
        lastGuardianDraw = millis();
        wifiGuardianScreen.draw(false);
    }

    if (currentScreen == Screen::BleSpam && !actionMenuOpen &&
        millis() - lastBleSpamDraw >= 300) {
        lastBleSpamDraw = millis();
        bleSpamScreen.drawActive(false);
    }

    if (currentScreen == Screen::BleKeyboard && !actionMenuOpen &&
        millis() - lastBleKeyboardDraw >= 500) {
        lastBleKeyboardDraw = millis();
        bleScreens.drawKeyboard(false);
    }

    if (currentScreen == Screen::Chameleon && chameleonContinuousScan &&
        chameleonClient.isConnected() &&
        millis() - lastChameleonScanMs >= 500) {
        lastChameleonScanMs = millis();
        performChameleonScan();
        if (!actionMenuOpen) chameleonScreen.draw(false);
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
            biscuitScreens.drawWardrive(false);
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
        networkScanScreens.drawWarDriveDynamic();
    }

    if (currentScreen == Screen::FamiliarPatrol && !actionMenuOpen &&
        millis() - lastFamiliarPatrolDraw >= 500) {
        lastFamiliarPatrolDraw = millis();
        familiarScreens.drawPatrol(false);
    }

    networkHostScanService.update();
    NetworkHostResult networkHostResult;
    while (networkHostScanService.nextHostResult(networkHostResult)) {
        networkHostResults.push_back(networkHostResult);
    }
    if (currentScreen == Screen::NetworkHostScan && !actionMenuOpen &&
        millis() - lastNetworkHostScanDraw >= 500) {
        lastNetworkHostScanDraw = millis();
        networkScanScreens.drawNetworkHostScan(false);
    }

    networkPortScanService.update();
    NetworkPortResult networkPortResult;
    while (networkPortScanService.nextPortResult(networkPortResult)) {
        networkPortResults.push_back(networkPortResult.port);
    }
    if (currentScreen == Screen::NetworkPortScan && !actionMenuOpen &&
        millis() - lastNetworkPortScanDraw >= 500) {
        lastNetworkPortScanDraw = millis();
        networkScanScreens.drawNetworkPortScan(false);
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
            if (newData) telnetScreens.drawSessionDynamic();
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
            if (newData) sshScreens.drawSessionDynamic();
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
        wifiScreens.drawConnectStatus(false);
    }

    if (currentScreen == Screen::WifiHandshakeCapture && !actionMenuOpen &&
        millis() - lastHandshakeCaptureDraw >= 500) {
        lastHandshakeCaptureDraw = millis();
        wifiScreens.drawHandshakeCapture(false);
    }

    if (currentScreen == Screen::Imu && !actionMenuOpen &&
        millis() - lastImuDraw >= 100) {
        lastImuDraw = millis();
        imuScreen.draw(false);
    }

    if (millis() - lastHeaderStatusDraw >= 1000) {
        lastHeaderStatusDraw = millis();
        drawHeaderStatus();
    }

    if (currentScreen == Screen::TimeStatus && !actionMenuOpen &&
        millis() - lastTimeStatusDraw >= 1000) {
        lastTimeStatusDraw = millis();
        systemScreens.drawTimeReadouts();
    }

    if (!screenSleeping && screenTimeoutSeconds > 0 &&
        millis() - lastUserActivity >= screenTimeoutSeconds * 1000UL) {
        screenSleeping = true;
        if (cyberFamiliar.idleMode()) {
            familiarIdleActive = true;
            familiarIdleDrawn = false;
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
