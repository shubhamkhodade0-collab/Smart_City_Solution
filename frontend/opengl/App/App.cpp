#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include "App.h"
#include "../../../backend/AsmEngine.h"
#include "../../../backend/Config.h"
#include "../../../backend/Exceptions.h"
#include "../../../backend/Validator.h"
#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace ui;
using namespace scms;

static const Palette& P() { return Theme::p(); }

#ifdef _WIN32
static bool readClipboardText(std::string& text) {
    if (!OpenClipboard(nullptr)) return false;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (!data) {
        CloseClipboard();
        return false;
    }
    const wchar_t* wide = static_cast<const wchar_t*>(GlobalLock(data));
    if (!wide) {
        CloseClipboard();
        return false;
    }
    int byteCount = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, nullptr, 0, nullptr, nullptr);
    if (byteCount <= 0) {
        GlobalUnlock(data);
        CloseClipboard();
        return false;
    }
    std::string converted(static_cast<std::size_t>(byteCount), '\0');
    int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide, -1, &converted[0], byteCount, nullptr, nullptr);
    GlobalUnlock(data);
    CloseClipboard();
    if (written <= 0) return false;
    converted.resize(static_cast<std::size_t>(written - 1));
    text.swap(converted);
    return true;
}
#endif

static DbConfig makeDbConfig() {
    Config& c = Config::instance();
    return {c.get("DB_HOST", "localhost"), c.get("DB_PORT", "5433"), c.get("DB_NAME", "scms"),
            c.get("DB_USER", "scms_app"), c.get("DB_PASSWORD", "")};
}

App::App() : db_(makeDbConfig()), svc_(db_), reports_(db_, svc_), agent_(db_, svc_) {}

bool App::guarded(const std::function<void()>& f) {
    try {
        f();
        return true;
    } catch (const NotConnectedException& e) {
        dbOnline_ = false;
        dbError_ = e.what();
        UI::get().toast(std::string("Database offline: ") + e.what(), ToastKind::ERR);
    } catch (const AppException& e) {
        UI::get().toast(std::string(e.kind()) + ": " + e.what(), ToastKind::ERR);
    } catch (const std::exception& e) {
        UI::get().toast(std::string("Unexpected error: ") + e.what(), ToastKind::ERR);
    }
    return false;
}

bool App::init() {
    groq_ = ai::loadLlmSettings();   // optional Groq key: process environment, legacy groq.conf, .env, or legacy app.conf
    Config& cfg = Config::instance();
    Renderer::init(cfg.get("FONT_REGULAR", "C:/Windows/Fonts/segoeui.ttf"), cfg.get("FONT_SEMIBOLD", "C:/Windows/Fonts/seguisb.ttf"),
                   cfg.get("FONT_BOLD", "C:/Windows/Fonts/segoeuib.ttf"));
    asmOnline_ = AsmEngine::selfTest();
    dbOnline_ = db_.connect();
    if (!dbOnline_) dbError_ = db_.lastError();
    else guarded([&] { svc_.loadReferenceData(); });
    try {
        india_.load(cfg.get("MAP_FILE", "opengl/data/india.map"));
    } catch (const AppException& e) {
        mapError_ = e.what();
    }
    // the search box behaves like a form: Enter runs the search
    UI::get().keys.onSubmit = [this]() {
        TextField* f = UI::get().keys.focused();
        if (f == &topSearch_.field) runSearch();
        else if (f == &lUser_.field || f == &lPass_.field) {
            loginError_.clear();
            if (guarded([&] {
                    user_ = svc_.login(lUser_.text(), lPass_.field.text());
                }) && user_) {
                lPass_.field.clear();
                UI::get().keys.blur();
                guarded([&] { svc_.loadRuntime(*user_); });
                markDirty();
                refreshNotifications();
                go(Screen::DASHBOARD);
            }
        } else if (f == &listSearch_.field) { listPage_ = 0; listDirty_ = true; UI::get().keys.blur(); }
        else if (f == &rankSearch_.field) { rankLoaded_ = false; UI::get().keys.blur(); }
        else if (f == &aiInput_.field) {   // AI assistant: Enter sends the question
            if (aiPending_.job) return;     // still waiting for the previous answer
            std::string q = aiInput_.text();
            aiInput_.field.clear();
            aiSend(q);
        }
        else if (f == &dsaKey_.field) { UI::get().keys.blur(); }
    };
    // Global shortcuts only fire when NO text field is focused - and none use letters/digits.
    UI::get().keys.onGlobalKey = [this](Key k) {
        if (k == Key::F5 && user_) { markDirty(); refreshNotifications(); UI::get().toast("Data refreshed from PostgreSQL", ToastKind::INFO); }
        if (k == Key::ESCAPE) { confirmOpen_ = submitOpen_ = userModal_ = false; }
    };
    if (dbOnline_) {
        guarded([&] {
            Summary s = db_.getSummary({});
            publicTotal_ = s.total;
            int st = 0;
            for (const auto& r : db_.stateStats({})) st += r.complaints > 0;
            publicStates_ = st;
        });
    }
    return true;
}

