#include "familiar_screens.h"

#include <M5Cardputer.h>
#include <WiFi.h>
#include <algorithm>
#include <cmath>

#include "branding.h"
#include "screen_chrome.h"

namespace {
// Avoids relying on math.h's M_PI (not guaranteed by strict builds) for the
// one stage (Hex Familiar) that needs real trig for its hexagon fan and
// orbiting aura dots.
constexpr float kPi = 3.14159265358979323846f;
}  // namespace

void AiChatScreen::drawComposer() {
    auto& display = M5Cardputer.Display;
    display.fillRect(4, 86, display.width() - 8, 31, Branding::background);
    display.drawFastHLine(4, 85, display.width() - 8, Branding::panel);
    display.setTextSize(1);
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(6, 91);
    const String promptLine = "> " + prompt_;
    display.print(promptLine.substring(
        promptLine.length() > 38 ? promptLine.length() - 38 : 0));
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(6, 106);
    const String notice = notice_.isEmpty() ? service_.status() : notice_;
    display.print(notice.substring(0, 39));
}

namespace {
// Word-wraps `prefix + text` to 38 columns, same rule AiChatScreen::draw()
// always used inline before extraction: break on the last space within the
// column budget, or mid-word if there's no good break.
void appendWrappedAiLines(std::vector<String>& lines, const String& prefix,
                          const String& text) {
    constexpr size_t width = 38;
    String remaining = prefix + text;
    remaining.replace("\r", "");
    while (!remaining.isEmpty()) {
        int newline = remaining.indexOf('\n');
        size_t take = std::min(width, remaining.length());
        if (newline >= 0 && static_cast<size_t>(newline) < take) {
            take = static_cast<size_t>(newline);
        } else if (take < remaining.length()) {
            const int space = remaining.substring(0, take + 1).lastIndexOf(' ');
            if (space > 4) take = static_cast<size_t>(space);
        }
        lines.push_back(remaining.substring(0, take));
        size_t remove = take;
        while (remove < remaining.length() &&
               (remaining[remove] == ' ' || remaining[remove] == '\n')) {
            ++remove;
        }
        remaining.remove(0, remove);
        if (take == 0 && remove == 0) break;
    }
}
}  // namespace

void AiChatScreen::draw() {
    String title = "AI: " + String(service_.providerName());
    ScreenChrome::drawHeader(title.c_str());
    auto& display = M5Cardputer.Display;
    display.setTextSize(1);
    std::vector<String> lines;
    for (const auto& turn : service_.history()) {
        appendWrappedAiLines(lines, turn.role == "user" ? "> " : "< ", turn.text);
    }
    constexpr size_t shown = 4;
    const size_t maxScroll = lines.size() > shown ? lines.size() - shown : 0;
    if (scrollLines_ > maxScroll) scrollLines_ = maxScroll;
    const size_t end =
        lines.size() > scrollLines_ ? lines.size() - scrollLines_ : 0;
    const size_t start = end > shown ? end - shown : 0;
    for (size_t index = start; index < end; ++index) {
        display.setTextColor(
            lines[index].startsWith("> ") ? Branding::accent : Branding::text,
            Branding::background);
        display.setCursor(6, 27 + (index - start) * 14);
        display.print(lines[index].substring(0, 39));
    }
    drawComposer();
    ScreenChrome::drawFooter("Up/Down scroll Enter send Tab provider");
}

void FamiliarScreens::drawSpeechBubble(lgfx::LGFXBase& gfx, int x, int y,
                                       int width) {
    if (speechBubble_.isEmpty() || millis() >= speechBubbleUntil_) return;
    auto& display = gfx;
    display.fillRoundRect(x, y, width, 18, 5, Branding::text);
    display.fillTriangle(x + 12, y + 17, x + 20, y + 17, x + 16, y + 23,
                         Branding::text);
    display.setTextColor(Branding::background, Branding::text);
    display.setCursor(x + 5, y + 5);
    display.print(speechBubble_.substring(0, (width - 10) / 6));
}

const char* FamiliarScreens::face() const {
    const bool blink = (millis() / 180U) % 23U == 0;
    if (blink) return "(-_-)";
    if (millis() < reactionUntil_) {
        switch (reaction_) {
            case FamiliarReaction::HostFound: return "(^.^)!";
            case FamiliarReaction::ServiceFound: return "(o.o)+";
            case FamiliarReaction::Warning: return "(O.O)!";
            case FamiliarReaction::Complete: return "(^.^)7";
            case FamiliarReaction::Searching: return "(o.o)?";
            default: break;
        }
    }
    switch (familiar_.mood()) {
        case FamiliarMood::Curious: return "(o_o)?";
        case FamiliarMood::Excited: return "(^o^)";
        case FamiliarMood::Sleepy: return "(-.-)z";
        case FamiliarMood::Proud: return "(^_^)7";
        case FamiliarMood::Worried: return "(O_O)!";
        case FamiliarMood::Dizzy: return "(@_@)";
        default: return "(._.)";
    }
}

void FamiliarScreens::drawCreature(lgfx::LGFXBase& gfx, int centerX,
                                   int baseY, float scale) {
    const uint16_t color = reaction_ == FamiliarReaction::Warning &&
                                   millis() < reactionUntil_
                               ? Branding::warning
                               : Branding::accent;
    drawCreatureStage(gfx, familiar_.stageIndex(), centerX, baseY, scale, color);
}

