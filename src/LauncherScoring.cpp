#include "LauncherScoring.h"

#include "Utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>

namespace Kohiko::Scoring
{

namespace
{

// Splits an already-lower-cased field into "words" the way a person
// reading it out loud would - not just whitespace (SplitWhitespace()
// alone would miss "Developer Portal"'s hyphen/underscore-joined
// cousins, e.g. "disk-usage-analyzer").
std::vector<std::string> SplitWords(
    const std::string& fieldLower)
{
    std::vector<std::string> words;
    std::string current;

    for (char c : fieldLower)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
        {
            current += c;
            continue;
        }

        if (!current.empty())
        {
            words.push_back(std::move(current));
            current.clear();
        }
    }

    if (!current.empty())
        words.push_back(std::move(current));

    return words;
}

}

int ExactMatch(
    const std::string& queryLower,
    const std::string& fieldLower)
{
    if (queryLower.empty() || fieldLower.empty())
        return kNoMatch;

    return fieldLower == queryLower ? 1000 : kNoMatch;
}

int PrefixMatch(
    const std::string& queryLower,
    const std::string& fieldLower)
{
    if (queryLower.empty() || fieldLower.size() <= queryLower.size())
        return kNoMatch; // equal length is Exact's job, not this one's

    if (!fieldLower.starts_with(queryLower))
        return kNoMatch;

    int overflow = static_cast<int>(fieldLower.size() - queryLower.size());
    return std::max(820, 900 - overflow);
}

int WordPrefixMatch(
    const std::string& queryLower,
    const std::string& fieldLower)
{
    if (queryLower.empty())
        return kNoMatch;

    std::vector<std::string> words = SplitWords(fieldLower);

    for (std::size_t i = 0; i < words.size(); ++i)
    {
        if (words[i].size() > queryLower.size() && words[i].starts_with(queryLower))
            return std::max(700, 800 - static_cast<int>(i) * 20);
    }

    return kNoMatch;
}

int SubstringMatch(
    const std::string& queryLower,
    const std::string& fieldLower)
{
    if (queryLower.empty())
        return kNoMatch;

    std::size_t pos = fieldLower.find(queryLower);

    if (pos == std::string::npos)
        return kNoMatch;

    return std::max(500, 620 - static_cast<int>(pos));
}

bool IsSubsequence(
    const std::string& queryLower,
    const std::string& textLower)
{
    std::size_t q = 0;

    for (char c : textLower)
    {
        if (q < queryLower.size() && c == queryLower[q])
            ++q;
    }

    return q == queryLower.size();
}

int SubsequenceMatch(
    const std::string& queryLower,
    const std::string& fieldLower)
{
    if (queryLower.empty())
        return kNoMatch;

    int firstIndex = -1;
    int lastIndex = -1;
    std::size_t q = 0;

    for (std::size_t i = 0; i < fieldLower.size() && q < queryLower.size(); ++i)
    {
        if (fieldLower[i] == queryLower[q])
        {
            if (firstIndex < 0)
                firstIndex = static_cast<int>(i);

            lastIndex = static_cast<int>(i);
            ++q;
        }
    }

    if (q != queryLower.size())
        return kNoMatch;

    // How much wider the actual match span is than the tightest
    // possible packing of `query`'s characters - "ff" matching the
    // two f's right next to each other in a field scores better than
    // matching two f's fifteen characters apart in a longer one.
    int span = lastIndex - firstIndex + 1;
    int extra = span - static_cast<int>(queryLower.size());

    return std::max(150, 350 - extra * 10);
}

int LevenshteinDistance(
    const std::string& a,
    const std::string& b)
{
    // Single-row DP - O(min(|a|,|b|)) memory instead of the naive
    // O(|a|*|b|) table, which matters only in that this can run once
    // per (query, field) pair on every keystroke; for the short
    // strings involved here (app names, a typed query) either would
    // be fast, but there's no reason not to take the cheaper one.
    const std::string& shorter = a.size() <= b.size() ? a : b;
    const std::string& longer  = a.size() <= b.size() ? b : a;

    std::vector<int> previousRow(shorter.size() + 1);

    for (std::size_t i = 0; i <= shorter.size(); ++i)
        previousRow[i] = static_cast<int>(i);

    for (std::size_t j = 1; j <= longer.size(); ++j)
    {
        std::vector<int> currentRow(shorter.size() + 1);
        currentRow[0] = static_cast<int>(j);

        for (std::size_t i = 1; i <= shorter.size(); ++i)
        {
            int deletionCost     = previousRow[i] + 1;
            int insertionCost    = currentRow[i - 1] + 1;
            int substitutionCost = previousRow[i - 1] + (shorter[i - 1] == longer[j - 1] ? 0 : 1);

            currentRow[i] = std::min({deletionCost, insertionCost, substitutionCost});
        }

        previousRow = std::move(currentRow);
    }

    return previousRow[shorter.size()];
}

