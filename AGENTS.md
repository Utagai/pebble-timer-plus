# AGENTS.md

This document describes the Timer+ Pebble watchapp for anyone (human or AI) making changes.

## Project summary

Timer+ is a native Pebble OS watchapp written in C. It implements a list of up to eight independent
timers, each with a stopwatch/chrono mode. There is no companion JavaScript app, no settings screen,
and no automated test suite.

Key behavior:

- A countdown timer runs in the background using the Pebble WakeUp API.
- Starting from `0:00` enters stopwatch/chrono mode.
- A countdown that reaches zero does **not** stop; it starts counting upward (chrono/overtime) and
  vibrates for the first 20 seconds of overtime. If it is not acknowledged within that window, it
  rewinds itself to the configured duration (paused). Stopwatches keep counting.
- Multiple timers run independently. A long press on Select opens a timer list to switch between,
  add, reset, and delete timers.
- The app supports seven platforms: Emery ONLY.
- The UI is drawn manually with a custom LECO vector font and a custom animation framework.

## Build and run

### Prerequisites

- Pebble Tool and SDK (`pebble --version` currently reports Pebble Tool v5.0.40, SDK v4.33.1).
- Node.js/npm (the build installs the `pebble-scalable` dependency).
- Git LFS for binary assets.

### First-time setup

```sh
git lfs install --local
git lfs pull
npm install
```

### Build

```sh
pebble build
```

The build output is in `build/`. The build compiles every `src/c/**/*.c` file for each platform
listed in `package.json` (see `wscript`).

### Install and inspect on an emulator

```sh
pebble install --emulator emery
pebble logs --emulator emery
```

There is no unit-test or integration-test target; test manually on emulators and/or a device.

### Formatting

- `.clang-format` uses LLVM style with a 100-column limit.
- C files use 2-space indentation and spaces, not tabs.
- `.clang-format-ignore` excludes `build/` and `node_modules/`.

## Repository layout

| Path | Purpose |
| --- | --- |
| `src/c/main.c`, `src/c/main.h` | App lifecycle, window/layer setup, button handling, control-mode state machine, refresh scheduling, wakeup scheduling. |
| `src/c/timer.c`, `src/c/timer.h` | Timer state, time math, persistence, vibration, stopwatch/chrono logic. Holds the multi-timer store. |
| `src/c/timer_list.c`, `src/c/timer_list.h` | The timer list window: switching between, adding, resetting, and deleting timers. |
| `src/c/drawing.c`, `src/c/drawing.h` | All rendering: progress ring, focus rectangle, time text, header/footer text, colors, layout. |
| `src/c/animation.c`, `src/c/animation.h` | Custom animation engine for `GRect` and `int32_t` values. |
| `src/c/interpolation.c`, `src/c/interpolation.h` | Easing curves used by the animation engine. |
| `src/c/text_render.c`, `src/c/text_render.h` | LECO vector font renderer for digits and colon. |
| `src/c/utility.c`, `src/c/utility.h` | Assertions, checked malloc, millisecond epoch, Aplite compatibility, grey fill. |
| `resources/fonts/` | Custom Bebas Neue font used on Emery. |
| `resources/images/` | App menu icons. |
| `assets/` | Marketing screenshots, banners, and source art. Not packaged into the app. |
| `package.json` | Pebble app metadata, UUID, target platforms, resources, npm dependency. |
| `wscript` | Waf build script. Compiles all C sources for each target platform. |
| `.vscode/` | clangd/formatting setup. |

## Runtime architecture

### Entry point and lifecycle

`main.c` owns the app:

1. `prv_initialize()` cancels pending wakeups, loads timer state from persistent storage, chooses the
   initial control mode, creates the window and a full-screen layer, initializes drawing, and
   subscribes to the minute tick service.
2. The first refresh is delayed by `SYSTEM_ENTRANCE_ANIMATION_MS` (400 ms) unless the app was
   launched by quick launch or a wakeup, in which case it refreshes immediately. This avoids
   overlapping the system launch animation.
3. `app_event_loop()` runs until the app exits.
4. `prv_terminate()` unsubscribes from the tick service, schedules a wakeup if a countdown timer is
   running, persists the timer, tears down drawing, and destroys the layer/window.

### Control modes and buttons

`ControlMode` is defined in `main.h`:

