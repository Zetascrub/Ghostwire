#include <unity.h>

#include "ota_version.h"

namespace {

void testParsesPlainAndVPrefixedVersions() {
    int out[3];
    OtaVersion::parse("1.2.3", out);
    TEST_ASSERT_EQUAL_INT(1, out[0]);
    TEST_ASSERT_EQUAL_INT(2, out[1]);
    TEST_ASSERT_EQUAL_INT(3, out[2]);

    OtaVersion::parse("v0.4.10", out);
    TEST_ASSERT_EQUAL_INT(0, out[0]);
    TEST_ASSERT_EQUAL_INT(4, out[1]);
    TEST_ASSERT_EQUAL_INT(10, out[2]);
}

void testIgnoresNonNumericSuffix() {
    int out[3];
    OtaVersion::parse("0.4.5-dev", out);
    TEST_ASSERT_EQUAL_INT(0, out[0]);
    TEST_ASSERT_EQUAL_INT(4, out[1]);
    TEST_ASSERT_EQUAL_INT(5, out[2]);

    OtaVersion::parse("v1.0.0-rc1+buildmeta", out);
    TEST_ASSERT_EQUAL_INT(1, out[0]);
    TEST_ASSERT_EQUAL_INT(0, out[1]);
    TEST_ASSERT_EQUAL_INT(0, out[2]);
}

void testHandlesMissingComponents() {
    int out[3];
    OtaVersion::parse("1", out);
    TEST_ASSERT_EQUAL_INT(1, out[0]);
    TEST_ASSERT_EQUAL_INT(0, out[1]);
    TEST_ASSERT_EQUAL_INT(0, out[2]);

    OtaVersion::parse("1.5", out);
    TEST_ASSERT_EQUAL_INT(1, out[0]);
    TEST_ASSERT_EQUAL_INT(5, out[1]);
    TEST_ASSERT_EQUAL_INT(0, out[2]);

    OtaVersion::parse("", out);
    TEST_ASSERT_EQUAL_INT(0, out[0]);
    TEST_ASSERT_EQUAL_INT(0, out[1]);
    TEST_ASSERT_EQUAL_INT(0, out[2]);
}

void testIsNewerComparesNumerically() {
    TEST_ASSERT_TRUE(OtaVersion::isNewer("v0.4.6", "0.4.5"));
    TEST_ASSERT_TRUE(OtaVersion::isNewer("v0.5.0", "0.4.9"));
    TEST_ASSERT_TRUE(OtaVersion::isNewer("v1.0.0", "0.9.9"));
    TEST_ASSERT_FALSE(OtaVersion::isNewer("v0.4.5", "0.4.5"));
    TEST_ASSERT_FALSE(OtaVersion::isNewer("v0.4.4", "0.4.5"));
}

// The running-firmware side of the comparison always carries a "-dev"
// suffix during development; this is the actual case OTA update-checking
// runs in day to day, so it gets its own explicit test rather than being
// left to the general suffix-handling case above.
void testDevSuffixOnCurrentVersionComparesByNumberOnly() {
    TEST_ASSERT_TRUE(OtaVersion::isNewer("v0.4.6", "0.4.5-dev"));
    TEST_ASSERT_FALSE(OtaVersion::isNewer("v0.4.5", "0.4.5-dev"));
}

}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(testParsesPlainAndVPrefixedVersions);
    RUN_TEST(testIgnoresNonNumericSuffix);
    RUN_TEST(testHandlesMissingComponents);
    RUN_TEST(testIsNewerComparesNumerically);
    RUN_TEST(testDevSuffixOnCurrentVersionComparesByNumberOnly);
    return UNITY_END();
}