// Same dispatch drawCreature() uses, but for an explicit stage index and
// color rather than always the player's own familiar_.stageIndex() and
// reaction-based color -- lets drawBattle() render the opponent's actual
// evolved silhouette (from the battle handshake) side-by-side with the
// player's own, not just a name/HP readout.
void FamiliarScreens::drawCreatureStage(lgfx::LGFXBase& gfx, uint8_t stageIndex,
                                        int centerX, int baseY, float scale,
                                        uint16_t color) {
    const uint32_t phase = millis() / 180U;
    const int bob = (phase % 8U == 1U || phase % 8U == 2U) ? -1 : 0;
    const int wag = (phase / 2U) % 2U == 0U ? 0 : 3;
    const int anchorY = baseY + bob;

    // One silhouette per evolution stage -- see stageName()/stageIndex()
    // (cyber_familiar.h). Each stage keeps its own reference size at
    // scale=1.0 (the creature visibly grows across the ladder); callers
    // pick their own `scale` for the render context (idle screensaver vs.
    // page 0's bigger dashboard view vs. the battle screen's side-by-side
    // layout).
    switch (stageIndex) {
        case 0:
            drawStageScriptSprite(gfx, centerX, anchorY, scale, phase, wag, color);
            break;
        case 1:
            drawStagePacketGremlin(gfx, centerX, anchorY, scale, phase, wag, color);
            break;
        case 2:
            drawStageGridImp(gfx, centerX, anchorY, scale, phase, wag, color);
            break;
        case 3:
            drawStageSignalWyrm(gfx, centerX, anchorY, scale, phase, wag, color);
            break;
        case 4:
            drawStageBeaconWarden(gfx, centerX, anchorY, scale, phase, wag, color);
            break;
        default:
            drawStageHexFamiliar(gfx, centerX, anchorY, scale, phase, wag, color);
            break;
    }
}

void FamiliarScreens::drawFaceGlyph(lgfx::LGFXBase& gfx, int cx, int cy,
                                    float scale, uint16_t color) {
    auto& display = gfx;
    display.setTextSize(scale >= 1.0f ? 2 : 1);
    display.setTextColor(color, Branding::background);
    const String faceText = face();
    display.setCursor(cx - static_cast<int>(faceText.length()) * (scale >= 1.0f ? 6 : 3),
                      cy);
    display.print(faceText);
    display.setTextSize(1);
}

void FamiliarScreens::drawSearchPulse(lgfx::LGFXBase& gfx, int cx, int cy,
                                      uint32_t phase, uint16_t color) {
    if (!(patrol_.isActive() &&
          patrol_.state() == FamiliarPatrolState::Discovery)) {
        return;
    }
    auto& display = gfx;
    const int radius = 4 + static_cast<int>((phase % 4U) * 3U);
    display.drawCircle(cx, cy, radius, color);
    display.drawPixel(cx, cy, Branding::text);
}

// Stage 0 -- Script Sprite (0 XP): the original creature shape, unchanged.
// Small, round, plain -- the origin point every later stage grows from.
void FamiliarScreens::drawStageScriptSprite(lgfx::LGFXBase& gfx, int centerX,
                                            int baseY, float scale,
                                            uint32_t phase, int wag,
                                            uint16_t color) {
    auto& display = gfx;
    const float S = scale;
    const auto s = [S](float v) { return static_cast<int>(v * S); };
    const int w = s(44), h = s(26);
    const int x = centerX - w / 2, y = baseY - h;

    display.drawLine(x + w - s(2), y + h - s(8), x + w + s(9),
                     y + h - s(12) - s(wag), color);
    display.fillTriangle(x + s(6), y + s(6), x + s(12), y - s(5), x + s(19),
                         y + s(4), color);
    display.fillTriangle(x + w - s(19), y + s(4), x + w - s(12), y - s(5),
                         x + w - s(6), y + s(6), color);
    display.fillRoundRect(x, y, w, h, s(9), color);
    display.fillRoundRect(x + s(3), y + s(3), w - s(6), h - s(6), s(7),
                          Branding::background);

    drawFaceGlyph(gfx, centerX, y + h / 2, scale, color);
    drawSearchPulse(gfx, x - s(8), y + h / 2, phase, color);
}

// Stage 1 -- Packet Gremlin (400 XP): ears sharpen into horns, the tail
// picks up a second bend, and stray packet blips twinkle nearby.
void FamiliarScreens::drawStagePacketGremlin(lgfx::LGFXBase& gfx,
                                             int centerX, int baseY,
                                             float scale, uint32_t phase,
                                             int wag, uint16_t color) {
    auto& display = gfx;
    const float S = scale;
    const auto s = [S](float v) { return static_cast<int>(v * S); };
    const int w = s(48), h = s(28);
    const int x = centerX - w / 2, y = baseY - h;

    display.drawLine(x + w - s(2), y + h - s(8), x + w + s(7),
                     y + h - s(11) - s(wag), color);
    display.drawLine(x + w + s(7), y + h - s(11) - s(wag), x + w + s(13),
                     y + h - s(6) - s(wag), color);
    display.fillTriangle(x + s(5), y + s(5), x + s(11), y - s(9), x + s(18),
                         y + s(3), color);
    display.fillTriangle(x + w - s(18), y + s(3), x + w - s(11), y - s(9),
                         x + w - s(5), y + s(5), color);
    display.fillRoundRect(x, y, w, h, s(8), color);
    display.fillRoundRect(x + s(3), y + s(3), w - s(6), h - s(6), s(6),
                          Branding::background);
    if (phase % 6U < 3U) {
        display.fillRect(x - s(12), y + s(3), std::max(1, s(3)),
                         std::max(1, s(3)), Branding::muted);
    } else {
        display.fillRect(x + w + s(9), y + s(10), std::max(1, s(3)),
                         std::max(1, s(3)), Branding::muted);
    }

    drawFaceGlyph(gfx, centerX, y + h / 2, scale, color);
    drawSearchPulse(gfx, x - s(8), y + h / 2, phase, color);
}