```c
ControlModeEditHr,
ControlModeEditMin,
ControlModeEditSec,
ControlModeCounting
```

| Button | Behavior |
| --- | --- |
| Up | In edit mode, increment the selected field. Holding repeats every 100 ms. In counting mode, ignored (except for stopping a vibration). |
| Down | In edit mode, decrement the selected field. Holding repeats every 100 ms. In counting mode, ignored (except for stopping a vibration). |
| Select (short) | Cycle edit fields: hour → minute → second → counting. In counting mode, pause the timer and return to second editing. |
| Select (raw press) | Fires on every select press. Cancels vibration and starts the focus-rectangle "reset" animation. |
| Select (double click) | Reset the active timer to zero and return to minute editing. |
| Select (long, 750 ms) | Open the timer list window. |
| Back | In edit mode, move back one field (second → minute → hour). Exits the app when already in hour editing, or when in minute editing with zero hours. In counting mode, exits the app. |

### Timer list window

Pushed by a long press on Select. It lists every stored timer with its remaining/elapsed time and a
running/paused indicator, and highlights the selected one.

| Button | Behavior |
| --- | --- |
| Up / Down | Move the highlight. |
| Select (short) | Open the highlighted timer and return to the main window. |
| Select (long, 500 ms) | Open a context menu to add a new timer, reset the highlighted timer, or delete it. |
| Back | Return to the main window (default window pop). |

The main window's `appear` handler re-syncs the control mode to the newly active timer when the list
is popped. Because the timer list is a separate `Window`, its Back button is the SDK default pop and
needs no handler.

`BUTTON_HOLD_REPEAT_MS` is 100 ms. `BUTTON_HOLD_RESET_MS` is 750 ms (now the long-press-to-list
threshold).

`main_timer_rewind()` acknowledges alerts: if any timer is vibrating it cancels vibration and rewinds
every vibrating timer (`timer_acknowledge()`). If the active timer was among them it sets the control
mode to `ControlModeEditSec`. Once the vibration window ends (20 seconds of overtime),
`timer_check_elapsed()` rewinds an unacknowledged countdown automatically and the app returns to
second editing.

### Refresh scheduling

- `prv_app_timer_callback()` runs while **any** timer is running (not just the active one), so
  background timers keep counting and can vibrate while the app is open. While the active timer is
  counting it schedules the next tick for the next whole-second boundary (`+5 ms`); otherwise it
  ticks once a second.
- The same tick calls `timer_list_refresh()`, which is how the timer list updates live while it is
  visible.
- `tick_timer_service_subscribe(MINUTE_UNIT, ...)` marks the layer dirty once per minute. This
  updates the footer clock while paused/editing.
- The animation engine runs its own 30 ms timer while animations are active.

## Timer model (`timer.c`)

The persisted state is a `TimerStore` containing a count, the active index, and a fixed array of
`Timer` structs:

```c
#define MAX_TIMERS 8

typedef struct {
  int64_t length_ms; // total timer length
  int64_t start_ms;  // start epoch when running; negative elapsed time when paused
  bool elapsed;      // declared but currently unused
  bool can_vibrate;  // whether the timer should vibrate after elapsing
} Timer;

typedef struct {
  uint8_t count;            // number of valid timers (always at least 1)
  uint8_t active;           // index of the timer the UI is working with
  Timer   timers[MAX_TIMERS];
} TimerStore;
```

The public `timer_*()` functions operate on the **active** timer, so the rest of the app did not have
to change. Index-aware variants (`timer_*_at(index)`) exist for the list window. `timer_count()`,
`timer_set_active()`, `timer_add()`, and `timer_delete()` manage the store, which never drops below
one timer.

### Semantics

- **Running timer:** `start_ms > 0` and equals the epoch (milliseconds) at which the timer started.
- **Paused timer:** `start_ms <= 0` and equals the negative amount of time already elapsed.
- **Timer length:** `length_ms` is the original countdown duration. It can be zero for a stopwatch.
- **Remaining/overtime value:** `timer_get_value_ms()` returns the absolute value of a signed
  quantity. While a countdown is running, that quantity is remaining time. After it elapses (or for
  a stopwatch), the quantity is elapsed time. The modulo-with-epoch expression is deliberate and
  subtle; do not simplify it without testing all running/paused/elapsed states.

### Chrono vs. countdown

