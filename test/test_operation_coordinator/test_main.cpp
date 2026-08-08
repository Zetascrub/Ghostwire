#include <unity.h>
#include <cstring>

#include "operation_coordinator.h"

void testTracksActiveOperations() {
    OperationCoordinator coordinator;
    TEST_ASSERT_FALSE(coordinator.anyActive());
    coordinator.setActive(OperationKind::WifiGuardian, true);
    coordinator.setActive(OperationKind::Audio, true);
    TEST_ASSERT_TRUE(coordinator.isActive(OperationKind::WifiGuardian));
    TEST_ASSERT_EQUAL_UINT32(2, coordinator.activeCount());
    TEST_ASSERT_EQUAL_STRING("Audio", coordinator.primaryLabel());
    coordinator.clear();
    TEST_ASSERT_EQUAL_STRING("Idle", coordinator.primaryLabel());
}

void testUpdateRequiresAnIdleDeck() {
    OperationCoordinator coordinator;
    OperationKind conflict = OperationKind::Count;
    coordinator.setActive(OperationKind::FamiliarPatrol, true);
    TEST_ASSERT_FALSE(
        coordinator.canStart(OperationKind::FirmwareUpdate, &conflict));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationKind::FamiliarPatrol),
                            static_cast<uint8_t>(conflict));
    coordinator.clear();
    TEST_ASSERT_TRUE(coordinator.canStart(OperationKind::FirmwareUpdate));
}

void testRadioConflictPolicy() {
    OperationCoordinator coordinator;
    coordinator.setActive(OperationKind::WifiGuardian, true);
    TEST_ASSERT_FALSE(coordinator.canStart(OperationKind::NetworkScan));
    TEST_ASSERT_FALSE(coordinator.canStart(OperationKind::BleCapture));
    TEST_ASSERT_TRUE(coordinator.canStart(OperationKind::Audio));

    coordinator.clear();
    coordinator.setActive(OperationKind::BleCapture, true);
    TEST_ASSERT_FALSE(coordinator.canStart(OperationKind::BleTransmit));
    TEST_ASSERT_FALSE(coordinator.canStart(OperationKind::BleAccessory));
    TEST_ASSERT_FALSE(coordinator.canStart(OperationKind::WarDrive));

    coordinator.clear();
    coordinator.setActive(OperationKind::BleAccessory, true);
    TEST_ASSERT_FALSE(coordinator.canStart(OperationKind::WifiGuardian));
    TEST_ASSERT_TRUE(coordinator.canStart(OperationKind::FamiliarPatrol));
}

void testConflictPolicyIsSymmetric() {
    for (uint8_t left = 0;
         left < static_cast<uint8_t>(OperationKind::Count); ++left) {
        for (uint8_t right = 0;
             right < static_cast<uint8_t>(OperationKind::Count); ++right) {
            OperationCoordinator leftActive;
            leftActive.setActive(static_cast<OperationKind>(left), true);
            OperationCoordinator rightActive;
            rightActive.setActive(static_cast<OperationKind>(right), true);
            TEST_ASSERT_EQUAL(
                leftActive.canStart(static_cast<OperationKind>(right)),
                rightActive.canStart(static_cast<OperationKind>(left)));
        }
    }
}

void testEveryOperationHasALabel() {
    for (uint8_t value = 0;
         value < static_cast<uint8_t>(OperationKind::Count); ++value) {
        const char* label = OperationCoordinator::label(
            static_cast<OperationKind>(value));
        TEST_ASSERT_NOT_NULL(label);
        TEST_ASSERT_NOT_EQUAL(0, label[0]);
        TEST_ASSERT_NOT_EQUAL(0, std::strcmp("Unknown", label));
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(testTracksActiveOperations);
    RUN_TEST(testUpdateRequiresAnIdleDeck);
    RUN_TEST(testRadioConflictPolicy);
    RUN_TEST(testConflictPolicyIsSymmetric);
    RUN_TEST(testEveryOperationHasALabel);
    return UNITY_END();
}
