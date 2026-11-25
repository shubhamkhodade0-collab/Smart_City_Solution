#include "Widgets.h"
#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ui {

namespace {
constexpr float PI = 3.14159265358979f;
const Palette& P() { return Theme::p(); }
ScrollPanel* wheelTarget = nullptr;
float wheelDelta = 0;
}

// ============================================================ UI context
UI& UI::get() {
    static UI ui;
    return ui;
}

void UI::beginFrame(double t) {
    timeSec = t;
    clickConsumed_ = false;
    hand_ = false;
    tip_.clear();
    modal_ = modalNext_;
    modalNext_ = false;
    popupCapture_ = false;
    for (const Rect& r : lastPopupRects_)
        if (r.contains(mouse.x, mouse.y)) popupCapture_ = true;
    popups_.clear();
}

void UI::endFrame() {
    // popups (dropdown menus) on top of everything, with input priority
    popupPass_ = true;
    lastPopupRects_.clear();
    for (auto& p : popups_) {
        lastPopupRects_.push_back(p.r);
        p.draw();
    }
    popupPass_ = false;

    if (!tip_.empty()) {
        float w = Renderer::measure(tip_, 12) + 20;
        float x = std::min(mouse.x + 14, Renderer::width() - w - 8);
        float y = mouse.y + 18;
        if (y + 30 > Renderer::height()) y = mouse.y - 36;
        Rect r{x, y, w, 28};
        Renderer::shadow(r, 6, 8, 1.5f);
        Renderer::roundRect(r, 6, Theme::dark() ? hex(0xE6E9EF) : hex(0x1D2433));
        Renderer::textIn(r.inset(10, 0), tip_, 12, Theme::dark() ? hex(0x101828) : hex(0xFFFFFF));
    }

    // toasts (top-right), auto-expire
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [&](const Toast& t) { return t.until < timeSec; }),
                  toasts_.end());
    float y = Theme::topbarH + 12;
    for (const auto& t : toasts_) {
        auto lines = Renderer::wrap(t.text, 340, 13);
        float h = 24 + lines.size() * 19.0f;
        Rect r{Renderer::width() - 400.0f, y, 380, h};
        Renderer::shadow(r, 10, 14, 2.0f);
        Renderer::roundRect(r, 10, P().surface);
        Renderer::roundRectOutline(r, 10, P().border);
        Color accent = t.kind == ToastKind::SUCCESS ? P().success : t.kind == ToastKind::ERR ? P().danger : P().info;
        Renderer::circle(r.x + 20, r.y + 22, 9, accent.withAlpha(0.15f));
        icon(t.kind == ToastKind::SUCCESS ? "check" : t.kind == ToastKind::ERR ? "alert" : "info", r.x + 20, r.y + 22, 11,
             accent);
        float ly = r.y + 26;
        for (const auto& l : lines) {
            Renderer::text(r.x + 40, ly, l, 13, P().text);
            ly += 19;
        }
        y += h + 10;
    }
    glutSetCursor(hand_ ? GLUT_CURSOR_INFO : GLUT_CURSOR_LEFT_ARROW);
}

bool UI::hover(const Rect& r) const {
    if (!r.contains(mouse.x, mouse.y)) return false;
    if (popupPass_) return true;
    if (popupCapture_) return false;
    if (modal_ && !modalRect_.contains(mouse.x, mouse.y)) return false;
    return true;
}

bool UI::clicked(const Rect& r) {
    if (mouse.pressed && !clickConsumed_ && hover(r)) {
        clickConsumed_ = true;
        return true;
    }
    return false;
}

void UI::deferPopup(const Rect& r, std::function<void()> draw) { popups_.push_back({r, std::move(draw)}); }
void UI::tooltip(const std::string& text) { tip_ = text; }
void UI::toast(const std::string& text, ToastKind kind) {
    toasts_.push_back({text, kind, timeSec + (kind == ToastKind::ERR ? 7.0 : 4.5)});
    if (toasts_.size() > 4) toasts_.erase(toasts_.begin());
}