void App::markDirty() {
    dashLoaded_ = false;
    dsaLoaded_ = false;
    listDirty_ = true;
    mapLoaded_ = false;
    rankLoaded_ = false;
    aiTriageLoaded_ = false;
    deptLoaded_ = false;
    reportLoaded_ = false;
    usersLoaded_ = false;
    auditLoaded_ = false;
    gridLoaded_ = false;
    if (user_) {
        guarded([&] { svc_.loadRuntime(*user_); });
    }
}

void App::go(Screen s) {
    UI::get().keys.blur();
    if (s != screen_) pageScroll_.offset = 0;
    if (s == Screen::COMPLAINTS) listDirty_ = true;
    screen_ = s;
}

void App::logout() {
    if (user_) guarded([&] { svc_.logout(*user_); });
    user_.reset();
    detail_.reset();
    aiReset();
    viewScope_ = 0;
    list_.clear();
    recent_.clear();
    notes_.clear();
    unread_ = 0;
    markDirty();
    lUser_.field.clear();
    lPass_.field.clear();
    go(Screen::LOGIN);
}

void App::refreshNotifications() {
    if (!user_) return;
    guarded([&] {
        notes_ = db_.getNotifications(user_->id(), 100);
        unread_ = db_.unreadCount(user_->id());
    });
}

void App::tick() {
    double now = UI::get().timeSec;
    if (now - lastTick_ < 20) return;   // controlled refresh: every 20 s, never per frame
    lastTick_ = now;
    if (!db_.isConnected()) {
        dbOnline_ = db_.ensureConnected();
        if (dbOnline_) UI::get().toast("Reconnected to PostgreSQL", ToastKind::SUCCESS);
        return;
    }
    refreshNotifications();
}

