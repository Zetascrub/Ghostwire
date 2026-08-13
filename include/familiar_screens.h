#pragma once

#include <Arduino.h>
#include <M5GFX.h>

#include "ai_service.h"
#include "cyber_familiar.h"
#include "familiar_battle_service.h"
#include "familiar_patrol_service.h"

// Familiar reaction state (set by triggerFamiliarReaction(), read by the
// creature animation). Moved here (from main.cpp) since both main.cpp
// (triggered from many places -- Guardian start, patrol events, tool
// self-tests, ...) and FamiliarScreens need it -- see
// docs/screen-extraction.md.
enum class FamiliarReaction : uint8_t {
    None,
    Searching,
    HostFound,
    ServiceFound,
    Warning,
    Complete,
};

// Familiar Patrol interval choices (seconds, as milliseconds). Shared
// between main.cpp's Continuous Watch interval cycling and the confirm
// screen that displays the chosen one.
inline constexpr uint32_t kFamiliarPatrolIntervals[] = {
    60000, 300000, 900000, 1800000, 3600000,
};

// AI Chat screen: scrolling turn history plus the composer/status strip.
// Draw-only (see docs/screen-extraction.md); sending prompts and voice
// recording stay in main.cpp.
class AiChatScreen {
public:
    AiChatScreen(AiService& service, String& prompt, String& notice,
                size_t& scrollLines)
        : service_(service),
          prompt_(prompt),
          notice_(notice),
          scrollLines_(scrollLines) {}

    void draw();
    // Redraws just the prompt/status strip -- called on every keystroke
    // while composing, so it must not touch drawHeader()'s fillScreen() or
    // redraw the scrolled history.
    void drawComposer();

private:
    AiService& service_;
    String& prompt_;
    String& notice_;
    size_t& scrollLines_;
};

// Cyber Familiar dashboard/stats/journal, its reset confirmation, and the
// Familiar Patrol status/confirm screens. Grouped as one module (see
// docs/screen-extraction.md) since they share the CyberFamiliar reference
// and the creature/speech-bubble drawing primitives.
//
// drawCreature()/drawSpeechBubble() are also called from main.cpp's
// drawCyberFamiliarIdle() (the ambient idle-screen animation, outside the
// normal Screen enum) -- kept public and called from there rather than
// duplicated, same reasoning as every other centralized helper in this
// extraction (e.g. WifiScreens::authName).
class FamiliarScreens {
public:
    FamiliarScreens(CyberFamiliar& familiar, FamiliarPatrolService& patrol,
                    uint8_t& page, String& workflowStatus,
                    FamiliarReaction& reaction, unsigned long& reactionUntil,
                    String& speechBubble, unsigned long& speechBubbleUntil,
                    bool& patrolContinuousChoice, uint8_t& patrolIntervalIndex,
                    bool& sdAvailable, size_t& listSelection,
                    bool& handshakeMissionRunning, uint32_t& lootHostsFound,
                    uint32_t& lootServicesFound, uint32_t& lootWarningsRaised,
                    uint32_t& lootHandshakesCaptured,
                    uint32_t& lootCredsCaptured,
                    FamiliarBattleService& battle)
        : familiar_(familiar),
          patrol_(patrol),
          page_(page),
          workflowStatus_(workflowStatus),
          reaction_(reaction),
          reactionUntil_(reactionUntil),
          speechBubble_(speechBubble),
          speechBubbleUntil_(speechBubbleUntil),
          patrolContinuousChoice_(patrolContinuousChoice),
          patrolIntervalIndex_(patrolIntervalIndex),
          sdAvailable_(sdAvailable),
          listSelection_(listSelection),
          handshakeMissionRunning_(handshakeMissionRunning),
          lootHostsFound_(lootHostsFound),
          lootServicesFound_(lootServicesFound),
          lootWarningsRaised_(lootWarningsRaised),
          lootHandshakesCaptured_(lootHandshakesCaptured),
          lootCredsCaptured_(lootCredsCaptured),
          battle_(battle) {}

    void drawCreature(lgfx::LGFXBase& gfx, int centerX, int baseY,
                      float scale);
    void drawSpeechBubble(lgfx::LGFXBase& gfx, int x, int y, int width);

    void drawFamiliar(bool fullDraw = true);
    void drawResetConfirm();
    // The Familiar's unified "what can it do unattended" hub -- currently
    // Network Recon (Familiar Patrol) and Handshake Capture; new mission
    // types get a row here rather than their own scattered entry point.
    void drawMissions();
    // Bjorn/Ragnar-style trophy case: lifetime discovery totals, bumped in
    // main.cpp at each genuine find (see lootHostsFound and friends there).
    void drawLootBoard();
    // Evolution progress bar + full stage ladder -- split out from page 0
    // so the dashboard can stay uncluttered and give the creature more
    // room to roam.
    void drawEvolution();
    void drawPatrol(bool fullDraw = true);
    void drawPatrolConfirm();

