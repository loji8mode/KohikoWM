#pragma once

#include "ConfigSchema.h"
#include "ConfigWriter.h"
#include "Font.h"
#include "Types.h"

#include <X11/Xlib.h>

#include <string>
#include <vector>

namespace Kohiko
{

// Kohiko Settings: the GUI half of "Configuration GUI" from the spec.
// Unlike every other Kohiko-drawn window (Bar/Launcher/Notepad/
// PowerMenu/LockScreen - all override-redirect windows owned by the
// window manager process itself), this is a completely ordinary
// top-level client application, in its own `kohiko-settings` process,
// that Kohiko (or any other WM) tiles/manages exactly like a
// terminal or browser - see tools/kohiko-settings.cpp and the
// installed .desktop entry. Same plain-Xlib-shapes-plus-Xft-text
// approach as the rest of the project though, and no GTK/Qt/toolkit
// dependency - see the README for why that matters here.
//
// Reads every scalar (single-value) key from ConfigSchema and shows
// it as an editable field grouped by category/group; the four
// repeatable directives (bind=/exec.<name>=/windowrule=/monitor=) and
// the dynamically-numbered workspace<N>= family are each shown as a
// small raw-syntax text block instead of pretending they're
// individually-typed scalar settings - see BuildRawBlocks()/
// BuildWorkspaceFields(). All writes go through ConfigWriter, which
// edits kohiko.conf's existing lines/comments/ordering in place -
// manual editing of the same file remains fully supported before,
// after, or interleaved with using this GUI.
class SettingsWindow
{
public:

    SettingsWindow();
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    // Opens the display, loads kohiko.conf + the schema, creates the
    // window. Logs why to stderr and returns false if anything
    // essential fails (no DISPLAY, config file missing, ...).
    bool Initialize();

    // Runs the event loop until the window is closed.
    void Run();

private:

    // --- one editable scalar setting -----------------------------------------

    struct Field
    {
        const ConfigOption* option = nullptr;
        std::string loadedValue;  // as last read from disk / last Applied
        std::string currentValue; // live edit buffer - see Dirty()
        std::string error;        // "" = valid; see ValidateField()
        bool infoExpanded = false;

        Rect rowRect{};      // this row's full clickable area, incl. any expanded info panel
        Rect widgetRect{};   // just the checkbox/text field/swatch part
        Rect infoIconRect{};
    };

    // --- a repeatable-directive family (bind=/exec./windowrule=/monitor=) ----

    struct RawBlockPanel
    {
        std::string id;         // stable id, e.g. "windowrule"
        std::string category;   // which sidebar Category this appears under
        std::string title;
        std::string helpText;   // short explanation shown above the text area
        std::string prefix;     // e.g. "windowrule=" ("exec." for the named-command family)

        std::vector<std::string> loadedLines; // as last read from disk / last Applied
        std::vector<std::string> lines;        // live edit buffer, one entry per line, prefix stripped
        std::vector<std::string> lineErrors;   // parallel to `lines`; "" = valid

        std::size_t caretRow = 0;
        std::size_t caretCol = 0;

        Rect rect{}; // the whole panel, including title/help/textarea
        Rect textAreaRect{};
    };

    // --- a sidebar entry --------------------------------------------------------

    struct Category
    {
        std::string name;
        bool hasRawBlock = false; // Window Rules / Monitors / Developer / Input's Keybindings group
    };

private:

    // --- setup ------------------------------------------------------------------

    void BuildFields();
    void BuildRawBlocks();
    void BuildWorkspaceFields(); // workspace<N>= - regenerated whenever workspace.count changes

    void CreateWindow();
    void SetWmProperties();

    // --- event loop ---------------------------------------------------------------

    void HandleEvent(const XEvent& event);
    void HandleKeyPress(const XKeyEvent& event);
    void HandleButtonPress(const XButtonEvent& event);
    void HandleScroll(int direction);

    // --- field interaction --------------------------------------------------------

    void FocusField(Field* field, bool caretAtEnd);
    void FocusRawBlock(RawBlockPanel* panel);
    void ClearFocus();

    void InsertCodepoint(const std::string& utf8Codepoint);
    void HandleFieldKey(const XKeyEvent& event, KeySym keysym, const std::string& typed);
    void HandleRawBlockKey(const XKeyEvent& event, KeySym keysym, const std::string& typed);
    void HandleSearchKey(const XKeyEvent& event, KeySym keysym, const std::string& typed);

