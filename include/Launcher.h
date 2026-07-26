#pragma once

#include <filesystem>

#include "Font.h"
#include "Types.h"

#include <X11/Xlib.h>

#include <unordered_map>

#include <string>

#include <string>

#include <vector>

namespace Kohiko
{

class XConnection;
class Config;

// What happened to one XKeyEvent fed into a currently-open Launcher.
enum class LauncherResult
{
    Editing,    // consumed - the query text changed, just redraw
    Confirmed,  // Enter - Query() is what should be run, then close
    Cancelled   // Escape - close without running anything
};

// Kohiko's native Super+D launcher, replacing the old
// `exec.launcher=dmenu_run` default: a small centered input box that
// "instantly opens, cursor already in the input field, type, Enter,
// program starts, launcher closes" - no results list, no external
// process, no toolkit. Rendered the same way as Bar: plain Xlib
// shapes plus Xft text (see Font.h), for the same "own bar, no
// toolkit" reason.
//
// Deliberately never uses an active keyboard grab (XGrabKeyboard) -
// WindowManager just gives it ordinary X input focus and, while it's
// open, routes every KeyPress to it directly (see
// WindowManager::HandleModalKeyPress). That keeps this feature from
// ever being the reason Kohiko's global hotkeys stop responding - the
// exact failure mode the Super+Q hardening in WindowManager::Manage()
// guards against.

struct LauncherEntry
{
    std::string name;
    std::string desktopId;
    std::string exec;
    std::string icon;
    std::string iconPath;

    Pixmap iconPixmap = 0;

    // 1-bit shape mask that goes with iconPixmap (see Launcher::LoadIcon)
    // - lets Redraw() clip its XCopyArea to just the icon's own opaque
    // pixels instead of painting a solid square of whatever the
    // Pixmap's memory happened to contain.
    Pixmap iconMask = 0;
};

struct FileEntry
{
    std::string name;
    std::string path;
    bool isDirectory = false;
};

class Launcher
{
public:

    explicit Launcher(
        XConnection& connection
    );

    ~Launcher();

    void Configure(
        const Config& config,
        const Rect& monitorGeometry
    );

    // Opens the box empty, mapped, raised - WindowManager still has to
    // give it real input focus afterwards (it owns focus policy).
    void Open();

    // Unmaps the box. Safe to call even when already closed.
    void Close();

    bool IsOpen() const;

    ::Window WindowId() const;

    // The currently typed line - only meaningful while IsOpen().
    const std::string& Query() const;
    std::string SelectedCommand();
    std::string SelectedPath();
bool SelectedIsFile() const;

    LauncherResult HandleKeyPress(
        const XKeyEvent& event
    );

    void HandleExpose();
    void HandleButtonPress(const XButtonEvent& event);
    // Called on Kohiko's ~1s idle heartbeat while open, to blink the
    // caret - cheap enough not to need a dedicated timer.
    void Blink();

    // Rebuilds the cached application list (from .desktop files) and
    // the file index (from $HOME) from scratch, so newly-installed
    // programs and newly-created/removed files show up without
    // restarting Kohiko. Not called automatically - wired up to
    // CommandType::LauncherReload (default bind Super+Shift+D) and
    // the `reloadlauncher` IPC verb, so it can be triggered on demand
    // (e.g. right after installing something) instead of on every
    // single Open(), which would mean re-walking the entire home
    // directory every time Super+D is pressed.
    void ReloadDesktopEntries();

private:

    void Redraw();
    void UpdateMatches();
    void LoadDesktopEntries();
    void BuildFileIndex();

    // Returns the icon scaled to a 20x20 Pixmap and, via maskOut, the
    // 1-bit shape mask that goes with it (see the .cpp for why both
    // are needed to avoid a black box around every icon).
    Pixmap LoadIcon(
        const std::string& path,
        Pixmap& maskOut);

    std::string FindIconPath(
        const std::string& iconName);

private:

    XConnection& m_connection;

    ::Window m_window = 0;
    GC m_gc = nullptr;
    Font m_font;
    XftDraw* m_xftDraw = nullptr;

    XIM m_xim = nullptr;
    XIC m_xic = nullptr;

    Rect m_geometry;
    bool m_open = false;

    std::string m_query;
    std::size_t m_cursor = 0;
    bool m_caretOn = true;

    unsigned long m_backgroundPixel = 0;
    unsigned long m_foregroundPixel = 0;
    unsigned long m_borderPixel = 0;
    unsigned long m_placeholderPixel = 0;
std::vector<LauncherEntry> m_entries;
bool m_entriesLoaded = false;
enum class MatchType
{
    Application,
    File
};

struct MatchResult
{
    MatchType type;
    std::size_t index;
    int score;
};

std::vector<MatchResult> m_matches;
std::size_t m_selectedIndex = 0;
std::size_t m_scrollOffset = 0;
std::unordered_map<std::string, Pixmap> m_iconCache;
std::unordered_map<std::string, int> m_launchCounts;
std::vector<FileEntry> m_files;
Pixmap m_folderIconPixmap = 0;
Pixmap m_fileIconPixmap = 0;
Pixmap m_folderIconMask = 0;
Pixmap m_fileIconMask = 0;
void LoadLaunchHistory();
void SaveLaunchHistory();

int CalculateFinalScore(
    const LauncherEntry& app,
    int baseScore) const;
int VisibleRows() const;
};

}
