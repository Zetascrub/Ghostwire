#include "terminal_buffer.h"

void appendTerminalByte(char value, std::vector<String>& lines,
                        String& pending, TerminalEscState& escapeState,
                        size_t maxLines) {
    const unsigned char byte = static_cast<unsigned char>(value);
    if (escapeState == TerminalEscState::Esc) {
        escapeState = value == '[' ? TerminalEscState::Csi
                      : value == ']' ? TerminalEscState::Osc
                                     : TerminalEscState::None;
        return;
    }
    if (escapeState == TerminalEscState::Csi) {
        if (value >= '@' && value <= '~') escapeState = TerminalEscState::None;
        return;
    }
    if (escapeState == TerminalEscState::Osc) {
        if (byte == 0x07 || value == '\\') escapeState = TerminalEscState::None;
        return;
    }
    if (byte == 0x1B) {
        escapeState = TerminalEscState::Esc;
        return;
    }
    if (value == '\r') {
        // Carriage return moves a terminal cursor to column zero; it does not
        // erase the line. Ignoring it is safer for this simple line renderer
        // than blanking prompts and echoed input before the following LF.
        return;
    }
    if (value == '\n') {
        lines.push_back(pending);
        if (lines.size() > maxLines) lines.erase(lines.begin());
        pending = "";
        return;
    }
    if (byte == 0x08 || byte == 0x7F) {
        if (!pending.isEmpty()) pending.remove(pending.length() - 1);
        return;
    }
    pending += (byte < 32 || byte > 126) ? '.' : value;
    if (pending.length() > 200) {
        lines.push_back(pending);
        if (lines.size() > maxLines) lines.erase(lines.begin());
        pending = "";
    }
}