int FuzzyMatch(
    const std::string& queryLower,
    const std::string& fieldLower)
{
    // Single characters fuzzy-match almost anything at distance 1,
    // which is just noise - fuzzy search only kicks in once there's
    // enough of a query to make an edit-distance comparison mean
    // something (bullet 8's own example, "firfox", is 6 characters).
    if (queryLower.size() < 3 || fieldLower.empty())
        return kNoMatch;

    // A query and a field whose lengths are already wildly different
    // can't plausibly be "one or two typos apart" - skip the DP
    // entirely rather than spend time confirming that.
    int lengthGap = std::abs(static_cast<int>(fieldLower.size()) - static_cast<int>(queryLower.size()));
    int maxDistance = std::max(1, static_cast<int>(queryLower.size()) / 3);

    if (lengthGap > maxDistance + 2)
        return kNoMatch;

    int distance = LevenshteinDistance(queryLower, fieldLower);

    if (distance > maxDistance)
        return kNoMatch;

    return std::max(150, 400 - distance * 60);
}

int AcronymMatch(
    const std::string& queryLower,
    const std::string& humpInitialsLower)
{
    if (queryLower.size() < 2 || humpInitialsLower.empty())
        return kNoMatch;

    if (humpInitialsLower == queryLower)
        return 780;

    if (humpInitialsLower.size() > queryLower.size() && humpInitialsLower.starts_with(queryLower))
        return 750;

    return kNoMatch;
}

int CamelCaseMatch(
    const std::string& queryLower,
    const std::string& humpInitialsLower)
{
    if (queryLower.size() < 2 || humpInitialsLower.empty())
        return kNoMatch;

    // Already scored (higher) by AcronymMatch - don't award it twice.
    if (humpInitialsLower == queryLower || humpInitialsLower.starts_with(queryLower))
        return kNoMatch;

    if (!IsSubsequence(queryLower, humpInitialsLower))
        return kNoMatch;

    return 700;
}

int BestFieldMatch(
    const std::string& queryLower,
    const std::string& fieldLower)
{
    if (queryLower.empty() || fieldLower.empty())
        return kNoMatch;

    int best = kNoMatch;

    auto consider = [&best](int candidate)
    {
        if (candidate != kNoMatch && candidate > best)
            best = candidate;
    };

    consider(ExactMatch(queryLower, fieldLower));
    consider(PrefixMatch(queryLower, fieldLower));
    consider(WordPrefixMatch(queryLower, fieldLower));
    consider(SubstringMatch(queryLower, fieldLower));
    consider(SubsequenceMatch(queryLower, fieldLower));
    consider(FuzzyMatch(queryLower, fieldLower));

    return best;
}

std::string HumpInitials(
    const std::string& text)
{
    std::string initials;
    bool atWordStart = true;

    for (std::size_t i = 0; i < text.size(); ++i)
    {
        unsigned char c = static_cast<unsigned char>(text[i]);

        if (!std::isalnum(c))
        {
            atWordStart = true;
            continue;
        }

        bool isUpper = std::isupper(c) != 0;

        bool prevLower = i > 0 &&
            std::islower(static_cast<unsigned char>(text[i - 1])) != 0;

        bool prevUpper = i > 0 &&
            std::isupper(static_cast<unsigned char>(text[i - 1])) != 0;

        bool nextLower = i + 1 < text.size() &&
            std::islower(static_cast<unsigned char>(text[i + 1])) != 0;

        // A hump starts at the beginning of a whitespace/punctuation-
        // delimited word, at a lowercase-to-uppercase transition
        // ("libreOffice" -> the 'O'), or at the last capital of a
        // run of capitals immediately followed by a lowercase letter
        // ("IOStream" -> the 'S', not the 'I' or 'O').
        bool humpStart = atWordStart ||
            (isUpper && prevLower) ||
            (isUpper && prevUpper && nextLower);

        if (humpStart)
            initials += static_cast<char>(std::tolower(c));

        atWordStart = false;
    }

    return initials;
}