// ============================================================ icons (vector, line-art)
void icon(const char* n, float cx, float cy, float s, const Color& c) {
    const float h = s * 0.5f, w = 1.6f;
    auto L = [&](float x0, float y0, float x1, float y1) { Renderer::line(cx + x0 * h, cy + y0 * h, cx + x1 * h, cy + y1 * h, c, w); };
    auto O = [&](float x, float y, float r, int seg = 28) {
        std::vector<float> v;
        for (int i = 0; i <= seg; ++i) {
            float a = 2 * PI * i / seg;
            v.push_back(cx + (x + r * std::cos(a)) * h);
            v.push_back(cy + (y + r * std::sin(a)) * h);
        }
        Renderer::polyline(v, c, w);
    };
    auto B = [&](float x0, float y0, float x1, float y1) { L(x0, y0, x1, y0); L(x1, y0, x1, y1); L(x1, y1, x0, y1); L(x0, y1, x0, y0); };
    std::string k = n;
    if (k == "grid") { B(-0.9f, -0.9f, -0.1f, -0.1f); B(0.1f, -0.9f, 0.9f, -0.1f); B(-0.9f, 0.1f, -0.1f, 0.9f); B(0.1f, 0.1f, 0.9f, 0.9f); }
    else if (k == "plus") { L(0, -0.8f, 0, 0.8f); L(-0.8f, 0, 0.8f, 0); }
    else if (k == "list") { for (float y : {-0.6f, 0.0f, 0.6f}) { L(-0.4f, y, 0.9f, y); Renderer::circle(cx - 0.8f * h, cy + y * h, 1.6f, c); } }
    else if (k == "bell") { O(0, -0.1f, 0.6f); L(-0.8f, 0.5f, 0.8f, 0.5f); L(-0.15f, 0.8f, 0.15f, 0.8f); }
    else if (k == "map") { O(0, -0.25f, 0.45f); L(-0.38f, 0.0f, 0, 0.9f); L(0.38f, 0.0f, 0, 0.9f); Renderer::circle(cx, cy - 0.25f * h, 1.8f, c); }
    else if (k == "chart") { L(-0.9f, 0.9f, 0.9f, 0.9f); B(-0.7f, 0.1f, -0.35f, 0.9f); B(-0.15f, -0.5f, 0.2f, 0.9f); B(0.4f, -0.1f, 0.75f, 0.9f); }
    else if (k == "users") { O(-0.3f, -0.35f, 0.32f); O(0.45f, -0.25f, 0.25f); L(-0.85f, 0.85f, -0.85f, 0.45f); L(-0.85f, 0.45f, 0.25f, 0.45f); L(0.25f, 0.45f, 0.25f, 0.85f); L(0.4f, 0.3f, 0.9f, 0.3f); L(0.9f, 0.3f, 0.9f, 0.75f); }
    else if (k == "user") { O(0, -0.4f, 0.38f); L(-0.7f, 0.85f, -0.7f, 0.45f); L(-0.7f, 0.45f, 0.7f, 0.45f); L(0.7f, 0.45f, 0.7f, 0.85f); }
    else if (k == "building") { B(-0.7f, -0.9f, 0.7f, 0.9f); for (float y : {-0.5f, 0.0f}) { L(-0.35f, y, -0.1f, y); L(0.1f, y, 0.35f, y); } B(-0.15f, 0.45f, 0.15f, 0.9f); }
    else if (k == "file") { L(-0.6f, -0.9f, 0.3f, -0.9f); L(0.3f, -0.9f, 0.6f, -0.6f); L(0.6f, -0.6f, 0.6f, 0.9f); L(0.6f, 0.9f, -0.6f, 0.9f); L(-0.6f, 0.9f, -0.6f, -0.9f); L(-0.3f, -0.1f, 0.3f, -0.1f); L(-0.3f, 0.3f, 0.3f, 0.3f); }
    else if (k == "settings") { O(0, 0, 0.32f); for (int i = 0; i < 8; ++i) { float a = i * PI / 4; L(0.55f * std::cos(a), 0.55f * std::sin(a), 0.85f * std::cos(a), 0.85f * std::sin(a)); } O(0, 0, 0.62f); }
    else if (k == "search") { O(-0.15f, -0.15f, 0.55f); L(0.25f, 0.25f, 0.85f, 0.85f); }
    else if (k == "logout") { L(-0.1f, -0.85f, -0.85f, -0.85f); L(-0.85f, -0.85f, -0.85f, 0.85f); L(-0.85f, 0.85f, -0.1f, 0.85f); L(-0.3f, 0, 0.85f, 0); L(0.5f, -0.35f, 0.85f, 0); L(0.5f, 0.35f, 0.85f, 0); }
    else if (k == "sun") { O(0, 0, 0.38f); for (int i = 0; i < 8; ++i) { float a = i * PI / 4; L(0.6f * std::cos(a), 0.6f * std::sin(a), 0.9f * std::cos(a), 0.9f * std::sin(a)); } }
    else if (k == "moon") { std::vector<float> v; for (int i = 0; i <= 24; ++i) { float a = PI * 0.35f + 1.3f * PI * i / 24; v.push_back(cx + 0.8f * h * std::cos(a)); v.push_back(cy + 0.8f * h * std::sin(a)); } Renderer::polyline(v, c, w); v.clear(); for (int i = 0; i <= 24; ++i) { float a = PI * 0.35f + 1.3f * PI * i / 24; v.push_back(cx + 0.25f * h + 0.55f * h * std::cos(a)); v.push_back(cy - 0.2f * h + 0.55f * h * std::sin(a)); } Renderer::polyline(v, c, w); }
    else if (k == "queue") { for (float x : {-0.75f, -0.15f, 0.45f}) B(x, -0.35f, x + 0.4f, 0.35f); L(-0.95f, 0.7f, 0.9f, 0.7f); L(0.6f, 0.5f, 0.9f, 0.7f); L(0.6f, 0.9f, 0.9f, 0.7f); }
    else if (k == "stack") { B(-0.7f, 0.35f, 0.7f, 0.85f); B(-0.7f, -0.25f, 0.7f, 0.25f); B(-0.7f, -0.85f, 0.7f, -0.35f); }
    else if (k == "cpu") { B(-0.55f, -0.55f, 0.55f, 0.55f); B(-0.22f, -0.22f, 0.22f, 0.22f); for (float t : {-0.3f, 0.0f, 0.3f}) { L(t, -0.55f, t, -0.85f); L(t, 0.55f, t, 0.85f); L(-0.55f, t, -0.85f, t); L(0.55f, t, 0.85f, t); } }
    else if (k == "ai") {   // two four-point sparkles
        auto star = [&](float x, float y, float r, float ri) {
            L(x, y - r, x + ri, y - ri); L(x + ri, y - ri, x + r, y); L(x + r, y, x + ri, y + ri); L(x + ri, y + ri, x, y + r);
            L(x, y + r, x - ri, y + ri); L(x - ri, y + ri, x - r, y); L(x - r, y, x - ri, y - ri); L(x - ri, y - ri, x, y - r);
        };
        star(-0.2f, 0.2f, 0.78f, 0.2f);
        star(0.62f, -0.62f, 0.32f, 0.09f);
    }
    else if (k == "cube") { L(0, -0.9f, 0.8f, -0.45f); L(0.8f, -0.45f, 0.8f, 0.45f); L(0.8f, 0.45f, 0, 0.9f); L(0, 0.9f, -0.8f, 0.45f); L(-0.8f, 0.45f, -0.8f, -0.45f); L(-0.8f, -0.45f, 0, -0.9f); L(-0.8f, -0.45f, 0, 0); L(0.8f, -0.45f, 0, 0); L(0, 0, 0, 0.9f); }
    else if (k == "shield") { L(0, -0.9f, 0.75f, -0.6f); L(0.75f, -0.6f, 0.65f, 0.25f); L(0.65f, 0.25f, 0, 0.9f); L(0, 0.9f, -0.65f, 0.25f); L(-0.65f, 0.25f, -0.75f, -0.6f); L(-0.75f, -0.6f, 0, -0.9f); }
    else if (k == "check") { L(-0.7f, 0.05f, -0.2f, 0.55f); L(-0.2f, 0.55f, 0.75f, -0.5f); }
    else if (k == "x") { L(-0.6f, -0.6f, 0.6f, 0.6f); L(0.6f, -0.6f, -0.6f, 0.6f); }
    else if (k == "clock") { O(0, 0, 0.82f); L(0, 0, 0, -0.5f); L(0, 0, 0.38f, 0.2f); }
    else if (k == "alert") { L(0, -0.55f, 0, 0.2f); Renderer::circle(cx, cy + 0.55f * h, 1.7f, c); }
    else if (k == "info") { L(0, -0.1f, 0, 0.6f); Renderer::circle(cx, cy - 0.5f * h, 1.7f, c); }
    else if (k == "refresh") { std::vector<float> v; for (int i = 0; i <= 24; ++i) { float a = -PI * 0.3f + 1.6f * PI * i / 24; v.push_back(cx + 0.75f * h * std::cos(a)); v.push_back(cy + 0.75f * h * std::sin(a)); } Renderer::polyline(v, c, w); L(0.45f, -0.9f, 0.45f, -0.45f); L(0.45f, -0.45f, 0.9f, -0.5f); }
    else if (k == "download") { L(0, -0.85f, 0, 0.35f); L(-0.4f, -0.05f, 0, 0.35f); L(0.4f, -0.05f, 0, 0.35f); L(-0.8f, 0.85f, 0.8f, 0.85f); }
    else if (k == "layers") { L(0, -0.8f, 0.85f, -0.35f); L(0.85f, -0.35f, 0, 0.1f); L(0, 0.1f, -0.85f, -0.35f); L(-0.85f, -0.35f, 0, -0.8f); L(-0.85f, 0.1f, 0, 0.55f); L(0, 0.55f, 0.85f, 0.1f); }
    else if (k == "chevron-down") { L(-0.5f, -0.2f, 0, 0.3f); L(0, 0.3f, 0.5f, -0.2f); }
    else if (k == "chevron-left") { L(0.2f, -0.5f, -0.3f, 0); L(-0.3f, 0, 0.2f, 0.5f); }
    else if (k == "chevron-right") { L(-0.2f, -0.5f, 0.3f, 0); L(0.3f, 0, -0.2f, 0.5f); }
    else if (k == "eye" || k == "eye-off") {
        std::vector<float> v{-0.9f * h, 0, -0.45f * h, -0.48f * h, 0, -0.68f * h,
                             0.45f * h, -0.48f * h, 0.9f * h, 0, 0.45f * h, 0.48f * h,
                             0, 0.68f * h, -0.45f * h, 0.48f * h, -0.9f * h, 0};
        Renderer::polyline(v, c, w);
        O(0, 0, 0.25f, 20);
        if (k == "eye-off") L(-0.9f, -0.9f, 0.9f, 0.9f);
    }
    else if (k == "arrow-right") { L(-0.8f, 0, 0.8f, 0); L(0.35f, -0.45f, 0.8f, 0); L(0.35f, 0.45f, 0.8f, 0); }
    else if (k == "trophy") { L(-0.5f, -0.8f, 0.5f, -0.8f); L(-0.5f, -0.8f, -0.4f, -0.1f); L(0.5f, -0.8f, 0.4f, -0.1f); L(-0.4f, -0.1f, 0.4f, -0.1f); L(0, -0.1f, 0, 0.5f); L(-0.4f, 0.8f, 0.4f, 0.8f); L(-0.25f, 0.5f, 0.25f, 0.5f); }
    else if (k == "db") { O(0, -0.55f, 0.0f); std::vector<float> v; for (int i = 0; i <= 28; ++i) { float a = 2 * PI * i / 28; v.push_back(cx + 0.7f * h * std::cos(a)); v.push_back(cy - 0.55f * h + 0.25f * h * std::sin(a)); } Renderer::polyline(v, c, w); L(-0.7f, -0.55f, -0.7f, 0.6f); L(0.7f, -0.55f, 0.7f, 0.6f); v.clear(); for (int i = 0; i <= 14; ++i) { float a = PI * i / 14; v.push_back(cx + 0.7f * h * std::cos(a)); v.push_back(cy + 0.6f * h + 0.25f * h * std::sin(a)); } Renderer::polyline(v, c, w); }
    else if (k == "inbox") { L(-0.85f, 0.1f, -0.3f, 0.1f); L(-0.3f, 0.1f, -0.15f, 0.4f); L(-0.15f, 0.4f, 0.15f, 0.4f); L(0.15f, 0.4f, 0.3f, 0.1f); L(0.3f, 0.1f, 0.85f, 0.1f); L(-0.85f, 0.1f, -0.55f, -0.75f); L(0.85f, 0.1f, 0.55f, -0.75f); L(-0.55f, -0.75f, 0.55f, -0.75f); L(-0.85f, 0.1f, -0.85f, 0.8f); L(0.85f, 0.1f, 0.85f, 0.8f); L(-0.85f, 0.8f, 0.85f, 0.8f); }
    else { O(0, 0, 0.5f); }
}