// Stage 2 -- Grid Imp (1100 XP): body squares off, a grid crosshatch
// appears on its chest, back spikes replace the horns, tail forks, stub
// legs land it on the ground.
void FamiliarScreens::drawStageGridImp(lgfx::LGFXBase& gfx, int centerX, int baseY, float scale,
                                       uint32_t phase, int wag,
                                       uint16_t color) {
    auto& display = gfx;
    const float S = scale;
    const auto s = [S](float v) { return static_cast<int>(v * S); };
    const int w = s(52), h = s(30);
    const int x = centerX - w / 2, y = baseY - h;

    display.drawLine(x + w - s(2), y + h - s(9), x + w + s(9),
                     y + h - s(13) - s(wag), color);
    display.fillTriangle(x + w + s(9), y + h - s(13) - s(wag),
                         x + w + s(15), y + h - s(17) - s(wag), x + w + s(13),
                         y + h - s(9) - s(wag), color);
    for (int i = 0; i < 3; ++i) {
        const int sx = x + s(10) + i * s(12);
        display.fillTriangle(sx, y + s(2), sx + s(4), y - s(7), sx + s(8),
                             y + s(2), color);
    }
    display.fillRoundRect(x, y, w, h, s(4), color);
    display.fillRoundRect(x + s(3), y + s(3), w - s(6), h - s(6), s(3),
                          Branding::background);
    display.drawLine(centerX, y + s(8), centerX, y + h - s(8), Branding::muted);
    display.drawLine(x + s(10), y + h / 2, x + w - s(10), y + h / 2,
                     Branding::muted);
    display.drawLine(x + s(14), y + h, x + s(14), y + h + s(4), color);
    display.drawLine(x + w - s(14), y + h, x + w - s(14), y + h + s(4), color);

    drawFaceGlyph(gfx, centerX, y + h / 2 - s(2), scale, color);
    drawSearchPulse(gfx, x - s(8), y + h / 2, phase, color);
}

// Stage 3 -- Signal Wyrm (2400 XP): stretches serpentine with a raised
// head/neck segment and wifi-wave antenna arcs.
void FamiliarScreens::drawStageSignalWyrm(lgfx::LGFXBase& gfx, int centerX, int baseY, float scale,
                                          uint32_t phase, int wag,
                                          uint16_t color) {
    auto& display = gfx;
    const float S = scale;
    const auto s = [S](float v) { return static_cast<int>(v * S); };
    const int w = s(64), h = s(26);
    const int x = centerX - w / 2, y = baseY - h;

    display.drawLine(x + w - s(6), y + h - s(6), x + w + s(6),
                     y + h - s(4) - s(wag), color);
    display.drawLine(x + w + s(6), y + h - s(4) - s(wag), x + w + s(14),
                     y + h - s(10) + s(wag), color);
    display.drawLine(x + w + s(14), y + h - s(10) + s(wag), x + w + s(20),
                     y + h - s(6) - s(wag), color);
    display.fillTriangle(x + s(16), y + h - s(2), x + s(2), y + h + s(8),
                         x + s(20), y + h + s(4), Branding::muted);
    display.fillTriangle(x + w - s(30), y + h - s(2), x + w - s(16),
                         y + h + s(8), x + w - s(34), y + h + s(4),
                         Branding::muted);
    display.fillRoundRect(x, y + s(8), w - s(14), h - s(8), s(11), color);
    display.fillRoundRect(x + s(3), y + s(11), w - s(20), h - s(14), s(9),
                          Branding::background);
    display.fillRoundRect(x + w - s(30), y - s(4), s(26), s(20), s(9), color);
    display.fillRoundRect(x + w - s(27), y - s(1), s(20), s(14), s(7),
                          Branding::background);
    display.drawArc(x + w - s(17), y - s(8), s(10), s(6), 200, 340, color);

    drawFaceGlyph(gfx, x + w - s(17), y + s(4), scale, color);
    drawSearchPulse(gfx, x - s(8), y + h / 2, phase, color);
}

