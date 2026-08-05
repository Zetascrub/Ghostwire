#include "ota_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include "branding.h"
#include "ota_root_certs.h"
#include "ota_signing_key.h"
#include "ota_version.h"

namespace {

constexpr char kReleasesApiUrl[] =
    "https://api.github.com/repos/Zetascrub/Ghostwire/releases/latest";
// DER-encoded ECDSA-P256 signatures run ~70-72 bytes; well clear of that.
constexpr size_t kMaxSignatureBytes = 128;
constexpr size_t kDownloadChunk = 1024;
constexpr unsigned long kStallTimeoutMs = 10000;

// Downloads a small (<= bufCap) asset fully into a caller-provided buffer.
// Used for the signature file, which is only ever tens of bytes.
bool fetchSmallAsset(const String& url, uint8_t* buf, size_t bufCap,
                     size_t& outLen) {
    WiFiClientSecure client;
    client.setCACert(kOtaTrustedRootCerts);
    HTTPClient http;
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) return false;
    http.addHeader("Accept", "application/octet-stream");
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }
    const int size = http.getSize();
    if (size <= 0 || static_cast<size_t>(size) > bufCap) {
        http.end();
        return false;
    }
    WiFiClient* stream = http.getStreamPtr();
    size_t got = 0;
    unsigned long lastByteAt = millis();
    while (got < static_cast<size_t>(size)) {
        if (stream->available()) {
            const int n = stream->read(buf + got, size - got);
            if (n > 0) {
                got += n;
                lastByteAt = millis();
            }
        } else if (millis() - lastByteAt > kStallTimeoutMs) {
            break;
        } else {
            delay(5);
        }
    }
    http.end();
    if (got != static_cast<size_t>(size)) return false;
    outLen = got;
    return true;
}

}  // namespace

OtaService::CheckResult OtaService::checkForUpdate(const char* currentVersion) {
    latestVersion_ = "";
    firmwareUrl_ = "";
    signatureUrl_ = "";
    firmwareSize_ = 0;

    WiFiClientSecure client;
    client.setCACert(kOtaTrustedRootCerts);
    HTTPClient http;
    http.setUserAgent(String(Branding::productName) + "/" + Branding::version);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    if (!http.begin(client, kReleasesApiUrl)) {
        status_ = "Could not reach GitHub";
        return CheckResult::Failed;
    }
    http.addHeader("Accept", "application/vnd.github+json");
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        status_ = code == HTTP_CODE_NOT_FOUND
                      ? "No public release available yet"
                      : "Release check failed (HTTP " + String(code) + ")";
        http.end();
        return CheckResult::Failed;
    }

    // Release JSON can run to several KB once description text is
    // included; filter down to just the fields used here so the parsed
    // DOM doesn't need the whole body live in RAM (no PSRAM on this board).
    JsonDocument filter;
    filter["tag_name"] = true;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["url"] = true;
    filter["assets"][0]["size"] = true;
    JsonDocument doc;
    const DeserializationError parseError = deserializeJson(
        doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (parseError) {
        status_ = "Release check failed: bad response";
        return CheckResult::Failed;
    }

    const String tag = doc["tag_name"] | "";
    if (tag.isEmpty()) {
        status_ = "No releases published yet";
        return CheckResult::Failed;
    }
    latestVersion_ = tag;

    const String firmwareName = "ghostwire-" + tag + "-firmware.bin";
    const String signatureName = firmwareName + ".sig";
    for (JsonObject asset : doc["assets"].as<JsonArray>()) {
        const String name = asset["name"] | "";
        if (name == firmwareName) {
            firmwareUrl_ = asset["url"] | "";
            firmwareSize_ = asset["size"] | 0;
        } else if (name == signatureName) {
            signatureUrl_ = asset["url"] | "";
        }
    }

    if (!OtaVersion::isNewer(tag.c_str(), currentVersion)) {
        status_ = "Already up to date (" + tag + ")";
        return CheckResult::UpToDate;
    }
    if (firmwareUrl_.isEmpty() || signatureUrl_.isEmpty()) {
        status_ = "Release " + tag + " is missing a signed firmware asset";
        return CheckResult::Failed;
    }
    status_ = "Update available: " + tag;
    return CheckResult::UpdateAvailable;
}

