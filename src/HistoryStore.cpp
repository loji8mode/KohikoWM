#include "HistoryStore.h"

#include "Xdg.h"

#include <chrono>
#include <cmath>
#include <fstream>

namespace Kohiko
{

namespace
{

std::filesystem::path HistoryFilePath()
{
    return Xdg::DataDir() / "launch_history";
}

long long NowEpochSeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

}

HistoryStore::HistoryStore()
{
    Load();
}

void HistoryStore::Load()
{
    m_records.clear();

    std::ifstream file(HistoryFilePath());

    std::string desktopId;
    int launchCount;
    long long lastLaunchEpoch;
    double favoriteScore;

    // Desktop IDs (filenames, effectively) never contain whitespace in
    // practice, so a plain whitespace-delimited format is fine here -
    // no need for the escaping AppIndex's cache format uses for
    // arbitrary free-text fields like Name or Comment.
    while (file >> desktopId >> launchCount >> lastLaunchEpoch >> favoriteScore)
    {
        m_records[desktopId] = HistoryRecord{launchCount, lastLaunchEpoch, favoriteScore};
    }
}

void HistoryStore::Save() const
{
    std::ofstream file(HistoryFilePath(), std::ios::trunc);

    for (const auto& [desktopId, record] : m_records)
    {
        file
            << desktopId << ' '
            << record.launchCount << ' '
            << record.lastLaunchEpoch << ' '
            << record.favoriteScore << '\n';
    }
}

void HistoryStore::RecordLaunch(
    const std::string& desktopId)
{
    HistoryRecord& record = m_records[desktopId]; // default-constructed if this is the first launch

    long long now = NowEpochSeconds();

    if (record.lastLaunchEpoch > 0 && now > record.lastLaunchEpoch)
    {
        // A simple frecency decay, the same idea browsers/shells use:
        // halve the running score for roughly every 10 days of
        // inactivity, so a handful of launches from months ago don't
        // permanently outrank something used constantly this week.
        double daysSinceLastLaunch = static_cast<double>(now - record.lastLaunchEpoch) / 86400.0;
        record.favoriteScore *= std::pow(0.5, daysSinceLastLaunch / 10.0);
    }

    record.favoriteScore += 1.0;
    record.launchCount += 1;
    record.lastLaunchEpoch = now;

    Save();
}

const HistoryRecord* HistoryStore::Find(
    const std::string& desktopId) const
{
    auto it = m_records.find(desktopId);
    return it == m_records.end() ? nullptr : &it->second;
}

}
