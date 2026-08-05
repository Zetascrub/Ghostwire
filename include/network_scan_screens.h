#pragma once

#include <IPAddress.h>
#include <vector>

#include "gnss_service.h"
#include "network_host_scan_service.h"
#include "network_port_scan_service.h"
#include "war_drive_service.h"

// Network scouting screens: War Drive summary, the Network Dashboard
// (current Wi-Fi connection details -- no state of its own beyond WiFi.h's
// globals), Host Discovery, and Port Scan. Grouped as one module since
// they're all reached from the Network menu -- see
// docs/screen-extraction.md. Scanning itself, and CSV export, stay in
// main.cpp.
class NetworkScanScreens {
public:
    NetworkScanScreens(WarDriveService& warDrive, GnssService& gnss,
                       size_t& listSelection, size_t& listOffset,
                       NetworkHostScanService& hostScan,
                       std::vector<NetworkHostResult>& hostResults,
                       String& hostScanExportStatus,
                       NetworkPortScanService& portScan,
                       std::vector<uint16_t>& portResults,
                       String& portScanExportStatus, IPAddress& portScanTarget,
                       bool& portScanIsFull)
        : warDrive_(warDrive),
          gnss_(gnss),
          listSelection_(listSelection),
          listOffset_(listOffset),
          hostScan_(hostScan),
          hostResults_(hostResults),
          hostScanExportStatus_(hostScanExportStatus),
          portScan_(portScan),
          portResults_(portResults),
          portScanExportStatus_(portScanExportStatus),
          portScanTarget_(portScanTarget),
          portScanIsFull_(portScanIsFull) {}

    // Redraws just the live counters (no header/footer) -- called on every
    // update while war-driving, so it must not touch drawHeader()'s
    // fillScreen() or the screen flickers.
    void drawWarDriveDynamic();
    void drawWarDrive();
    void drawNetworkDashboard();
    void drawNetworkHostScan(bool fullDraw = true);
    void drawNetworkPortScan(bool fullDraw = true);

private:
    WarDriveService& warDrive_;
    GnssService& gnss_;
    size_t& listSelection_;
    size_t& listOffset_;
    NetworkHostScanService& hostScan_;
    std::vector<NetworkHostResult>& hostResults_;
    String& hostScanExportStatus_;
    NetworkPortScanService& portScan_;
    std::vector<uint16_t>& portResults_;
    String& portScanExportStatus_;
    IPAddress& portScanTarget_;
    bool& portScanIsFull_;
    uint32_t lastHostScanSignature_ = UINT32_MAX;
    uint32_t lastPortScanSignature_ = UINT32_MAX;
};
