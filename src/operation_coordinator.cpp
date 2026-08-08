#include "operation_coordinator.h"

namespace {

bool isWifiMonitor(OperationKind operation) {
    return operation == OperationKind::WifiCapture ||
           operation == OperationKind::WifiGuardian ||
           operation == OperationKind::HandshakeCapture;
}

bool isConnectedNetwork(OperationKind operation) {
    return operation == OperationKind::FamiliarPatrol ||
           operation == OperationKind::NetworkScan ||
           operation == OperationKind::RemoteSession;
}

bool isBleRadio(OperationKind operation) {
    return operation == OperationKind::BleCapture ||
           operation == OperationKind::BleTransmit ||
           operation == OperationKind::BleAccessory;
}

}  // namespace

uint32_t OperationCoordinator::maskFor(OperationKind operation) {
    return uint32_t{1} << static_cast<uint8_t>(operation);
}

void OperationCoordinator::clear() { activeMask_ = 0; }

void OperationCoordinator::setActive(OperationKind operation, bool active) {
    if (active) activeMask_ |= maskFor(operation);
    else activeMask_ &= ~maskFor(operation);
}

bool OperationCoordinator::isActive(OperationKind operation) const {
    return (activeMask_ & maskFor(operation)) != 0;
}

size_t OperationCoordinator::activeCount() const {
    size_t count = 0;
    for (uint8_t value = 0;
         value < static_cast<uint8_t>(OperationKind::Count); ++value) {
        if ((activeMask_ & (uint32_t{1} << value)) != 0) ++count;
    }
    return count;
}

bool OperationCoordinator::conflicts(OperationKind left, OperationKind right) {
    if (left == right) return false;
    if (left == OperationKind::FirmwareUpdate ||
        right == OperationKind::FirmwareUpdate) return true;
    if (left == OperationKind::WarDrive || right == OperationKind::WarDrive) {
        return isWifiMonitor(left) || isWifiMonitor(right) ||
               isConnectedNetwork(left) || isConnectedNetwork(right) ||
               isBleRadio(left) || isBleRadio(right);
    }
    if (isWifiMonitor(left) &&
        (isWifiMonitor(right) || isConnectedNetwork(right) ||
         isBleRadio(right))) return true;
    if (isWifiMonitor(right) &&
        (isConnectedNetwork(left) || isBleRadio(left))) return true;
    if (isBleRadio(left) && isBleRadio(right)) return true;
    if (isConnectedNetwork(left) && isConnectedNetwork(right)) return true;
    return false;
}

bool OperationCoordinator::canStart(OperationKind requested,
                                    OperationKind* conflict) const {
    for (uint8_t value = 0;
         value < static_cast<uint8_t>(OperationKind::Count); ++value) {
        const auto active = static_cast<OperationKind>(value);
        if (isActive(active) && conflicts(requested, active)) {
            if (conflict) *conflict = active;
            return false;
        }
    }
    return true;
}

const char* OperationCoordinator::primaryLabel() const {
    for (int value = static_cast<int>(OperationKind::Count) - 1;
         value >= 0; --value) {
        const auto operation = static_cast<OperationKind>(value);
        if (isActive(operation)) return label(operation);
    }
    return "Idle";
}

const char* OperationCoordinator::label(OperationKind operation) {
    switch (operation) {
        case OperationKind::WifiCapture: return "Wi-Fi capture";
        case OperationKind::WifiGuardian: return "Guardian";
        case OperationKind::HandshakeCapture: return "Handshake capture";
        case OperationKind::WarDrive: return "War drive";
        case OperationKind::BleCapture: return "BLE capture";
        case OperationKind::BleTransmit: return "BLE transmit";
        case OperationKind::BleAccessory: return "BLE accessory";
        case OperationKind::FamiliarPatrol: return "Familiar patrol";
        case OperationKind::NetworkScan: return "Network scan";
        case OperationKind::RemoteSession: return "Remote session";
        case OperationKind::Audio: return "Audio";
        case OperationKind::FirmwareUpdate: return "Firmware update";
        case OperationKind::Count: break;
    }
    return "Unknown";
}