// Stage 4 -- Beacon Warden (4400 XP): armored double-outline body, shoulder
// spikes, real wings, a pulsing beacon orb overhead -- a guardian
// silhouette, standing watch.
void FamiliarScreens::drawStageBeaconWarden(lgfx::LGFXBase& gfx, int centerX,
                                            int baseY, float scale,
                                            uint32_t phase, int wag,
                                            uint16_t color) {
    auto& display = gfx;
    const float S = scale;
    const auto s = [S](float v) { return static_cast<int>(v * S); };
    const int w = s(62), h = s(34);
    const int x = centerX - w / 2, y = baseY - h;
    const bool beaconOn = (phase % 10U) < 5U;

    display.fillTriangle(x + s(8), y + s(8), x - s(14), y - s(4), x + s(14),
                         y + s(16), Branding::muted);
    display.fillTriangle(x + w - s(8), y + s(8), x + w + s(14), y - s(4),
                         x + w - s(14), y + s(16), Branding::muted);
    display.drawLine(x + w - s(4), y + h - s(8), x + w + s(12),
                     y + h - s(10) - s(wag), color);
    display.fillTriangle(x + w + s(12), y + h - s(10) - s(wag),
                         x + w + s(20), y + h - s(16) - s(wag), x + w + s(18),
                         y + h - s(6) - s(wag), color);
    display.fillTriangle(x + s(2), y + s(6), x + s(10), y - s(8), x + s(16),
                         y + s(6), color);
    display.fillTriangle(x + w - s(16), y + s(6), x + w - s(10), y - s(8),
                         x + w - s(2), y + s(6), color);
    display.fillRoundRect(x, y, w, h, s(10), color);
    display.fillRoundRect(x + s(3), y + s(3), w - s(6), h - s(6), s(8),
                          Branding::background);
    display.fillRoundRect(x + s(6), y + s(6), w - s(12), h - s(12), s(6),
                          color);
    display.fillRoundRect(x + s(9), y + s(9), w - s(18), h - s(18), s(4),
                          Branding::background);
    display.drawLine(centerX, y - s(4), centerX, y - s(15), color);
    display.fillCircle(centerX, y - s(18), std::max(1, s(4)),
                       beaconOn ? Branding::warning : Branding::muted);
    display.drawLine(x + s(16), y + h, x + s(13), y + h + s(5), color);
    display.drawLine(x + w - s(16), y + h, x + w - s(13), y + h + s(5), color);

    drawFaceGlyph(gfx, centerX, y + h / 2 + s(2), scale, color);
    drawSearchPulse(gfx, x - s(8), y + h / 2, phase, color);
}

// Stage 5 -- Hex Familiar (6900 XP, max): final form. A true hexagonal
// body (breaking from the "rounded blob" every earlier stage shares, on
// purpose), a spike crown, layered wings, an orbiting aura, a glowing core.
void FamiliarScreens::drawStageHexFamiliar(lgfx::LGFXBase& gfx, int centerX,
                                           int baseY, float scale,
                                           uint32_t phase, int wag,
                                           uint16_t color) {
    auto& display = gfx;
    const float S = scale;
    const auto s = [S](float v) { return static_cast<int>(v * S); };
    const int w = s(70), h = s(36);
    const int x = centerX - w / 2, y = baseY - h;
    const int cy = y + h / 2;
    const float R = h / 2.0f;

    for (int i = 0; i < 3; ++i) {
        const float a = static_cast<float>(phase) * 0.15f +
                        (static_cast<float>(i) * 2.0f * kPi) / 3.0f;
        const int ax = centerX + static_cast<int>(std::cos(a) * (R + s(16)));
        const int ay =
            cy + static_cast<int>(std::sin(a) * (R + s(16)) * 0.55f);
        display.fillCircle(ax, ay, std::max(1, s(2)), Branding::muted);
    }
    display.fillTriangle(x + s(10), cy - s(4), x - s(18), cy - s(16),
                         x + s(16), cy + s(10), Branding::muted);
    display.fillTriangle(x + s(10), cy - s(4), x - s(8), cy - s(8), x + s(14),
                         cy + s(2), color);
    display.fillTriangle(x + w - s(10), cy - s(4), x + w + s(18), cy - s(16),
                         x + w - s(16), cy + s(10), Branding::muted);
    display.fillTriangle(x + w - s(10), cy - s(4), x + w + s(8), cy - s(8),
                         x + w - s(14), cy + s(2), color);
    display.drawLine(x + w - s(6), cy + s(6), x + w + s(14),
                     cy + s(2) - s(wag), color);
    display.fillTriangle(x + w + s(14), cy + s(2) - s(wag), x + w + s(22),
                         cy - s(5) - s(wag), x + w + s(19), cy + s(3) - s(wag),
                         color);
    display.fillTriangle(x + w + s(14), cy + s(2) - s(wag), x + w + s(22),
                         cy + s(9) - s(wag), x + w + s(19), cy + s(1) - s(wag),
                         color);

    float px[6], py[6];
    for (int i = 0; i < 6; ++i) {
        const float a = kPi / 6.0f + (static_cast<float>(i) * kPi) / 3.0f;
        px[i] = centerX + std::cos(a) * R * 1.15f;
        py[i] = cy + std::sin(a) * R;
    }
    for (int i = 0; i < 6; ++i) {
        const int j = (i + 1) % 6;
        display.fillTriangle(centerX, cy, static_cast<int>(px[i]),
                             static_cast<int>(py[i]), static_cast<int>(px[j]),
                             static_cast<int>(py[j]), color);
    }
    for (int i = 0; i < 6; ++i) {
        const int j = (i + 1) % 6;
        const int qx0 = centerX + static_cast<int>((px[i] - centerX) * 0.76f);
        const int qy0 = cy + static_cast<int>((py[i] - cy) * 0.76f);
        const int qx1 = centerX + static_cast<int>((px[j] - centerX) * 0.76f);
        const int qy1 = cy + static_cast<int>((py[j] - cy) * 0.76f);
        display.fillTriangle(centerX, cy, qx0, qy0, qx1, qy1,
                             Branding::background);
    }
    for (int i = 0; i < 5; ++i) {
        const int sx = x + s(9) + i * s(13);
        display.fillTriangle(sx, y + s(2), sx + s(4), y - s(10), sx + s(8),
                             y + s(2), color);
    }
    const int pulse = (phase % 8U) < 4U ? s(3) : s(2);
    display.fillCircle(centerX, cy + s(3), std::max(1, pulse),
                       Branding::warning);

    drawFaceGlyph(gfx, centerX, cy - s(3), scale, color);
    drawSearchPulse(gfx, x - s(8), y + h / 2, phase, color);
}