void spinnerDots(float cx, float cy, const Color& c) {
    double t = UI::get().timeSec;
    for (int i = 0; i < 3; ++i) {
        float a = 0.35f + 0.65f * static_cast<float>(0.5 + 0.5 * std::sin(t * 6 - i * 0.9));
        Renderer::circle(cx + (i - 1) * 9.0f, cy, 3, c.withAlpha(a));
    }
}

// ============================================================ basic widgets
bool button(const Rect& r, const std::string& label, ButtonStyle style, bool enabled, const char* ic) {
    UI& u = UI::get();
    bool hov = enabled && u.hover(r);
    bool pressed = hov && u.mouse.down;
    Color bg, fg, border{0, 0, 0, 0};
    switch (style) {
        case ButtonStyle::PRIMARY: bg = P().primary; fg = hex(0xFFFFFF); break;
        case ButtonStyle::DANGER: bg = P().danger; fg = hex(0xFFFFFF); break;
        case ButtonStyle::SUCCESS: bg = P().success; fg = hex(0xFFFFFF); break;
        case ButtonStyle::SECONDARY: bg = P().surface; fg = P().text; border = P().borderStrong; break;
        case ButtonStyle::GHOST: bg = hov ? P().hover : Color{0, 0, 0, 0}; fg = P().textMuted; break;
    }
    if (style == ButtonStyle::PRIMARY || style == ButtonStyle::DANGER || style == ButtonStyle::SUCCESS) {
        if (hov) bg = bg.mix(hex(0x000000), 0.10f);
        if (pressed) bg = bg.mix(hex(0x000000), 0.18f);
        Renderer::shadow(r, Theme::radiusSm, 4, 0.8f);
    } else if (style == ButtonStyle::SECONDARY && hov) bg = P().hover;
    if (!enabled) { bg = bg.withAlpha(bg.a * 0.45f); fg = fg.withAlpha(0.55f); }
    Renderer::roundRect(r, Theme::radiusSm, bg);
    if (border.a > 0) Renderer::roundRectOutline(r, Theme::radiusSm, border);
    float size = r.h >= 40 ? 14.0f : 13.0f;
    float tw = Renderer::measure(label, size, Weight::SEMIBOLD) + (ic ? 22 : 0);
    float x = r.x + (r.w - tw) * 0.5f;
    if (ic) {
        icon(ic, x + 7, r.y + r.h * 0.5f, 13, fg);
        x += 22;
    }
    Renderer::text(x, r.y + r.h * 0.5f + size * 0.36f, label, size, fg, Weight::SEMIBOLD);
    if (hov) u.wantHand();
    return enabled && u.clicked(r);
}

