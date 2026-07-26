#pragma once

#include "AppIndex.h"

#include <string>
#include <vector>

// The launcher's ranking engine: everything from "does this query
// match this text at all" up to "what's this application's final
// combined score". Kept free of any Launcher/X11/Config dependency so
// it can be exercised directly by tests/test_launcherscoring.cpp - see
// that file for the worked examples from the task brief (dev ->
// Developer Portal, VSC -> Visual Studio Code, LOW -> LibreOffice
// Writer, ff -> Firefox, firfox -> Firefox, ...).
namespace Kohiko::Scoring
{

// Returned by the single-field matchers below when `query` doesn't
// match `field` under that particular algorithm at all.
constexpr int kNoMatch = -1;

// --- individual match algorithms ---------------------------------
//
// Every one of these takes already-lower-cased strings (see
// PrepareForSearch() / IndexedApp's `*Lower` members) and returns a
// score on a shared, comparable scale, highest = best, so a caller
// can freely compare results from different algorithms against each
// other. Each implements exactly one bullet from the task's SEARCH
// ENGINE section - kept as separate, individually testable functions
// rather than one large tangled routine.

// Bullet 1: `query` and `field` are identical.
int ExactMatch(const std::string& queryLower, const std::string& fieldLower);

// Bullet 2: `field` starts with `query`.
int PrefixMatch(const std::string& queryLower, const std::string& fieldLower);

// Bullet 3: some whitespace-delimited word inside `field` starts with
// `query` - e.g. "dev" matching the second word of "Developer Portal".
int WordPrefixMatch(const std::string& queryLower, const std::string& fieldLower);

// Same, but for a hot per-keystroke loop that already has `field`'s
// word-split precomputed (see SplitWords()/BestFieldMatch() below) -
// avoids re-splitting the same field on every keystroke.
int WordPrefixMatch(const std::string& queryLower, const std::vector<std::string>& wordsLower);

// Bullet 6: `query` appears anywhere inside `field` as one contiguous run.
int SubstringMatch(const std::string& queryLower, const std::string& fieldLower);

// Bullet 7: every character of `query` appears somewhere in `field`,
// in the same order, not necessarily contiguously - e.g. "ff" matching
// "Firefox" (there is no "ff" substring in "Firefox", but its two f's
// still appear in order).
int SubsequenceMatch(const std::string& queryLower, const std::string& fieldLower);

// Bullet 8: `query` is within a small edit distance of `field` (or of
// its closest-length substring window) - e.g. "firfox" (one dropped
// letter) still matching "Firefox".
int FuzzyMatch(const std::string& queryLower, const std::string& fieldLower);

// Bullet 4: `query` matches the initials of `field`'s "humps" - both
// whitespace-separated words *and* internal CamelCase boundaries, so
// "LibreOffice Writer" produces the initials "low" (Libre-Office-
// Writer), not just "lw". Matches when `query` equals, or is a prefix
// of, those initials.
int AcronymMatch(const std::string& queryLower, const std::string& humpInitialsLower);

// Bullet 5: like AcronymMatch, but `query` only needs to appear as a
// *subsequence* of the hump initials rather than matching them from
// the start - e.g. "vc" against "Visual Studio Code"'s initials "vsc"
// (skipping the "s"). Deliberately scored below AcronymMatch: a
// caller typing initials in order is a stronger signal than one
// skipping some.
int CamelCaseMatch(const std::string& queryLower, const std::string& humpInitialsLower);

// The best score any of the above algorithms gives for this
// (query, field) pair, or kNoMatch if none of them match at all. This
// is what a caller normally wants - `field` might be a Name, a
// Comment, a Keyword, ... and the caller doesn't need to care which
// specific algorithm ends up explaining the match, only how good it
// is relative to other apps/fields.
int BestFieldMatch(const std::string& queryLower, const std::string& fieldLower);

// Same, but for a hot per-keystroke loop that already has `field`'s
// word-split precomputed - see SplitWords() below. Used by Launcher's
// $HOME file-search loop, where recomputing SplitWords() for every
// file on every keystroke was profiled as the dominant cost of a
// query (see Launcher.cpp's FileEntry/ScanHomeFiles).
int BestFieldMatch(const std::string& queryLower, const std::string& fieldLower, const std::vector<std::string>& wordsLower);

// --- shared helpers, exposed for testing and reuse ----------------

// Splits an already-lower-cased field into "words" the way a person
// reading it out loud would - not just whitespace (e.g. "disk-usage-
// analyzer" splits into three words at the hyphens too). Exposed so
// a caller with a large, unchanging field list can precompute this
// once per field instead of paying for it on every WordPrefixMatch/
// BestFieldMatch call.
std::vector<std::string> SplitWords(const std::string& fieldLower);

int LevenshteinDistance(const std::string& a, const std::string& b);

bool IsSubsequence(const std::string& queryLower, const std::string& textLower);

// Initials of every "hump" in `text` - each whitespace/punctuation-
// separated word, further split at every lowercase-to-uppercase (or
// run-of-uppercase-to-uppercase-followed-by-lowercase) boundary.
// Returned already lower-cased, ready to compare against a lower-cased
// query. Computed once per app by PrepareForSearch(), not per keystroke.
std::string HumpInitials(const std::string& text);

// --- whole-app scoring ---------------------------------------------

// Fills in every `*Lower`/`nameHumpInitials`/`*Lower` derived field on
// every app in `apps`, in place. Must be called once after AppIndex
// builds or loads a batch of apps and before any query is scored
// against them - every keystroke afterwards reads these precomputed
// fields instead of re-lowering/re-tokenizing raw text, which is what
// keeps UpdateMatches() comfortably under the task's ~10ms budget even
// with several hundred apps installed.
void PrepareForSearch(std::vector<IndexedApp>& apps);

// Extra, per-application signals folded into the final score
// alongside the text match itself - kept as one small struct so
// ApplicationProvider can gather them from Config/HistoryStore/
// AppRatings once per query without ScoreApp() itself needing to know
// where any of them come from.
struct Bonuses
{
    int launchCountBonus = 0;  // bullet 10 - "Launch history bonus"
    int popularityBonus = 0;   // bullet 11 - "Popularity bonus" (see AppRatings.h)
    int recencyBonus = 0;      // bullet 12 - "Recently launched bonus"
    int favoriteBonus = 0;     // HistoryStore's blended frecency-style "favorite score"
};

// Combines the best match across every searchable field (Name,
// GenericName, Comment, Exec, Keywords, Categories - bullet 9) with a
// small reward for matching in more than one of them, plus `bonuses`,
// into the single number results are finally sorted by. Returns
// kNoMatch if `queryLower` doesn't match any searchable field at all
// (the app should be excluded from results entirely, not just ranked
// low) - except when `queryLower` is empty, in which case every app
// matches trivially and this returns a pure bonuses+recency ranking
// (bullet: "list everything" for an empty query).
int ScoreApp(
    const IndexedApp& app,
    const std::string& queryLower,
    const Bonuses& bonuses
);

}