// Page 0's content, at coordinates relative to the top of the content pane
// (screen y=22 is local y=0) -- shared by the canvas path and the
// direct-to-display fallback in drawFamiliar() below.
void FamiliarScreens::drawFamiliarDashboard(lgfx::LGFXBase& gfx) {
    auto& display = gfx;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(6, 6);
    display.printf("Lv %u", static_cast<unsigned>(familiar_.level()));
    display.setCursor(6, 23);
    display.print(familiar_.name().substring(0, 8));
    display.setCursor(6, 40);
    display.printf("Bond %u", static_cast<unsigned>(familiar_.bond()));

    // The creature wanders slowly left/right across the open half of the
    // screen -- purely a function of millis(), same stateless approach as
    // its bob/blink/tail-wag (see drawCreature()). The range is kept
    // narrow enough that even Hex Familiar's wings/tail (its widest
    // silhouette) stay clear of the stat column on the left and the
    // screen edge on the right at kCreatureScale.
    constexpr float kCreatureScale = 1.2f;
    constexpr int kWanderCenterX = 150;
    constexpr int kWanderRangePx = 20;
    constexpr uint32_t kWanderPeriodMs = 7000;
    const float wanderPhase = static_cast<float>(millis() % kWanderPeriodMs) /
                              static_cast<float>(kWanderPeriodMs);
    const int wanderOffset = static_cast<int>(
        std::sin(wanderPhase * 2.0f * kPi) * kWanderRangePx);
    const int creatureX = kWanderCenterX + wanderOffset;

    drawCreature(gfx, creatureX, 78, kCreatureScale);
    drawSpeechBubble(gfx, 132, 2, 96);

    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 82);
    display.print((workflowStatus_.isEmpty() ? familiar_.lastMessage()
                                              : workflowStatus_)
                      .substring(0, 37));
}

void FamiliarScreens::drawFamiliar(bool fullDraw) {
    if (page_ == 0) {
        // Drawn into an offscreen canvas and blitted with one pushSprite()
        // so the per-tick creature animation doesn't flicker the way
        // clear-then-redraw-several-dozen-shapes straight to the real
        // display would -- same double-buffering approach as main.cpp's
        // cyberdeckIdleCanvas.
        if (fullDraw) ScreenChrome::drawHeader(familiar_.name().c_str());
        if (!familiarCanvasReady_) {
            // 8-bit, not the default 16-bit -- halves the buffer so it
            // still fits once Wi-Fi/BLE scans and everything else have
            // fragmented the heap. See the matching note on
            // familiarIdleCanvas in main.cpp, where this was found to
            // matter in practice (a 240x135 16-bit canvas failed to
            // allocate with 73KB free but only a 63KB contiguous block).
            familiarCanvas_.setColorDepth(8);
            familiarCanvasReady_ = familiarCanvas_.createSprite(
                M5Cardputer.Display.width(), 98);
        }
        if (familiarCanvasReady_) {
            familiarCanvas_.fillSprite(Branding::background);
            drawFamiliarDashboard(familiarCanvas_);
            familiarCanvas_.pushSprite(&M5Cardputer.Display, 0, 22);
        } else {
            // OOM fallback: draw straight to the display, flicker and all,
            // rather than show nothing.
            M5Cardputer.Display.fillRect(0, 22, M5Cardputer.Display.width(),
                                         98, Branding::background);
            drawFamiliarDashboard(M5Cardputer.Display);
        }
        if (fullDraw) ScreenChrome::drawFooter("Up/Down: pages   Tab: menu");
        return;
    }

    ScreenChrome::beginContentUpdate(familiar_.name().c_str(), fullDraw);
    auto& display = M5Cardputer.Display;
    if (page_ == 1) {
        display.setTextColor(Branding::text, Branding::background);
        display.setCursor(8, 30);
        display.printf("Mood:        %s", familiar_.moodName());
        display.setCursor(8, 47);
        const uint32_t seconds = familiar_.ageSeconds();
        const uint32_t days = seconds / 86400U;
        const uint8_t hours = (seconds / 3600U) % 24U;
        display.printf("Age:         %lud %uh", static_cast<unsigned long>(days),
                       hours);
        display.setCursor(8, 64);
        display.printf("Wi-Fi seen:  %lu",
                       static_cast<unsigned long>(familiar_.wifiDiscoveries()));
        display.setCursor(8, 81);
        display.printf("BLE seen:    %lu",
                       static_cast<unsigned long>(familiar_.bleDiscoveries()));
        display.setCursor(8, 98);
        display.printf("Tools known: %lu",
                       static_cast<unsigned long>(familiar_.toolCount()));
    } else {
        const auto& journal = familiar_.journal();
        display.setTextColor(Branding::text, Branding::background);
        const size_t shown = std::min<size_t>(6, journal.size());
        const size_t start = journal.size() - shown;
        for (size_t row = 0; row < shown; ++row) {
            display.setCursor(6, 27 + static_cast<int>(row) * 15);
            display.print(("> " + journal[start + row]).substring(0, 39));
        }
    }
    if (fullDraw) ScreenChrome::drawFooter("Up/Down: pages   Tab: menu");
}