void card(const Rect& r, bool elevated) {
    if (elevated) Renderer::shadow(r, Theme::radius, 10, 1.0f);
    Renderer::roundRect(r, Theme::radius, P().surface);
    Renderer::roundRectOutline(r, Theme::radius, P().border);
}

float badge(float x, float cy, const std::string& t, const Color& fg, const Color& bg, float size) {
    float w = Renderer::measure(t, size, Weight::SEMIBOLD) + 18;
    Rect r{x, cy - 11, w, 22};
    Renderer::roundRect(r, 11, bg);
    Renderer::circle(r.x + 9, cy, 2.6f, fg);
    Renderer::text(r.x + 15, cy + size * 0.36f, t, size, fg, Weight::SEMIBOLD);
    return w + 4;   // dot shifts text: widen
}

void progressBar(const Rect& r, float f, const Color& c) {
    f = std::clamp(f, 0.0f, 1.0f);
    Renderer::roundRect(r, r.h * 0.5f, P().neutralSoft);
    if (f > 0) Renderer::roundRect({r.x, r.y, std::max(r.h, r.w * f), r.h}, r.h * 0.5f, c);
}

bool tabs(const Rect& r, const std::vector<std::string>& items, int& sel) {
    UI& u = UI::get();
    bool changed = false;
    float x = r.x;
    Renderer::line(r.x, r.bottom() - 0.5f, r.right(), r.bottom() - 0.5f, P().border);
    for (int i = 0; i < (int)items.size(); ++i) {
        float w = Renderer::measure(items[i], 13.5f, Weight::SEMIBOLD) + 28;
        Rect t{x, r.y, w, r.h};
        bool hov = u.hover(t);
        Color fg = i == sel ? P().primary : hov ? P().text : P().textMuted;
        Renderer::textIn(t, items[i], 13.5f, fg, Weight::SEMIBOLD, Align::CENTER);
        if (i == sel) Renderer::roundRect({t.x + 8, t.bottom() - 2.5f, t.w - 16, 2.5f}, 1.2f, P().primary);
        if (hov) u.wantHand();
        if (u.clicked(t) && sel != i) { sel = i; changed = true; }
        x += w + 2;
    }
    return changed;
}