// ============================================================ navigation
std::vector<NavItem> App::navItems() const {
    if (!user_) return {};
    switch (user_->role()) {
        case Role::CITIZEN:
            return {{Screen::DASHBOARD, "Dashboard", "grid", "MENU"},
                    {Screen::REPORT, "Report Complaint", "plus", "MENU"},
                    {Screen::COMPLAINTS, "My Complaints", "list", "MENU"},
                    {Screen::NOTIFICATIONS, "Notifications", "bell", "MENU"},
                    {Screen::MAP, "India Map", "map", "INSIGHTS"},
                    {Screen::RANKING, "City Ranking", "trophy", "INSIGHTS"},
                    {Screen::ASSISTANT, "AI Assistant", "ai", "INSIGHTS"},
                    {Screen::PROFILE, "Profile", "user", "ACCOUNT"},
                    {Screen::SETTINGS, "Settings", "settings", "ACCOUNT"}};
        case Role::DEPARTMENT:
            return {{Screen::DASHBOARD, "Dashboard", "grid", "MENU"},
                    {Screen::COMPLAINTS, "Work Queue", "list", "MENU"},
                    {Screen::REPORTS, "Statistics", "chart", "MENU"},
                    {Screen::NOTIFICATIONS, "Notifications", "bell", "MENU"},
                    {Screen::MAP, "India Map", "map", "INSIGHTS"},
                    {Screen::RANKING, "City Ranking", "trophy", "INSIGHTS"},
                    {Screen::ASSISTANT, "AI Assistant", "ai", "INSIGHTS"},
                    {Screen::PROFILE, "Profile", "user", "ACCOUNT"},
                    {Screen::SETTINGS, "Settings", "settings", "ACCOUNT"}};
        case Role::ADMIN:
            return {{Screen::DASHBOARD, "Dashboard", "grid", "OVERVIEW"},
                    {Screen::COMPLAINTS, "All Complaints", "list", "OVERVIEW"},
                    {Screen::QUEUE, "Processing Queue", "queue", "OVERVIEW"},
                    {Screen::MAP, "India Map", "map", "OVERVIEW"},
                    {Screen::RANKING, "City Ranking", "trophy", "ANALYTICS"},
                    {Screen::DEPARTMENTS, "Departments", "building", "ANALYTICS"},
                    {Screen::REPORTS, "Reports", "chart", "ANALYTICS"},
                    {Screen::ANALYTICS3D, "3D Analytics", "cube", "ANALYTICS"},
                    {Screen::USERS, "Users", "users", "ADMINISTRATION"},
                    {Screen::AUDIT, "Audit Logs", "shield", "ADMINISTRATION"},
                    {Screen::SETTINGS, "System & Settings", "settings", "ENGINES"}};
    }
    return {};
}

std::string App::screenTitle(Screen s) const {
    for (const auto& n : navItems())
        if (n.screen == s) return n.label;
    switch (s) {
        case Screen::DETAIL: return "Complaint Details";
        case Screen::SEARCH: return "Search Results";
        case Screen::NOTIFICATIONS: return "Notifications";
        default: return "";
    }
}

Rect App::content() const {
    return {Theme::sidebarW, Theme::topbarH, width_ - Theme::sidebarW, height_ - Theme::topbarH};
}