// The progress bar and full stage ladder, split out of page 0 (see
// drawFamiliar()) so the dashboard stays uncluttered.
void FamiliarScreens::drawEvolution() {
    ScreenChrome::drawHeader("Evolution");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 24);
    display.printf("%s (Lv %u)", familiar_.stageName(),
                   static_cast<unsigned>(familiar_.level()));
    display.setCursor(8, 36);
    display.printf("XP: %lu", static_cast<unsigned long>(familiar_.xp()));

    const uint8_t progress = familiar_.stageProgressPercent();
    display.drawRect(8, 46, 224, 8, Branding::muted);
    display.fillRect(10, 48, std::min<uint32_t>(220, progress * 220U / 100U),
                     4, Branding::accent);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 58);
    if (familiar_.isMaxStage()) {
        display.print("Max stage reached.");
    } else {
        display.printf(
            "%u%% to %s (%lu XP)", progress,
            CyberFamiliar::stageNameAt(familiar_.stageIndex() + 1),
            static_cast<unsigned long>(familiar_.xpToNextStage()));
    }

    const uint8_t count = CyberFamiliar::stageCount();
    const uint8_t current = familiar_.stageIndex();
    for (uint8_t i = 0; i < count; ++i) {
        const bool reached = i <= current;
        display.setTextColor(
            i == current ? Branding::accent
                         : (reached ? Branding::text : Branding::muted),
            Branding::background);
        display.setCursor(8, 70 + i * 8);
        display.printf(
            "%s%-15s %lu XP", i == current ? "> " : "  ",
            CyberFamiliar::stageNameAt(i),
            static_cast<unsigned long>(CyberFamiliar::stageXpThresholdAt(i)));
    }
    ScreenChrome::drawFooter("Esc: back");
}

void FamiliarScreens::drawResetConfirm() {
    ScreenChrome::drawHeader("Reset Cyber Familiar");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 34);
    display.print("This clears progression, bond,");
    display.setCursor(8, 50);
    display.print("discoveries, journal and tools.");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 76);
    display.print("Name and idle preference remain.");
    ScreenChrome::drawFooter("Enter: reset   Esc: cancel");
}

void FamiliarScreens::drawMissions() {
    ScreenChrome::drawHeader("Familiar Missions");
    ScreenChrome::normalizeListPosition(kMissionCount);
    const char* const labels[kMissionCount] = {
        "Network Recon",
        "Handshake Capture",
    };
    const String values[kMissionCount] = {
        patrol_.isActive() ? "ACTIVE" : "",
        handshakeMissionRunning_ ? "ACTIVE" : "",
    };
    for (size_t row = 0; row < kMissionCount; ++row) {
        ScreenChrome::drawListRow(row, labels[row], row == listSelection_,
                                  values[row]);
    }
    ScreenChrome::drawFooter("Enter: open   Q: back");
}

void FamiliarScreens::drawLootBoard() {
    ScreenChrome::drawHeader("Loot Board");
    static constexpr size_t kRowCount = 5;
    const char* const labels[kRowCount] = {
        "Hosts found",
        "Services found",
        "Warnings raised",
        "Handshakes captured",
        "Logins captured",
    };
    const uint32_t values[kRowCount] = {
        lootHostsFound_,
        lootServicesFound_,
        lootWarningsRaised_,
        lootHandshakesCaptured_,
        lootCredsCaptured_,
    };
    for (size_t row = 0; row < kRowCount; ++row) {
        ScreenChrome::drawListRow(row, labels[row], false,
                                  String(values[row]));
    }
    ScreenChrome::drawFooter("Lifetime totals   Q: back");
}

void FamiliarScreens::drawPatrolConfirm() {
    ScreenChrome::drawHeader("Confirm Familiar Patrol");
    auto& display = M5Cardputer.Display;
    if (!sdAvailable_ || WiFi.status() != WL_CONNECTED) {
        display.setTextColor(Branding::warning, Branding::background);
        display.setCursor(8, 34);
        display.print(!sdAvailable_ ? "microSD card required"
                                    : "Connect Wi-Fi first");
        ScreenChrome::drawFooter("Esc: cancel");
        return;
    }
    const String scope =
        FamiliarPatrolService::cidrText(WiFi.localIP(), WiFi.subnetMask());
    const uint32_t count = FamiliarPatrolService::usableAddressCount(
        WiFi.localIP(), WiFi.subnetMask());
    display.setTextColor(Branding::warning, Branding::background);
    display.setCursor(8, 28);
    display.print("AUTHORIZED SCOPE REQUIRED");
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 47);
    display.printf("Scope: %s", scope.c_str());
    display.setCursor(8, 63);
    display.printf("Addresses: %lu", static_cast<unsigned long>(count));
    display.setCursor(8, 79);
    display.printf("Mode: %s", patrolContinuousChoice_ ? "Continuous Watch"
                                                        : "One-shot scout");
    display.setCursor(8, 95);
    display.printf(
        "Interval: %lus",
        static_cast<unsigned long>(
            kFamiliarPatrolIntervals[patrolIntervalIndex_] / 1000));
    ScreenChrome::drawFooter("C: mode  V: interval  Enter: go");
}

