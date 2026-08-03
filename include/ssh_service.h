#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Thin synchronous wrapper around a single libssh interactive shell
// session -- deliberately shaped like WiFiClient (connected()-style
// state, non-blocking read()/write()) rather than this project's
// async-service pattern (WifiSnifferService, NetworkPortScanService,
// etc.), since those need real concurrency to scan *many* targets and
// SSH doesn't. An earlier version of this class ran the whole session
// on a background FreeRTOS task with a mutex, purely to avoid blocking
// the UI during the multi-second connect/auth handshake -- that turned
// out to make the whole device sluggish while a session was active
// (confirmed against a reference implementation, Evil-Cardputer's own
// SSH client, which is also fully synchronous with no task at all).
// connect() is a single bounded blocking call, same convention already
// used for the Telnet client's connect and this app's Wi-Fi/BLE scans;
// everything after that is plain non-blocking polling from the caller,
// same as WiFiClient.
class SshService {
public:
    using StageCallback = void (*)(uint8_t);
    ~SshService();
    void begin();

    bool connect(const String& host, uint16_t port, const String& username,
                const String& password, const String& expectedFingerprint,
                bool trustUnknownHost);
    void stop();

    bool isConnected() const { return connected_; }
    String statusMessage() const { return statusMessage_; }
    String serverFingerprint() const { return serverFingerprint_; }
    bool needsHostTrust() const { return needsHostTrust_; }
    uint8_t crashStage() const;
    void clearCrashStage();
    void setStageCallback(StageCallback callback);

    // Wraps ssh_channel_read_nonblocking(): returns >0 bytes read, 0 if
    // nothing available right now, <0 on error/remote EOF (caller
    // should treat that as disconnected).
    int read(uint8_t* buffer, size_t maxLength);
    bool write(const uint8_t* data, size_t length);
    void runIoLoop();

private:
    void cleanup();

    // ssh_session/ssh_channel, kept opaque so libssh's own headers
    // don't need to leak into main.cpp's include graph.
    void* session_ = nullptr;
    void* channel_ = nullptr;
    bool connected_ = false;
    bool initialized_ = false;
    volatile bool workerRunning_ = false;
    volatile bool stopRequested_ = false;
    bool needsHostTrust_ = false;
    String statusMessage_;
    String serverFingerprint_;
    QueueHandle_t rxQueue_ = nullptr;
    QueueHandle_t txQueue_ = nullptr;
};
