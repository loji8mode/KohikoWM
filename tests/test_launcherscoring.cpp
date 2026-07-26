// Standalone correctness test for the launcher's ranking engine - no
// X11 display needed, since LauncherScoring only depends on plain
// strings/IndexedApp, never on an actual X connection. Covers the
// worked examples from the launcher spec directly (dev -> Developer
// Portal, VSC/GTE/LOW acronyms, ff/firfox -> Firefox, ...).
//
// Build & run: see the "test-launcherscoring" target in the Makefile.

#include "LauncherScoring.h"
#include "Utils.h"

#include <cassert>
#include <cstdio>

using namespace Kohiko;

namespace
{

int g_pass = 0;

void Check(bool condition, const char* what)
{
    if (condition)
    {
        ++g_pass;
        std::printf("  PASS: %s\n", what);
    }
    else
    {
        std::printf("  FAIL: %s\n", what);
        std::exit(1);
    }
}

IndexedApp MakeApp(const std::string& name)
{
    IndexedApp app;
    app.name = name;
    app.desktopId = name;
    return app;
}

// Scores a single app, in isolation, against `query` - exactly what
// ApplicationProvider-equivalent code in Launcher.cpp does per app,
// per keystroke, just without the surrounding index/bonuses.
int ScoreOf(const std::string& name, const std::string& query, Scoring::Bonuses bonuses = {})
{
    std::vector<IndexedApp> apps = {MakeApp(name)};
    Scoring::PrepareForSearch(apps);
    return Scoring::ScoreApp(apps[0], Utils::Lower(query), bonuses);
}

}

int main()
{
    std::printf("-- Exact / prefix / word-prefix matching --\n");

    Check(ScoreOf("Firefox", "Firefox") > Scoring::kNoMatch, "exact match");
    Check(ScoreOf("Firefox", "Fire") > Scoring::kNoMatch, "prefix match");
    Check(ScoreOf("Developer Portal", "dev") > Scoring::kNoMatch, "word-prefix match: dev -> Developer Portal");
    Check(ScoreOf("Firefox", "Firefox") > ScoreOf("Firefox Developer Edition", "Firefox"),
          "an exact match outranks a same-prefix match on a longer name");

    std::printf("\n-- Acronym / CamelCase matching --\n");

    Check(ScoreOf("Visual Studio Code", "VSC") > Scoring::kNoMatch, "acronym: VSC -> Visual Studio Code");
    Check(ScoreOf("GNOME Text Editor", "GTE") > Scoring::kNoMatch, "acronym: GTE -> GNOME Text Editor");
    Check(ScoreOf("LibreOffice Writer", "LOW") > Scoring::kNoMatch,
          "acronym across an internal CamelCase boundary: LOW -> LibreOffice Writer");
    Check(ScoreOf("Visual Studio Code", "VC") > Scoring::kNoMatch,
          "CamelCase subsequence match, skipping a hump: VC -> Visual Studio Code");
    Check(ScoreOf("Visual Studio Code", "VSC") > ScoreOf("Visual Studio Code", "VC"),
          "a full acronym match outranks a skip-some CamelCase match");

    std::printf("\n-- Substring / subsequence matching --\n");

    Check(ScoreOf("LibreOffice Writer", "office") > Scoring::kNoMatch, "substring match");
    Check(ScoreOf("Firefox", "ff") > Scoring::kNoMatch, "subsequence match: ff -> Firefox");

    std::printf("\n-- Fuzzy (Levenshtein) matching --\n");

    Check(ScoreOf("Firefox", "firfox") > Scoring::kNoMatch, "fuzzy match (one dropped letter): firfox -> Firefox");
    Check(ScoreOf("Firefox", "xkcdxkcd") == Scoring::kNoMatch, "unrelated garbage still excluded, not fuzzy-matched");

    std::printf("\n-- Non-matches are excluded, not just ranked low --\n");

    Check(ScoreOf("Firefox", "zzzznomatchzzzz") == Scoring::kNoMatch, "a query matching nothing returns kNoMatch");

    std::printf("\n-- Bonuses (history/popularity/recency) --\n");

    Scoring::Bonuses noBonus{};
    Scoring::Bonuses withBonus{.launchCountBonus = 100};

    Check(ScoreOf("Firefox", "fire", withBonus) > ScoreOf("Firefox", "fire", noBonus),
          "launch-count bonus increases a matching app's score");

    Check(ScoreOf("Firefox", "", withBonus) > ScoreOf("Firefox", "", noBonus),
          "an empty query still ranks purely by bonuses (frecency), matching every app");

    std::printf("\n-- Field search beyond Name (bullet 9) --\n");

    {
        IndexedApp app = MakeApp("Code");
        app.keywords = {"editor", "ide"};
        app.genericName = "Text Editor";
        app.comment = "Edit and compile source code";
        std::vector<IndexedApp> apps = {app};
        Scoring::PrepareForSearch(apps);

        Check(Scoring::ScoreApp(apps[0], "ide", {}) > Scoring::kNoMatch, "matches via Keywords");
        Check(Scoring::ScoreApp(apps[0], "editor", {}) > Scoring::kNoMatch, "matches via GenericName/Keywords");
        Check(Scoring::ScoreApp(apps[0], "compile", {}) > Scoring::kNoMatch, "matches via Comment");
    }

    std::printf("\nALL %d CHECKS PASSED.\n", g_pass);
    return 0;
}