`timer_is_chrono()` is true when the timer's value has reached or passed zero. This means:

- A timer started from `0:00` is chrono from the start.
- A countdown that reaches zero becomes chrono and starts counting upward.
- A paused stopwatch with `length_ms == 0` still reports chrono.

`timer_is_vibrating()` is true only when chrono, running, and `can_vibrate` is set. A stopwatch
started from zero has `can_vibrate == false`, so it does not vibrate. A countdown that elapses does
vibrate. `timer_check_elapsed()` enqueues the vibration pattern, then once a running countdown has
been counting overtime for `VIBRATION_LENGTH_MS` (20 seconds) without acknowledgement it rewinds the
timer to its original length (paused) and returns `true`. Stopwatches (`length_ms == 0`) are never
rewound automatically.

### Editing and wrapping

`timer_increment()` wraps values within the selected unit interval (minute, hour, or 100 hours) and
can adjust a paused timer's remaining value. When a timer is chrono and `start_ms` is nonzero,
pressing up/down first rewinds it to its original length and returns without incrementing. If the
timer's value is under one second, it resets to zero. This function is sensitive to the sign of
`start_ms`; test positive/negative and chrono/countdown cases when changing it.

### Persistence and wakeups

- `timer_persist_store()` writes a version key and the raw `TimerStore` struct (count, active index,
  and the `Timer` array) under a single key.
- `timer_persist_read()` reads the current store key. If only the legacy single-timer key exists it
  migrates that timer into slot 0 and deletes the old key. It also deletes a legacy key (`3456`) and,
  if the app was launched by a wakeup, applies a 5-second increment and toggles play/pause. The
  version key is written but not currently read or validated.
- `TimerStore` is about 200 bytes, within the 256-byte `persist_write_data` limit.
- On exit, `prv_terminate()` schedules a wakeup only for a running, non-chrono timer. The wakeup time
  is `(epoch() + remaining_ms) / 1000`, so it is truncated to whole seconds and can fire up to one
  second early. Chrono timers do not schedule wakeups. **Only the active timer schedules a wakeup**;
  background timers are not yet covered, so they will not relaunch the app once it is closed.

## Rendering (`drawing.c`)

### Drawing order

`drawing_render()` draws in this order:

1. Fill the whole layer with `ring_color` (grey dither on black-and-white platforms).
2. Draw the progress "pie" in `back_color` from the current angle to `TRIG_MAX_ANGLE`.
3. Draw the main circle in `mid_color`.
4. Draw the focus rectangle.
5. Draw the main time text in `fore_color`.
6. Draw header text ("Timer" or "Chrono").
7. Draw footer text (clock time).

The names are slightly counterintuitive. On color platforms the full-screen `ring_color` fill is the
visible progress arc and background; `back_color` is the wedge that covers it. On black-and-white
platforms the background is a grey dither instead of `ring_color`. The center circle covers most of
the screen, leaving the ring visible around the edge.

### Progress ring

- In countdown mode, the angle is `TRIG_MAX_ANGLE * remaining / length`, so the colored wedge shrinks
  as time passes.
- In chrono mode, the angle is `TRIG_MAX_ANGLE * (elapsed % 60s) / 60s`, so the wedge resets each
  minute.
- Large angle changes are animated over 250 ms; small changes are applied immediately.
- `length_ms` can be zero for a stopwatch. The current code divides by `timer_get_length_ms()` before
  checking `timer_is_chrono()`. On ARM this typically yields zero rather than trapping, but guard
  this if you change the ring math.

### Main time text

The time is split into five fields: hours, colon, minutes, colon, seconds. Each field has its own
`GRect` in `drawing_data.text_fields[]` so fields can animate independently.

- Edit mode uses two-digit fields for nonzero values and blank separators, with spacing to keep the
  layout stable.
- Counting mode uses `H:MM:SS` when hours are nonzero and `M:SS` when hours are zero. Seconds always
  use two digits. Minutes use two digits only when hours are nonzero; hours never use a leading
  zero.
- The font size is chosen so the whole string fits the current bounds. Each field is then drawn with
  `text_render_draw_scalable_text()`.
- `DrawState` comparison avoids recalculating text positions unless the control mode or digit counts
  change. The initial state uses `hr_digits = 99` to force the first refresh. Note that the compare
  condition rejects `hr_digits >= 3`, so 100+ hour values may not trigger a layout refresh.