void FamiliarScreens::drawPatrol(bool fullDraw) {
    ScreenChrome::beginContentUpdate("Familiar Patrol", fullDraw);
    auto& display = M5Cardputer.Display;
    const FamiliarPatrolState state = patrol_.state();
    display.setTextColor(state == FamiliarPatrolState::Error ? Branding::warning
                          : patrol_.isActive()                ? Branding::accent
                                                               : Branding::text,
                         Branding::background);
    display.setCursor(8, 28);
    display.printf("%s", patrol_.stateName());
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(158, 28);
    display.printf("Mood:%s", familiar_.moodName());
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 44);
    if (state == FamiliarPatrolState::WatchWait) {
        display.printf(
            "Cycle %lu; next scan in %lus",
            static_cast<unsigned long>(patrol_.cycle()),
            static_cast<unsigned long>(patrol_.nextScanSeconds()));
    } else if (state == FamiliarPatrolState::Discovery) {
        display.printf(
            "Addresses: %lu / %lu",
            static_cast<unsigned long>(patrol_.discoveryScanned()),
            static_cast<unsigned long>(patrol_.addressCount()));
    } else {
        display.printf(
            "Host: %lu / %lu", static_cast<unsigned long>(patrol_.hostIndex() + 1),
            static_cast<unsigned long>(patrol_.cycleHostsFound()));
    }
    display.setCursor(8, 60);
    display.printf("Found: %lu hosts  %lu open ports",
                   static_cast<unsigned long>(patrol_.hostsFound()),
                   static_cast<unsigned long>(patrol_.openPortsFound()));
    display.setCursor(8, 76);
    const uint32_t port = patrol_.currentPort();
    if (port > 0) {
        display.printf("%s : %lu", patrol_.currentHost().toString().c_str(),
                       static_cast<unsigned long>(port));
    } else {
        display.print(patrol_.status().substring(0, 37));
    }
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 94);
    if (!speechBubble_.isEmpty() && millis() < speechBubbleUntil_) {
        drawSpeechBubble(M5Cardputer.Display, 7, 91, 226);
    } else {
        String path = patrol_.sessionPath();
        const int slash = path.lastIndexOf('/');
        if (slash >= 0) path = path.substring(slash + 1);
        display.print(path.substring(0, 37));
    }
    if (fullDraw) {
        ScreenChrome::drawFooter(patrol_.isActive() ? "Tab: stop menu   Q: background"
                                                    : "Q: back");
    }
}

// PvP battle screens -- see include/familiar_battle_service.h.

void FamiliarScreens::drawBattleMenu() {
    ScreenChrome::drawHeader("PvP Battle");
    auto& display = M5Cardputer.Display;
    static const char* const kItems[kBattleMenuCount] = {
        "Host (wait for challenger)",
        "Find Opponent (scan)",
    };
    for (size_t i = 0; i < kBattleMenuCount; ++i) {
        const bool selected = i == listSelection_;
        display.setTextColor(selected ? Branding::accent : Branding::text,
                             Branding::background);
        display.setCursor(8, 28 + static_cast<int>(i) * 18);
        display.print(selected ? "> " : "  ");
        display.print(kItems[i]);
    }
    ScreenChrome::drawFooter("Up/Down: select   Enter: choose   Esc: back");
}

// Shared by both roles while waiting for the handshake to complete: a Host
// waiting for a challenger to connect, or a challenger (having just picked
// a target from Find Opponent) waiting for the connection + HELLO exchange
// to finish. battle_.state() tells them apart.
void FamiliarScreens::drawBattleHost() {
    const bool hosting = battle_.state() == FamiliarBattleState::Hosting;
    ScreenChrome::drawHeader(hosting ? "Hosting Battle" : "Connecting...");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 28);
    display.printf("%s  Lv %u  %s", familiar_.name().c_str(),
                   static_cast<unsigned>(familiar_.level()), familiar_.stageName());
    display.setTextColor(Branding::accent, Branding::background);
    display.setCursor(8, 48);
    display.print(battle_.status());
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 70);
    if (hosting) {
        display.print("Advertising as battle-enabled.");
        display.setCursor(8, 86);
        display.print("Waiting for another badge to");
        display.setCursor(8, 100);
        display.print("challenge you...");
    } else {
        display.print("Exchanging opening handshake...");
    }
    ScreenChrome::drawFooter("Esc: cancel");
}

