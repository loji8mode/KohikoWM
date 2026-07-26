#pragma once

#include "Types.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Kohiko
{

class ManagedWindow;
class MonitorManager;

// One window's state as of the last clean shutdown. See
// WindowManager::Manage() for how session.restore_priority decides
// whether this or a matching windowrule= wins when both have an
// opinion about the same window.
struct SessionWindowState
{
    int workspace = 1;
    bool floating = false;
    bool fullscreen = false;
    Rect floatingGeometry{};   // width/height <= 0 means "none saved"
    std::string monitorName;   // XRandr output name; may be empty
};

// Persists every managed window's placement across a Kohiko restart,
// keyed by X11 Window ID - which, unlike everything else about a
// window, survives a restart of Kohiko itself unchanged (restarting
// the *window manager* process never touches any other client's
// windows; only a full X session restart hands out fresh IDs, and a
// session file that no longer matches anything simply goes unused,
// the same as it never having existed). Follows the same "flat file
// under Xdg::DataDir()" approach as HistoryStore.
class SessionStore
{
public:

    SessionStore();

    // Loaded once at startup, before the first Manage() call -
    // WindowManager::Initialize() calls this ahead of
    // AdoptExistingWindows().
    void Load();

    // nullptr if `id` has no saved state (a window that didn't exist,
    // or wasn't managed, last time Kohiko ran).
    const SessionWindowState* Find(
        WindowID id
    ) const;

    // Snapshots every currently managed window and writes it to disk.
    // Called once, from WindowManager::Shutdown(). Scratchpad windows
    // are deliberately skipped - Scratchpad's own single-slot state
    // isn't part of what session restore covers.
    void Save(
        const std::vector<ManagedWindow*>& windows,
        const MonitorManager& monitors
    );

private:

    std::unordered_map<WindowID, SessionWindowState> m_records;

};

}
