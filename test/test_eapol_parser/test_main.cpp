#include <unity.h>

#include <array>
#include <cstring>

#include "eapol_parser.h"

namespace {
std::array<uint8_t, 121> makeMessageOneWithPmkid() {
    std::array<uint8_t, 121> frame{};
    frame[1] = 3;      // EAPOL-Key
    frame[5] = 0x00;
    frame[6] = 0x80;   // ACK, no MIC => message 1
    frame[97] = 0x00;
    frame[98] = 0x16;  // 22 bytes of key data
    frame[99] = 0xDD;
    frame[100] = 0x14;
    frame[101] = 0x00;
    frame[102] = 0x0F;
    frame[103] = 0xAC;
    frame[104] = 0x04;
    for (size_t i = 0; i < 16; ++i) frame[105 + i] = static_cast<uint8_t>(i);
    return frame;
}

void testRejectsTruncatedAndNonKeyFrames() {
    EapolInfo info;
    const uint8_t shortFrame[3] = {};
    TEST_ASSERT_FALSE(EapolParser::parse(shortFrame, sizeof(shortFrame), info));

    std::array<uint8_t, 99> nonKey{};
    nonKey[1] = 0;
    TEST_ASSERT_FALSE(EapolParser::parse(nonKey.data(), nonKey.size(), info));
}

void testClassifiesFourWayMessages() {
    std::array<uint8_t, 99> frame{};
    frame[1] = 3;
    struct Case { uint16_t flags; uint8_t message; };
    const Case cases[] = {
        {0x0080, 1}, {0x0100, 2}, {0x0380, 3}, {0x0300, 4},
    };
    for (const auto& item : cases) {
        frame[5] = static_cast<uint8_t>(item.flags >> 8);
        frame[6] = static_cast<uint8_t>(item.flags);
        EapolInfo info;
        TEST_ASSERT_TRUE(EapolParser::parse(frame.data(), frame.size(), info));
        TEST_ASSERT_EQUAL_UINT8(item.message, info.messageNumber);
    }
}

void testExtractsPmkidAndRejectsOverrun() {
    auto frame = makeMessageOneWithPmkid();
    EapolInfo info;
    TEST_ASSERT_TRUE(EapolParser::parse(frame.data(), frame.size(), info));
    TEST_ASSERT_TRUE(info.hasPmkid);
    for (size_t i = 0; i < 16; ++i) TEST_ASSERT_EQUAL_UINT8(i, info.pmkid[i]);

    frame[98] = 0x30;
    TEST_ASSERT_TRUE(EapolParser::parse(frame.data(), frame.size(), info));
    TEST_ASSERT_FALSE(info.hasPmkid);
}
}  // namespace

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(testRejectsTruncatedAndNonKeyFrames);
    RUN_TEST(testClassifiesFourWayMessages);
    RUN_TEST(testExtractsPmkidAndRejectsOverrun);
    return UNITY_END();
}
