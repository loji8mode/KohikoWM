#pragma once

#include <string>
#include <unordered_map>

// Persists how often and how recently each application has been
// launched from Kohiko's own launcher, so frequently- and recently-
// used apps naturally rank higher (see Scoring::Bonuses / ScoreApp()
// in LauncherScoring.h). Stored under Xdg::DataDir() - never /tmp,
// unlike the previous implementation, whose history vanished on every
// reboot and was, in principle, readable by any local user.
namespace Kohiko
{

struct HistoryRecord
{
    int launchCount = 0;
    long long lastLaunchEpoch = 0; // seconds since epoch, 0 = never launched
    double favoriteScore = 0.0;    // recency-weighted usage - see HistoryStore::RecordLaunch()
};

class HistoryStore
{
public:

    // Loads from disk immediately - cheap (one small flat file), so
    // there's no need to make this async the way index building is.
    HistoryStore();

    // Bumps `desktopId`'s launch count, timestamp and favorite score,
    // and saves immediately. A launch is a rare enough event (at most
    // a handful per minute) that writing one small file synchronously
    // here isn't worth the complexity of doing it any other way.
    void RecordLaunch(
        const std::string& desktopId
    );

    // nullptr if `desktopId` has never been launched from here.
    const HistoryRecord* Find(
        const std::string& desktopId
    ) const;

private:

    void Load();
    void Save() const;

    std::unordered_map<std::string, HistoryRecord> m_records;
};

}
