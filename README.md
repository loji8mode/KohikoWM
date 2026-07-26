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
- [Multi-monitor](#multi-monitor)
- [Autostart](#autostart)
- [Keyboard layouts / languages](#keyboard-layouts--languages)
- [Default keybindings](#default-keybindings)
- [EWMH support](#ewmh-support)
- [The mouse: swap, move, and resize](#the-mouse-swap-move-and-resize)
- [The launcher (Super+D)](#the-launcher-superd)
- [The notepad (Super+N)](#the-notepad-supern)
- [Fonts and languages](#fonts-and-languages)
- [kohikoctl / IPC](#kohikoctl--ipc)
- [Architecture](#architecture)
- [Known limitations](#known-limitations)

## Building

You need a C++20 compiler and the X11 development headers, plus Imlib2
(icon loading and rendering for the launcher/notepad - icon-theme *lookup*
is Kohiko's own freedesktop Icon Theme Specification implementation, see
`include/IconResolver.h`, so no toolkit dependency is needed for that) and
Xft/fontconfig (text rendering - see
[Fonts and languages](#fonts-and-languages)). There's no Qt, GTK, or
compositor dependency of any kind, and no toolkit is used for the
bar/launcher/notepad's own UI - just plain Xlib shapes plus Xft text.

```sh
sudo apt install build-essential libx11-dev libimlib2-dev \
                  libxft-dev libfontconfig-dev fonts-dejavu-core
# optional, for multi-monitor geometry:
sudo apt install libxrandr-dev
```

Two build systems are provided; pick whichever you'd rather have installed.

**Plain `make`** (no cmake required):

```sh
make -j$(nproc)          # -> ./kohiko, ./kohikoctl
make test                 # unit tests for the BSP tree (no X server needed)
make test-monitors        # MonitorManager/XRandr tests (needs a real X server - skips gracefully without one)
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
`libxrandr`, `imlib2`, `xorg-fonts-misc`, `xorg-server`), builds
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
comments. Four kinds of key are allowed to repeat (`bind=`, `exec.<name>=`,
`windowrule=` - see [Window rules](#window-rules) - and `monitor=` - see
[Multi-monitor](#multi-monitor)); every other key is a single setting
where the last occurrence wins. See
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

workspace1=zen-browser             # same idea, but pinned to a workspace - see Autostart below
workspace2=discord telegram-desktop
workspace3=steam

keyboard.layouts=us,ua           # XKB layouts, applied via setxkbmap
keyboard.layout_toggle=grp:alt_shift_toggle

mouse.swap=SUPER+BTN1
mouse.resize=SUPER+BTN3

bind=SUPER+RETURN exec terminal
bind=SUPER+Q close

windowrule=fullscreen class:flameshot   # see Window rules below
windowrule=tile class:tlauncher

monitor=HDMI-1,workspace=1              # see Multi-monitor below
```

Reload after editing without restarting: `kohikoctl reload` (also bound to
`Super+Shift+C` by default). `auto_start_programs` and `workspace<N>=` are
the one exception - both only ever run right after Kohiko itself starts,
never on reload, so reloading the config doesn't relaunch every autostart
program.

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
transient windows (anything with `WM_TRANSIENT_FOR` set) and every
dialog/utility/splash/toolbar/popup-menu `_NET_WM_WINDOW_TYPE` float
automatically and never enter the BSP tree - only a plain window with
neither (EWMH's definition of NORMAL) ever gets tiled. A transient
child additionally always lands on whatever workspace *and monitor*
its parent is actually on right now (bringing that workspace into view
on the focused monitor if the parent isn't visible anywhere), opens
centered over the parent's own window rather than the middle of the
monitor, and takes focus the moment it appears - so a GIMP color
picker, an IntelliJ/Android Studio dialog, a file picker, a
confirmation prompt, or TLauncher's own update/login prompts always
show up attached to the window that spawned them instead of floating
in the wrong place or getting silently left behind on another
workspace. Any floating window - automatic or `float` above - opens
sized at whatever it actually asked for (its `WM_NORMAL_HINTS`, or
failing that its own size at the moment it asked to be mapped) rather
than a flat fraction of the screen, so it doesn't get squashed or
stretched into a size it was never designed for. `windowrule=` exists
for the apps that don't play along with that on their own.

## Multi-monitor

Kohiko detects every connected output via XRandr (falling back to
treating the whole X display as one monitor if XRandr isn't available)
and gives each one its own independent workspace, exactly like
i3/bspwm/Hyprland: switching workspace on one monitor never touches
what any other monitor is showing, each has its own completely
separate `BSPTree`, its own bar, and fullscreen only ever covers the
monitor it was toggled on.

```
Monitor 1 (HDMI-1) -> Workspace 1        Monitor 1: Workspace 1 -> Workspace 2
Monitor 2 (DP-1)   -> Workspace 5   ->   Monitor 2: Workspace 5 (unchanged)
```

**Focus follows the mouse, monitor for monitor** - whichever monitor
the cursor is currently over is "the focused one": where a new window
opens, which one `workspace <N>`/`movetoworkspace <N>` operate on, and
so on. Just move the pointer there - onto a window, or onto bare
desktop - there's no keybind for it and nothing to press first. (This
is a separate, always-on mechanism from `general.focus_follows_mouse`,
which only controls whether hovering a *specific window* steals its
focus within a monitor; which monitor is focused follows the cursor
either way.) `focusmonitor`/`movetomonitor <left|right|up|down|N>`
still exist as plain commands if you'd rather bind explicit keyboard
control yourself (`kohikoctl dispatch focusmonitor right`, or a `bind=`
line) - nothing is bound to them by default anymore.

**Every monitor has its own bar**, each showing that monitor's *own*
active workspace - Monitor 1's bar highlights workspace 1 while
Monitor 2's highlights workspace 5, independently, exactly matching
whatever `kohikoctl monitors` reports for each. Only the currently-
focused monitor's bar shows a window title (real X input focus is
singular, so that's the only one with a genuine "focused window" to
show); the clock and the scratchpad/notepad indicators appear on every
bar. The system tray is a single X11-wide selection, so it can only
ever dock on one bar - it stays on whichever monitor is Primary().

**Dragging a floating window (Super+LMB) across a monitor boundary
transfers it live** - grab it, drag past the edge, and it belongs to
the new monitor's workspace immediately, still glued to the cursor;
releasing the button just clamps its final position to whatever
monitor it's over so it can't end up partly off-screen. (Super+LMB on
a *tiled* window keeps doing what it always did - pick it up and drop
it onto another tile to swap the two - dragging is only "move it
around" for a floating one.)

**Workspace conflicts are rejected, never swapped or stolen:** asking
a monitor to switch to a workspace that's already visible on a
*different* monitor does nothing to either monitor except show a
notification (right there on the requesting monitor's own bar, for a
few seconds) explaining why:

```
Workspace 3 visible on monitor 1
                                        Result: nothing changes -
User asks monitor 2 to switch to it -> monitor 2's bar shows
                                        "Workspace 3 is already
                                         visible on monitor 1"
```

**New windows** open on whichever monitor is currently focused (i.e.
under the cursor), except a transient/dialog child, which always
follows its *parent's* monitor instead (see [Window rules](#window-rules)
above) - so a launcher on monitor 2 always gets its own update dialog
on monitor 2 too, even if the cursor is over monitor 1 at that instant.

**Hotplug** is handled automatically: connecting or disconnecting a
monitor re-detects the whole layout, gives any newly-connected output
its own workspace (see `monitor=` below), rebuilds each monitor's own
bar to match, and re-homes any floating window whose position no
longer lands on *any* remaining monitor onto whichever one now shows
its workspace, so nothing is ever left positioned in space that no
longer exists. A tiled window needs no such fixup - its layout ratios
simply apply to whatever monitor ends up showing that workspace next.

Pin a specific output to a specific starting workspace with
`monitor=<name>,workspace=<N>` (run `kohikoctl monitors` to see the
exact output names XRandr knows your monitors by):

```ini
monitor=HDMI-1,workspace=1
monitor=DP-1,workspace=2
```

This only affects an output the *next* time it connects (plugging it
in, or starting Kohiko with it already attached) - it deliberately
never yanks a workspace already on screen out from under a monitor
just because a rule for it changed underneath it. The `monitor=`
syntax is intentionally the same `key,key=value,...` shape
`windowrule=` uses, so more per-monitor settings can be added later
without a new config syntax.

`kohikoctl monitors` dumps the full live state as JSON - id, XRandr
output name, geometry, `workArea` (geometry minus that monitor's own
bar), which workspace is active there, and whether it's the
primary/focused one:

```json
[{"id":1,"name":"HDMI-1","x":0,"y":0,"width":1920,"height":1080,
  "workArea":{"x":0,"y":26,"width":1920,"height":1054},
  "workspace":1,"primary":true,"focused":true}]
```

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

### Autostart on a specific workspace

`workspace<N>=` (N from 1 to `workspace.count`) is the same thing, except
each program listed also gets pinned to workspace N once its window
actually appears, instead of landing on whichever workspace happened to
be focused when Kohiko itself started:

```ini
workspace1=zen-browser
workspace2=discord telegram-desktop
workspace3=steam
```

List a program under `workspace<N>=` instead of `auto_start_programs=`,
not in addition to it - either one launches it, and `workspace<N>=` is
just `auto_start_programs=` with a destination attached. Same
space-separated, `/bin/sh -c`-through-DISPLAY syntax either way, and a
program can only be pinned to one workspace at a time.

Kohiko places it by matching the window's own `_NET_WM_PID` back to the
process it just spawned for that line, walking up the process tree to
find it (`/bin/sh -c` itself already forks one extra level before ever
running your command, and a program with its own launcher script - e.g.
Steam - adds more on top of that), which is also why this only ever
affects a window that shows up within a minute or so of Kohiko
starting - long enough to cover even a slow starter, but not so long
that the same program opening some unrelated window an hour into the
session gets redirected too. A `windowrule=workspace:N` for the same
window (see [Window rules](#window-rules)) always wins over this if both
apply, since that's a more specific, deliberate override.

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

Which *monitor* is focused isn't a keybind at all - it just follows
the mouse pointer (see [Multi-monitor](#multi-monitor)); `Super+LMB`
drag on a floating window also carries it across monitor boundaries
live. `focusmonitor`/`movetomonitor` remain available as commands for
anyone who wants an explicit keybind of their own.

All of the above are just entries in `kohiko.conf` - remove, remap, or add
to them freely; nothing is hardcoded.

## System tray

The bar implements the freedesktop System Tray Protocol, so applets that
dock an icon there (NetworkManager, Bluetooth, volume, etc.) show up at
the right edge of the bar, just left of the clock, the same way they
would in any other status bar. No configuration needed - Kohiko takes
ownership of the tray selection on startup and lays out whatever docks
itself with it, left to right, in the order it arrived. On a
multi-monitor setup (every monitor has its own bar - see
[Multi-monitor](#multi-monitor)) the tray is a single X11-wide
selection, so it can only ever live on one of them - it stays on
whichever monitor is Primary(), following it if a hotplug changes
which one that is.

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

## The mouse: swap, move, and resize

These are the gestures the whole layout is built around:

- **`Super` + left-click and drag on a *tiled* window** picks it up: it
  detaches and follows the cursor exactly, and nothing else on screen
  moves while you're dragging - this is deliberately not a live
  swap-on-hover, so the only thing that happens *is* the thing you're
  doing. Whichever tiled window you're currently over gets a highlighted
  border as a preview of what you're about to swap with. Release over a
  window and the two trade places, each sliding smoothly into its new
  tile; release over empty space (or back over the window you picked up)
  and it slides back home instead. Geometry itself never changes - only
  which window occupies which tile does - the sliding motion is just
  confirming that for you, not decoration.
- **`Super` + left-click and drag on a *floating* window** just moves
  it, 1:1 with the cursor, the classic i3-style floating drag - and on
  a multi-monitor setup, dragging it across a monitor boundary
  transfers it onto the destination monitor's workspace immediately,
  live, mid-drag (see [Multi-monitor](#multi-monitor)); releasing the
  button clamps it to stay fully on whichever monitor it ends up over.
- **`Super` + right-click and drag** resizes: on a *tiled* window, the
  window you grabbed grows or shrinks in whichever direction you drag
  it, and its neighbour shrinks or grows to match, exactly like
  dragging the divider between them - updated on every reported mouse
  movement with no queued-up lag, even under a fast/laggy pointer
  (Kohiko collapses a backlog of motion events down to the latest one
  rather than working through it one at a time). On a *floating*
  window it resizes that window directly instead - whichever edge(s)
  you grabbed nearest to (an edge, or a corner for both axes at once)
  grow or shrink with the cursor while the opposite edge(s) stay put,
  clamped to the window's own minimum size and to whichever monitor
  it's on.

Both left-click gestures share one bind (`mouse.swap=`, default
`SUPER+BTN1`) - which one happens just depends on whether the window
under the cursor is tiled or floating. Resize is separately rebindable
via `mouse.resize=` (e.g. `SUPER+BTN2` for the middle button).

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
kohikoctl dispatch focusmonitor right # see Multi-monitor above
kohikoctl clients                      # JSON: every managed window
kohikoctl monitors                     # JSON: detected monitors
kohikoctl activewindow                 # JSON: the focused window, or null
kohikoctl tree                         # JSON: the focused monitor's workspace's BSP tree (or `tree <id>` for any workspace)
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
| `MouseManager`             | The Super+drag state machine described above, plus routing ambient pointer motion to monitor-focus-follow when no drag is active |
| `Animator`                 | The small rect-tween stepper behind the Swap-drop animation |
| `Launcher`                 | The native `Super+D` input box |
| `Notepad`                  | The native `Super+N` scratch-notes box, with disk persistence |
| `EventDispatcher`          | One `switch` over every X11 event type |
| `XConnection`              | The only file that calls Xlib directly (almost) |
| `WorkspaceManager` / `Workspace` | Owns every workspace (and its own independent `BSPTree`) for the process lifetime - no notion of a single "current" one; see [Multi-monitor](#multi-monitor) |
| `WindowRepository`         | `WindowID -> ManagedWindow*`, plus the `Focused()/Floating()/Scratchpad()/Visible()` filters |
| `ManagedWindow`            | Everything Kohiko knows about one window |
| `Config` / `ConfigParser`  | The `key=value` file, with repeatable keys for `bind=`/`exec.*=`/`windowrule=`/`monitor=` |
| `WindowRule`                | Parses/matches `windowrule=` lines - see [Window rules](#window-rules) |
| `IPCServer` / `kohikoctl`  | The Unix-socket control protocol and its CLI client |
| `Bar`                      | One instance per monitor - its own workspaces/active-highlight, a title (focused monitor only), scratchpad/notepad indicators, clock, transient notifications - plain Xlib text, no toolkit |
| `MonitorManager` / `Monitor` / `MonitorRule` | XRandr detection and hotplug, one monitor's geometry/`WorkArea()`/active workspace, `monitor=` rule parsing - see [Multi-monitor](#multi-monitor) |

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

- **The launcher and notepad are single, global panels, always on the
  primary monitor** - not one per output, unlike the bar (see
  [Multi-monitor](#multi-monitor)). Every workspace/window still tiles
  and floats correctly on whichever monitor it's actually on; it's
  specifically these two modal overlays that don't (yet) follow you
  across monitors.
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
