#pragma once

#include <Arduino.h>
#include <functional>

// Firmware update checking/installation against the project's public
// GitHub Releases. See docs/roadmap.md item 11 and docs/ota-updates.md.
//
// Both steps are bounded blocking calls, the same "Connecting..."
// status-then-blocking-call convention already used by connectSsh()/
// scanNetworkPorts() elsewhere in this codebase -- there's no non-blocking
// async HTTP path available here, and a one-shot progress screen with
// periodic redraws (via progressCallback) fits how every other network
// operation in this app already works.
class OtaService {
public:
    enum class CheckResult {
        UpdateAvailable,
        UpToDate,
        Failed,
    };

    enum class InstallResult {
        Success,          // verified and committed; caller should reboot
        Cancelled,        // progressCallback returned false
        Failed,           // network/download/Update error; nothing committed
        SignatureInvalid, // downloaded but rejected; nothing committed
    };

    // Blocking. Queries the public repo's latest release and compares its
    // tag against currentVersion (ignoring a "-dev"/other non-numeric
    // suffix on currentVersion). Populates latestVersion()/statusMessage()
    // either way; populates the asset URLs used by downloadAndInstall()
    // only when it returns UpdateAvailable.
    CheckResult checkForUpdate(const char* currentVersion);

    // Blocking. Downloads and verifies the release found by a prior
    // checkForUpdate() that returned UpdateAvailable, streaming straight
    // into the inactive OTA partition (never buffers the full image in
    // RAM -- there's no PSRAM on this board). progressCallback(downloaded,
    // total) is polled periodically; returning false aborts the download
    // and discards the partial write. Only commits the partition
    // (Update.end()) if the ECDSA-P256 signature verifies against
    // kReleaseSigningPublicKeyDer -- an unsigned or tampered image is
    // rejected before anything is committed, not after.
    InstallResult downloadAndInstall(
        const std::function<bool(size_t downloaded, size_t total)>&
            progressCallback);

    const String& latestVersion() const { return latestVersion_; }
    const String& statusMessage() const { return status_; }
    void setStatusMessage(const String& status) { status_ = status; }
    size_t totalBytes() const { return firmwareSize_; }
    // True once checkForUpdate() has found and verified a newer, signed
    // release is available to install.
    bool hasVerifiedUpdate() const {
        return !firmwareUrl_.isEmpty() && !signatureUrl_.isEmpty();
    }
    // Live progress during downloadAndInstall(), for the progress screen to
    // read on its periodic redraws (same idiom as
    // NetworkPortScanService::scannedCount()).
    size_t downloadedBytes() const { return downloadedBytes_; }

private:
    String latestVersion_;
    String firmwareUrl_;
    String signatureUrl_;
    size_t firmwareSize_ = 0;
    size_t downloadedBytes_ = 0;
    String status_;
};