void App::drawSidebar() {
    UI& u = UI::get();
    Rect sb{0, 0, Theme::sidebarW, (float)height_};
    Renderer::rect(sb, P().sidebar);
    Renderer::line(sb.right() - 0.5f, 0, sb.right() - 0.5f, (float)height_, P().border);
    // brand
    Renderer::roundRect({20, 18, 34, 34}, 9, P().primary);
    icon("building", 37, 35, 18, hex(0xFFFFFF));
    Renderer::text(64, 33, "SmartCity", 16, P().text, Weight::BOLD);
    Renderer::text(64, 49, "Complaint Management", 11, P().textMuted);

    float y = 78;
    std::string section;
    // when the window is short (about 760 px) the admin menu does not fit: shrink the item spacing just enough
    // so the last entry never ends up behind the user card (normal-size windows keep the 40 px pitch)
    float pitch = 40;
    {
        std::string sec;
        int items = 0, sections = 0;
        for (const auto& n : navItems()) { ++items; if (sec != n.section) { sec = n.section; ++sections; } }
        float avail = (height_ - 74.0f - 8.0f) - 78.0f;
        float need = sections * 26.0f + items * 40.0f;
        if (need > avail && items > 0) pitch = std::max(32.0f, (avail - sections * 26.0f) / items);
    }
    const float itemH = pitch - 2;
    for (const auto& n : navItems()) {
        if (section != n.section) {
            section = n.section;
            y += 14;
            Renderer::text(24, y + 4, section, 10.5f, P().textFaint, Weight::SEMIBOLD);
            y += 12;
        }
        Rect item{12, y, Theme::sidebarW - 24, itemH};
        bool active = screen_ == n.screen || (screen_ == Screen::DETAIL && n.screen == backFromDetail_);
        bool hov = u.hover(item);
        if (active) Renderer::roundRect(item, 8, P().primarySoft);
        else if (hov) Renderer::roundRect(item, 8, P().hover);
        Color fg = active ? P().primaryText : hov ? P().text : P().textMuted;
        icon(n.icon, item.x + 22, item.y + itemH / 2, 15, active ? P().primary : fg);
        Renderer::text(item.x + 44, item.y + itemH / 2 + 5, n.label, 13.5f, fg, active ? Weight::SEMIBOLD : Weight::REGULAR);
        if (n.screen == Screen::NOTIFICATIONS && unread_ > 0) {
            std::string c = std::to_string(unread_);
            float bw = std::max(22.0f, Renderer::measure(c, 11, Weight::SEMIBOLD) + 12);
            float by = item.y + (itemH - 18) / 2;
            Renderer::roundRect({item.right() - bw - 10, by, bw, 18}, 9, P().danger);
            Renderer::textIn({item.right() - bw - 10, by, bw, 18}, c, 11, hex(0xFFFFFF), Weight::SEMIBOLD, Align::CENTER);
        }
        if (hov) u.wantHand();
        if (u.clicked(item)) {
            if (n.screen == Screen::COMPLAINTS) { listPage_ = 0; listDirty_ = true; }
            go(n.screen);
        }
        y += pitch;
    }

    // user card
    Rect uc{12, height_ - 74.0f, Theme::sidebarW - 24, 60};
    Renderer::roundRect(uc, 10, P().surfaceAlt);
    Renderer::roundRectOutline(uc, 10, P().border);
    Renderer::circle(uc.x + 28, uc.y + 30, 17, P().primary.withAlpha(0.15f));
    Renderer::textIn({uc.x + 11, uc.y + 13, 34, 34}, user_->initials(), 12.5f, P().primary, Weight::BOLD, Align::CENTER);
    Renderer::textIn({uc.x + 54, uc.y + 10, uc.w - 100, 20}, user_->fullName(), 13, P().text, Weight::SEMIBOLD);
    Renderer::textIn({uc.x + 54, uc.y + 30, uc.w - 100, 18}, user_->portalName(), 11.5f, P().textMuted);
    Rect lo{uc.right() - 40, uc.y + 15, 30, 30};
    if (u.hover(lo)) { Renderer::roundRect(lo, 8, P().hover); u.tooltip("Sign out"); u.wantHand(); }
    icon("logout", lo.x + 15, lo.y + 15, 14, P().textMuted);
    if (u.clicked(lo)) logout();
}

