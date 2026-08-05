#include <unity.h>

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
    TEST_ASSERT_FALSE(coordinator.canStart(OperationKind::WarDrive));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(testTracksActiveOperations);
    RUN_TEST(testUpdateRequiresAnIdleDeck);
    RUN_TEST(testRadioConflictPolicy);
    return UNITY_END();
}
