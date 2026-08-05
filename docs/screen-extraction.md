# Extracting screens out of src/main.cpp

`src/main.cpp` currently holds the draw/input logic for every screen in one
~9,000-line file, inside a single anonymous namespace. Every other subsystem
(Wi-Fi, BLE, GNSS, SSH, etc.) already lives in its own `include/x_service.h` +
`src/x_service.cpp`. This doc describes the pattern used to bring screens in
line with that, one at a time, per the roadmap's "gradual screen/controller
extraction from `src/main.cpp`" goal.

## Why gradual, not a rewrite

main.cpp's ~100+ globals (`listSelection`, `sdAvailable`, per-screen state,
...) are read and written across most of the file. Extracting everything at
once means touching all of it under one unreviewable diff. Extracting one
screen at a time keeps each change small, independently buildable, and
revertable, and lets the pattern get corrected after the first couple of
screens instead of being locked in early.

## The pattern

1. **New module per screen (or small screen family)**: `include/x_screen.h` +
   `src/x_screen.cpp`, following the same shape as the existing services — a
   class holding whatever state is specific to that screen, with a `draw()`
   and the input-handling entry points the screen needs (see
   `include/ir_screen.h` for the smallest possible example: a stateless
   screen that just displays and drives an existing service).

2. **Shared chrome via `include/screen_chrome.h`**: extracted screens need
   `drawHeader`/`drawFooter`/etc., which are defined in main.cpp's anonymous
   namespace and so have internal linkage — invisible to another translation
   unit. Rather than move them (they reach deep into main.cpp's global
   service instances via `drawHeaderStatus`), `screen_chrome.h` declares a
   small external-linkage surface, and main.cpp provides thin forwarders
   right after its anonymous namespace closes:

   ```cpp
   }  // namespace

   namespace ScreenChrome {
   void drawHeader(const char* title) { ::drawHeader(title); }
   void drawFooter(const char* text) { ::drawFooter(text); }
   }  // namespace ScreenChrome
   ```

   Add to `screen_chrome.h` only when an extracted screen actually needs a
   new primitive (e.g. `drawListRow`/`normalizeListPosition` once a screen
   with a list is extracted) — keep the surface hand-picked, not a dump of
   everything main.cpp has.

   This is a deliberate stopgap, not the end state: once enough screens are
   extracted that main.cpp's remaining share of the file shrinks
   substantially, revisit moving `drawHeader`/`drawFooter`/`drawHeaderStatus`
   and the service globals they read into their own module for real, instead
   of forwarding into main.cpp's anonymous namespace.

3. **Wire it into main.cpp**: instantiate the screen next to the service
   it wraps (e.g. `IrScreen irScreen(irService);` right after
   `IrService irService;`), delete the old free functions, and repoint the
   two call sites — the draw dispatch switch and the input-handling switch
   — at the new instance's methods.

4. **Verify**: `pio run -e cardputer_adv` should succeed with an unchanged
   RAM/Flash footprint (a pure refactor moves code, it doesn't add any), and
   `pio test -e native` should still pass.

## Picking the next screen

Start with screens that touch little shared state — a good proxy is how much
of `listSelection`/`listOffset` and other cross-screen globals the screen's
draw/input functions read. `Screen::Infrared` had none (see
`include/ir_screen.h`/`src/ir_screen.cpp`, the first extraction). Screens that
only wrap a single existing service and a small confirm/result flow (e.g.
`Screen::BleSpamSelect`/`Screen::BleSpam`, `Screen::Gnss`) are reasonable next
candidates; menu screens with shared list-cursor state are better done after
`drawListRow`/`normalizeListPosition` are exposed through `screen_chrome.h`.

## Status

- [x] `Screen::Infrared` — `include/ir_screen.h`, `src/ir_screen.cpp`.
- [ ] Everything else.
