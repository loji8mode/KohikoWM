# Kohiko

A small, fast tiling window manager for X11, built around a Hyprland-style
BSP (binary space partitioning) layout: every new window splits the space
next to whatever's focused, `Super+LMB` drags swap two windows outright,
and `Super+RMB` drags resize a window and everything next to it adjusts
automatically.

Kohiko is **not** a Hyprland replacement and it isn't Wayland - Hyprland is
a Wayland compositor, which means competing with it feature-for-feature
would mean writing a compositor, a renderer, and an input stack from
scratch. Kohiko instead borrows the parts of Hyprland's tiling *behaviour*
that make it pleasant to use (the BSP dwindle-style layout, `hyprctl`-style
IPC, sane defaults) and implements them as a plain X11 window manager, on
top of decades-old, extremely well understood APIs. The payoff: no
compositing, no GPU shaders, no animations, nothing running that doesn't
have to - just gaps, borders, and tiling. It should run comfortably on
hardware that would struggle with a full compositor.

## Contents

- [Building](#building)
- [Running it](#running-it)
- [Configuration](#configuration)
- [Default keybindings](#default-keybindings)
- [The mouse: swap and resize](#the-mouse-swap-and-resize)
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
general.bar_height=26
general.focus_follows_mouse=true

workspace.count=10
scratchpad.width=70%
scratchpad.height=70%

exec.terminal=xterm              # referenced from `bind=... exec terminal`

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
| `Super+D`              | launch `exec.launcher`                    |
| `Super+Q`              | close the focused window                  |
| `Super+Space`          | toggle floating                           |
| `Super+F`              | toggle fullscreen                         |
| `Super+F1`             | toggle scratchpad                          |
| `Super+R`              | rotate the focused split (vertical <-> horizontal) |
| `Super+Shift+R`        | flip the focused split (mirror the two panes) |
| `Super+H/J/K/L`        | move focus left/down/up/right             |
| `Super+1..0`           | switch to workspace 1-10                  |
| `Super+Shift+1..0`     | send the focused window to workspace 1-10 |
| `Super+Shift+C`        | reload the config                         |
| `Super+Shift+Q`        | quit Kohiko                               |

All of the above are just entries in `kohiko.conf` - remove, remap, or add
to them freely; nothing is hardcoded.

## The mouse: swap and resize

These are the two gestures the whole layout is built around:

- **`Super` + left-click and drag** picks up whatever window is under the
  cursor and swaps it with whatever window you drag onto. Geometry never
  changes; only which window occupies which tile does, so it survives the
  very next layout pass without any teleporting or animation - it just
  looks like the two windows traded places. Drag across several windows in
  one gesture and each crossing swaps again, so you can shuffle a window
  several tiles over in one motion.
- **`Super` + right-click and drag** resizes: the window you grabbed grows
  or shrinks in whichever direction you drag it, and its neighbour shrinks
  or grows to match, exactly like dragging the divider between them.

Both are rebindable via `mouse.swap=` / `mouse.resize=` (e.g. `SUPER+BTN2`
for the middle button). Floating windows are intentionally not part of
either gesture - dragging a floating window around isn't something this
layout is trying to do (per the original design note: no floating-window
move like i3, specifically Hyprland-style BSP dragging only).

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
| `BSPTree` / `BSPNode` / `BSPLeaf` / `BSPSplit` | The tree itself: insert-next-to-focused, remove-and-collapse, swap, resize, rotate, flip, neighbor search, hit-testing, JSON dump |
| `LayoutEngine`             | Walks the tree and turns ratios into pixels (gaps, borders, smart gaps/borders) |
| `WindowManager`            | Coordinator - owns everything else, sequences the actual X11 event handling |
| `KeyboardManager`          | Config binds -> grabbed keys -> `Command`s. No other logic. |
| `MouseManager`             | The Super+drag state machine described above |
| `EventDispatcher`          | One `switch` over every X11 event type |
| `XConnection`              | The only file that calls Xlib directly (almost) |
| `WorkspaceManager` / `Workspace` | Which workspace is current/previous |
| `WindowRepository`         | `WindowID -> ManagedWindow*`, plus the `Focused()/Floating()/Scratchpad()/Visible()` filters |
| `ManagedWindow`            | Everything Kohiko knows about one window |
| `Config` / `ConfigParser`  | The `key=value` file, with repeatable keys for `bind=`/`exec.*=` |
| `IPCServer` / `kohikoctl`  | The Unix-socket control protocol and its CLI client |
| `Bar`                      | Workspaces, active title, scratchpad indicator, clock - plain Xlib text, no toolkit |
| `MonitorManager` / `Monitor` | XRandr geometry when available, one-monitor fallback otherwise |

A `BSPNode`'s `Geometry()` is its raw tree-partition rect (no gaps - used
for hit-testing and neighbor search because it tiles the workspace with no
dead zones); a `ManagedWindow`'s `Geometry()` is the actual on-screen rect
after gaps and border are subtracted. Swapping re-points which window a
leaf refers to rather than touching any rect, which is what makes it
survive being laid out again immediately afterward.

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
