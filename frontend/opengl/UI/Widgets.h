// =====================================================================
// Widgets.h - reusable UI components (Button, TextInput, Card, Badge, Table,
// Dropdown, Modal, Tooltip, ProgressBar, ScrollPanel, Tab, Toast, Icon ...)
//
// Immediate-mode style: screens call the widget every frame with a Rect;
// widgets that own state (TextInput, Dropdown, ScrollPanel, Table) are classes.
// Interaction (hover / click / focus / capture by popups and modals) is
// arbitrated by the UI context singleton.
// =====================================================================
#pragma once
#include "Input.h"
#include "Renderer.h"
#include <functional>
#include <string>
#include <vector>

namespace ui {

struct Mouse {
    float x = -1, y = -1;
    bool down = false, pressed = false, released = false, rightPressed = false;
    float wheel = 0;
};

enum class ToastKind { INFO, SUCCESS, ERR };

class UI {
public:
    static UI& get();
    Mouse mouse;
    KeyRouter keys;
    double timeSec = 0;

    void beginFrame(double t);
    void endFrame();                         // draws popups, tooltip, toasts

    bool hover(const Rect& r) const;         // mouse inside and not captured by a popup / modal
    bool clicked(const Rect& r);             // pressed inside this frame (consumes the click)
    bool wheelOver(const Rect& r) const { return hover(r) && mouse.wheel != 0; }
    void consumeClick() { clickConsumed_ = true; }
    bool clickAvailable() const { return mouse.pressed && !clickConsumed_; }
    void wantHand() { hand_ = true; }

    // overlays
    void deferPopup(const Rect& r, std::function<void()> draw);   // drawn on top, captures input
    void tooltip(const std::string& text);
    void toast(const std::string& text, ToastKind kind = ToastKind::INFO);

    // modal: everything outside the modal rect ignores input
    void setModal(const Rect& r) { modal_ = true; modalRect_ = r; }
    bool modalActive() const { return modal_; }

    bool popupCaptured() const { return popupCapture_; }
    bool inPopupPass() const { return popupPass_; }

private:
    bool clickConsumed_ = false, hand_ = false;
    bool modal_ = false, modalNext_ = false;
    Rect modalRect_;
    struct Popup { Rect r; std::function<void()> draw; };
    std::vector<Popup> popups_, nextPopups_;
    std::vector<Rect> lastPopupRects_;
    bool popupCapture_ = false, popupPass_ = false;
    std::string tip_;
    struct Toast { std::string text; ToastKind kind; double until; };
    std::vector<Toast> toasts_;
};

// ---------------------------------------------------------------- primitives
enum class ButtonStyle { PRIMARY, SECONDARY, GHOST, DANGER, SUCCESS };
bool button(const Rect& r, const std::string& label, ButtonStyle style = ButtonStyle::PRIMARY, bool enabled = true,
            const char* icon = nullptr);
void card(const Rect& r, bool elevated = true);
float badge(float x, float centerY, const std::string& text, const Color& fg, const Color& bg, float size = 11.5f);
void progressBar(const Rect& r, float fraction, const Color& c);
bool tabs(const Rect& r, const std::vector<std::string>& items, int& selected);
bool segmented(const Rect& r, const std::vector<std::string>& items, int& selected);
bool checkbox(const Rect& r, const std::string& label, bool& value);
bool toggle(const Rect& r, bool& value);
bool linkText(float x, float baseline, const std::string& s, float size = 13.0f);
void sectionTitle(const Rect& r, const std::string& title, const std::string& subtitle = "");
void icon(const char* name, float cx, float cy, float size, const Color& c);
void spinnerDots(float cx, float cy, const Color& c);
void kpiCard(const Rect& r, const std::string& label, const std::string& value, const std::string& hint,
             const Color& accent, const char* iconName);
void emptyState(const Rect& r, const std::string& title, const std::string& hint);

// ---------------------------------------------------------------- stateful widgets
class TextInput {
public:
    explicit TextInput(std::size_t maxLen = 120, bool numeric = false, bool password = false)
        : field(maxLen, numeric, password) {}
    TextField field;
    std::string error;                                     // shown under the field
    // returns true when Enter was pressed while focused
    void draw(const Rect& r, const std::string& placeholder, const char* leadingIcon = nullptr);
    bool focused() const { return UI::get().keys.focused() == &field; }
    void focus() { UI::get().keys.focus(&field); }
    const std::string& text() const { return field.text(); }
};

class Dropdown {
public:
    std::vector<std::string> items;
    int selected = -1;
    bool open = false;
    float scroll = 0;
    std::string error;
    // returns true when the selection changed this frame
    bool draw(const Rect& r, const std::string& placeholder);
    std::string value() const { return selected >= 0 && selected < (int)items.size() ? items[selected] : ""; }
private:
    bool changed_ = false;
    bool pendingChange_ = false;   // set in the popup pass, reported by the next draw()
};

class ScrollPanel {
public:
    float offset = 0;
    // begin: pushes a clip for r and returns the y origin of the content
    float begin(const Rect& r, float contentHeight);
    void end();
private:
    Rect r_;
    float content_ = 0;
    bool dragging_ = false;
    float dragStartY_ = 0, dragStartOffset_ = 0;
};

struct Cell {
    std::string text;
    Color fg{0, 0, 0, 0};          // alpha 0 -> theme text colour
    Color badgeBg{0, 0, 0, 0};     // alpha > 0 -> render as badge
    Weight weight = Weight::REGULAR;
};
struct Column {
    std::string title;
    float width;                   // > 1 : pixels, <= 1 : fraction of the remaining width
    Align align = Align::LEFT;
};

class Table {
public:
    ScrollPanel scroll;
    int hoverRow = -1;
    int sortColumn = -1;
    bool sortAscending = true;
    // returns index of the clicked row, -1 otherwise. Header clicks update sortColumn/sortAscending
    int draw(const Rect& r, const std::vector<Column>& cols, const std::vector<std::vector<Cell>>& rows,
             bool sortable = false, int selectedRow = -1);
    bool headerClicked = false;
    static constexpr float rowH = 44.0f, headH = 40.0f;
};

int pager(const Rect& r, int page, int pages, int totalRows);   // returns new page

bool modalBegin(const Rect& r, const std::string& title, bool& open);   // returns false when closed
void modalEnd();

Cell textCell(const std::string& t, Weight w = Weight::REGULAR);
Cell mutedCell(const std::string& t);
Cell badgeCell(const std::string& t, const Color& fg, const Color& bg);

} // namespace ui
