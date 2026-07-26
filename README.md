# Kohiko

A small, fast tiling window manager for X11, built around a Hyprland-style
BSP (binary space partitioning) layout: every new window splits the space
next to whatever's focused, `Super+LMB` drags pick a window up and swap it
with wherever you drop it, and `Super+RMB` drags resize a window and
everything next to it adjusts automatically. A native launcher (`Super+D`)
and a small scratch notepad (`Super+N`) round it out - both drawn the same
plain-Xlib way as the bar, with no extra process or toolkit dependency.

Kohiko is **not** a Hyprland replacement and it isn't Wayland - Hyprland is
a Wayland compositor, which means competing with it feature-for-feature
would mean writing a compositor, a renderer, and an input stack from
scratch. Kohiko instead borrows the parts of Hyprland's tiling *behaviour*
that make it pleasant to use (the BSP dwindle-style layout, `hyprctl`-style
IPC, sane defaults) and implements them as a plain X11 window manager, on
top of decades-old, extremely well understood APIs. The payoff: no
compositing, no GPU shaders, no decorative effects, nothing running that
doesn't have to - just gaps, borders, and tiling, plus the one animation
rule the project does follow: motion only ever plays to confirm an action
you just took (a swapped window sliding into its new tile), never as
decoration. It should run comfortably on hardware that would struggle with
a full compositor.

## Contents