void PrepareForSearch(
    std::vector<IndexedApp>& apps)
{
    for (auto& app : apps)
    {
        app.nameLower = Utils::Lower(app.name);
        app.genericNameLower = Utils::Lower(app.genericName);
        app.commentLower = Utils::Lower(app.comment);
        app.execLower = Utils::Lower(app.execBinary.empty() ? app.exec : app.execBinary);
        app.nameHumpInitials = HumpInitials(app.name);

        app.keywordsLower.clear();
        app.keywordsLower.reserve(app.keywords.size());

        for (const auto& keyword : app.keywords)
            app.keywordsLower.push_back(Utils::Lower(keyword));

        app.categoriesLower.clear();
        app.categoriesLower.reserve(app.categories.size());

        for (const auto& category : app.categories)
            app.categoriesLower.push_back(Utils::Lower(category));
    }
}

namespace
{

// Field importance - how much a match in this field alone should
// count towards the final score, relative to a match in the
// application's Name (weight 1.0). Comment/Categories matter far less
// than Name/Keywords: almost every app's Comment is a generic
// one-line blurb that happens to share common words with dozens of
// unrelated apps ("a simple, fast..."), so it's kept as a tie-breaker
// rather than a strong signal.
constexpr float kNameWeight = 1.00f;
constexpr float kGenericNameWeight = 0.55f;
constexpr float kKeywordWeight = 0.65f;
constexpr float kExecWeight = 0.40f;
constexpr float kCategoryWeight = 0.30f;
constexpr float kCommentWeight = 0.22f;

// Reward for matching in more than one field at once (e.g. the query
// hits both the Name and a Keyword) - capped so a handful of weak
// secondary matches can never outweigh one strong primary one.
constexpr int kSecondaryFieldBonus = 18;
constexpr int kMaxSecondaryBonus = 70;

int BestOf(
    const std::string& queryLower,
    const std::vector<std::string>& fieldsLower)
{
    int best = kNoMatch;

    for (const auto& field : fieldsLower)
    {
        int candidate = BestFieldMatch(queryLower, field);

        if (candidate > best)
            best = candidate;
    }

    return best;
}

}

int ScoreApp(
    const IndexedApp& app,
    const std::string& queryLower,
    const Bonuses& bonuses)
{
    int bonusTotal =
        bonuses.launchCountBonus +
        bonuses.popularityBonus +
        bonuses.recencyBonus +
        bonuses.favoriteBonus;

    // Small, constant tie-breaker in favour of shorter/more direct
    // names, same reasoning the previous implementation used.
    int lengthPenalty = static_cast<int>(std::min<std::size_t>(app.name.size(), 40));

    if (queryLower.empty())
    {
        // Nothing to match against - list everything, ranked purely
        // by how much the person actually uses it (bullet: "list
        // everything" behaviour for an empty query, preserved from
        // the previous implementation).
        return bonusTotal - lengthPenalty;
    }

    // Name gets two extra shots on top of the plain text algorithms:
    // its precomputed acronym initials, tried both strictly
    // (AcronymMatch) and as a skip-tolerant subsequence
    // (CamelCaseMatch) - see bullets 4 and 5.
    int nameScore = BestFieldMatch(queryLower, app.nameLower);
    nameScore = std::max(nameScore, AcronymMatch(queryLower, app.nameHumpInitials));
    nameScore = std::max(nameScore, CamelCaseMatch(queryLower, app.nameHumpInitials));

    struct WeightedField
    {
        int score;
        float weight;
    };

    std::array<WeightedField, 5> fields =
    {
        WeightedField{nameScore, kNameWeight},
        WeightedField{BestFieldMatch(queryLower, app.genericNameLower), kGenericNameWeight},
        WeightedField{BestOf(queryLower, app.keywordsLower), kKeywordWeight},
        WeightedField{BestFieldMatch(queryLower, app.execLower), kExecWeight},
        WeightedField{BestOf(queryLower, app.categoriesLower), kCategoryWeight},
    };

    int commentScore = BestFieldMatch(queryLower, app.commentLower);

    int bestWeighted = kNoMatch;
    int matchedFieldCount = 0;

    for (const auto& field : fields)
    {
        if (field.score == kNoMatch)
            continue;

        ++matchedFieldCount;
        int weighted = static_cast<int>(static_cast<float>(field.score) * field.weight);

        if (weighted > bestWeighted)
            bestWeighted = weighted;
    }

    if (commentScore != kNoMatch)
    {
        ++matchedFieldCount;
        int weighted = static_cast<int>(static_cast<float>(commentScore) * kCommentWeight);
        bestWeighted = std::max(bestWeighted, weighted);
    }

    if (bestWeighted == kNoMatch)
        return kNoMatch; // query doesn't match this app under any searchable field (bullet 9) - exclude it entirely

    int secondaryBonus = std::min(
        kMaxSecondaryBonus,
        (matchedFieldCount - 1) * kSecondaryFieldBonus);

    return bestWeighted + secondaryBonus + bonusTotal - lengthPenalty;
}

}
