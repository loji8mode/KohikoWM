#include "WindowRule.h"

#include "Config.h"
#include "Utils.h"

namespace Kohiko
{

namespace
{

bool ContainsSubstring(
    const std::string& haystackLower,
    const std::string& patternLower)
{
    return haystackLower.find(patternLower) != std::string::npos;
}

}

bool WindowRule::Matches(
    const std::string& className,
    const std::string& instanceName,
    const std::string& title) const
{
    if (classPattern.empty() && instancePattern.empty() && titlePattern.empty())
        return false;

    if (!classPattern.empty() &&
        !ContainsSubstring(Utils::Lower(className), classPattern))
        return false;

    if (!instancePattern.empty() &&
        !ContainsSubstring(Utils::Lower(instanceName), instancePattern))
        return false;

    if (!titlePattern.empty() &&
        !ContainsSubstring(Utils::Lower(title), titlePattern))
        return false;

    return true;
}

bool WindowRule::Parse(
    const std::string& text,
    WindowRule& out)
{
    std::vector<std::string> tokens = Utils::SplitWhitespace(text);

    if (tokens.empty())
        return false;

    std::string actionText = Utils::Lower(tokens[0]);

    WindowRule parsed;

    if (actionText == "float")
        parsed.action = Action::Float;
    else if (actionText == "tile")
        parsed.action = Action::Tile;
    else if (actionText == "fullscreen")
        parsed.action = Action::Fullscreen;
    else if (actionText == "nofullscreen")
        parsed.action = Action::NoFullscreen;
    else if (actionText.rfind("workspace:", 0) == 0)
    {
        parsed.action = Action::Workspace;

        try
        {
            parsed.workspace = std::stoi(actionText.substr(std::string("workspace:").size()));
        }
        catch (...)
        {
            return false;
        }

        if (parsed.workspace < 1)
            return false;
    }
    else
        return false;

    bool haveSelector = false;

    for (std::size_t i = 1; i < tokens.size(); ++i)
    {
        const std::string& token = tokens[i];
        auto colon = token.find(':');

        if (colon == std::string::npos || colon == 0)
            continue;

        std::string key   = Utils::Lower(token.substr(0, colon));
        std::string value = Utils::Lower(token.substr(colon + 1));

        if (value.empty())
            continue;

        if (key == "class")
        {
            parsed.classPattern = value;
            haveSelector = true;
        }
        else if (key == "instance")
        {
            parsed.instancePattern = value;
            haveSelector = true;
        }
        else if (key == "title")
        {
            parsed.titlePattern = value;
            haveSelector = true;
        }
    }

    if (!haveSelector)
        return false;

    out = parsed;
    return true;
}

std::vector<WindowRule> LoadWindowRules(
    const Config& config)
{
    std::vector<WindowRule> rules;

    for (const std::string& entry : config.GetAll("windowrule"))
    {
        WindowRule rule;

        if (WindowRule::Parse(entry, rule))
            rules.push_back(rule);
    }

    return rules;
}

}
