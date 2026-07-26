#pragma once

#include <cstdio>
#include <string>

// Minimal helpers for producing the JSON text the IPC protocol
// returns (kohikoctl clients / monitors / activewindow / tree).
//
// This is deliberately not a general purpose JSON library: Kohiko
// never needs to *parse* JSON, only emit small flat objects, so a
// full parser/DOM would just be dead weight in a "lightweight" WM.
namespace Kohiko::Json
{

inline std::string Escape(const std::string& value)
{
    std::string out;
    out.reserve(value.size() + 2);

    for (char c : value)
    {
        switch (c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;

            default:

                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    out += buf;
                }
                else
                {
                    out += c;
                }
        }
    }

    return out;
}

inline std::string String(const std::string& value)
{
    return "\"" + Escape(value) + "\"";
}

inline std::string Boolean(bool value)
{
    return value ? "true" : "false";
}

}