bool segmented(const Rect& r, const std::vector<std::string>& items, int& sel) {
    UI& u = UI::get();
    bool changed = false;
    Renderer::roundRect(r, Theme::radiusSm, P().neutralSoft);
    float w = (r.w - 6) / items.size();
    for (int i = 0; i < (int)items.size(); ++i) {
        Rect t{r.x + 3 + i * w, r.y + 3, w, r.h - 6};
        if (i == sel) {
            Renderer::shadow(t, 6, 3, 0.8f);
            Renderer::roundRect(t, 6, P().surface);
        }
        bool hov = u.hover(t);
        Renderer::textIn(t, items[i], 12.5f, i == sel ? P().text : hov ? P().text : P().textMuted, Weight::SEMIBOLD,
                         Align::CENTER);
        if (hov) u.wantHand();
        if (u.clicked(t) && sel != i) { sel = i; changed = true; }
    }
    return changed;
}

bool checkbox(const Rect& r, const std::string& label, bool& v) {
    UI& u = UI::get();
    Rect box{r.x, r.y + (r.h - 18) / 2, 18, 18};
    Renderer::roundRect(box, 5, v ? P().primary : P().surface);
    if (!v) Renderer::roundRectOutline(box, 5, P().borderStrong);
    else icon("check", box.x + 9, box.y + 9, 12, hex(0xFFFFFF));
    Renderer::textIn({r.x + 26, r.y, r.w - 26, r.h}, label, 13, P().text);
    if (u.hover(r)) u.wantHand();
    if (u.clicked(r)) { v = !v; return true; }
    return false;
}

bool toggle(const Rect& r, bool& v) {
    UI& u = UI::get();
    Rect t{r.x, r.y + (r.h - 22) / 2, 40, 22};
    Renderer::roundRect(t, 11, v ? P().primary : P().borderStrong);
    Renderer::circle(v ? t.right() - 11 : t.x + 11, t.y + 11, 8, hex(0xFFFFFF));
    if (u.hover(t)) u.wantHand();
    if (u.clicked(t)) { v = !v; return true; }
    return false;
}

bool linkText(float x, float baseline, const std::string& s, float size) {
    UI& u = UI::get();
    float w = Renderer::measure(s, size, Weight::SEMIBOLD);
    Rect r{x, baseline - size, w, size * 1.4f};
    bool hov = u.hover(r);
    Renderer::text(x, baseline, s, size, hov ? P().primaryText : P().primary, Weight::SEMIBOLD);
    if (hov) u.wantHand();
    return u.clicked(r);
}

void sectionTitle(const Rect& r, const std::string& title, const std::string& sub) {
    Renderer::text(r.x, r.y + 18, title, 16, P().text, Weight::SEMIBOLD);
    if (!sub.empty()) Renderer::text(r.x, r.y + 38, sub, 12.5f, P().textMuted);
}

void kpiCard(const Rect& r, const std::string& label, const std::string& value, const std::string& hint,
             const Color& accent, const char* ic) {
    card(r);
    Renderer::circle(r.right() - 30, r.y + 30, 15, accent.withAlpha(Theme::dark() ? 0.18f : 0.10f));
    icon(ic, r.right() - 30, r.y + 30, 14, accent);
    Renderer::text(r.x + 18, r.y + 30, label, 12, P().textMuted, Weight::SEMIBOLD);
    Renderer::text(r.x + 18, r.y + 66, value, 28, P().text, Weight::BOLD);
    if (!hint.empty()) Renderer::text(r.x + 18, r.y + r.h - 16, hint, 11.5f, P().textFaint);
}

void emptyState(const Rect& r, const std::string& title, const std::string& hint) {
    Renderer::circle(r.x + r.w / 2, r.y + r.h / 2 - 26, 24, P().neutralSoft);
    icon("inbox", r.x + r.w / 2, r.y + r.h / 2 - 26, 22, P().textFaint);
    Renderer::textIn({r.x, r.y + r.h / 2 + 8, r.w, 22}, title, 14, P().text, Weight::SEMIBOLD, Align::CENTER);
    Renderer::textIn({r.x, r.y + r.h / 2 + 30, r.w, 20}, hint, 12.5f, P().textMuted, Weight::REGULAR, Align::CENTER);
}

