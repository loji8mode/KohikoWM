#include "AppIndex.h"

#include "DesktopEntry.h"
#include "IconResolver.h"
#include "Utils.h"
#include "Xdg.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace Kohiko::AppIndex
{

namespace
{

namespace fs = std::filesystem;

// ---------------------------------------------------------------
// Step 1: parse every .desktop file in every application directory,
// in priority order, dropping anything ShouldDisplay()/
// LooksLikeHelperEntry() says isn't a real, launchable application.
// ---------------------------------------------------------------

// freedesktop desktop-id: the path from the applications/ directory
// down to the file, with '/' replaced by '-' and ".desktop" dropped -
// e.g. applications/kde4/foo.desktop -> "kde4-foo". Nearly everything
// lives directly under applications/, where this is just the
// filename stem, but a handful of desktop environments do nest.
std::string ComputeDesktopId(
    const fs::path& base,
    const fs::path& file)
{
    fs::path relative = fs::relative(file, base);
    std::string id = relative.string();

    for (char& c : id)
    {
        if (c == '/')
            c = '-';
    }

    constexpr std::string_view suffix = ".desktop";

    if (id.size() > suffix.size())
        id.erase(id.size() - suffix.size());

    return id;
}

std::vector<DesktopEntry> ParseAllDesktopEntries(
    bool includeHiddenNoDisplay,
    bool strictFiltering)
{
    std::vector<DesktopEntry> entries;

    const auto dirs = Xdg::ApplicationDirs();

    for (std::size_t priority = 0; priority < dirs.size(); ++priority)
    {
        std::error_code ec;

        if (!fs::exists(dirs[priority], ec) || ec)
            continue;

        fs::recursive_directory_iterator it(dirs[priority], fs::directory_options::skip_permission_denied, ec);
        fs::recursive_directory_iterator end;

        for (; it != end && !ec; it.increment(ec))
        {
            const fs::path& path = it->path();

            if (path.extension() != ".desktop")
                continue;

            auto parsed = ParseDesktopFile(path, static_cast<int>(priority));

            if (!parsed)
                continue;

            parsed->desktopId = ComputeDesktopId(dirs[priority], path);

            if (!ShouldDisplay(*parsed, includeHiddenNoDisplay))
                continue;

            if (strictFiltering && LooksLikeHelperEntry(*parsed))
                continue;

            entries.push_back(std::move(*parsed));
        }
    }

    return entries;
}

// ---------------------------------------------------------------
// Step 2: collapse to one entry per desktop ID, keeping the one from
// the highest-priority directory - this is the standard freedesktop
// override mechanism (a user's ~/.local/share/applications/foo.desktop
// replaces the system one of the same name).
// ---------------------------------------------------------------
std::vector<DesktopEntry> DeduplicateByDesktopId(
    std::vector<DesktopEntry> entries)
{
    std::unordered_map<std::string, std::size_t> bestForId; // desktopId -> index into `entries`
    std::vector<DesktopEntry> result;

    for (auto& entry : entries)
    {
        auto [it, inserted] = bestForId.try_emplace(entry.desktopId, result.size());

        if (inserted)
        {
            result.push_back(std::move(entry));
            continue;
        }

        if (entry.sourcePriority < result[it->second].sourcePriority)
            result[it->second] = std::move(entry);
    }

    return result;
}

// A handful of Exec= binaries that dozens of unrelated applications
// are launched through (sandboxing wrappers, shell shims, interpreter
// front-ends) - matching on these directly would wrongly merge every
// Flatpak app, every shell script, etc. into "one application".
constexpr std::array<const char*, 9> kGenericExecWrappers =
{
    "flatpak", "snap", "sh", "bash", "env",
    "python3", "python", "gtk-launch", "sh -c",
};

bool IsGenericWrapper(
    const std::string& execBinaryLower)
{
    for (const char* wrapper : kGenericExecWrappers)
    {
        if (execBinaryLower == wrapper)
            return true;
    }

    return false;
}

// Ranks two candidates for "the same application" so DeduplicateAcrossIds()
// can decide which one to keep - lower sourcePriority (a more
// user-specific directory) always wins first; among equal-priority
// candidates, prefer the one with more useful metadata.
bool IsBetterCandidate(
    const DesktopEntry& candidate,
    const DesktopEntry& incumbent)
{
    if (candidate.sourcePriority != incumbent.sourcePriority)
        return candidate.sourcePriority < incumbent.sourcePriority;

    if (candidate.icon.empty() != incumbent.icon.empty())
        return incumbent.icon.empty();

    if (candidate.startupWMClass.empty() != incumbent.startupWMClass.empty())
        return incumbent.startupWMClass.empty();

    return candidate.desktopId.size() < incumbent.desktopId.size();
}

// ---------------------------------------------------------------
// Step 3: collapse different desktop IDs that nonetheless represent
// the same application - e.g. a distro packaging both
// "firefox.desktop" and a stale "firefox-esr.desktop" pointing at the
// same StartupWMClass/binary, or the "multiple Maps entries" bug the
// task calls out by name. Matched on StartupWMClass, then Exec
// binary (skipping generic wrappers), then exact lower-cased Name.
// ---------------------------------------------------------------
std::vector<DesktopEntry> DeduplicateAcrossIds(
    std::vector<DesktopEntry> entries)
{
    std::vector<DesktopEntry> accepted;

    std::unordered_map<std::string, std::size_t> byWmClass;
    std::unordered_map<std::string, std::size_t> byExecBinary;
    std::unordered_map<std::string, std::size_t> byName;

    for (auto& entry : entries)
    {
        std::string wmClass = Utils::Lower(entry.startupWMClass);
        std::string execBinary = Utils::Lower(entry.ExecBinary());
        std::string nameKey = Utils::Lower(entry.name);

        std::size_t* existingIndex = nullptr;

        if (!wmClass.empty())
        {
            if (auto it = byWmClass.find(wmClass); it != byWmClass.end())
                existingIndex = &it->second;
        }

        if (!existingIndex && !execBinary.empty() && !IsGenericWrapper(execBinary))
        {
            if (auto it = byExecBinary.find(execBinary); it != byExecBinary.end())
                existingIndex = &it->second;
        }

        if (!existingIndex && !nameKey.empty())
        {
            if (auto it = byName.find(nameKey); it != byName.end())
                existingIndex = &it->second;
        }

        if (existingIndex)
        {
            if (IsBetterCandidate(entry, accepted[*existingIndex]))
                accepted[*existingIndex] = std::move(entry);

            continue;
        }

        std::size_t newIndex = accepted.size();
        accepted.push_back(std::move(entry));

        if (!wmClass.empty()) byWmClass.emplace(wmClass, newIndex);
        if (!execBinary.empty() && !IsGenericWrapper(execBinary)) byExecBinary.emplace(execBinary, newIndex);
        if (!nameKey.empty()) byName.emplace(nameKey, newIndex);
    }

    return accepted;
}

// ---------------------------------------------------------------
// Step 4: a minimal AppStream reader - not a general XML parser, just
// enough tag-scraping to pull <id>/<name>/<summary> out of the simple,
// non-nested elements every metainfo.xml file has (see
// Xdg::AppStreamDirs()). Used to fill in a better Comment when a
// .desktop file's own is missing, and nothing else - deliberately
// small given how narrow that one use is.
// ---------------------------------------------------------------

struct AppStreamInfo
{
    std::string summary;
};

std::string ExtractTagContent(
    const std::string& xml,
    const std::string& tag)
{
    std::string openPrefix = "<" + tag;
    std::string closeTag = "</" + tag + ">";

    std::size_t openStart = xml.find(openPrefix);

    // Skip past any occurrence that isn't really this tag (e.g. "<id"
    // inside "<identifier") and any translated variant carrying an
    // xml:lang attribute - only the bare, untranslated tag is wanted.
    while (openStart != std::string::npos)
    {
        std::size_t afterName = openStart + openPrefix.size();
        char next = afterName < xml.size() ? xml[afterName] : '\0';

        std::size_t openEnd = xml.find('>', openStart);

        if (openEnd == std::string::npos)
            return "";

        bool exactTag = (next == '>' || next == ' ' || next == '/');
        bool translated = xml.substr(afterName, openEnd - afterName).find("xml:lang") != std::string::npos;

        if (exactTag && !translated)
        {
            std::size_t closeStart = xml.find(closeTag, openEnd);

            if (closeStart == std::string::npos)
                return "";

            return Utils::Trim(xml.substr(openEnd + 1, closeStart - openEnd - 1));
        }

        openStart = xml.find(openPrefix, openStart + 1);
    }

    return "";
}

std::string NormalizeAppStreamId(
    std::string id)
{
    constexpr std::string_view suffix = ".desktop";

    if (id.size() > suffix.size() &&
        id.compare(id.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        id.erase(id.size() - suffix.size());
    }

    return id;
}

std::unordered_map<std::string, AppStreamInfo> ScanAppStream()
{
    std::unordered_map<std::string, AppStreamInfo> result;

    for (const auto& dir : Xdg::AppStreamDirs())
    {
        std::error_code ec;

        if (!fs::exists(dir, ec) || ec)
            continue;

        for (const auto& entry : fs::directory_iterator(dir, ec))
        {
            if (ec)
                break;

            if (entry.path().extension() != ".xml")
                continue;

            std::ifstream file(entry.path());

            if (!file.is_open())
                continue;

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string xml = buffer.str();

            std::string id = ExtractTagContent(xml, "id");

            if (id.empty())
                continue;

            std::string summary = ExtractTagContent(xml, "summary");

            if (summary.empty())
                continue;

            result.emplace(NormalizeAppStreamId(id), AppStreamInfo{summary});
        }
    }

    return result;
}

// ---------------------------------------------------------------
// Step 5: turn the deduplicated DesktopEntry list into IndexedApp
// records - merging AppStream data and resolving each icon path.
// ---------------------------------------------------------------
std::vector<IndexedApp> Finalize(
    std::vector<DesktopEntry> entries)
{
    std::unordered_map<std::string, AppStreamInfo> appStream = ScanAppStream();
    IconResolver iconResolver;

    std::vector<IndexedApp> apps;
    apps.reserve(entries.size());

    for (auto& entry : entries)
    {
        IndexedApp app;
        app.desktopId = std::move(entry.desktopId);
        app.name = std::move(entry.name);
        app.genericName = std::move(entry.genericName);
        app.comment = std::move(entry.comment);
        app.execBinary = entry.ExecBinary(); // must run before entry.exec is moved out of, below
        app.exec = std::move(entry.exec);
        app.icon = std::move(entry.icon);
        app.startupWMClass = std::move(entry.startupWMClass);
        app.categories = std::move(entry.categories);
        app.keywords = std::move(entry.keywords);

        if (auto it = appStream.find(app.desktopId); it != appStream.end() && app.comment.empty())
            app.comment = it->second.summary;

        app.iconPath = iconResolver.Resolve(app.icon);

        apps.push_back(std::move(app));
    }

    std::sort(apps.begin(), apps.end(), [](const IndexedApp& a, const IndexedApp& b)
    {
        return Utils::Lower(a.name) < Utils::Lower(b.name);
    });

    return apps;
}

fs::path CacheFilePath()
{
    return Xdg::CacheDir() / "app_index.cache";
}

constexpr const char kFieldSep = '\x1f';
constexpr const char* kCacheVersion = "KOHIKO-APPINDEX 2";

// Cached fields never contain real newlines (desktop entry values are
// single-line) or the unit-separator byte in practice, but a rogue
// value here should degrade to "cache miss on next run" rather than
// corrupt the file - so any that do are sanitized away at write time.
std::string SanitizeField(
    std::string value)
{
    for (char& c : value)
    {
        if (c == '\n' || c == '\r' || c == kFieldSep)
            c = ' ';
    }

    return value;
}

std::string JoinList(
    const std::vector<std::string>& items)
{
    std::string joined;

    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (i > 0)
            joined += ';';

        joined += SanitizeField(items[i]);
    }

    return joined;
}

}

std::vector<IndexedApp> Build(
    bool includeHiddenNoDisplay,
    bool strictFiltering)
{
    std::vector<DesktopEntry> entries = ParseAllDesktopEntries(includeHiddenNoDisplay, strictFiltering);
    entries = DeduplicateByDesktopId(std::move(entries));
    entries = DeduplicateAcrossIds(std::move(entries));

    return Finalize(std::move(entries));
}

std::vector<DirStamp> CurrentDirStamps()
{
    std::vector<DirStamp> stamps;

    for (const auto& dir : Xdg::ApplicationDirs())
    {
        std::error_code ec;
        auto mtime = fs::last_write_time(dir, ec);

        // A missing directory is still a meaningful stamp (its
        // "mtime" is the default-constructed sentinel) - if it later
        // gets created, that's a real change and should invalidate
        // the cache just like any other modification would.
        stamps.push_back(DirStamp{dir.string(), ec ? fs::file_time_type{} : mtime});
    }

    return stamps;
}

bool IsFresh(
    const std::vector<DirStamp>& stamps)
{
    std::vector<DirStamp> current = CurrentDirStamps();

    if (current.size() != stamps.size())
        return false;

    for (std::size_t i = 0; i < current.size(); ++i)
    {
        if (current[i].path != stamps[i].path || current[i].mtime != stamps[i].mtime)
            return false;
    }

    return true;
}

bool LoadCache(
    std::vector<IndexedApp>& outApps,
    std::vector<DirStamp>& outStamps)
{
    std::ifstream file(CacheFilePath());

    if (!file.is_open())
        return false;

    std::string line;

    if (!std::getline(file, line) || line != kCacheVersion)
        return false;

    if (!std::getline(file, line))
        return false;

    std::size_t dirCount = 0;

    try { dirCount = std::stoul(line); }
    catch (...) { return false; }

    std::vector<DirStamp> stamps;
    stamps.reserve(dirCount);

    for (std::size_t i = 0; i < dirCount; ++i)
    {
        if (!std::getline(file, line))
            return false;

        std::size_t sep = line.find(kFieldSep);

        if (sep == std::string::npos)
            return false;

        DirStamp stamp;
        stamp.path = line.substr(0, sep);

        try
        {
            long long ticks = std::stoll(line.substr(sep + 1));
            stamp.mtime = fs::file_time_type(fs::file_time_type::duration(ticks));
        }
        catch (...) { return false; }

        stamps.push_back(std::move(stamp));
    }

    if (!std::getline(file, line))
        return false;

    std::size_t appCount = 0;

    try { appCount = std::stoul(line); }
    catch (...) { return false; }

    std::vector<IndexedApp> apps;
    apps.reserve(appCount);

    for (std::size_t i = 0; i < appCount; ++i)
    {
        if (!std::getline(file, line))
            return false;

        std::vector<std::string> fields;
        std::size_t pos = 0;

        while (true)
        {
            std::size_t sep = line.find(kFieldSep, pos);
            fields.push_back(line.substr(pos, sep == std::string::npos ? std::string::npos : sep - pos));

            if (sep == std::string::npos)
                break;

            pos = sep + 1;
        }

        if (fields.size() != 10)
            return false;

        IndexedApp app;
        app.desktopId = fields[0];
        app.name = fields[1];
        app.genericName = fields[2];
        app.comment = fields[3];
        app.exec = fields[4];
        app.icon = fields[5];
        app.iconPath = fields[6];
        app.startupWMClass = fields[7];
        app.categories = Utils::Split(fields[8], ';');
        app.keywords = Utils::Split(fields[9], ';');

        // execBinary isn't cached separately - it's a cheap,
        // deterministic function of Exec, re-derived here the same
        // way Finalize()/DesktopEntry::ExecBinary() compute it.
        std::vector<std::string> execTokens = Utils::SplitWhitespace(app.exec);
        std::string firstToken = execTokens.empty() ? "" : execTokens.front();

        if (!firstToken.empty() && (firstToken.front() == '"' || firstToken.front() == '\''))
            firstToken.erase(0, 1);

        app.execBinary = fs::path(firstToken).filename().string();

        apps.push_back(std::move(app));
    }

    outApps = std::move(apps);
    outStamps = std::move(stamps);
    return true;
}

void SaveCache(
    const std::vector<IndexedApp>& apps,
    const std::vector<DirStamp>& stamps)
{
    std::ofstream file(CacheFilePath(), std::ios::trunc);

    if (!file.is_open())
        return;

    file << kCacheVersion << '\n';
    file << stamps.size() << '\n';

    for (const auto& stamp : stamps)
        file << SanitizeField(stamp.path) << kFieldSep << stamp.mtime.time_since_epoch().count() << '\n';

    file << apps.size() << '\n';

    for (const auto& app : apps)
    {
        file
            << SanitizeField(app.desktopId) << kFieldSep
            << SanitizeField(app.name) << kFieldSep
            << SanitizeField(app.genericName) << kFieldSep
            << SanitizeField(app.comment) << kFieldSep
            << SanitizeField(app.exec) << kFieldSep
            << SanitizeField(app.icon) << kFieldSep
            << SanitizeField(app.iconPath) << kFieldSep
            << SanitizeField(app.startupWMClass) << kFieldSep
            << JoinList(app.categories) << kFieldSep
            << JoinList(app.keywords)
            << '\n';
    }
}

}
