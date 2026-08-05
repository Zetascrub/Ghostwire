#pragma once

#include <cstddef>

// Firmware version comparison for the OTA update checker. Pure logic, no
// Arduino/ESP32 dependency, so it can be unit tested under env:native (see
// test/test_ota_version/test_main.cpp) the same way eapol_parser.cpp is --
// this is exactly the kind of small, easy-to-get-subtly-wrong parsing logic
// worth testing on a host rather than only ever exercised on hardware.
namespace OtaVersion {

// Parses "vX.Y.Z" / "X.Y.Z[-suffix]" into a (major, minor, patch) triple.
// A non-numeric suffix like "-dev" simply stops parsing; whatever numeric
// components came before it are kept (missing trailing components are 0).
void parse(const char* text, int out[3]);

// True if latestTag names a strictly newer release than currentVersion,
// comparing (major, minor, patch) as parsed by parse(). Ignores any
// non-numeric suffix on either input, so "0.4.5-dev" and "0.4.5" compare
// equal (not newer).
bool isNewer(const char* latestTag, const char* currentVersion);

}  // namespace OtaVersion
