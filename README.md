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
- [Default keybindings](#default-keybindings)
- [The mouse: swap and resize](#the-mouse-swap-and-resize)
- [The launcher (Super+D)](#the-launcher-superd)
- [The notepad (Super+N)](#the-notepad-supern)
- [kohikoctl / IPC](#kohikoctl--ipc)
- [Architecture](#architecture)
- [Known limitations](#known-limitations)

## Building

You need a C++20 compiler and the X11 development headers. Everything
else (gaps, borders, the bar) is plain Xlib - there's no GTK, Qt, Xft, or
compositor dependency of any kind.

```sh
sudo apt install build-essential libx11-dev        # Debian/Ubuntu
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
comments. Two kinds of key are allowed to repeat (`bind=` and
`exec.<name>=`); every other key is a single setting where the last
occurrence wins. See [`config/default.conf`](config/default.conf) for the
full annotated set - the highlights:

```ini
general.inner_gap=6              # gap between tiled windows
general.outer_gap=8              # gap around the edge of the screen
general.border_size=2
general.border_color_active=0x89b4fa
general.border_color_inactive=0x45475a
general.smart_gaps=true          # gaps collapse to 0 with only one tiled window
general.smart_borders=true       # same, for the border
general.min_tile_width=100       # a new tile is never allowed to shrink below this...
general.min_tile_height=60       # ...falls back to another workspace, then floating
general.bar_height=26
general.focus_follows_mouse=true

workspace.count=10
scratchpad.width=70%
scratchpad.height=70%
notepad.width=40%
notepad.height=50%

exec.terminal=xterm              # referenced from `bind=... exec terminal`
exec.screenshot=flameshot gui    # referenced from `bind=Print exec screenshot`

mouse.swap=SUPER+BTN1
mouse.resize=SUPER+BTN3

bind=SUPER+RETURN exec terminal
bind=SUPER+Q close
```

Reload after editing without restarting: `kohikoctl reload` (also bound to
`Super+Shift+C` by default).

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

## The notepad (Super+N)

`Super+N` toggles a small native scratch-notes box: free-form multi-line
text (typing, `Enter` for a new line, `Backspace`/`Delete`, arrow keys,
`Home`/`End`), saved automatically to `~/.config/kohiko/notepad.txt` and
restored from there on every restart. `Escape` hides it again (saving on
the way out) without closing or discarding anything.

The bar shows a `[N]` indicator whenever the notepad has any saved content
or is currently open - a bracketed-letter indicator in the same style as
the scratchpad's `[S]`, rather than an actual icon glyph, since Kohiko's
bar only ever draws with a plain X11 core font and those don't carry emoji
glyphs to draw with.

Deliberately, this is *not* "spawn a text editor into a hidden special
workspace and toggle it," which is the common pattern in the Hyprland
ecosystem for this kind of quick-notes utility - it's a genuinely native
widget with its own tiny text buffer, again with no toolkit or external
process involved. The trade-off is a correspondingly small feature set: no
undo, no selection, no copy/paste - it's meant for jotting something down,
not replacing a real editor.

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
| `BSPTree` / `BSPNode` / `BSPLeaf` / `BSPSplit` | The tree itself: insert-next-to-focused (with a minimum-tile-size guard), remove-and-collapse, swap, resize, rotate, flip, neighbor search, hit-testing, JSON dump |
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
| `Config` / `ConfigParser`  | The `key=value` file, with repeatable keys for `bind=`/`exec.*=` |
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
motion at all. Before a window is ever inserted, `BSPTree::HasSpaceForAnotherWindow()`
answers "would this shrink something below `general.min_tile_*`?" without
mutating anything, which is what lets `WindowManager` fall back to another
workspace (or, failing that, floating) instead of ever tiling a window
into an unusably small space.

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