void App::drawTopbar() {
    UI& u = UI::get();
    Rect tb{Theme::sidebarW, 0, width_ - Theme::sidebarW, Theme::topbarH};
    Renderer::rect(tb, P().surface);
    Renderer::line(tb.x, tb.bottom() - 0.5f, tb.right(), tb.bottom() - 0.5f, P().border);
    Renderer::text(tb.x + 28, 39, screenTitle(screen_), 18, P().text, Weight::SEMIBOLD);

    float x = tb.right() - 24;
    // notifications bell
    Rect bell{x - 38, 13, 38, 38};
    if (u.hover(bell)) { Renderer::roundRect(bell, 9, P().hover); u.wantHand(); u.tooltip("Notifications"); }
    icon("bell", bell.x + 19, bell.y + 19, 16, P().textMuted);
    if (unread_ > 0) {
        std::string c = unread_ > 99 ? "99+" : std::to_string(unread_);
        float bw = std::max(18.0f, Renderer::measure(c, 10, Weight::SEMIBOLD) + 10);
        Renderer::roundRect({bell.x + 20, bell.y + 4, bw, 17}, 8.5f, P().danger);
        Renderer::textIn({bell.x + 20, bell.y + 4, bw, 17}, c, 10, hex(0xFFFFFF), Weight::SEMIBOLD, Align::CENTER);
    }
    if (u.clicked(bell)) go(Screen::NOTIFICATIONS);
    x -= 46;
    // theme toggle
    Rect th{x - 38, 13, 38, 38};
    if (u.hover(th)) { Renderer::roundRect(th, 9, P().hover); u.wantHand(); u.tooltip(Theme::dark() ? "Light mode" : "Dark mode"); }
    icon(Theme::dark() ? "sun" : "moon", th.x + 19, th.y + 19, 16, P().textMuted);
    if (u.clicked(th)) Theme::toggle();
    x -= 50;
    // DB status pill
    std::string st = dbOnline_ ? "PostgreSQL live" : "Database offline";
    float pw = Renderer::measure(st, 12, Weight::SEMIBOLD) + 32;
    Rect pill{x - pw, 20, pw, 24};
    Renderer::roundRect(pill, 12, dbOnline_ ? P().successSoft : P().dangerSoft);
    Renderer::circle(pill.x + 13, pill.y + 12, 3.5f, dbOnline_ ? P().success : P().danger);
    Renderer::text(pill.x + 22, pill.y + 16.5f, st, 12, dbOnline_ ? P().success : P().danger, Weight::SEMIBOLD);
    if (u.hover(pill)) u.tooltip(dbOnline_ ? db_.connectionInfo() : dbError_);
    x = pill.x - 16;
    // global search
    float sw = std::min(380.0f, x - tb.x - 260);
    Rect search{x - sw, 14, sw, 36};
    topSearch_.draw(search, "Search complaints, cities, IDs...", "search");
}

void App::drawShell() {
    drawSidebar();
    drawTopbar();
}

// ============================================================ GLUT callbacks
void App::reshape(int w, int h) {
    // Keep the layout in logical pixels while GLUT reports physical pixels on high-DPI displays.
    int logicalW = static_cast<int>(std::lround(w / pixelScale_));
    int logicalH = static_cast<int>(std::lround(h / pixelScale_));
    winH_ = logicalH;
    width_ = std::max(logicalW, MIN_W);
    height_ = std::max(logicalH, MIN_H);
    if (logicalW < MIN_W || logicalH < MIN_H)
        glutReshapeWindow(static_cast<int>(width_ * pixelScale_), static_cast<int>(height_ * pixelScale_));
}

void App::display() {
    UI& u = UI::get();
    u.beginFrame(glutGet(GLUT_ELAPSED_TIME) / 1000.0);
    const Color& bg = P().bg;
    glClearColor(bg.r, bg.g, bg.b, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Renderer::beginFrame(width_, height_, winH_, pixelScale_);
    aiPoll();   // pick up a finished Groq answer / connection test from the worker thread

    if (!user_) {
        if (screen_ == Screen::REGISTER) screenRegister();
        else screenLogin();
    } else {
        Rect c = content();
        Renderer::pushClip(c);
        switch (screen_) {
            case Screen::DASHBOARD: screenDashboard(); break;
            case Screen::REPORT: screenReport(); break;
            case Screen::COMPLAINTS: screenComplaints(); break;
            case Screen::DETAIL: screenDetail(); break;
            case Screen::NOTIFICATIONS: screenNotifications(); break;
            case Screen::PROFILE: screenProfile(); break;
            case Screen::SETTINGS: screenSettings(); break;
            case Screen::QUEUE: screenQueue(); break;
            case Screen::MAP: screenMap(); break;
            case Screen::RANKING: screenRanking(); break;
            case Screen::DEPARTMENTS: screenDepartments(); break;
            case Screen::REPORTS: screenReports(); break;
            case Screen::USERS: screenUsers(); break;
            case Screen::AUDIT: screenAudit(); break;
            case Screen::DSA: screenDsa(); break;
            case Screen::ASSEMBLY: screenAssembly(); break;
            case Screen::ANALYTICS3D: screen3D(); break;
            case Screen::SEARCH: screenSearch(); break;
            case Screen::ASSISTANT: screenAssistant(); break;
            default: screenDashboard(); break;
        }
        Renderer::popClip();
        if (user_) drawShell();
    }
    // a click that no text field claimed removes keyboard focus
    if (u.mouse.pressed && u.clickAvailable()) u.keys.blur();
    u.endFrame();
    u.mouse.pressed = u.mouse.released = u.mouse.rightPressed = false;
    u.mouse.wheel = 0;
    glutSwapBuffers();
}

void App::mouseButton(int button, int state, int x, int y) {
    Mouse& m = UI::get().mouse;
    m.x = x / pixelScale_;
    m.y = y / pixelScale_;
    if (button == GLUT_LEFT_BUTTON) {
        m.down = state == GLUT_DOWN;
        if (state == GLUT_DOWN) m.pressed = true; else m.released = true;
    } else if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) m.rightPressed = true;
    else if ((button == 3 || button == 4) && state == GLUT_DOWN) m.wheel += button == 3 ? 1.0f : -1.0f;
    glutPostRedisplay();
}