    // PvP battle screens -- see include/familiar_battle_service.h for the
    // BLE/state-machine side. `battle_.state()` picks which of Host/Find/
    // Battling/Result this is showing; listSelection_ (shared with every
    // other list screen in this app) drives the Host-vs-Find picker and
    // the Find results list.
    void drawBattleMenu();
    void drawBattleHost();
    void drawBattleFind();
    void drawBattle();
    void drawBattleResult();

    static constexpr size_t kMissionCount = 2;
    static constexpr size_t kBattleMenuCount = 2;

private:
    const char* face() const;
    void drawFamiliarDashboard(lgfx::LGFXBase& gfx);
    // Same stage dispatch drawCreature() uses, but for an explicit stage
    // index/color rather than always familiar_'s own -- lets drawBattle()
    // render the opponent's real evolved silhouette too.
    void drawCreatureStage(lgfx::LGFXBase& gfx, uint8_t stageIndex,
                           int centerX, int baseY, float scale,
                           uint16_t color);
    // drawCreature() dispatches to one of these based on
    // familiar_.stageIndex() -- one silhouette per evolution stage, each
    // self-contained (own body/limb geometry, own call to drawFaceGlyph()/
    // drawSearchPulse()) rather than one shape with stage-conditional
    // branches sprinkled through it. `gfx` is the render target (the real
    // display, or an offscreen M5Canvas for flicker-free animation -- see
    // familiarCanvas_); `scale` sizes the render (1.0 = the idle
    // screensaver's reference size; page 0 and other callers can pass their
    // own); `phase`/`wag` are the shared animation clock (bob/blink/
    // tail-wag) drawCreature() derives from millis() once per call.
    void drawStageScriptSprite(lgfx::LGFXBase& gfx, int centerX, int baseY,
                               float scale, uint32_t phase, int wag,
                               uint16_t color);
    void drawStagePacketGremlin(lgfx::LGFXBase& gfx, int centerX, int baseY,
                                float scale, uint32_t phase, int wag,
                                uint16_t color);
    void drawStageGridImp(lgfx::LGFXBase& gfx, int centerX, int baseY,
                          float scale, uint32_t phase, int wag,
                          uint16_t color);
    void drawStageSignalWyrm(lgfx::LGFXBase& gfx, int centerX, int baseY,
                             float scale, uint32_t phase, int wag,
                             uint16_t color);
    void drawStageBeaconWarden(lgfx::LGFXBase& gfx, int centerX, int baseY,
                               float scale, uint32_t phase, int wag,
                               uint16_t color);
    void drawStageHexFamiliar(lgfx::LGFXBase& gfx, int centerX, int baseY,
                              float scale, uint32_t phase, int wag,
                              uint16_t color);
    void drawFaceGlyph(lgfx::LGFXBase& gfx, int cx, int cy, float scale,
                       uint16_t color);
    void drawSearchPulse(lgfx::LGFXBase& gfx, int cx, int cy, uint32_t phase,
                         uint16_t color);

    CyberFamiliar& familiar_;
    FamiliarPatrolService& patrol_;
    uint8_t& page_;
    String& workflowStatus_;
    FamiliarReaction& reaction_;
    unsigned long& reactionUntil_;
    String& speechBubble_;
    unsigned long& speechBubbleUntil_;
    bool& patrolContinuousChoice_;
    uint8_t& patrolIntervalIndex_;
    bool& sdAvailable_;
    size_t& listSelection_;
    bool& handshakeMissionRunning_;
    uint32_t& lootHostsFound_;
    uint32_t& lootServicesFound_;
    uint32_t& lootWarningsRaised_;
    uint32_t& lootHandshakesCaptured_;
    uint32_t& lootCredsCaptured_;
    FamiliarBattleService& battle_;
    // Offscreen buffer for page 0's animated dashboard -- drawn into once
    // per tick and blitted with a single pushSprite() so the creature's
    // bob/blink/wander doesn't visibly flicker the way drawing straight to
    // the display (clear, then several dozen shape calls) would. Sized and
    // allocated lazily on first use (see drawFamiliar()), same reasoning as
    // main.cpp's cyberdeckIdleCanvas.
    M5Canvas familiarCanvas_;
    bool familiarCanvasReady_ = false;
};