// ============================================================ TextInput
void TextInput::draw(const Rect& r, const std::string& placeholder, const char* lead) {
    UI& u = UI::get();
    bool hov = u.hover(r);
    bool foc = focused();
    const float passwordControlW = field.isPassword() ? 36.0f : 0.0f;
    Rect inputArea{r.x, r.y, r.w - passwordControlW, r.h};
    Rect passwordControl{r.right() - passwordControlW, r.y, passwordControlW, r.h};
    if (field.isPassword()) {
        bool controlHovered = u.hover(passwordControl);
        if (controlHovered) u.wantHand();
        if (u.clicked(passwordControl)) {
            field.togglePasswordVisibility();
            u.keys.focus(&field);
            foc = true;
        }
    }
    if (hov) u.wantHand();
    if (u.clicked(inputArea)) {
        u.keys.focus(&field);
        foc = true;
        float textX = r.x + 12 + (lead ? 24 : 0);
        float innerW = inputArea.right() - textX - 10;
        std::string clickedText = field.display();
        float cursorX = Renderer::measure(clickedText.substr(0, field.cursor()), 13.5f);
        float shift = std::max(0.0f, cursorX - innerW + 4);
        float targetX = u.mouse.x - textX + shift;
        std::size_t best = 0;
        float previousWidth = 0;
        for (std::size_t i = 0; i < clickedText.size();) {
            std::size_t next = i + 1;
            while (next < clickedText.size() &&
                   (static_cast<unsigned char>(clickedText[next]) & 0xC0) == 0x80)
                ++next;
            float nextWidth = Renderer::measure(clickedText.substr(0, next), 13.5f);
            if (targetX < (previousWidth + nextWidth) * 0.5f) break;
            best = next;
            previousWidth = nextWidth;
            i = next;
        }
        field.setCursor(best);
    } else if (foc && u.clickAvailable() && !r.contains(u.mouse.x, u.mouse.y)) {
        // a click elsewhere (not consumed yet) removes focus
    }
    Color border = !error.empty() ? P().danger : foc ? P().primary : hov ? P().borderStrong : P().border;
    if (foc) Renderer::roundRect(r.inset(-3), Theme::radiusSm + 3, (error.empty() ? P().primary : P().danger).withAlpha(0.15f));
    Renderer::roundRect(r, Theme::radiusSm, P().inputBg);
    Renderer::roundRectOutline(r, Theme::radiusSm, border, foc ? 1.5f : 1.0f);
    float x = r.x + 12;
    if (lead) {
        icon(lead, x + 7, r.y + r.h / 2, 14, P().textFaint);
        x += 24;
    }
    Rect inner{x, r.y, inputArea.right() - x - 10, r.h};
    std::string shown = field.display();
    Renderer::pushClip(inner);
    float size = 13.5f;
    float baseline = r.y + r.h / 2 + size * 0.36f;
    if (shown.empty() && !foc) Renderer::text(x, baseline, placeholder, size, P().textFaint);
    else {
        std::string before = shown.substr(0, field.cursor());
        float cw = Renderer::measure(before, size);
        float shift = std::max(0.0f, cw - inner.w + 4);
        if (shown.empty()) Renderer::text(x, baseline, placeholder, size, P().textFaint.withAlpha(0.6f));
        else Renderer::text(x - shift, baseline, shown, size, P().text);
        if (foc && std::fmod(u.timeSec, 1.0) < 0.55) {
            float cx = x - shift + cw;
            Renderer::line(cx + 0.5f, r.y + 9, cx + 0.5f, r.bottom() - 9, P().primary, 1.5f);
        }
    }
    Renderer::popClip();
    if (field.isPassword()) {
        bool controlHovered = u.hover(passwordControl);
        if (controlHovered) Renderer::roundRect(passwordControl.inset(5, 6), 6, P().hover);
        icon(field.isPasswordVisible() ? "eye-off" : "eye", passwordControl.x + passwordControl.w / 2,
             passwordControl.y + passwordControl.h / 2, 16, controlHovered ? P().primary : P().textMuted);
        if (controlHovered) u.tooltip(field.isPasswordVisible() ? "Hide password" : "Show password");
    }
    if (!error.empty()) Renderer::text(r.x + 2, r.bottom() + 15, error, 11.5f, P().danger);
}

// ============================================================ Dropdown
// Only one dropdown list may be open at a time.
static Dropdown* g_openDropdown = nullptr;