### Header/footer

- Header text is "Timer" for countdown and "Chrono" for chrono/stopwatch.
- Footer text shows the projected end time only when in edit mode and not chrono. While counting, it
  shows the current clock time. If you want a live end time during countdown, this is the code to
  change.
- The footer strips a leading zero from the formatted time and respects the 12/24-hour system
  setting.

### Focus rectangle

- In edit mode, the focus rectangle surrounds the selected field (hour, minute, or second) with
  `FOCUS_FIELD_BORDER`.
- In counting mode, the focus rectangle is animated off-screen to the right.
- On black-and-white platforms it is filled with a grey dither; on color platforms it uses
  `ring_color`.

### Colors and scaling

- Colors are set in `drawing_initialize()`:
  - `fore_color = GColorBlack`
  - `mid_color = GColorMintGreen` (color) / `GColorWhite` (BW)
  - `ring_color = GColorGreen` (color) / `GColorWhite` (BW)
  - `back_color = GColorDarkGray` (color) / `GColorBlack` (BW)
- Layout uses `pebble-scalable` (`scl_y`, `scl_set_fonts`, `scl_get_font`). The scale is based on the
  original 144×168 Pebble screen. The comment in `drawing.c` explains converting a pixel value to a
  scalable value. Use `scl_y()` for vertical/absolute dimensions rather than raw pixels.
- Platform conditionals:
  - `PBL_ROUND` for round displays (Chalk, Gabbro).
  - `PBL_BW` for black-and-white displays (Aplite, Diorite, Flint).
  - `PBL_PLATFORM_*` for header/footer offsets on Emery.
  - `PBL_SDK_2` compatibility paths for the original Pebble SDK.

## Animation framework (`animation.c`)

- Animates `GRect` values and `int32_t` values by pointer.
- Uses a singly linked list of `AnimationNode`s and a 30 ms `AppTimer`.
- Each node stores `from`, `to`, duration, delay, and an interpolation curve.
- `from` is captured on the first step, not at creation. This lets a delayed animation start from
  whatever value the target has when the delay expires.
- Animations auto-stop when their duration is reached. `animation_stop()` removes the first matching
  target from the list.
- Multiple animations on the same target are allowed and are used intentionally for bounce and
  stacked effects. If you start a new animation on a target and do not want stacking, call
  `animation_stop()` first.
- `animation_stop_all()` frees all nodes and cancels the timer. It is only called during shutdown. It
  does not clear `head_node` or `ani_timer`, so do not call it and continue using the engine.

## Text renderer (`text_render.c`)

- Implements a custom LECO vector font with point data for digits `0`–`9` and `:`.
- `prv_get_character_index()` accepts ASCII 48–58 only. Invalid characters assert in debug builds and
  index out of bounds in release builds. Do not pass arbitrary strings.
- `LECO_HEIGHT` is 20; font size is a vertical pixel height. Widths are computed from glyph widths
  plus kerning.
- Width calculations always add kerning after the last character for legacy reasons. Keep this in
  mind when changing layout.
- `text_render_get_content_bounds()` measures a string at a given font size.
- `text_render_get_max_font_size()` computes the largest font size that fits a rectangle.
- `text_render_draw_scalable_text()` centers and draws text at the largest fitting size.

## Code conventions

- Public API headers use `//!` Doxygen-style comments. Private functions use `prv_` prefixes.
- Module-level singletons are `static struct { ... }` values in the `.c` file.
- Use `ASSERT()` for pointer/state checks and `MALLOC()` for allocations that must succeed.
- Use `epoch()` for millisecond timestamps; it combines `time()` and `time_ms()`.
- Use `PBL_IF_COLOR_ELSE`, `PBL_BW`, `PBL_ROUND`, and `PBL_PLATFORM_*` for platform differences.
- Keep the UI simple. The app supports several timers, but only one is shown and edited at a time.
- `timer.h` has `#pragma once`.
- The app has no JavaScript and `enableMultiJS` is false. `wscript` still references `src/pkjs`
  paths, but that directory does not exist. If you add JS, enable multi-JS and provide the entry
  file.

## Known gotchas and fragile areas

1. **Timer math is non-obvious.** The modulo/epoch expression in `timer_get_value_ms()` and
   `timer_is_chrono()` encodes running, paused, countdown, and chrono states. Test all combinations
   before changing it.