void FamiliarScreens::drawBattleFind() {
    ScreenChrome::drawHeader("Find Opponent");
    auto& display = M5Cardputer.Display;
    const auto& results = battle_.scanResults();
    if (results.empty()) {
        display.setTextColor(Branding::muted, Branding::background);
        display.setCursor(8, 40);
        display.print(battle_.status());
        display.setCursor(8, 58);
        display.print("Tab: menu (rescan)");
    } else {
        for (size_t i = 0; i < results.size() && i < 6; ++i) {
            const bool selected = i == listSelection_;
            display.setTextColor(selected ? Branding::accent : Branding::text,
                                 Branding::background);
            display.setCursor(8, 28 + static_cast<int>(i) * 15);
            char line[40];
            // playerId 0 means the peer only advertised its GATT service
            // UUID (no manufacturer data parsed) -- see parseAdvertisement().
            // Real level/stats arrive with the HELLO handshake regardless.
            if (results[i].playerId == 0) {
                snprintf(line, sizeof(line), "%sUnknown VPet  %ddBm",
                        selected ? "> " : "  ", results[i].rssi);
            } else {
                snprintf(line, sizeof(line), "%sVPet #%04X  Lv%-3u %ddBm",
                        selected ? "> " : "  ",
                        static_cast<unsigned>(results[i].playerId & 0xFFFFU),
                        static_cast<unsigned>(results[i].level), results[i].rssi);
            }
            display.print(line);
        }
    }
    ScreenChrome::drawFooter("Enter: challenge   Tab: menu   Esc: back");
}

namespace {
// >25% healthy (theme accent), low HP switches to the warning color --
// same threshold philosophy as any HP/battery-style readout in this app.
uint16_t hpBarColor(uint16_t hp, uint16_t maxHp) {
    if (maxHp == 0) return Branding::muted;
    return (hp * 100U / maxHp) > 25U ? Branding::accent : Branding::warning;
}
}  // namespace

// Versus-style layout (a Pokemon-battle-screen nod, sized for this
// screen): both Familiars' actual evolved silhouettes side by side via
// drawCreatureStage() -- the player's own current stage on the left in
// its usual accent color, the opponent's stage (learned from the HELLO
// handshake, see FamiliarBattleService) on the right tinted the warning
// color so the two read as clearly distinct combatants. One dialogue-box
// line shows the latest log entry rather than a scrollback, same "one
// message at a time" convention the reference has; the move prompt lives
// in the footer instead of competing for content-area space.
void FamiliarScreens::drawBattle() {
    ScreenChrome::drawHeader("Battle!");
    auto& display = M5Cardputer.Display;

    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 24);
    display.printf("You Lv%u", static_cast<unsigned>(familiar_.level()));
    display.setCursor(132, 24);
    display.printf("Opp Lv%u", static_cast<unsigned>(battle_.opponent().level));

    display.drawRect(8, 34, 104, 8, Branding::muted);
    const uint32_t myPct =
        battle_.myMaxHp() ? battle_.myHp() * 100U / battle_.myMaxHp() : 0;
    display.fillRect(10, 36, std::min<uint32_t>(100, myPct * 100U / 100U), 4,
                     hpBarColor(battle_.myHp(), battle_.myMaxHp()));
    display.drawRect(128, 34, 104, 8, Branding::muted);
    const uint32_t oppPct = battle_.opponentMaxHp()
                                ? battle_.opponentHp() * 100U / battle_.opponentMaxHp()
                                : 0;
    display.fillRect(130, 36, std::min<uint32_t>(100, oppPct * 100U / 100U), 4,
                     hpBarColor(battle_.opponentHp(), battle_.opponentMaxHp()));

    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 46);
    display.printf("%u/%u", static_cast<unsigned>(battle_.myHp()),
                   static_cast<unsigned>(battle_.myMaxHp()));
    display.setCursor(132, 46);
    display.printf("%u/%u", static_cast<unsigned>(battle_.opponentHp()),
                   static_cast<unsigned>(battle_.opponentMaxHp()));

    drawCreatureStage(display, familiar_.stageIndex(), 62, 90, 0.6f,
                      Branding::accent);
    drawCreatureStage(display, battle_.opponent().stageIndex, 182, 90, 0.6f,
                      Branding::warning);

    const auto& log = battle_.log();
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 104);
    display.print(log.empty() ? String("") : log.back().substring(0, 38));

    ScreenChrome::drawFooter(battle_.myMoveSubmitted()
                                 ? "Waiting for opponent..."
                                 : "A:Attack D:Defend S:Special F:Flee");
}

void FamiliarScreens::drawBattleResult() {
    ScreenChrome::drawHeader("Battle Result");
    auto& display = M5Cardputer.Display;
    display.setTextColor(Branding::text, Branding::background);
    display.setCursor(8, 34);
    const char* headline = "Battle over.";
    switch (battle_.outcome()) {
        case FamiliarBattleOutcome::Victory: headline = "Victory!"; break;
        case FamiliarBattleOutcome::Defeat: headline = "Defeated."; break;
        case FamiliarBattleOutcome::Fled: headline = "You fled."; break;
        case FamiliarBattleOutcome::OpponentFled: headline = "Opponent fled."; break;
        case FamiliarBattleOutcome::Disconnected: headline = "Connection lost."; break;
        default: break;
    }
    display.setTextSize(2);
    display.print(headline);
    display.setTextSize(1);
    display.setTextColor(Branding::muted, Branding::background);
    display.setCursor(8, 64);
    display.printf("%u turns  You %u/%u  Opp %u/%u",
                   static_cast<unsigned>(battle_.turnNumber()),
                   static_cast<unsigned>(battle_.myHp()),
                   static_cast<unsigned>(battle_.myMaxHp()),
                   static_cast<unsigned>(battle_.opponentHp()),
                   static_cast<unsigned>(battle_.opponentMaxHp()));
    ScreenChrome::drawFooter("Enter/Esc: back to Familiar");
}