- [Building](#building)
- [Running it](#running-it)
- [Configuration](#configuration)
- [Window rules](#window-rules)
- [Autostart](#autostart)
- [Keyboard layouts / languages](#keyboard-layouts--languages)
- [Default keybindings](#default-keybindings)
- [EWMH support](#ewmh-support)
- [The mouse: swap and resize](#the-mouse-swap-and-resize)
- [The launcher (Super+D)](#the-launcher-superd)
- [The notepad (Super+N)](#the-notepad-supern)
- [Fonts and languages](#fonts-and-languages)
- [kohikoctl / IPC](#kohikoctl--ipc)
- [Architecture](#architecture)
- [Known limitations](#known-limitations)

## Building

You need a C++20 compiler and the X11 development headers, plus GTK3 and
Imlib2 (icon-theme lookup and icon rendering for the launcher - see
`scripts/install-arch.sh`'s dependency comments for exactly where each one
is used) and Xft/fontconfig (text rendering - see
[Fonts and languages](#fonts-and-languages)). There's no Qt or compositor
dependency of any kind, and no toolkit is used for the bar/launcher/
notepad's own UI - just plain Xlib shapes plus Xft text.

```sh
sudo apt install build-essential libx11-dev libgtk-3-dev libimlib2-dev \
                  libxft-dev libfontconfig-dev fonts-dejavu-core
# optional, for multi-monitor geometry:
sudo apt install libxrandr-dev
```

Two build systems are provided; pick whichever you'd rather have installed.

**Plain `make`** (no cmake required):

```sh
make -j$(nproc)          # -> ./kohiko, ./kohikoctl
make test                 # unit tests for the BSP tree (no X server needed)
sudo make install          # installs to /usr/local
```

**CMake:**

```sh
scripts/build.sh           # -> build/kohiko, build/kohikoctl
```

If `libxrandr-dev` is present, both build systems automatically compile in
XRandr-based monitor detection (`KOHIKO_HAVE_XRANDR`); if it isn't, Kohiko
falls back to treating the whole X display as one monitor, which is
correct for the common single-monitor case regardless.

### Arch Linux: one-command install

```sh
scripts/install-arch.sh
```

Installs every pacman dependency Kohiko needs (`base-devel`, `libx11`,
`libxrandr`, `imlib2`, `gtk3`, `xorg-fonts-misc`, `xorg-server`), builds
with `make -j$(nproc)`, runs `sudo make install`, drops a default config
in `~/.config/kohiko` if you don't have one yet, and registers Kohiko as
a session: an `xsessions` `.desktop` entry so it shows up in your display
manager's session list (SDDM/GDM/LightDM/...), plus a `~/.xinitrc` that
starts it - but only if you don't already have one, so it never
overwrites an existing setup. Run it as your normal user; it calls `sudo`
itself for the steps that need it.

## Running it

Kohiko is a normal X11 window manager, so it's started the same way as any
other (dwm, i3, ...): from your display manager's session list if you add
a `.desktop` entry for it, or directly from `~/.xinitrc`:

```sh
# ~/.xinitrc
exec /path/to/Kohiko/build/kohiko
```

To try it out **without** touching your real session, run it nested
inside a nested X server:

```sh
scripts/run-xephyr.sh 1     # opens a 1600x900 window running Kohiko on :1
```

On first run Kohiko looks for `~/.config/kohiko/kohiko.conf`; if it isn't
there yet, copy the shipped defaults to start from:

```sh
mkdir -p ~/.config/kohiko
cp config/default.conf ~/.config/kohiko/kohiko.conf
```

(You can also pass a config path explicitly: `kohiko /path/to/file.conf`.)

## Configuration

The format is deliberately simple: `key=value`, one per line, `#` for
comments. Three kinds of key are allowed to repeat (`bind=`, `exec.<name>=`,
and `windowrule=` - see [Window rules](#window-rules)); every other key is
a single setting where the last occurrence wins. See
[`config/default.conf`](config/default.conf) for the full annotated set -
the highlights:

```ini
general.inner_gap=6              # gap between tiled windows
general.outer_gap=8              # gap around the edge of the screen
general.border_size=2
general.border_color_active=0x89b4fa
general.border_color_inactive=0x45475a
general.smart_gaps=true          # gaps collapse to 0 with only one tiled window
general.smart_borders=true       # same, for the border
general.min_tile_width=100       # a new tile - or any existing one - never shrinks below this...
general.min_tile_height=60       # ...tries the other split direction/shrinking first, then another workspace, then floating
general.tiling_misbehavior_threshold=3    # how many conflicting resize requests in a row counts as "fighting" tiled geometry
general.tiling_misbehavior_fallback=floating  # floating | new_workspace - where a misbehaving window goes instead
general.bar_height=26
general.focus_follows_mouse=true

workspace.count=10
scratchpad.width=70%
scratchpad.height=70%
notepad.width=40%
notepad.height=50%

exec.terminal=xterm              # referenced from `bind=... exec terminal`
exec.screenshot=flameshot gui    # referenced from `bind=Print exec screenshot`

auto_start_programs=telegram-desktop discord zen-browser  # launched once at startup

keyboard.layouts=us,ua           # XKB layouts, applied via setxkbmap
keyboard.layout_toggle=grp:alt_shift_toggle

mouse.swap=SUPER+BTN1
mouse.resize=SUPER+BTN3

bind=SUPER+RETURN exec terminal
bind=SUPER+Q close

windowrule=fullscreen class:flameshot   # see Window rules below
windowrule=tile class:tlauncher
```

Reload after editing without restarting: `kohikoctl reload` (also bound to
`Super+Shift+C` by default). `auto_start_programs` is the one exception -
it only ever runs right after Kohiko itself starts, never on reload, so
reloading the config doesn't relaunch every autostart program.

## Window rules

No amount of automatic window-type detection can guess every
application's intentions correctly, so - the same as i3's `for_window` or
Hyprland's `windowrule` - Kohiko lets you pin down exactly how a specific
application behaves, overriding whatever it asks for itself:

```ini
windowrule=float class:pavucontrol
windowrule=tile class:tlauncher
windowrule=fullscreen class:flameshot
windowrule=nofullscreen class:mpv
windowrule=workspace:4 class:telegram title:photo
```

- `float` - always open floating (centered, sized to its own natural
  size - see below), never tiled, regardless of window type.
- `tile` - always tile it, strictly, even if it's the kind of window
  (a dialog, or one that insists on opening at its own fixed size -
  Tlauncher is the canonical example of a Java/Swing app that does this
  and fights being resized) Kohiko would otherwise float automatically.
- `fullscreen` - open already fullscreen. flameshot's screenshot overlay
  needs this to work at all (a partial-screen overlay can't select the
  rest of the screen); it already asks for real EWMH fullscreen on its
  own and Kohiko honours that automatically the moment it's mapped (see
  [EWMH support](#ewmh-support)), so this mostly exists as an explicit
  belt-and-suspenders pin, or for apps that don't ask for it themselves.
- `nofullscreen` - never allow real fullscreen for this app, whether it
  asks for it itself or `fullscreen`/`Super+F` asks on its behalf; it
  stays tiled/floating instead.
- `workspace:N` - always open on workspace N regardless of whichever
  workspace is current - e.g. exiling a chat app's media-viewer windows
  to their own workspace ("a new virtual display") instead of covering
  whatever you're doing on the current one.

The selector after the action is one or more of `class:`, `instance:`,
`title:` (case-insensitive substring match against `WM_CLASS`'s
class/instance and the window title, space-separated - a window has to
match every one given to match the rule at all). `xprop` (click the
window after running it) shows you the actual class/instance/title to
match on if you're not sure.

Every window still gets sensible behaviour with **no** rule at all:
transient/dialog windows float automatically, and any floating window -
automatic or `float` above - opens centered at whatever size it actually
asked for (its `WM_NORMAL_HINTS`, or failing that its own size at the
moment it asked to be mapped) rather than a flat fraction of the screen,
so it doesn't get squashed or stretched into a size it was never designed
for. `windowrule=` exists for the apps that don't play along with that on
their own.

## Autostart

`auto_start_programs=` takes a space-separated list of commands, each
launched once, right after the bar/tray/launcher finish starting up -
no keybind needed:

```ini
auto_start_programs=telegram-desktop discord zen-browser
```

Each entry runs exactly the way `exec.<name>=` does (through `/bin/sh -c`,
forced onto this session's `DISPLAY`), so anything you could put after
`exec.terminal=` works here too. Leave it empty, or delete the line, to
autostart nothing.

## Keyboard layouts / languages

Kohiko applies `keyboard.layouts=` via `setxkbmap` once at startup (and
again on reload), so it isn't limited to English - list any XKB layouts
you want, comma-separated:

```ini
keyboard.layouts=us,ua
keyboard.layout_toggle=grp:alt_shift_toggle
```

This is real XKB, so every layout listed works everywhere - in every
window, not just some of them - the same as it would under any other
window manager. `keyboard.layout_toggle` is any `setxkbmap` `grp:`
option; the default above cycles through the listed layouts with
Alt+Shift and is ignored while only one layout is configured.

Kohiko's own keybinds are unaffected by any of this either way:
`KeyboardManager` grabs `bind=` entries by physical keycode (see
[Architecture](#architecture)), so `Super+H/J/K/L` and the rest keep
working identically no matter which layout above is currently active -
only what other programs see you type changes.

## Default keybindings

| Bind                  | Action                                   |
|-----------------------|-------------------------------------------|
| `Super+Return`         | launch `exec.terminal`                    |
| `Super+D`              | toggle the native launcher                |
| `Super+N`              | toggle the native notepad                 |
| `Super+Q`              | close the focused window                  |
| `Super+Space`          | toggle floating                           |
| `Super+F`              | toggle fullscreen                         |
| `Super+F1`             | toggle scratchpad                          |
| `Super+R`              | rotate the focused split (vertical <-> horizontal) |
| `Super+Shift+R`        | flip the focused split (mirror the two panes) |
| `Print`                | screenshot via `exec.screenshot` (flameshot by default) |
| `Super+Shift+D`        | re-scan applications/files for the launcher (live update) |
| `Super+H/J/K/L`        | move focus left/down/up/right             |
| `Super+1..0`           | switch to workspace 1-10                  |
| `Super+Shift+1..0`     | send the focused window to workspace 1-10 |
| `Super+Shift+C`        | reload the config                         |
| `Super+Shift+Q`        | quit Kohiko                               |

All of the above are just entries in `kohiko.conf` - remove, remap, or add
to them freely; nothing is hardcoded.

## System tray

The bar implements the freedesktop System Tray Protocol, so applets that
dock an icon there (NetworkManager, Bluetooth, volume, etc.) show up at
the right edge of the bar, just left of the clock, the same way they
would in any other status bar. No configuration needed - Kohiko takes
ownership of the tray selection on startup and lays out whatever docks
itself with it, left to right, in the order it arrived.

## EWMH support

Kohiko advertises itself as a compliant window manager on startup, per
the EWMH/NetWM spec: it creates an invisible check window and publishes
`_NET_SUPPORTING_WM_CHECK` (proof there's a *live* WM, not a stale
property left over from one that already exited) and `_NET_SUPPORTED`
(the list below). It then keeps two more properties current for the
lifetime of the session:

- `_NET_CLIENT_LIST` - every window Kohiko currently manages, updated
  the moment one is added or removed.
- `_NET_ACTIVE_WINDOW` - whichever window is currently focused (unset
  when nothing is).

`_NET_WM_NAME` is honoured both ways: Kohiko reads it from client
windows for their title (shown in the bar), and sets it on its own
check window for the round-trip above.

Kohiko also implements the one EWMH window *state* it needs to:
`_NET_WM_STATE_FULLSCREEN`. A client can ask for it either of the two
ways the spec allows - a `_NET_WM_STATE` `ClientMessage` (add/remove/
toggle) once it's already mapped, or setting the property directly
before it's ever mapped at all (the form some toolkits use instead,
since the `ClientMessage` form is only meaningful for an already-mapped
window) - and Kohiko honours both, updating the property back to match
reality either way. `windowrule=fullscreen`/`nofullscreen` (see
[Window rules](#window-rules)) sit on top of this same mechanism.

This is what lets EWMH-aware tools that check for a compliant window
manager before trusting anything - flameshot's screenshot overlay is
the motivating example - work correctly under Kohiko instead of falling
back to less reliable window discovery, *and* actually receive the real
fullscreen they ask for once they do.

## The mouse: swap and resize

These are the two gestures the whole layout is built around:

- **`Super` + left-click and drag** picks up whatever window is under the
  cursor: it detaches and follows the cursor exactly, and nothing else on
  screen moves while you're dragging - this is deliberately not a live
  swap-on-hover, so the only thing that happens *is* the thing you're
  doing. Whichever tiled window you're currently over gets a highlighted
  border as a preview of what you're about to swap with. Release over a
  window and the two trade places, each sliding smoothly into its new
  tile; release over empty space (or back over the window you picked up)
  and it slides back home instead. Geometry itself never changes - only
  which window occupies which tile does - the sliding motion is just
  confirming that for you, not decoration.
- **`Super` + right-click and drag** resizes: the window you grabbed grows
  or shrinks in whichever direction you drag it, and its neighbour shrinks
  or grows to match, exactly like dragging the divider between them -
  updated on every reported mouse movement with no queued-up lag, even
  under a fast/laggy pointer (Kohiko collapses a backlog of motion events
  down to the latest one rather than working through it one at a time).

Both are rebindable via `mouse.swap=` / `mouse.resize=` (e.g. `SUPER+BTN2`
for the middle button). Floating windows are intentionally not part of
either gesture - dragging a floating window around isn't something this
layout is trying to do (per the original design note: no floating-window
move like i3, specifically Hyprland-style BSP dragging only).

## The launcher (Super+D)

`Super+D` opens a small centered input box - about a twelfth of the
screen's area, wide rather than square, so it reads as "a line to type
into" rather than a dialog. It opens with the cursor already in the field;
type a command, press `Enter`, and it runs (via the same `/bin/sh -c`
mechanism as `exec.*=` entries) and the box closes itself. `Escape` cancels
without running anything, and clicking anywhere outside the box also
dismisses it.

This replaced the earlier default of shelling out to `dmenu_run` - it's
Kohiko's own window, drawn the same plain-Xlib way as the bar, with no
external launcher binary required. If you'd rather use dmenu, rofi, or
similar instead, the general `exec.<name>=` + `bind=... exec <name>`
mechanism is still there for it; see the commented-out example in
`config/default.conf`.

It also stays raised above everything else for as long as it's open -
including a program that opens *while* it's up. The launcher deliberately
never uses an `XGrabKeyboard` (see its own header comment for why), so it
depends entirely on actually holding X input focus, on top, to be usable
at all; Kohiko re-raises it after every single window-stacking change for
exactly that reason, the same way `Super+N`'s notepad below does.

The application list (from `/usr/share/applications/*.desktop`) and the
file index (from `$HOME`) are both cached in memory rather than
re-scanned on every `Super+D` - walking your entire home directory on
every keystroke would make the launcher feel slow. Install something
new, or add/remove a file, and it won't show up until that cache is
refreshed - either `Super+Shift+D` or `kohikoctl reloadlauncher` does
that immediately, live, with no restart required.

## The notepad (Super+N)

`Super+N` toggles a small native scratch-notes box: free-form multi-line
text (typing, `Enter` for a new line, `Backspace`/`Delete`, arrow keys,
`Home`/`End`), saved automatically to `~/.config/kohiko/notepad.txt` and
restored from there on every restart. `Escape` hides it again (saving on
the way out) without closing or discarding anything.

The bar shows a `[N]` indicator whenever the notepad has any saved content
or is currently open - a bracketed-letter indicator in the same style as
the scratchpad's `[S]`, rather than an actual emoji glyph, since relying on
one specific emoji being installed (and rendering as more than a
missing-glyph box) is fragile in a way a plain letter never is.

Deliberately, this is *not* "spawn a text editor into a hidden special
workspace and toggle it," which is the common pattern in the Hyprland
ecosystem for this kind of quick-notes utility - it's a genuinely native
widget with its own tiny text buffer, again with no toolkit or external
process involved. The trade-off is a correspondingly small feature set: no
undo, no selection, no copy/paste - it's meant for jotting something down,
not replacing a real editor.

## Fonts and languages

The bar, launcher, and notepad all draw their own text through Xft/
fontconfig (`include/Font.h`), the one dependency this project takes on
beyond plain libX11 - not for decoration, but because classic X11 core
fonts (the old `XCreateFontSet`/`Xutf8DrawString` approach this replaced)
essentially never have real glyph coverage for anything beyond Latin text
on a modern system, which meant Cyrillic, CJK, and most other scripts
silently rendered as missing-glyph boxes no matter what font name was
requested.

`general.font=` names one font, as a fontconfig pattern - not an XLFD name:

```ini
general.font=monospace:pixelsize=14
```

That font only has to cover whatever script *you* mostly type in - any
character it doesn't have (Ukrainian on a Latin-only font, Chinese on
almost any font, ...) is automatically looked up against every other font
installed on the system and drawn with whichever one actually has that
glyph, the same per-character fallback technique dwm's own Xft patch uses.
There's nothing to configure per-language: install a font that covers the
script you need (e.g. `noto-fonts-cjk` on Arch, `fonts-noto-cjk` on
Debian/Ubuntu, for Chinese/Japanese/Korean) and it's picked up automatically
the next time Kohiko starts.

If a codepoint genuinely isn't covered by any installed font, it still
falls back to drawing with `general.font=`'s own font, which typically
means a missing-glyph "tofu" box - that's a "no such font is installed"
problem, not something Kohiko's fallback logic can work around further.

## kohikoctl / IPC

Kohiko listens on a Unix socket (`/tmp/kohiko_<DISPLAY>.sock`) for simple
line-based commands, in the spirit of `hyprctl`:

```sh
kohikoctl dispatch workspace 3        # anything you could put after `bind=... `
kohikoctl dispatch close
kohikoctl clients                      # JSON: every managed window
kohikoctl monitors                     # JSON: detected monitors
kohikoctl activewindow                 # JSON: the focused window, or null
kohikoctl tree                         # JSON: the current workspace's BSP tree
kohikoctl reload
kohikoctl reloadlauncher              # re-scan applications/files live, no restart needed
kohikoctl quit
```

`dispatch` accepts exactly the same action text as a `bind=` line (minus
the key combo), so anything you can bind to a key you can also trigger
from a script or a keybinding daemon that doesn't know about Kohiko
directly.

## Architecture

Roughly the file layout the project was designed around, one responsibility each:

| File                       | Responsibility |
|----------------------------|----------------|
| `BSPTree` / `BSPNode` / `BSPLeaf` / `BSPSplit` | The tree itself: placement-aware insert-next-to-focused (natural direction, then the other direction, then shrinking other tiles - never below `general.min_tile_*`), remove-and-collapse, swap, resize, rotate, flip, neighbor search, hit-testing, JSON dump |
| `LayoutEngine`             | Walks the tree and turns ratios into pixels (gaps, borders, smart gaps/borders) |
| `WindowManager`            | Coordinator - owns everything else, sequences the actual X11 event handling |
| `KeyboardManager`          | Config binds -> grabbed keys -> `Command`s. No other logic. |
| `MouseManager`             | The Super+drag state machine described above |
| `Animator`                 | The small rect-tween stepper behind the Swap-drop animation |
| `Launcher`                 | The native `Super+D` input box |
| `Notepad`                  | The native `Super+N` scratch-notes box, with disk persistence |
| `EventDispatcher`          | One `switch` over every X11 event type |
| `XConnection`              | The only file that calls Xlib directly (almost) |
| `WorkspaceManager` / `Workspace` | Which workspace is current/previous |
| `WindowRepository`         | `WindowID -> ManagedWindow*`, plus the `Focused()/Floating()/Scratchpad()/Visible()` filters |
| `ManagedWindow`            | Everything Kohiko knows about one window |
| `Config` / `ConfigParser`  | The `key=value` file, with repeatable keys for `bind=`/`exec.*=`/`windowrule=` |
| `WindowRule`                | Parses/matches `windowrule=` lines - see [Window rules](#window-rules) |
| `IPCServer` / `kohikoctl`  | The Unix-socket control protocol and its CLI client |
| `Bar`                      | Workspaces, active title, scratchpad/notepad indicators, clock - plain Xlib text, no toolkit |
| `MonitorManager` / `Monitor` | XRandr geometry when available, one-monitor fallback otherwise |

A `BSPNode`'s `Geometry()` is its raw tree-partition rect (no gaps - used
for hit-testing and neighbor search because it tiles the workspace with no
dead zones); a `ManagedWindow`'s `Geometry()` is the actual on-screen rect
after gaps and border are subtracted. Swapping re-points which window a
leaf refers to rather than touching any rect, which is what makes it safe
to lay out again immediately afterward - `Animator` then plays that
transition back over a couple of frames instead of applying it instantly,
which is the only place Kohiko's "no decorative animation" rule allows
motion at all.

Before a window is ever inserted, `BSPTree::Insert()`'s placement-aware
overload (and `HasSpaceForAnotherWindow()`, its non-mutating probe of the
same logic) tries, in order: the anchor's natural split direction; the
*other* split direction (a wide-but-shallow or tall-but-narrow anchor can
easily fit one way and not the other); and finally shrinking other tiles
- nearest to the anchor first, walking outward toward the root - but
never past `general.min_tile_width/height`, or a leaf's own declared
`WM_NORMAL_HINTS` minimum if it's larger. If none of that finds room,
`WindowManager` falls back to another workspace or, failing that,
floating, instead of ever tiling a window into an unusably small space.
The same escalating logic protects tiled windows afterward, too: a
window that keeps sending `ConfigureRequest`s asking for something other
than the tile it was actually given (`general.tiling_misbehavior_threshold`
in a row) gets pulled out of the tree and switched to floating -
`general.tiling_misbehavior_fallback` - rather than being forced back
into a tile it's already shown it won't render into correctly.

## Known limitations

- **Multi-monitor tiling is basic.** Monitor geometry is detected
  correctly via XRandr, but all tiling currently happens against the
  primary monitor; independent per-monitor workspaces are a plausible
  future addition, not implemented yet.
- **No system tray in the bar**, matching the original design note that
  called it out as later-stage work.
- **The scratchpad holds one window at a time** - assign a new one only
  after closing (or `Super+Space`-releasing) the current occupant.
- Moving a **floating** window around isn't bound to anything by design
  (see above); it spawns centered and stays wherever the client puts it or
  wherever you leave it via its own controls.
- **The notepad is intentionally minimal** - no undo, no text selection,
  no copy/paste. It's a quick-notes box, not a text editor.
- **Kohiko doesn't adopt windows that were already mapped by a previous
  window manager** - it only manages windows that map *after* it starts
  (the usual `MapRequest`-driven flow every X11 WM relies on). Restarting
  Kohiko itself while other windows are already open will leave those
  specific windows unmanaged until they're reopened; this isn't new
  behaviour, just worth knowing about.
- **`general.tiling_misbehavior_*` detects fighting via repeated,
  conflicting `ConfigureRequest`s** (see `config/default.conf`) - the
  only signal X11 actually gives a window manager for "this client
  isn't happy with the geometry it was given". It can't see rendering
  glitches directly, so a client that visually misrenders without ever
  asking to be resized won't trip it; `windowrule=float` (or `=tile`)
  on that specific application remains the direct fix for those.
