#pragma once

#include <cstddef>
#include <cstdint>

// Central inventory and conflict policy for Ghostwire's long-running work.
// Services remain responsible for their own hardware teardown; this class is
// deliberately pure state/policy so it can be tested in the native build.
enum class OperationKind : uint8_t {
    WifiCapture,
    WifiGuardian,
    HandshakeCapture,
    WarDrive,
    BleCapture,
    BleTransmit,
    BleAccessory,
    FamiliarPatrol,
    NetworkScan,
    RemoteSession,
    Audio,
    FirmwareUpdate,
    Count,
};

class OperationCoordinator {
public:
    void clear();
    void setActive(OperationKind operation, bool active);
    bool isActive(OperationKind operation) const;
    bool anyActive() const { return activeMask_ != 0; }
    size_t activeCount() const;

    // Returns false and optionally identifies the first conflicting active
    // operation. Asking to start an already-active operation is allowed.
    bool canStart(OperationKind requested,
                  OperationKind* conflict = nullptr) const;

    const char* primaryLabel() const;
    static const char* label(OperationKind operation);

private:
    static bool conflicts(OperationKind left, OperationKind right);
    static uint32_t maskFor(OperationKind operation);

    uint32_t activeMask_ = 0;
};
