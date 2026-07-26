#include "Font.h"

#include <fontconfig/fontconfig.h>

namespace Kohiko
{

namespace
{

// Decodes one UTF-8 codepoint starting at byte offset `pos`, writing
// the byte offset just past it to `nextPos`. Falls back to treating
// `pos` as a single raw byte (advancing by one) for anything that
// isn't valid UTF-8, so a corrupt/truncated string still makes
// forward progress instead of looping forever.
unsigned int DecodeUtf8(
    const std::string& text,
    std::size_t pos,
    std::size_t& nextPos)
{
    unsigned char lead = static_cast<unsigned char>(text[pos]);

    int extraBytes;
    unsigned int codepoint;

    if      ((lead & 0x80) == 0x00) { extraBytes = 0; codepoint = lead; }
    else if ((lead & 0xE0) == 0xC0) { extraBytes = 1; codepoint = lead & 0x1F; }
    else if ((lead & 0xF0) == 0xE0) { extraBytes = 2; codepoint = lead & 0x0F; }
    else if ((lead & 0xF8) == 0xF0) { extraBytes = 3; codepoint = lead & 0x07; }
    else
    {
        nextPos = pos + 1;
        return lead;
    }

    if (pos + static_cast<std::size_t>(extraBytes) >= text.size())
    {
        nextPos = pos + 1;
        return lead;
    }

    for (int i = 1; i <= extraBytes; ++i)
    {
        unsigned char continuation = static_cast<unsigned char>(text[pos + static_cast<std::size_t>(i)]);

        if ((continuation & 0xC0) != 0x80)
        {
            nextPos = pos + 1;
            return lead;
        }

        codepoint = (codepoint << 6) | (continuation & 0x3F);
    }

    nextPos = pos + static_cast<std::size_t>(extraBytes) + 1;
    return codepoint;
}

}

Font::~Font()
{
    Unload();
}

bool Font::Load(
    Display* display,
    int screen,
    const std::string& pattern)
{
    Unload();

    m_display = display;
    m_screen = screen;

    m_primary = XftFontOpenName(display, screen, pattern.c_str());

    return m_primary != nullptr;
}

void Font::Unload()
{
    if (m_display)
    {
        for (XftFont* fallback : m_fallbacks)
            XftFontClose(m_display, fallback);

        if (m_primary)
            XftFontClose(m_display, m_primary);
    }

    m_fallbacks.clear();
    m_primary = nullptr;
}

bool Font::IsLoaded() const
{
    return m_primary != nullptr;
}

int Font::Ascent() const
{
    return m_primary ? m_primary->ascent : 0;
}

int Font::Height() const
{
    return m_primary ? m_primary->height : 0;
}

XftFont* Font::FontFor(
    unsigned int codepoint) const
{
    if (m_primary && XftCharExists(m_display, m_primary, codepoint))
        return m_primary;

    for (XftFont* fallback : m_fallbacks)
    {
        if (XftCharExists(m_display, fallback, codepoint))
            return fallback;
    }

    if (!m_primary)
        return nullptr;

    // Ask fontconfig for whatever installed font actually covers this
    // one codepoint - the same technique dwm's Xft fallback patch
    // uses. FcConfigSubstitute()/FcDefaultSubstitute() apply the
    // user's normal fontconfig rules; adding FC_CHARSET is what makes
    // glyph coverage the deciding factor even though the duplicated
    // pattern still nominally asks for the *primary* font's family -
    // fontconfig's own match ordering weighs charset coverage above
    // family, so it reliably picks a different family (a CJK font, a
    // Cyrillic-friendly one, ...) exactly when it needs to.
    FcCharSet* charset = FcCharSetCreate();
    FcCharSetAddChar(charset, static_cast<FcChar32>(codepoint));

    FcPattern* pattern = FcPatternDuplicate(m_primary->pattern);
    FcPatternAddCharSet(pattern, FC_CHARSET, charset);

    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result{};
    FcPattern* match = XftFontMatch(m_display, m_screen, pattern, &result);

    FcCharSetDestroy(charset);
    FcPatternDestroy(pattern);

    if (!match)
        return m_primary;

    // XftFontOpenPattern() takes ownership of `match` either way -
    // opens it on success, frees it on failure - so `match` must never
    // be FcPatternDestroy()'d here regardless of what comes back.
    XftFont* fallback = XftFontOpenPattern(m_display, match);

    if (!fallback)
        return m_primary;

    m_fallbacks.push_back(fallback);

    return XftCharExists(m_display, fallback, codepoint) ? fallback : m_primary;
}

int Font::TextWidth(
    const std::string& utf8Text) const
{
    if (!m_primary || utf8Text.empty())
        return 0;

    int width = 0;
    std::size_t pos = 0;

    while (pos < utf8Text.size())
    {
        std::size_t next;
        unsigned int codepoint = DecodeUtf8(utf8Text, pos, next);

        XftFont* font = FontFor(codepoint);

        XGlyphInfo extents{};

        XftTextExtentsUtf8(
            m_display,
            font,
            reinterpret_cast<const FcChar8*>(utf8Text.data() + pos),
            static_cast<int>(next - pos),
            &extents);

        width += extents.xOff;
        pos = next;
    }

    return width;
}

void Font::DrawString(
    XftDraw* draw,
    int x,
    int baseline,
    const std::string& utf8Text,
    const XftColor& color) const
{
    if (!m_primary || !draw || utf8Text.empty())
        return;

    std::size_t pos = 0;

    while (pos < utf8Text.size())
    {
        std::size_t next;
        unsigned int codepoint = DecodeUtf8(utf8Text, pos, next);

        XftFont* font = FontFor(codepoint);

        const FcChar8* bytes = reinterpret_cast<const FcChar8*>(utf8Text.data() + pos);
        int len = static_cast<int>(next - pos);

        XftDrawStringUtf8(draw, &color, font, x, baseline, bytes, len);

        XGlyphInfo extents{};
        XftTextExtentsUtf8(m_display, font, bytes, len, &extents);

        x += extents.xOff;
        pos = next;
    }
}

TextColor::TextColor(
    Display* display,
    Visual* visual,
    Colormap colormap,
    unsigned long rgbHex)
    :
    m_display(display),
    m_visual(visual),
    m_colormap(colormap)
{
    XRenderColor renderColor{};

    // The rest of this codebase already treats "0xRRGGBB" config
    // values as literal X pixel values, valid only on a TrueColor/
    // DirectColor display (see bar.background/etc.'s doc comment in
    // config/default.conf) - pulling the same three byte fields back
    // out here and scaling each from 0..255 to 0..65535 (*0x101, since
    // 0xFF*0x101==0xFFFF) keeps every text color pixel-identical to
    // the rest of the UI instead of introducing a second, subtly
    // different color path.
    renderColor.red   = static_cast<unsigned short>(((rgbHex >> 16) & 0xFF) * 0x101);
    renderColor.green = static_cast<unsigned short>(((rgbHex >> 8)  & 0xFF) * 0x101);
    renderColor.blue  = static_cast<unsigned short>((rgbHex         & 0xFF) * 0x101);
    renderColor.alpha = 0xFFFF;

    XftColorAllocValue(m_display, m_visual, m_colormap, &renderColor, &m_color);
}

TextColor::~TextColor()
{
    XftColorFree(m_display, m_visual, m_colormap, &m_color);
}

const XftColor& TextColor::Get() const
{
    return m_color;
}

}
