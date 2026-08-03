#include "ssh_service.h"

#include <libssh/libssh.h>
#include <libssh_esp32.h>
#include <esp_attr.h>

namespace {
// Sized to what Screen::SshSession can actually display (see
// drawSshSessionDynamic() in main.cpp) rather than a dishonest default
// like 80x24 -- this client does no ANSI/VT100 interpretation, so a
// bigger PTY would just mean more output silently scrolled past unseen.
constexpr int kPtyCols = 39;
constexpr int kPtyRows = 6;
constexpr long kConnectTimeoutSeconds = 10;
constexpr uint32_t kCrashStageMagic = 0x53534831UL;
RTC_NOINIT_ATTR uint32_t crashStageMagic;
RTC_NOINIT_ATTR uint8_t lastCrashStage;
SshService::StageCallback stageCallback = nullptr;

void markCrashStage(uint8_t stage) {
    crashStageMagic = kCrashStageMagic;
    lastCrashStage = stage;
    if (stageCallback != nullptr) stageCallback(stage);
}

}  // namespace

SshService::~SshService() { cleanup(); }

void SshService::begin() {
    if (initialized_) return;
    libssh_begin();
    rxQueue_ = xQueueCreate(1024, sizeof(uint8_t));
    txQueue_ = xQueueCreate(512, sizeof(uint8_t));
    initialized_ = true;
}

void SshService::setStageCallback(StageCallback callback) {
    stageCallback = callback;
}

uint8_t SshService::crashStage() const {
    return crashStageMagic == kCrashStageMagic ? lastCrashStage : 0;
}

void SshService::clearCrashStage() {
    crashStageMagic = 0;
    lastCrashStage = 0;
}

void SshService::cleanup() {
    if (channel_ != nullptr) {
        ssh_channel channel = static_cast<ssh_channel>(channel_);
        ssh_channel_send_eof(channel);
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        channel_ = nullptr;
    }
    if (session_ != nullptr) {
        ssh_session session = static_cast<ssh_session>(session_);
        ssh_disconnect(session);
        ssh_free(session);
        session_ = nullptr;
    }
    connected_ = false;
    needsHostTrust_ = false;
}

void SshService::stop() {
    if (workerRunning_) {
        stopRequested_ = true;
        connected_ = false;
        return;
    }
    cleanup();
}