bool Dropdown::draw(const Rect& r, const std::string& placeholder) {
    UI& u = UI::get();
    // The list is drawn (and clicked) in the popup pass at the END of the frame, so a selection made there
    // is reported by the next draw() call. Resetting the flag here, before it was read, lost every change
    // - the district / city lists of the register and report forms were never filled.
    changed_ = pendingChange_;
    pendingChange_ = false;
    bool hov = u.hover(r);
    if (hov) u.wantHand();
    if (u.clicked(r)) {
        open = !open;
        scroll = 0;
        if (open && selected > 6) scroll = (selected - 3) * 34.0f;
        if (open) {
            if (g_openDropdown && g_openDropdown != this) g_openDropdown->open = false;
            g_openDropdown = this;
        }
    }
    if (!open && g_openDropdown == this) g_openDropdown = nullptr;
    Color border = !error.empty() ? P().danger : open ? P().primary : hov ? P().borderStrong : P().border;
    Renderer::roundRect(r, Theme::radiusSm, P().inputBg);
    Renderer::roundRectOutline(r, Theme::radiusSm, border, open ? 1.5f : 1.0f);
    std::string shown = value();
    Renderer::textIn({r.x + 12, r.y, r.w - 40, r.h}, shown.empty() ? placeholder : shown, 13.5f,
                     shown.empty() ? P().textFaint : P().text);
    icon("chevron-down", r.right() - 18, r.y + r.h / 2, 12, P().textMuted);
    if (!error.empty()) Renderer::text(r.x + 2, r.bottom() + 15, error, 11.5f, P().danger);

    if (open) {
        const float itemH = 34;
        // an empty list shows one explanatory row instead of an 8 px sliver
        float listH = std::min<float>(std::max<size_t>(items.size(), 1) * itemH, 8 * itemH) + 8;
        float py = r.bottom() + 6;
        if (py + listH > Renderer::height() - 10) py = r.y - listH - 6;
        Rect pr{r.x, py, std::max(r.w, 180.0f), listH};
        u.deferPopup(pr, [this, pr, r, itemH]() {
            UI& uu = UI::get();
            Renderer::shadow(pr, 10, 16, 2.0f);
            Renderer::roundRect(pr, 10, P().surface);
            Renderer::roundRectOutline(pr, 10, P().border);
            float content = items.size() * itemH;
            if (uu.hover(pr) && uu.mouse.wheel != 0) {
                scroll -= uu.mouse.wheel * 60;
                uu.mouse.wheel = 0;
            }
            scroll = std::clamp(scroll, 0.0f, std::max(0.0f, content - (pr.h - 8)));
            Renderer::pushClip(pr.inset(4));
            if (items.empty())
                Renderer::textIn({pr.x + 14, pr.y + 4, pr.w - 28, itemH}, "Nothing to choose yet", 13, P().textFaint);
            for (int i = 0; i < (int)items.size(); ++i) {
                Rect ir{pr.x + 4, pr.y + 4 + i * itemH - scroll, pr.w - 8, itemH};
                if (ir.bottom() < pr.y || ir.y > pr.bottom()) continue;
                bool h = uu.hover(ir) && pr.contains(uu.mouse.x, uu.mouse.y);
                if (h || i == selected) Renderer::roundRect(ir, 6, i == selected ? P().selected : P().hover);
                Renderer::textIn({ir.x + 10, ir.y, ir.w - 20, ir.h}, items[i], 13, i == selected ? P().primaryText : P().text,
                                 i == selected ? Weight::SEMIBOLD : Weight::REGULAR);
                if (h) uu.wantHand();
                if (h && uu.clicked(ir)) {
                    if (selected != i) pendingChange_ = true;
                    selected = i;
                    open = false;
                    error.clear();
                }
            }
            Renderer::popClip();
            if (open && uu.clickAvailable() && !pr.contains(uu.mouse.x, uu.mouse.y) && !r.contains(uu.mouse.x, uu.mouse.y))
                open = false;
        });
    }
    bool c = changed_;
    return c;
}

// ============================================================ ScrollPanel
float ScrollPanel::begin(const Rect& r, float contentHeight) {
    UI& u = UI::get();
    r_ = r;
    content_ = contentHeight;
    if (u.hover(r) && u.mouse.wheel != 0) {
        float maxOffset = std::max(0.0f, content_ - r_.h);
        float nextOffset = std::clamp(offset - u.mouse.wheel * 70, 0.0f, maxOffset);
        if (nextOffset != offset) {
            wheelTarget = this;
            wheelDelta = u.mouse.wheel;
        }
    }
    offset = std::clamp(offset, 0.0f, std::max(0.0f, content_ - r.h));
    Renderer::pushClip(r);
    return r.y - offset;
}

void ScrollPanel::end() {
    UI& u = UI::get();
    if (wheelTarget == this) {
        offset -= wheelDelta * 70;
        u.mouse.wheel = 0;
        wheelTarget = nullptr;
        wheelDelta = 0;
    }
    float maxOffset = std::max(0.0f, content_ - r_.h);
    offset = std::clamp(offset, 0.0f, maxOffset);
    if (content_ > r_.h + 1) {   // thin scrollbar
        float th = std::max(30.0f, r_.h * r_.h / content_);
        float ty = r_.y + (r_.h - th) * (offset / (content_ - r_.h));
        Rect track{r_.right() - 14, r_.y, 14, r_.h};
        Rect thumb{track.x, ty, track.w, th};
        if (dragging_) {
            if (!u.mouse.down) dragging_ = false;
            else {
                offset = std::clamp(dragStartOffset_ + (u.mouse.y - dragStartY_) * maxOffset / (r_.h - th),
                                    0.0f, maxOffset);
                ty = r_.y + (r_.h - th) * (offset / maxOffset);
                thumb.y = ty;
            }
        } else if (u.mouse.pressed && u.clickAvailable() && u.hover(track)) {
            u.consumeClick();
            if (thumb.contains(u.mouse.x, u.mouse.y)) {
                dragging_ = true;
                dragStartY_ = u.mouse.y;
                dragStartOffset_ = offset;
            } else {
                offset = std::clamp(offset + (u.mouse.y < ty ? -r_.h * 0.8f : r_.h * 0.8f), 0.0f, maxOffset);
                thumb.y = r_.y + (r_.h - th) * (offset / maxOffset);
            }
        }
        if (u.hover(track)) u.wantHand();
        Renderer::popClip();
        Renderer::roundRect({r_.right() - 6, ty, 4, th}, 2, P().borderStrong.withAlpha(0.8f));
        return;
    }
    Renderer::popClip();
}

// ============================================================ Table
Cell textCell(const std::string& t, Weight w) { Cell c; c.text = t; c.weight = w; return c; }
Cell mutedCell(const std::string& t) { Cell c; c.text = t; c.fg = Theme::p().textMuted; return c; }
Cell badgeCell(const std::string& t, const Color& fg, const Color& bg) { Cell c; c.text = t; c.fg = fg; c.badgeBg = bg; return c; }

