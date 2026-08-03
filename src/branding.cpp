#include "branding.h"

namespace Branding {

const Theme kThemes[] = {
    // name                 background  panel   accent  text    muted   warning
    {"Matrix",              0x0000,     0x0100, 0x07E0, 0x9FF3, 0x03E0, 0xFD20},
    {"Cyberpunk",           0x0001,     0x1846, 0xF816, 0x079F, 0x79F4, 0xFE80},
    {"Windows",             0x0410,     0x0010, 0xFFFF, 0xE73C, 0x0659, 0xFFE0},
    {"Amber Terminal",      0x0820,     0x28A0, 0xFD80, 0xFE4B, 0x9320, 0xF9E7},
    {"Space",               0x0022,     0x10A6, 0x55BF, 0xCF1F, 0x5B52, 0xFAE7},
    {"Pastel",              0x1083,     0x2926, 0xB51C, 0xEEBC, 0x9455, 0xFCB1},

    // D&D / fantasy themes
    {"Arcane Grimoire",     0x1061,     0x28E2, 0xB447, 0xF718, 0x8B8A, 0xC208},
    {"Necromancer",         0x0021,     0x1083, 0x79F3, 0xDE3D, 0x6ACF, 0xB989},
    {"Dragonfire",          0x1020,     0x2840, 0xE2C4, 0xFE91, 0x9A86, 0xFD80},
    {"Dungeon",             0x0841,     0x1903, 0x7C67, 0xD696, 0x738B, 0xC347},
    {"Celestial",           0x0083,     0x1147, 0x563F, 0xEFBF, 0x6C95, 0xFE8C},
    {"Mimic",               0x1060,     0x28C1, 0xD4C7, 0xF716, 0x8347, 0xDA47},

    // Red shell, pale display, blue interface and yellow alerts
    {"Pokedex",             0x4000,     0xB800, 0x05FF, 0xEFFF, 0x8410, 0xFFE0},

    // SwiftyNet sticker palette
    {"SwiftyNet",           0x10A2,     0x2944, 0xCA8B, 0xF75B, 0xACD1, 0xFACC},

    // Icy blue, snow white, black, beak orange and scarf red
    {"Pingu",               0x020C,     0x0473, 0xFBE0, 0xFFFF, 0x8410, 0xF800},
    {"Frog",                0x0081,     0x1162, 0x8F27, 0xD797, 0x5C2A, 0xFB45},
    {"Duck",                0x0062,     0x1125, 0xFE85, 0xE718, 0x5BF0, 0xFB45},
};
const size_t kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);

uint16_t background = kThemes[0].background;
uint16_t panel = kThemes[0].panel;
uint16_t accent = kThemes[0].accent;
uint16_t text = kThemes[0].text;
uint16_t muted = kThemes[0].muted;
uint16_t warning = kThemes[0].warning;

void applyTheme(size_t index) {
    if (index >= kThemeCount) return;
    const Theme& theme = kThemes[index];
    background = theme.background;
    panel = theme.panel;
    accent = theme.accent;
    text = theme.text;
    muted = theme.muted;
    warning = theme.warning;
}

}  // namespace Branding