    void ToggleBoolField(Field& field) const;
    void CycleEnumField(Field& field) const;

    void ValidateField(Field& field) const;
    void ValidateRawBlockLine(const RawBlockPanel& panel, const std::string& line, std::string& outError) const;

    bool FieldDirty(const Field& field) const;
    bool RawBlockDirty(const RawBlockPanel& panel) const;
    bool AnyDirty() const;

    // --- commands -----------------------------------------------------------------

    void Apply();
    void Save();
    void ResetVisibleToDefault();
    void ReloadRunningKohiko() const;
    void ShowStatus(const std::string& message);

    // --- layout & drawing -----------------------------------------------------------

    void Redraw();
    void LayoutAndDrawSidebar(int& outWidth);
    void LayoutAndDrawTopBar(int sidebarWidth);
    void LayoutAndDrawContent(int sidebarWidth);
    void LayoutAndDrawField(Field& field, int x, int y, int width, int& outHeight);
    void LayoutAndDrawRawBlock(RawBlockPanel& panel, int x, int y, int width, int& outHeight);
    void LayoutAndDrawBottomBar();

    void DrawText(int x, int y, const std::string& text, unsigned long pixel);
    void DrawCheckbox(const Rect& rect, bool checked);
    void DrawButton(const Rect& rect, const std::string& label, bool enabled, bool primary);
    std::vector<std::string> WrapText(const std::string& text, int maxWidth) const;

    // Every Field currently on screen, in on-screen order - either the
    // selected category's fields, or (while m_searchText is non-empty)
    // every field/panel anywhere matching it. What Reset to Default
    // and the layout pass both iterate.
    std::vector<Field*> VisibleFields();
    std::vector<RawBlockPanel*> VisibleRawBlocks();

    static std::string HumanizeKey(const std::string& key);

private:

    // --- X plumbing -----------------------------------------------------------------

    Display* m_display = nullptr;
    int m_screen = 0;
    ::Window m_window = 0;
    GC m_gc = nullptr;
    Font m_font;
    Font m_boldFont; // section/category headings - see Initialize()
    XftDraw* m_xftDraw = nullptr;
    XIM m_xim = nullptr;
    XIC m_xic = nullptr;
    Atom m_wmDeleteWindow = 0;

    Rect m_geometry;
    bool m_running = false;

    // --- config -----------------------------------------------------------------

    std::string m_configPath;
    ConfigWriter m_writer;

    // --- content -----------------------------------------------------------------

    std::vector<Category> m_categories;
    std::vector<Field> m_fields;
    std::vector<RawBlockPanel> m_rawBlocks;

    // workspace<N>= fields are regenerated (not fixed ConfigSchema
    // entries - N depends on the live value of workspace.count), so
    // their backing ConfigOptions have to live somewhere Field can
    // safely point into; this is that storage. See BuildWorkspaceFields().
    std::vector<ConfigOption> m_workspaceOptionStorage;
    std::vector<Field> m_workspaceFields;

    int m_selectedCategory = 0;
    std::string m_searchText;
    bool m_searchFocused = false;
    std::size_t m_searchCaret = 0;

    int m_scrollOffset = 0;
    int m_contentTotalHeight = 0;

    Field* m_focusedField = nullptr;
    std::size_t m_fieldCaret = 0;
    RawBlockPanel* m_focusedRawBlock = nullptr;

    std::string m_statusMessage;

    // --- layout (recomputed every Redraw()) -----------------------------------------

    Rect m_sidebarRect{};
    Rect m_searchRect{};
    Rect m_contentRect{};
    Rect m_applyButtonRect{};
    Rect m_saveButtonRect{};
    Rect m_resetButtonRect{};
    std::vector<Rect> m_categoryRects; // parallel to m_categories

    // --- colors (same palette convention as Bar/Notepad/LockScreen) ----------------

    unsigned long m_backgroundPixel = 0;
    unsigned long m_panelPixel = 0;
    unsigned long m_foregroundPixel = 0;
    unsigned long m_mutedPixel = 0;
    unsigned long m_accentPixel = 0;
    unsigned long m_fieldPixel = 0;
    unsigned long m_errorPixel = 0;
    unsigned long m_borderPixel = 0;

};

}