int Table::draw(const Rect& r, const std::vector<Column>& cols, const std::vector<std::vector<Cell>>& rows,
                bool sortable, int selectedRow) {
    UI& u = UI::get();
    headerClicked = false;
    float fixed = 0, frac = 0;
    for (const auto& c : cols) (c.width > 1 ? fixed : frac) += c.width;
    float avail = std::max(0.0f, r.w - 24 - fixed);
    std::vector<float> widths;
    for (const auto& c : cols) widths.push_back(c.width > 1 ? c.width : (frac > 0 ? avail * c.width / frac : 0));

    // header
    Rect head{r.x, r.y, r.w, headH};
    Renderer::rect(head, P().surfaceAlt);
    Renderer::line(r.x, head.bottom() - 0.5f, r.right(), head.bottom() - 0.5f, P().border);
    float x = r.x + 12;
    for (std::size_t i = 0; i < cols.size(); ++i) {
        Rect hc{x, head.y, widths[i], headH};
        std::string title = cols[i].title;
        if (sortable && sortColumn == (int)i) title += sortAscending ? "  \xE2\x86\x91" : "  \xE2\x86\x93";
        Renderer::textIn(hc.inset(6, 0), title, 11.5f, P().textMuted, Weight::SEMIBOLD, cols[i].align);
        if (sortable && u.hover(hc)) u.wantHand();
        if (sortable && u.clicked(hc)) {
            if (sortColumn == (int)i) sortAscending = !sortAscending;
            else { sortColumn = (int)i; sortAscending = true; }
            headerClicked = true;
        }
        x += widths[i];
    }

    Rect body{r.x, head.bottom(), r.w, r.h - headH};
    float y0 = scroll.begin(body, rows.size() * rowH);
    int clickedRow = -1;
    hoverRow = -1;
    int first = std::max(0, (int)((body.y - y0) / rowH) - 1);
    for (int ri = first; ri < (int)rows.size(); ++ri) {
        float y = y0 + ri * rowH;
        if (y > body.bottom()) break;
        Rect rr{r.x, y, r.w, rowH};
        Rect hitRow{rr.x, rr.y, std::max(0.0f, rr.w - 14), rr.h};
        bool hov = u.hover(hitRow) && body.contains(u.mouse.x, u.mouse.y);
        if (ri == selectedRow) Renderer::rect(rr, P().selected);
        else if (hov) Renderer::rect(rr, P().hover);
        if (hov) { hoverRow = ri; u.wantHand(); }
        if (hov && u.clicked(hitRow)) clickedRow = ri;
        float cx = r.x + 12;
        for (std::size_t ci = 0; ci < cols.size() && ci < rows[ri].size(); ++ci) {
            const Cell& c = rows[ri][ci];
            Rect cr{cx + 6, y, widths[ci] - 12, rowH};
            if (c.badgeBg.a > 0) {
                float bw = Renderer::measure(c.text, 11.5f, Weight::SEMIBOLD) + 22;
                float bx = cols[ci].align == Align::RIGHT ? cr.right() - bw : cols[ci].align == Align::CENTER ? cr.x + (cr.w - bw) / 2 : cr.x;
                badge(bx, y + rowH / 2, c.text, c.fg, c.badgeBg);
            } else {
                Renderer::textIn(cr, c.text, 13, c.fg.a > 0 ? c.fg : P().text, c.weight, cols[ci].align);
            }
            cx += widths[ci];
        }
        Renderer::line(r.x, y + rowH - 0.5f, r.right(), y + rowH - 0.5f, P().border.withAlpha(0.7f));
    }
    scroll.end();
    return clickedRow;
}

int pager(const Rect& r, int page, int pages, int total) {
    Renderer::textIn({r.x, r.y, 300, r.h}, "Page " + std::to_string(page + 1) + " of " + std::to_string(std::max(1, pages)) +
                                               "   \xC2\xB7   " + std::to_string(total) + " records",
                     12.5f, P().textMuted);
    Rect next{r.right() - 92, r.y, 92, r.h};
    Rect prev{r.right() - 192, r.y, 92, r.h};
    if (button(prev, "Previous", ButtonStyle::SECONDARY, page > 0)) return page - 1;
    if (button(next, "Next", ButtonStyle::SECONDARY, page + 1 < pages)) return page + 1;
    return page;
}

// ============================================================ Modal
bool modalBegin(const Rect& r, const std::string& title, bool& open) {
    if (!open) return false;
    UI& u = UI::get();
    u.setModal(r);
    Renderer::rect({0, 0, (float)Renderer::width(), (float)Renderer::height()}, P().overlay);
    Renderer::shadow(r, Theme::radiusLg, 24, 3.0f);
    Renderer::roundRect(r, Theme::radiusLg, P().surface);
    Renderer::text(r.x + 24, r.y + 36, title, 17, P().text, Weight::SEMIBOLD);
    Rect close{r.right() - 44, r.y + 14, 30, 30};
    if (u.hover(close)) Renderer::roundRect(close, 8, P().hover);
    icon("x", close.x + 15, close.y + 15, 12, P().textMuted);
    if (u.hover(close)) u.wantHand();
    if (u.clicked(close)) { open = false; return false; }
    Renderer::line(r.x, r.y + 58, r.right(), r.y + 58, P().border);
    return true;
}

void modalEnd() {}

} // namespace ui
