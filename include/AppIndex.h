#pragma once

#include <filesystem>
#include <string>
#include <vector>

// Builds and caches the launcher's application list: scans every
// freedesktop applications directory, parses each .desktop file,
// drops the ones that shouldn't show up in a launcher, merges in
// AppStream metadata where it exists, resolves icons, and collapses
// duplicates down to one entry per real application.
//
// Plain data + free functions rather than a class - there's no
// meaningful state to encapsulate between calls (Launcher already
// owns the resulting std::vector<IndexedApp> and the mutex guarding
// it), just a build step and a cache load/save step.
namespace Kohiko
{

struct IndexedApp
{
    std::string desktopId;
    std::string name;
    std::string genericName;
    std::string comment;
    std::string exec;          // cleaned - ready to hand straight to Process::Spawn
    std::string icon;          // raw Icon= value, kept for debugging/cache purposes
    std::string iconPath;      // resolved absolute path - "" if it couldn't be found anywhere
    std::string startupWMClass;
    std::string execBinary;

    std::vector<std::string> categories;
    std::vector<std::string> keywords;

    // --- derived fields, filled in once by Scoring::PrepareForSearch()
    // after the index is built or loaded, and never recomputed again
    // while it stays in memory - see LauncherScoring.h. ---
    std::string nameLower;
    std::string genericNameLower;
    std::string commentLower;
    std::string execLower;
    std::string nameHumpInitials;
    std::vector<std::string> keywordsLower;
    std::vector<std::string> categoriesLower;
};

namespace AppIndex
{

// One application directory's modification time, used to tell
// whether anything has changed since the cache was last written.
struct DirStamp
{
    std::string path;
    std::filesystem::file_time_type mtime;
};

// The real work: scans Xdg::ApplicationDirs(), parses every
// .desktop file, filters, deduplicates, merges AppStream metadata and
// resolves icons via IconResolver. Pure filesystem/string work (never
// touches X11/Imlib2) - safe to run on a background thread, which is
// exactly how Launcher uses it (see Launcher::RebuildIndexAsync()).
//
// `includeHiddenNoDisplay` mirrors `launcher.show_hidden=`;
// `strictFiltering` mirrors `launcher.strict_filtering=` and enables
// the extra heuristic pass for non-app helper entries that don't
// already set NoDisplay=true (see DesktopEntry::LooksLikeHelperEntry).
std::vector<IndexedApp> Build(
    bool includeHiddenNoDisplay,
    bool strictFiltering
);

// A stamp for every directory Build() would scan, as of right now.
std::vector<DirStamp> CurrentDirStamps();

// True if `stamps` (as returned by a previous CurrentDirStamps() call,
// normally the one saved alongside a cache) still matches reality -
// i.e. Build() would produce the same result, so there's no need to
// re-scan/re-parse anything.
bool IsFresh(
    const std::vector<DirStamp>& stamps
);

// Cache location: $XDG_CACHE_HOME/kohiko/app_index.cache (see Xdg.h).
bool LoadCache(
    std::vector<IndexedApp>& outApps,
    std::vector<DirStamp>& outStamps
);

void SaveCache(
    const std::vector<IndexedApp>& apps,
    const std::vector<DirStamp>& stamps
);

}

}
