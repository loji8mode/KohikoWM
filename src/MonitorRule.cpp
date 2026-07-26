#include "MonitorRule.h"

#include "Config.h"
#include "Utils.h"

namespace Kohiko
{

bool MonitorRule::Parse(
    const std::string& text,
    MonitorRule& out)
{
    std::vector<std::string> tokens;

    {
        std::string current;

        for (char c : text)
        {
            if (c == ',')
            {
                tokens.push_back(Utils::Trim(current));
                current.clear();
            }
            else
            {
                current += c;
            }
        }

        tokens.push_back(Utils::Trim(current));
    }

    if (tokens.empty() || tokens.front().empty())
        return false;

    MonitorRule parsed;
    parsed.outputName = tokens.front();

    for (std::size_t i = 1; i < tokens.size(); ++i)
    {
        const std::string& token = tokens[i];
        auto equals = token.find('=');

        if (equals == std::string::npos || equals == 0)
            continue;

        std::string key   = Utils::Lower(Utils::Trim(token.substr(0, equals)));
        std::string value = Utils::Trim(token.substr(equals + 1));

        if (value.empty())
            continue;

        if (key == "workspace")
        {
            try
            {
                int id = std::stoi(value);

                if (id >= 1)
                    parsed.workspace = id;
            }
            catch (...)
            {
                // Silently ignored, same as an unrecognised key - a
                // typo here shouldn't take down config loading.
            }
        }

        // Unknown keys are deliberately ignored, not rejected - see
        // the class comment on why.
    }

    out = parsed;
    return true;
}

std::vector<MonitorRule> LoadMonitorRules(
    const Config& config)
{
    std::vector<MonitorRule> rules;

    for (const std::string& entry : config.GetAll("monitor"))
    {
        MonitorRule rule;

        if (MonitorRule::Parse(entry, rule))
            rules.push_back(rule);
    }

    return rules;
}

}