bool SshService::connect(const String& host, uint16_t port,
                         const String& username, const String& password,
                         const String& expectedFingerprint,
                         bool trustUnknownHost) {
    begin();
    stopRequested_ = false;
    if (rxQueue_ != nullptr) xQueueReset(rxQueue_);
    if (txQueue_ != nullptr) xQueueReset(txQueue_);
    const bool continuePendingTrust =
        trustUnknownHost && needsHostTrust_ && session_ != nullptr;
    ssh_session session = static_cast<ssh_session>(session_);

    if (!continuePendingTrust) {
        stop();
        needsHostTrust_ = false;
        serverFingerprint_ = "";

        markCrashStage(10);
        session = ssh_new();
        markCrashStage(11);
        if (session == nullptr) {
            statusMessage_ = "ssh_new failed";
            return false;
        }
        session_ = session;

        markCrashStage(12);
        if (ssh_options_set(session, SSH_OPTIONS_HOST, host.c_str()) != SSH_OK) {
            statusMessage_ = "Invalid SSH host";
            cleanup();
            return false;
        }
        unsigned int optPort = port;
        if (ssh_options_set(session, SSH_OPTIONS_PORT, &optPort) != SSH_OK ||
            ssh_options_set(session, SSH_OPTIONS_USER, username.c_str()) !=
                SSH_OK) {
            statusMessage_ = "Invalid SSH options";
            cleanup();
            return false;
        }
        long timeout = kConnectTimeoutSeconds;
        ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout);
        int strictHostKeyCheck = 0;
        ssh_options_set(session, SSH_OPTIONS_STRICTHOSTKEYCHECK,
                        &strictHostKeyCheck);

        markCrashStage(13);
        if (ssh_connect(session) != SSH_OK) {
            statusMessage_ = String("Connect failed: ") + ssh_get_error(session);
            cleanup();
            return false;
        }

        markCrashStage(14);
        ssh_key serverKey = nullptr;
        unsigned char* hash = nullptr;
        size_t hashLength = 0;
        markCrashStage(15);
        if (ssh_get_server_publickey(session, &serverKey) != SSH_OK ||
            ssh_get_publickey_hash(serverKey, SSH_PUBLICKEY_HASH_SHA256, &hash,
                                   &hashLength) != SSH_OK) {
            if (serverKey != nullptr) ssh_key_free(serverKey);
            statusMessage_ = "Unable to read SSH host key";
            cleanup();
            return false;
        }
        static constexpr char kHex[] = "0123456789abcdef";
        serverFingerprint_.reserve(hashLength * 2);
        for (size_t i = 0; i < hashLength; ++i) {
            serverFingerprint_ += kHex[hash[i] >> 4];
            serverFingerprint_ += kHex[hash[i] & 0x0F];
        }
        ssh_clean_pubkey_hash(&hash);
        ssh_key_free(serverKey);
        markCrashStage(16);

        markCrashStage(19);
        if (!expectedFingerprint.isEmpty() &&
            expectedFingerprint != serverFingerprint_) {
            statusMessage_ = "HOST KEY CHANGED - connection refused";
            cleanup();
            return false;
        }
        if (expectedFingerprint.isEmpty() && !trustUnknownHost) {
            // Keep the negotiated session alive while the operator confirms
            // trust. Reconnecting here caused a reboot in LibSSH-ESP32 and is
            // unnecessary for a TOFU prompt.
            needsHostTrust_ = true;
            // Keep the footer bounded and allocation-light. The full
            // fingerprint remains available through serverFingerprint().
            statusMessage_ = "Unknown key - Enter again to trust";
            markCrashStage(20);
            return false;
        }
    }

    needsHostTrust_ = false;

    markCrashStage(17);
    if (ssh_userauth_password(session, username.c_str(), password.c_str()) !=
        SSH_AUTH_SUCCESS) {
        statusMessage_ = "Authentication failed";
        cleanup();
        return false;
    }

    markCrashStage(18);
    ssh_channel channel = ssh_channel_new(session);
    if (channel == nullptr) {
        statusMessage_ = "Channel allocation failed";
        cleanup();
        return false;
    }
    channel_ = channel;
    if (ssh_channel_open_session(channel) != SSH_OK) {
        statusMessage_ = "Channel open failed";
        cleanup();
        return false;
    }

    if (ssh_channel_request_pty_size(channel, "xterm", kPtyCols, kPtyRows) !=
        SSH_OK) {
        statusMessage_ = "PTY request failed";
        cleanup();
        return false;
    }
    if (ssh_channel_request_shell(channel) != SSH_OK) {
        statusMessage_ = "Shell request failed";
        cleanup();
        return false;
    }

    connected_ = true;
    statusMessage_ = "";
    clearCrashStage();
    return true;
}

int SshService::read(uint8_t* buffer, size_t maxLength) {
    if (rxQueue_ == nullptr) return -1;
    size_t got = 0;
    while (got < maxLength && xQueueReceive(rxQueue_, buffer + got, 0) == pdTRUE) {
        ++got;
    }
    if (got == 0 && !connected_ && !workerRunning_) return -1;
    return static_cast<int>(got);
}

bool SshService::write(const uint8_t* data, size_t length) {
    if (!connected_ || txQueue_ == nullptr) return false;
    for (size_t i = 0; i < length; ++i) {
        if (xQueueSend(txQueue_, data + i, 0) != pdTRUE) return false;
    }
    return true;
}

void SshService::runIoLoop() {
    if (!connected_ || channel_ == nullptr) return;
    workerRunning_ = true;
    ssh_channel channel = static_cast<ssh_channel>(channel_);
    uint8_t buffer[256];

    while (!stopRequested_ && connected_) {
        size_t pending = 0;
        while (pending < sizeof(buffer) &&
               xQueueReceive(txQueue_, buffer + pending, 0) == pdTRUE) {
            ++pending;
        }
        if (pending > 0) {
            const int written = ssh_channel_write(
                channel, buffer, static_cast<uint32_t>(pending));
            if (written < 0) {
                statusMessage_ = "SSH write failed";
                connected_ = false;
                break;
            }
            for (size_t i = pending; i > static_cast<size_t>(written); --i) {
                xQueueSendToFront(txQueue_, buffer + i - 1, 0);
            }
        }

        for (int stream = 0; stream <= 1 && connected_; ++stream) {
            int available = ssh_channel_poll(channel, stream);
            while (available > 0) {
                const int amount = available < static_cast<int>(sizeof(buffer))
                                       ? available
                                       : static_cast<int>(sizeof(buffer));
                const int got = ssh_channel_read(channel, buffer, amount, stream);
                if (got <= 0) break;
                for (int i = 0; i < got; ++i) xQueueSend(rxQueue_, buffer + i, 0);
                available = ssh_channel_poll(channel, stream);
            }
        }
        if (!ssh_channel_is_open(channel) || ssh_channel_is_eof(channel)) {
            connected_ = false;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    cleanup();
    workerRunning_ = false;
    stopRequested_ = false;
}