2. **Chrono is not only "started from zero".** A countdown that hits zero becomes chrono. This is
   intentional and drives the overtime vibration.
3. **`Timer.elapsed` is unused.** Do not assume it is maintained.
4. **Persistence is a raw struct.** The version key is written but not read. There is a migration
   from the legacy single-timer key into slot 0, but changing the `TimerStore`/`Timer` layout still
   invalidates existing saved data.
5. **Progress ring divides by timer length.** `length_ms == 0` is valid for stopwatch mode. Guard the
   division if you touch the ring math.
6. **Text refresh condition ignores 3-digit hours.** `prv_text_state_compare()` rejects
   `hr_digits >= 3`, so a timer of 100+ hours may not recalculate the layout.
7. **Animation stacking is intentional.** Do not globally "fix" multiple animations per target
   without checking the bounce/reset animations.
8. **The overtime window is also the acknowledgement window.** After 20 seconds of overtime,
   `timer_check_elapsed()` rewinds an unacknowledged countdown to its original length and the app
   returns to second editing, so overtime never accumulates indefinitely. Double-click select resets;
   long press opens the timer list.
9. **Raw select fires on every press.** It cancels the vibration device and starts the reset
   animation even for short presses, but it does not rewind the timer state, so a still-vibrating
   timer will re-trigger vibration on the next tick.
10. **Adding multi-click delays single-click.** Because select has a long-click and a double-click
    handler, a single select click is only recognized after the click window closes, so starting or
    pausing has a small built-in delay.
11. **Wakeups are one-second precision and at least a minute apart.** `timer_schedule_wakeups()`
    schedules one wakeup per running countdown (earliest first) and skips any that fall within a
    minute of an already scheduled one, because the SDK rejects those. Chrono timers never schedule
    wakeups. The cookie is the timer index; a wakeup launch makes that timer active. If several
    timers expire close together, only the first gets a wakeup and the rest are handled once the app
    is open.
12. **`animation_stop_all()` is shutdown-only.** It leaves stale list/timer pointers after freeing
    nodes.

## Manual test checklist

- Build for all seven platforms.
- Set hours/minutes/seconds with up/down, including wrapping and holding to repeat.
- Move through edit fields with select/back.
- Start a countdown; verify the ring shrinks and the display updates each second.
- Let a countdown expire; verify chrono/overtime counting, vibration, and that select rewinds while
  vibrating.
- Leave a countdown unacknowledged past 20 seconds of overtime; verify it rewinds to the configured
  duration (paused) and returns to second editing.
- Verify a stopwatch keeps counting past 20 seconds instead of rewinding.
- Verify double-click select resets to zero from edit, counting, and overtime states.
- Start from `0:00`; verify stopwatch mode, ring behavior, and that up/down can convert it to a timer.
- Long press select to open the timer list; verify up/down move the highlight, select opens the
  highlighted timer, long press opens the New/Reset/Delete menu, and back returns.
- Add several timers; verify each keeps its own time and running/paused state.
- Start one timer and leave a different timer active; verify the running timer still counts down and
  alerts, and that its row updates live in the list.
- Delete timers down to one and verify the app always keeps a timer.
- Close and reopen the app with a paused timer, a running timer, and a stopwatch; verify persistence.
- Exit with several running countdowns and verify the wakeups launch the app when the earliest one
  expires.
- Check 12-hour and 24-hour clock formatting.
- Check color and black-and-white platforms, round and rectangular displays, and Emery fonts.
- Watch for animation smoothness and text layout at 1, 2, and 3-digit hour values.
- Check the timer list layout on round displays (Chalk/Gabbro) and large displays (Emery).

## Where to make common changes

- **Timer semantics / persistence / vibration / wakeups:** `src/c/timer.c`, `src/c/timer.h`.
- **Button behavior / control-mode transitions:** `src/c/main.c`.
- **Timer list (switch/add/reset/delete timers):** `src/c/timer_list.c`, `src/c/timer_list.h`.
- **Layout, colors, ring, focus rectangle, header/footer:** `src/c/drawing.c`.
- **Font glyphs or text measurement:** `src/c/text_render.c`.
- **Animation timing/easing:** `src/c/animation.c`, `src/c/interpolation.c`.
- **Platform list, app metadata, resources:** `package.json`.
- **Build sources:** `wscript`.