void App::mouseMove(int x, int y) {
    Mouse& m = UI::get().mouse;
    m.x = x / pixelScale_;
    m.y = y / pixelScale_;
    glutPostRedisplay();
}

void App::wheel(int dir, int x, int y) {
    Mouse& m = UI::get().mouse;
    m.x = x / pixelScale_;
    m.y = y / pixelScale_;
    m.wheel += dir > 0 ? 1.0f : -1.0f;
    glutPostRedisplay();
}

void App::keyChar(unsigned char c) {
    // KeyRouter: focused text field first; global handler only when nothing is focused
#ifdef _WIN32
    if (c == 22 && UI::get().keys.hasFocus()) {
        std::string text;
        if (readClipboardText(text)) UI::get().keys.handlePaste(text);
        else UI::get().toast("Could not read text from the clipboard.", ToastKind::ERR);
        glutPostRedisplay();
        return;
    }
#endif
    UI::get().keys.handleChar(static_cast<char>(c));
    glutPostRedisplay();
}

void App::keySpecial(int key) {
    Key k = Key::NONE;
    switch (key) {
        case GLUT_KEY_LEFT: k = Key::LEFT; break;
        case GLUT_KEY_RIGHT: k = Key::RIGHT; break;
        case GLUT_KEY_HOME: k = Key::HOME; break;
        case GLUT_KEY_END: k = Key::END; break;
        case GLUT_KEY_DELETE: k = Key::DEL; break;
        case GLUT_KEY_F5: k = Key::F5; break;
        default: break;
    }
    UI::get().keys.handleKey(k);
    glutPostRedisplay();
}

// ============================================================ shared helpers
Color App::statusColor(Status s, bool soft) {
    switch (s) {
        case Status::PENDING: return soft ? P().warningSoft : P().warning;
        case Status::ASSIGNED: return soft ? P().infoSoft : P().info;
        case Status::IN_PROGRESS: return soft ? P().primarySoft : P().primary;
        case Status::RESOLVED: return soft ? P().successSoft : P().success;
        case Status::CLOSED: return soft ? P().neutralSoft : P().textMuted;
    }
    return P().textMuted;
}

Color App::severityColor(SeverityLevel l, bool soft) {
    switch (l) {
        case SeverityLevel::LOW: return soft ? P().successSoft : P().success;
        case SeverityLevel::MEDIUM: return soft ? P().infoSoft : P().info;
        case SeverityLevel::HIGH: return soft ? P().warningSoft : P().warning;
        case SeverityLevel::CRITICAL: return soft ? P().dangerSoft : P().danger;
    }
    return P().textMuted;
}

