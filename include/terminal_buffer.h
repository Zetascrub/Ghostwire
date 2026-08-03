#pragma once

#include <Arduino.h>
#include <vector>

enum class TerminalEscState : uint8_t { None, Esc, Csi, Osc };

// Append one terminal byte to a bounded line history. ANSI CSI/OSC sequences
// are discarded; this is deliberately a small-screen text view rather than a
// complete terminal emulator.
void appendTerminalByte(char value, std::vector<String>& lines,
                        String& pending, TerminalEscState& escapeState,
                        size_t maxLines);
