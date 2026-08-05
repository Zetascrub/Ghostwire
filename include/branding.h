#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Branding {

// Change these values to rename or restyle the entire firmware.
static constexpr char productName[] = "Ghostwire";
static constexpr char creatorName[] = "Zetascrub";
static constexpr char version[] = "0.4.5-dev";

struct Theme {
    const char* name;
    uint16_t background;
    uint16_t panel;
    uint16_t accent;
    uint16_t text;
    uint16_t muted;
    uint16_t warning;
};

// Built-in palettes. Index 0 ("Matrix") is this firmware's original,
// default look. Selected in Settings, persisted, and applied before the
// boot sequence runs so the boot screen matches too -- see
// applyTheme()/src/branding.cpp.
extern const Theme kThemes[];
extern const size_t kThemeCount;

// Every screen draws using these six shared colours rather than literal
// hex values, so switching themes takes effect everywhere at once.
// Backed by mutable storage in branding.cpp (not `constexpr`) so
// applyTheme() can repoint them at runtime.
extern uint16_t background;
extern uint16_t panel;
extern uint16_t accent;
extern uint16_t text;
extern uint16_t muted;
extern uint16_t warning;

void applyTheme(size_t index);

}  // namespace Branding
