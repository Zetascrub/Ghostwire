#include "ota_version.h"

#include <cctype>

namespace OtaVersion {

void parse(const char* text, int out[3]) {
    out[0] = out[1] = out[2] = 0;
    if (!text) return;
    const char* s = text;
    if (*s == 'v' || *s == 'V') ++s;

    int part = 0;
    long value = 0;
    bool sawDigit = false;
    // Walk one past the end so a trailing numeric component (no terminating
    // '.') still gets flushed into out[], same as the '.' branch below.
    for (size_t i = 0;; ++i) {
        const char c = s[i];
        if (part >= 3) break;
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            sawDigit = true;
        } else if (c == '.') {
            if (!sawDigit) break;
            out[part++] = static_cast<int>(value);
            value = 0;
            sawDigit = false;
        } else {
            // '\0' or a non-numeric suffix ("-dev", build metadata, ...):
            // flush whatever numeric component we were mid-parsing, then
            // stop -- the suffix itself is deliberately not part of the
            // comparison.
            if (sawDigit) out[part++] = static_cast<int>(value);
            break;
        }
    }
}

bool isNewer(const char* latestTag, const char* currentVersion) {
    int latest[3];
    int current[3];
    parse(latestTag, latest);
    parse(currentVersion, current);
    for (int i = 0; i < 3; ++i) {
        if (latest[i] != current[i]) return latest[i] > current[i];
    }
    return false;
}

}  // namespace OtaVersion