std::string App::fmtTime(std::time_t t, bool withTime) {
    char b[40];
    std::strftime(b, sizeof b, withTime ? "%d %b %Y, %H:%M" : "%d %b %Y", std::localtime(&t));
    return b;
}

std::vector<Cell> App::complaintRow(const Complaint& c, bool showDept) {
    std::string sev = std::string(toString(c.severityLevel())) + " " + std::to_string(c.severityScore());
    std::vector<Cell> row{textCell(c.referenceCode(), Weight::SEMIBOLD), textCell(c.typeName()),
                          textCell(c.location().city + ", " + c.location().state),
                          badgeCell(sev, severityColor(c.severityLevel(), false), severityColor(c.severityLevel(), true)),
                          badgeCell(displayName(c.status()), statusColor(c.status(), false), statusColor(c.status(), true))};
    if (showDept) row.push_back(mutedCell(c.departmentName().empty() ? "Not assigned" : c.departmentName()));
    row.push_back(mutedCell(fmtTime(c.createdAt(), false)));
    return row;
}

void App::complaintTable(const Rect& r, std::vector<std::unique_ptr<Complaint>>& rows, Table& table, bool showDept) {
    std::vector<Column> cols{{"REFERENCE", 150}, {"CATEGORY", 0.5f}, {"LOCATION", 1.0f}, {"SEVERITY", 128},
                             {"STATUS", 122}};
    if (showDept) cols.push_back({"DEPARTMENT", 0.85f});
    cols.push_back({"CREATED", 108});
    std::vector<std::vector<Cell>> cells;
    for (const auto& c : rows) cells.push_back(complaintRow(*c, showDept));
    int clicked = table.draw(r, cols, cells);
    if (clicked >= 0 && clicked < (int)rows.size()) {
        backFromDetail_ = screen_ == Screen::DETAIL ? backFromDetail_ : screen_;
        openDetail(rows[clicked]->id());
    }
}

void App::statusStepper(const Rect& r, Status s) {
    const auto& all = allStatuses();
    int cur = static_cast<int>(s);
    float step = r.w / (all.size() - 1);
    for (std::size_t i = 0; i + 1 < all.size(); ++i)
        Renderer::roundRect({r.x + step * i, r.y + 13, step, 4}, 2, (int)i < cur ? P().success : P().border);
    for (std::size_t i = 0; i < all.size(); ++i) {
        float cx = r.x + step * i;
        bool done = (int)i < cur, now = (int)i == cur;
        Renderer::circle(cx, r.y + 15, 13, done ? P().success : now ? P().primary : P().surface);
        if (!done && !now) Renderer::circle(cx, r.y + 15, 12, P().border), Renderer::circle(cx, r.y + 15, 10.5f, P().surface);
        if (done) icon("check", cx, r.y + 15, 11, hex(0xFFFFFF));
        else Renderer::textIn({cx - 13, r.y + 2, 26, 26}, std::to_string(i + 1), 11.5f, now ? hex(0xFFFFFF) : P().textMuted,
                              Weight::SEMIBOLD, Align::CENTER);
        std::string lab = displayName(all[i]);
        float tw = Renderer::measure(lab, 12, Weight::SEMIBOLD);
        float lx = std::clamp(cx - tw / 2, r.x - 10, r.right() - tw + 10);
        Renderer::text(lx, r.y + 48, lab, 12, now ? P().text : P().textMuted, now ? Weight::SEMIBOLD : Weight::REGULAR);
    }
}

void App::loadStatesInto(Dropdown& dd, std::vector<IdName>& store) {
    guarded([&] {
        store = db_.getStates();
        dd.items.clear();
        for (const auto& s : store) dd.items.push_back(s.name);
    });
}

void App::runSearch() {
    if (!user_) return;
    searchedFor_ = topSearch_.text();
    if (Validator::trim(searchedFor_).empty()) return;
    guarded([&] { search_ = svc_.search(*user_, searchedFor_); });
    UI::get().keys.blur();
    go(Screen::SEARCH);
}
