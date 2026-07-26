#pragma once

#include <X11/Xft/Xft.h>

#include <string>
#include <vector>

namespace Kohiko
{

// Xft/fontconfig-backed text rendering, shared by Bar/Launcher/Notepad
// - the one place this codebase uses anything beyond plain libX11
// (see each of those classes' header comments for why, and the
// README's "Text rendering" section for the classic-core-font
// approach this replaced).
//
// The single feature this exists for is automatic per-character font
// fallback: `general.font=` only has to name ONE font (whatever looks
// right for the scripts someone mostly types), and any character it
// doesn't cover - Cyrillic on a Latin-only font, CJK on almost any
// font, ... - is looked up against the system's installed fonts via
// fontconfig and drawn with whichever one actually has that glyph,
// instead of silently turning into a missing-glyph "tofu" box. This
// is the same technique dwm's own well-known Xft-fallback patch uses.
class Font
{
public:

    Font() = default;
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    // `pattern` is a fontconfig pattern - e.g. "monospace:pixelsize=14"
    // - NOT an XLFD name (this is Xft, not the old XCreateFontSet).
    // Safe to call again later (e.g. on `reload`); replaces whatever
    // was loaded before, including any cached fallback fonts.
    bool Load(
        Display* display,
        int screen,
        const std::string& pattern
    );

    void Unload();

    bool IsLoaded() const;

    int Ascent() const;
    int Height() const;

    // Advance width of `utf8Text` if it were drawn with DrawString()
    // below - walks the same per-character fallback logic, so a
    // string mixing scripts measures exactly as wide as it will draw.
    int TextWidth(
        const std::string& utf8Text
    ) const;

    void DrawString(
        XftDraw* draw,
        int x,
        int baseline,
        const std::string& utf8Text,
        const XftColor& color
    ) const;

private:

    // Whichever loaded font (primary or a cached fallback) actually
    // has a glyph for `codepoint`, loading and caching a new fallback
    // via fontconfig charset matching if none does yet. Returns the
    // primary font if fontconfig can't find anything better, so an
    // unsupported codepoint still draws *something* (typically a tofu
    // box) rather than being silently skipped.
    XftFont* FontFor(
        unsigned int codepoint
    ) const;

private:

    Display* m_display = nullptr;
    int m_screen = 0;

    XftFont* m_primary = nullptr;
    mutable std::vector<XftFont*> m_fallbacks;

};

// Builds an XftColor straight from the same "0xRRGGBB" values already
// used everywhere else in this codebase (bar.background,
// bar.foreground, general.border_color_active, ...) instead of
// needing a whole separate color-configuration path just for text -
// see the .cpp for why this stays pixel-identical to the rest of the
// UI. Frees itself on destruction; cheap enough to build per DrawText()
// call rather than caching (see Bar::DrawText()).
class TextColor
{
public:

    TextColor(
        Display* display,
        Visual* visual,
        Colormap colormap,
        unsigned long rgbHex
    );

    ~TextColor();

    TextColor(const TextColor&) = delete;
    TextColor& operator=(const TextColor&) = delete;

    const XftColor& Get() const;

private:

    Display* m_display;
    Visual* m_visual;
    Colormap m_colormap;
    XftColor m_color{};

};

}