OtaService::InstallResult OtaService::downloadAndInstall(
    const std::function<bool(size_t, size_t)>& progressCallback) {
    if (firmwareUrl_.isEmpty() || signatureUrl_.isEmpty()) {
        status_ = "No verified update to install";
        return InstallResult::Failed;
    }

    uint8_t signature[kMaxSignatureBytes];
    size_t signatureLen = 0;
    if (!fetchSmallAsset(signatureUrl_, signature, sizeof(signature),
                         signatureLen)) {
        status_ = "Could not download signature";
        return InstallResult::Failed;
    }

    WiFiClientSecure client;
    client.setCACert(kOtaTrustedRootCerts);
    HTTPClient http;
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    if (!http.begin(client, firmwareUrl_)) {
        status_ = "Could not reach download host";
        return InstallResult::Failed;
    }
    http.addHeader("Accept", "application/octet-stream");
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        status_ = "Download failed (HTTP " + String(code) + ")";
        http.end();
        return InstallResult::Failed;
    }
    const int contentLength = http.getSize();
    if (contentLength <= 0) {
        status_ = "Download failed: unknown size";
        http.end();
        return InstallResult::Failed;
    }

    if (!Update.begin(static_cast<size_t>(contentLength), U_FLASH)) {
        status_ = String("Could not start update: ") + Update.errorString();
        http.end();
        return InstallResult::Failed;
    }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha, 0);

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[kDownloadChunk];
    size_t downloaded = 0;
    downloadedBytes_ = 0;
    bool cancelled = false;
    bool ioError = false;
    unsigned long lastByteAt = millis();
    while (downloaded < static_cast<size_t>(contentLength)) {
        const size_t remaining = static_cast<size_t>(contentLength) - downloaded;
        const size_t want = remaining < sizeof(buf) ? remaining : sizeof(buf);
        if (stream->available()) {
            const int n = stream->read(buf, want);
            if (n > 0) {
                const size_t written =
                    Update.write(buf, static_cast<size_t>(n));
                if (written != static_cast<size_t>(n)) {
                    ioError = true;
                    break;
                }
                mbedtls_sha256_update_ret(&sha, buf, static_cast<size_t>(n));
                downloaded += static_cast<size_t>(n);
                downloadedBytes_ = downloaded;
                lastByteAt = millis();
            }
        } else if (millis() - lastByteAt > kStallTimeoutMs) {
            ioError = true;
            break;
        } else {
            delay(5);
        }
        if (progressCallback &&
            !progressCallback(downloaded, static_cast<size_t>(contentLength))) {
            cancelled = true;
            break;
        }
    }
    http.end();

    // Finish the digest before freeing the context -- mbedtls_sha256_free()
    // wipes the context's internal state, so it must happen after, not
    // before (finishing on a freed context would silently hash garbage and
    // make every signature check fail, download notwithstanding).
    unsigned char digest[32];
    const bool downloadComplete =
        !cancelled && !ioError && downloaded == static_cast<size_t>(contentLength);
    if (downloadComplete) mbedtls_sha256_finish_ret(&sha, digest);
    mbedtls_sha256_free(&sha);

    if (!downloadComplete) {
        Update.abort();
        status_ = cancelled ? "Update cancelled" : "Download interrupted";
        return cancelled ? InstallResult::Cancelled : InstallResult::Failed;
    }

    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    bool signatureValid = false;
    if (mbedtls_pk_parse_public_key(&pk, kReleaseSigningPublicKeyDer,
                                    kReleaseSigningPublicKeyDerLen) == 0) {
        signatureValid = mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, digest,
                                           sizeof(digest), signature,
                                           signatureLen) == 0;
    }
    mbedtls_pk_free(&pk);

    if (!signatureValid) {
        Update.abort();
        status_ = "Signature verification failed -- update rejected";
        return InstallResult::SignatureInvalid;
    }

    if (!Update.end(true)) {
        status_ = String("Update failed to commit: ") + Update.errorString();
        return InstallResult::Failed;
    }

    status_ = "Update installed -- restarting";
    return InstallResult::Success;
}
