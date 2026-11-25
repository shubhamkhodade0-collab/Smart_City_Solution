// Screens used by every role: auth, dashboards, report form, lists, details, notifications, search
#include "App.h"
#include "../../../backend/AsmEngine.h"
#include "../../../backend/Exceptions.h"
#include "../../../backend/Validator.h"
#include <GL/freeglut.h>
#include <algorithm>
#include <cstdio>

using namespace ui;
using namespace scms;

static const Palette& P() { return Theme::p(); }
static std::string num(long long v) { return std::to_string(v); }

// ============================================================ LOGIN
void App::screenLogin() {
    float W = (float)width_, H = (float)height_;
    // brand panel
    Rect brand{0, 0, W * 0.44f, H};
    Renderer::vgradient(brand, hex(0x1E1B4B), hex(0x3730A3));
    for (int i = 0; i < 7; ++i)   // subtle concentric rings as texture
        Renderer::ring(brand.w * 0.82f, H * 0.82f, 90.0f + i * 70, 91.5f + i * 70, 0, 6.2832f, hex(0xFFFFFF, 0.05f));
    Renderer::roundRect({56, 56, 40, 40}, 10, hex(0xFFFFFF, 0.14f));
    icon("building", 76, 76, 20, hex(0xFFFFFF));
    Renderer::text(108, 82, "SmartCity", 20, hex(0xFFFFFF), Weight::BOLD);
    float y = H * 0.30f;
    Renderer::text(56, y, "Every civic complaint,", 34, hex(0xFFFFFF), Weight::BOLD);
    Renderer::text(56, y + 46, "tracked to resolution.", 34, hex(0xFFFFFF), Weight::BOLD);
    auto lines = Renderer::wrap("Report problems in your city, follow them through every department and see live statistics "
                                "for all of India - powered by PostgreSQL, C++ and native OpenGL.",
                                brand.w - 112, 15);
    float ly = y + 92;
    for (const auto& l : lines) { Renderer::text(56, ly, l, 15, hex(0xC7D2FE)); ly += 24; }
    const char* feats[] = {"Citizen, Department and Administrator portals", "Live India map from real district boundaries",
                           "FIFO routing to the right department, full audit trail"};
    ly += 18;
    for (const char* f : feats) {
        Renderer::circle(66, ly - 5, 10, hex(0xFFFFFF, 0.14f));
        icon("check", 66, ly - 5, 10, hex(0xFFFFFF));
        Renderer::text(88, ly, f, 14, hex(0xE0E7FF));
        ly += 34;
    }
    if (publicTotal_ >= 0) {
        Rect st{56, H - 124, 150, 70};
        Renderer::roundRect(st, 12, hex(0xFFFFFF, 0.10f));
        Renderer::text(st.x + 16, st.y + 34, num(publicTotal_), 24, hex(0xFFFFFF), Weight::BOLD);
        Renderer::text(st.x + 16, st.y + 56, "complaints tracked", 12, hex(0xC7D2FE));
        Rect s2{222, H - 124, 150, 70};
        Renderer::roundRect(s2, 12, hex(0xFFFFFF, 0.10f));
        Renderer::text(s2.x + 16, s2.y + 34, num(publicStates_), 24, hex(0xFFFFFF), Weight::BOLD);
        Renderer::text(s2.x + 16, s2.y + 56, "states reporting", 12, hex(0xC7D2FE));
    }

    // form
    float formW = std::min(480.0f, W - brand.w - 80.0f);
    float fx = brand.w + (W - brand.w - formW) / 2, fy = std::max(64.0f, H * 0.18f);
    Renderer::text(fx, fy, "Sign in", 30, P().text, Weight::BOLD);
    Renderer::text(fx, fy + 32, "Use your SmartCity account to continue.", 15, P().textMuted);
    fy += 72;
    Renderer::text(fx, fy, "Username", 14, P().text, Weight::SEMIBOLD);
    lUser_.draw({fx, fy + 10, formW, 48}, "e.g. citizen", "user");
    fy += 82;
    Renderer::text(fx, fy, "Password", 14, P().text, Weight::SEMIBOLD);
    lPass_.draw({fx, fy + 10, formW, 48}, "Your password", "shield");
    fy += 78;
    if (!loginError_.empty()) {
        Renderer::roundRect({fx, fy, formW, 42}, 8, P().dangerSoft);
        Renderer::textIn({fx + 14, fy, formW - 28, 42}, loginError_, 14, P().danger, Weight::SEMIBOLD);
        fy += 54;
    }
    if (button({fx, fy, formW, 50}, "Sign in", ButtonStyle::PRIMARY, dbOnline_)) {
        loginError_.clear();
        try {
            user_ = svc_.login(lUser_.text(), lPass_.field.text());
            lPass_.field.clear();
            svc_.loadRuntime(*user_);
            markDirty();
            refreshNotifications();
            go(Screen::DASHBOARD);
        } catch (const AppException& e) {
            loginError_ = e.what();
            user_.reset();
        }
    }
    fy += 68;
    Renderer::text(fx, fy, "New citizen?", 14.5f, P().textMuted);
    if (linkText(fx + Renderer::measure("New citizen? ", 14.5f), fy, "Create an account", 14.5f)) {
        loadStatesInto(gState_, gStates_);
        go(Screen::REGISTER);
    }
    fy += 30;
    if (!dbOnline_) {
        Rect off{fx, fy, formW, 56};
        Renderer::roundRect(off, 10, P().dangerSoft);
        Renderer::text(off.x + 14, off.y + 22, "PostgreSQL is not reachable", 13, P().danger, Weight::SEMIBOLD);
        Renderer::text(off.x + 14, off.y + 42, "Start it with: bash database/setup_db.sh start", 12, P().danger);
        if (button({off.right() - 96, off.y + 12, 84, 32}, "Retry", ButtonStyle::SECONDARY)) {
            dbOnline_ = db_.connect();
            if (dbOnline_) { guarded([&] { svc_.loadReferenceData(); }); UI::get().toast("Connected to PostgreSQL", ToastKind::SUCCESS); }
            else dbError_ = db_.lastError();
        }
    }
}

// ============================================================ REGISTER
void App::screenRegister() {
    float W = (float)width_, H = (float)height_;
    Rect cardR{(W - 640) / 2, std::max(30.0f, (H - 640) / 2), 640, 640};
    card(cardR);
    float x = cardR.x + 36, y = cardR.y + 50;
    Renderer::text(x, y, "Create a citizen account", 22, P().text, Weight::BOLD);
    Renderer::text(x, y + 26, "Report and track complaints in your city.", 13.5f, P().textMuted);
    y += 60;
    float cw = (cardR.w - 72 - 20) / 2;
    auto label = [&](float lx, float ly, const char* t) { Renderer::text(lx, ly, t, 12.5f, P().text, Weight::SEMIBOLD); };
    label(x, y, "Full name");             gName_.draw({x, y + 8, cw, 40}, "Your full name");
    label(x + cw + 20, y, "Username");    gUser_.draw({x + cw + 20, y + 8, cw, 40}, "lowercase, e.g. riya.k");
    y += 84;
    label(x, y, "Mobile number");         gPhone_.draw({x, y + 8, cw, 40}, "10-digit mobile");
    label(x + cw + 20, y, "State");
    if (gState_.draw({x + cw + 20, y + 8, cw, 40}, "Select state")) {
        gDistrict_.items.clear(); gDistrict_.selected = -1; gCity_.items.clear(); gCity_.selected = -1;
        guarded([&] { gDistricts_ = db_.getDistricts(gStates_[gState_.selected].id); for (auto& d : gDistricts_) gDistrict_.items.push_back(d.name); });
    }
    y += 84;
    label(x, y, "District");
    if (gDistrict_.draw({x, y + 8, cw, 40}, gState_.selected < 0 ? "Select a state first" : "Select district")) {
        gCity_.items.clear(); gCity_.selected = -1;
        guarded([&] { gCities_ = db_.getCities(gDistricts_[gDistrict_.selected].id); for (auto& c : gCities_) gCity_.items.push_back(c.name); });
        if (gCity_.items.size() == 1) gCity_.selected = 0;
    }
    label(x + cw + 20, y, "City");        gCity_.draw({x + cw + 20, y + 8, cw, 40}, "Select city");
    y += 84;
    label(x, y, "Password");              gPass_.draw({x, y + 8, cw, 40}, "Min 8, Aa1");
    label(x + cw + 20, y, "Confirm password"); gConfirm_.draw({x + cw + 20, y + 8, cw, 40}, "Repeat password");
    y += 84;
    Renderer::text(x, y + 6, "Passwords are hashed with bcrypt inside PostgreSQL - never stored as text.", 12, P().textFaint);
    y += 30;
    if (button({x, y, cardR.w - 72, 44}, "Create account", ButtonStyle::PRIMARY, dbOnline_)) {
        for (TextInput* t : {&gName_, &gUser_, &gPhone_, &gPass_, &gConfirm_}) t->error.clear();
        NewUser nu;
        nu.fullName = gName_.text(); nu.username = gUser_.text(); nu.phone = gPhone_.text(); nu.password = gPass_.field.text();
        nu.cityId = gCity_.selected >= 0 ? gCities_[gCity_.selected].id : 0;
        try {
            svc_.registerCitizen(nu, gConfirm_.field.text());
            UI::get().toast("Account created - you can sign in now", ToastKind::SUCCESS);
            lUser_.field.setText(Validator::trim(nu.username));
            go(Screen::LOGIN);
        } catch (const ValidationException& e) {
            std::string f = e.field();
            TextInput* t = f == "fullName" ? &gName_ : f == "username" ? &gUser_ : f == "phone" ? &gPhone_ : f == "confirm" ? &gConfirm_ : &gPass_;
            t->error = e.what();
        } catch (const AppException& e) {
            UI::get().toast(e.what(), ToastKind::ERR);
        }
    }
    y += 62;
    Renderer::text(x, y, "Already registered?", 13.5f, P().textMuted);
    if (linkText(x + Renderer::measure("Already registered? ", 13.5f), y, "Sign in", 13.5f)) go(Screen::LOGIN);
}

// ============================================================ DASHBOARD
void App::loadDashboard() {
    if (dashLoaded_ || !user_) return;
    guarded([&] {
        Scope s = svc_.scopeFor(*user_);
        sum_ = db_.getSummary(s);
        byCat_ = db_.countByCategory(s);
        bySev_ = db_.countBySeverity(s);
        monthly_ = db_.monthlyTrend(s, 8);
        avgRes_ = db_.averageResolutionHours(s);
        ComplaintFilter f;
        f.scope = s;
        f.limit = 8;
        if (user_->role() == Role::DEPARTMENT) f.activeOnly = true, f.orderBy = "severity";
        recent_ = db_.getComplaints(f);
        if (user_->role() == Role::ADMIN || user_->role() == Role::DEPARTMENT) {
            auto r = svc_.departmentRanking();
            deptRank_ = r.rows;
            deptSort_ = r.sort;
        }
        dashLoaded_ = true;
    });
}

void App::screenDashboard() {
    loadDashboard();
    Rect c = content();
    float H = user_->role() == Role::ADMIN ? 1260 : user_->role() == Role::DEPARTMENT ? 1010 : 900;
    float y0 = pageScroll_.begin(c, H);
    Rect r{c.x + 28, y0 + 24, c.w - 56, H - 48};
    if (user_->role() == Role::ADMIN) dashboardAdmin(r);
    else if (user_->role() == Role::DEPARTMENT) dashboardDepartment(r);
    else dashboardCitizen(r);
    pageScroll_.end();
}

static void kpiRow(const Rect& r, const std::vector<std::tuple<std::string, int, std::string, Color, const char*>>& items) {
    float gap = 16, w = (r.w - gap * (items.size() - 1)) / items.size();
    for (std::size_t i = 0; i < items.size(); ++i) {
        auto& [label, v, hint, col, ic] = items[i];
        kpiCard({r.x + i * (w + gap), r.y, w, r.h}, label, std::to_string(v), hint, col, ic);
    }
}

void App::dashboardAdmin(const Rect& r) {
    float y = r.y;
    Renderer::text(r.x, y + 8, "Good to see you, " + user_->fullName(), 20, P().text, Weight::SEMIBOLD);
    Renderer::text(r.x, y + 32, "System-wide view of every complaint, computed live from PostgreSQL.", 13.5f, P().textMuted);
    if (button({r.right() - 132, y, 132, 38}, "Refresh", ButtonStyle::SECONDARY, true, "refresh")) { markDirty(); refreshNotifications(); }
    y += 58;
    auto pct = [&](int v) { return sum_.total ? std::to_string(v * 100 / sum_.total) + "% of total" : std::string("-"); };
    kpiRow({r.x, y, r.w, 112},
           {{"TOTAL", sum_.total, "all complaints", P().primary, "inbox"},
            {"PENDING", sum_.pending, pct(sum_.pending), P().warning, "clock"},
            {"ASSIGNED", sum_.assigned, pct(sum_.assigned), P().info, "building"},
            {"IN PROGRESS", sum_.inProgress, pct(sum_.inProgress), P().primary, "refresh"},
            {"RESOLVED", sum_.resolved, pct(sum_.resolved), P().success, "check"},
            {"CLOSED", sum_.closed, pct(sum_.closed), P().textMuted, "shield"},
            {"HIGH SEVERITY", sum_.high, num(sum_.overdue) + " overdue", P().danger, "alert"}});
    y += 132;

    // status donut + monthly trend
    Rect a{r.x, y, r.w * 0.38f - 8, 300}, b{r.x + r.w * 0.38f + 8, y, r.w * 0.62f - 8, 300};
    card(a);
    sectionTitle({a.x + 20, a.y + 14, a.w, 40}, "Status distribution", "Lifecycle stage of every complaint");
    charts::donut({a.x + 18, a.y + 70, a.w - 36, a.h - 86}, {"Pending", "Assigned", "In Progress", "Resolved", "Closed"},
                  {double(sum_.pending), double(sum_.assigned), double(sum_.inProgress), double(sum_.resolved), double(sum_.closed)},
                  {P().warning, P().info, P().primary, P().success, P().textFaint}, "complaints");
    card(b);
    sectionTitle({b.x + 20, b.y + 14, b.w, 40}, "Monthly trend", "Complaints created vs. resolved, last 8 months");
    std::vector<std::string> ml;
    std::vector<double> mc, mr;
    for (const auto& m : monthly_) { ml.push_back(m.label); mc.push_back(m.count); mr.push_back(m.extra); }
    charts::lineChart({b.x + 14, b.y + 62, b.w - 28, b.h - 72}, ml, mc, mr, "Created", "Resolved");
    y += 316;

    // category, department workload (bubble sort), severity
    float w3 = (r.w - 32) / 3;
    Rect c1{r.x, y, w3, 300}, c2{r.x + w3 + 16, y, w3, 300}, c3{r.x + 2 * (w3 + 16), y, w3, 300};
    card(c1);
    sectionTitle({c1.x + 20, c1.y + 14, c1.w, 40}, "Category distribution", "Complaints per category");
    std::vector<std::string> cl; std::vector<double> cv;
    for (const auto& x : byCat_) { cl.push_back(x.label); cv.push_back(x.count); }
    charts::barChart({c1.x + 12, c1.y + 66, c1.w - 24, c1.h - 76}, cl, cv, P().chart[0]);
    card(c2);
    sectionTitle({c2.x + 20, c2.y + 14, c2.w, 40}, "Department workload", "Open work, ranked with " + deptSort_.algorithm);
    std::vector<std::string> dl; std::vector<double> dv;
    for (const auto& d : deptRank_) { dl.push_back(d.code()); dv.push_back(d.workload().open()); }
    std::vector<Color> dc(P().chart, P().chart + 8);
    charts::hbarChart({c2.x + 20, c2.y + 74, c2.w - 40, c2.h - 90}, dl, dv, dc);
    card(c3);
    sectionTitle({c3.x + 20, c3.y + 14, c3.w, 40}, "Severity distribution", "Scores computed by the NASM routine");
    std::vector<std::string> sl; std::vector<double> sv;
    for (const auto& x : bySev_) { sl.push_back(x.label); sv.push_back(x.count); }
    charts::hbarChart({c3.x + 20, c3.y + 74, c3.w - 40, c3.h - 90}, sl, sv, {P().success, P().info, P().warning, P().danger}, true);
    y += 316;

    // recent complaints + system health
    Rect rc{r.x, y, r.w * 0.66f - 8, 380}, sh{r.x + r.w * 0.66f + 8, y, r.w * 0.34f - 8, 380};
    card(rc);
    sectionTitle({rc.x + 20, rc.y + 14, rc.w, 40}, "Recent complaints", "Newest first - click a row for details");
    if (linkText(rc.right() - 80, rc.y + 32, "View all", 13)) go(Screen::COMPLAINTS);
    complaintTable({rc.x + 1, rc.y + 62, rc.w - 2, rc.h - 63}, recent_, recentTable_, false);
    card(sh);
    sectionTitle({sh.x + 20, sh.y + 14, sh.w, 40}, "System health");
    float hy = sh.y + 64;
    auto row = [&](const char* ic, const std::string& k, const std::string& v, const Color& c) {
        Renderer::circle(sh.x + 34, hy + 18, 15, c.withAlpha(0.12f));
        icon(ic, sh.x + 34, hy + 18, 13, c);
        Renderer::text(sh.x + 58, hy + 14, k, 12, P().textMuted);
        Renderer::text(sh.x + 58, hy + 32, v, 13.5f, P().text, Weight::SEMIBOLD);
        hy += 50;
    };
    row("db", "Database", dbOnline_ ? "PostgreSQL " + db_.serverVersion() + " - connected" : "Offline", dbOnline_ ? P().success : P().danger);
    row("cpu", "Assembly engine", std::string(asmOnline_ ? "ONLINE" : "FAULT") + " - " + num(AsmEngine::totalCalls()) + " calls", asmOnline_ ? P().success : P().danger);
    row("queue", "FIFO processing queue", num(svc_.queue().size()) + " waiting for assignment", P().warning);
    auto hs = svc_.index().stats();
    char lf[64];
    std::snprintf(lf, sizeof lf, "%zu records, load %.2f, %zu collisions", hs.records, hs.loadFactor, hs.collisions);
    row("layers", "Hash table", lf, P().info);
    char ar[48];
    std::snprintf(ar, sizeof ar, "%.1f hours average to resolve", avgRes_);
    row("clock", "Resolution time", ar, P().primary);
    row("list", "Active list (doubly linked)", num(svc_.activeList().size()) + " open complaints in memory", P().primary);
}

void App::dashboardDepartment(const Rect& r) {
    auto& du = static_cast<DepartmentUser&>(*user_);
    float y = r.y;
    Renderer::text(r.x, y + 8, du.departmentName(), 20, P().text, Weight::SEMIBOLD);
    Renderer::text(r.x, y + 32, "Only complaints assigned to your department are visible here.", 13.5f, P().textMuted);
    if (button({r.right() - 150, y, 150, 38}, "Open work queue", ButtonStyle::PRIMARY, true, "list")) go(Screen::COMPLAINTS);
    if (button({r.right() - 150 - 12 - 130, y, 130, 38}, "AI triage", ButtonStyle::SECONDARY, true, "ai")) go(Screen::ASSISTANT);
    y += 58;
    kpiRow({r.x, y, r.w, 112},
           {{"ASSIGNED", sum_.assigned, "waiting to start", P().info, "inbox"},
            {"IN PROGRESS", sum_.inProgress, "being worked on", P().primary, "refresh"},
            {"RESOLVED", sum_.resolved, "awaiting citizen", P().success, "check"},
            {"CLOSED", sum_.closed, "confirmed fixed", P().textMuted, "shield"},
            {"HIGH SEVERITY", sum_.high, num(sum_.overdue) + " past SLA", P().danger, "alert"}});
    y += 132;
    Rect a{r.x, y, r.w * 0.5f - 8, 300}, b{r.x + r.w * 0.5f + 8, y, r.w * 0.5f - 8, 300};
    card(a);
    sectionTitle({a.x + 20, a.y + 14, a.w, 40}, "Resolution statistics");
    int total = sum_.total ? sum_.total : 1;
    float py = a.y + 70;
    for (auto [label, v, col] : std::vector<std::tuple<const char*, int, Color>>{
             {"Resolved + closed", sum_.resolved + sum_.closed, P().success}, {"Open", sum_.assigned + sum_.inProgress, P().primary},
             {"High severity", sum_.high, P().danger}, {"Overdue (SLA)", sum_.overdue, P().warning}}) {
        Renderer::text(a.x + 20, py, label, 13, P().text);
        std::string pv = num(v) + "  (" + num(v * 100 / total) + "%)";
        Renderer::text(a.right() - 20 - Renderer::measure(pv, 13, Weight::SEMIBOLD), py, pv, 13, P().textMuted, Weight::SEMIBOLD);
        progressBar({a.x + 20, py + 10, a.w - 40, 8}, float(v) / total, col);
        py += 50;
    }
    char ar[64];
    std::snprintf(ar, sizeof ar, "Average resolution time: %.1f hours", avgRes_);
    Renderer::text(a.x + 20, py + 8, ar, 13, P().textMuted);
    card(b);
    sectionTitle({b.x + 20, b.y + 14, b.w, 40}, "Department workload", "All departments - open assigned work");
    std::vector<std::string> dl; std::vector<double> dv;
    for (const auto& d : deptRank_) {
        dl.push_back(d.code());
        dv.push_back(d.workload().open());
    }
    if (dl.empty()) emptyState({b.x, b.y + 60, b.w, b.h - 60}, "No departments", "Department data is unavailable");
    else {
        std::vector<Color> dc(P().chart, P().chart + 8);
        charts::hbarChart({b.x + 20, b.y + 74, b.w - 40, b.h - 90}, dl, dv, dc);
    }
    y += 316;
    Rect t{r.x, y, r.w, 440};
    card(t);
    sectionTitle({t.x + 20, t.y + 14, t.w, 40}, "Highest-severity open work", "Sorted by Assembly severity score");
    complaintTable({t.x + 1, t.y + 62, t.w - 2, t.h - 63}, recent_, recentTable_, false);
}

void App::dashboardCitizen(const Rect& r) {
    float y = r.y;
    Rect hero{r.x, y, r.w, 132};
    Renderer::shadow(hero, 14, 12, 1.2f);
    Renderer::hgradient(hero, hex(0x312E81), hex(0x4F46E5));
    Renderer::roundRectOutline(hero, 1, hex(0x312E81, 0.0f));
    Renderer::text(hero.x + 28, hero.y + 50, "Hello, " + user_->fullName(), 22, hex(0xFFFFFF), Weight::BOLD);
    Renderer::text(hero.x + 28, hero.y + 78, "Spotted a problem in your city? Report it and track every step until it is fixed.", 14, hex(0xE0E7FF));
    if (button({hero.right() - 210, hero.y + 22, 180, 42}, "Report a complaint", ButtonStyle::SECONDARY, true, "plus")) go(Screen::REPORT);
    if (button({hero.right() - 210, hero.y + 74, 180, 42}, "Ask AI assistant", ButtonStyle::SECONDARY, true, "ai")) go(Screen::ASSISTANT);
    y += 156;
    kpiRow({r.x, y, r.w, 112},
           {{"MY COMPLAINTS", sum_.total, "since you joined", P().primary, "inbox"},
            {"OPEN", sum_.pending + sum_.assigned + sum_.inProgress, "pending / assigned / in progress", P().warning, "clock"},
            {"RESOLVED", sum_.resolved, "confirm to close", P().success, "check"},
            {"CLOSED", sum_.closed, "completed", P().textMuted, "shield"}});
    y += 132;
    Rect a{r.x, y, r.w * 0.64f - 8, 420}, b{r.x + r.w * 0.64f + 8, y, r.w * 0.36f - 8, 420};
    card(a);
    sectionTitle({a.x + 20, a.y + 14, a.w, 40}, "My recent complaints", "Click a complaint to see its full history");
    if (recent_.empty()) emptyState({a.x, a.y + 60, a.w, a.h - 60}, "No complaints yet", "Use 'Report a complaint' to file your first one");
    else complaintTable({a.x + 1, a.y + 62, a.w - 2, a.h - 63}, recent_, recentTable_, false);
    card(b);
    sectionTitle({b.x + 20, b.y + 14, b.w, 40}, "Latest notifications");
    float ny = b.y + 62;
    int shown = 0;
    for (const auto& n : notes_) {
        if (shown++ >= 6) break;
        if (!n.read) Renderer::circle(b.x + 24, ny + 14, 4, P().primary);
        Renderer::textIn({b.x + 36, ny, b.w - 110, 20}, n.title, 13, P().text, n.read ? Weight::REGULAR : Weight::SEMIBOLD);
        Renderer::textIn({b.x + 36, ny + 20, b.w - 56, 18}, n.message, 12, P().textMuted);
        Renderer::textIn({b.right() - 70, ny, 52, 20}, n.when, 11.5f, P().textFaint, Weight::REGULAR, Align::RIGHT);
        ny += 56;
    }
    if (notes_.empty()) emptyState({b.x, b.y + 60, b.w, b.h - 60}, "You're all caught up", "Updates on your complaints appear here");
}

// ============================================================ REPORT COMPLAINT
void App::screenReport() {
    Rect c = content();
    if (rState_.items.empty()) {
        loadStatesInto(rState_, rStates_);
        rCategory_.items.clear();
        for (const auto& cat : svc_.categories()) rCategory_.items.push_back(cat.name);
        rPriority_.items = {"Low", "Normal", "High", "Urgent"};
        rPriority_.selected = 1;
        rName_.field.setText(user_->fullName());
        rPhone_.field.setText(user_->phone());
    }
    float H = 1010;
    float y0 = pageScroll_.begin(c, H);
    Rect f{c.x + 28, y0 + 24, std::min(c.w - 56 - 360, 820.0f), 960};
    card(f);
    float x = f.x + 28, y = f.y + 40, cw = (f.w - 56 - 20) / 2, cw3 = (f.w - 56 - 40) / 3;
    Renderer::text(x, y, "Report a complaint", 20, P().text, Weight::SEMIBOLD);
    Renderer::text(x, y + 24, "All fields are required. Severity is calculated by the Assembly engine.", 13, P().textMuted);
    y += 56;
    auto label = [&](float lx, float ly, const char* t) { Renderer::text(lx, ly, t, 12.5f, P().text, Weight::SEMIBOLD); };
    Renderer::text(x, y + 6, "CONTACT", 11, P().textFaint, Weight::SEMIBOLD); y += 18;
    label(x, y + 12, "Citizen name"); rName_.draw({x, y + 20, cw, 40}, "Full name");
    label(x + cw + 20, y + 12, "Phone"); rPhone_.draw({x + cw + 20, y + 20, cw, 40}, "10-digit mobile");
    y += 90;
    Renderer::text(x, y + 6, "LOCATION", 11, P().textFaint, Weight::SEMIBOLD); y += 18;
    label(x, y + 12, "State");
    if (rState_.draw({x, y + 20, cw3, 40}, "Select state")) {
        rDistrict_.items.clear(); rDistrict_.selected = -1; rCity_.items.clear(); rCity_.selected = -1;
        guarded([&] { rDistricts_ = db_.getDistricts(rStates_[rState_.selected].id); for (auto& d : rDistricts_) rDistrict_.items.push_back(d.name); });
    }
    label(x + cw3 + 20, y + 12, "District");
    if (rDistrict_.draw({x + cw3 + 20, y + 20, cw3, 40}, rState_.selected < 0 ? "Select state first" : "Select district")) {
        rCity_.items.clear(); rCity_.selected = -1;
        guarded([&] { rCities_ = db_.getCities(rDistricts_[rDistrict_.selected].id); for (auto& ci : rCities_) rCity_.items.push_back(ci.name); });
        if (rCity_.items.size() == 1) rCity_.selected = 0;
    }
    label(x + 2 * (cw3 + 20), y + 12, "City");
    rCity_.draw({x + 2 * (cw3 + 20), y + 20, cw3, 40}, "Select city");
    y += 84;
    label(x, y + 12, "Locality / landmark"); rLocality_.draw({x, y + 20, f.w - 56, 40}, "e.g. Near Shivaji Chowk bus stop");
    y += 90;
    Renderer::text(x, y + 6, "ISSUE", 11, P().textFaint, Weight::SEMIBOLD); y += 18;
    label(x, y + 12, "Category"); rCategory_.draw({x, y + 20, cw, 40}, "Select category");
    label(x + cw + 20, y + 12, "Priority"); rPriority_.draw({x + cw + 20, y + 20, cw, 40}, "Select priority");
    y += 84;
    label(x, y + 12, "Description");
    rDesc_.draw({x, y + 20, f.w - 56, 40}, "What is wrong, since when, and how it affects people (min 15 characters)");
    Renderer::text(f.right() - 28 - Renderer::measure(num(rDesc_.text().size()) + " / 1000", 11.5f), y + 12,
                   num(rDesc_.text().size()) + " / 1000", 11.5f, P().textFaint);
    y += 84;
    label(x, y + 12, "Impact (how many people are affected)");
    int imp = rImpact_ - 1;
    if (segmented({x, y + 20, cw, 38}, {"1", "2", "3", "4", "5"}, imp)) { rImpact_ = imp + 1; rImpactErr_.clear(); }
    if (!rImpactErr_.empty()) Renderer::text(x, y + 74, rImpactErr_, 11.5f, P().danger);
    label(x + cw + 20, y + 12, "Urgency (how soon it must be fixed)");
    int urg = rUrgency_ - 1;
    if (segmented({x + cw + 20, y + 20, cw, 38}, {"1", "2", "3", "4", "5"}, urg)) { rUrgency_ = urg + 1; rUrgencyErr_.clear(); }
    if (!rUrgencyErr_.empty()) Renderer::text(x + cw + 20, y + 74, rUrgencyErr_, 11.5f, P().danger);
    y += 96;

    // live severity preview (genuine Assembly call; counted in the engine statistics)
    int catCode = rCategory_.selected >= 0 ? svc_.categories()[rCategory_.selected].asmCode : -1;
    Rect prev{x, y, f.w - 56, 76};
    Renderer::roundRect(prev, 10, P().surfaceAlt);
    Renderer::roundRectOutline(prev, 10, P().border);
    icon("cpu", prev.x + 30, prev.y + 38, 18, P().primary);
    if (catCode >= 0 && rImpact_ > 0 && rUrgency_ > 0) {
        static int lastKey = -1, lastScore = 0, lastLevel = 1;
        int key = catCode * 100 + rImpact_ * 10 + rUrgency_;
        if (key != lastKey) {   // only call Assembly when the inputs change, not every frame
            lastScore = AsmEngine::severityScore(catCode, rImpact_, rUrgency_);
            lastLevel = AsmEngine::classify(lastScore);
            lastKey = key;
        }
        auto lvl = static_cast<SeverityLevel>(lastLevel);
        Renderer::text(prev.x + 56, prev.y + 32, "Severity (calculateSeverityAsm)", 12, P().textMuted);
        Renderer::text(prev.x + 56, prev.y + 56, num(lastScore) + " / 100", 18, P().text, Weight::BOLD);
        badge(prev.x + 160, prev.y + 50, toString(lvl), severityColor(lvl, false), severityColor(lvl, true));
        progressBar({prev.x + 300, prev.y + 34, prev.w - 330, 8}, lastScore / 100.0f, severityColor(lvl, false));
        Renderer::text(prev.x + 300, prev.y + 62, "score = weight[category] + 3 x (impact x urgency), add-and-shift multiply in NASM", 11.5f, P().textFaint);
    } else {
        Renderer::text(prev.x + 56, prev.y + 34, "Severity preview", 13, P().text, Weight::SEMIBOLD);
        Renderer::text(prev.x + 56, prev.y + 54, "Choose a category, impact and urgency to see the Assembly-computed severity.", 12.5f, P().textMuted);
    }
    y += 100;

    if (button({x, y, 200, 44}, "Submit complaint", ButtonStyle::PRIMARY, dbOnline_, "check")) {
        for (TextInput* t : {&rName_, &rPhone_, &rLocality_, &rDesc_}) t->error.clear();
        for (Dropdown* d : {&rState_, &rDistrict_, &rCity_, &rCategory_}) d->error.clear();
        rImpactErr_.clear(); rUrgencyErr_.clear();
        ComplaintInput in;
        in.citizenName = rName_.text(); in.phone = rPhone_.text(); in.locality = rLocality_.text(); in.description = rDesc_.text();
        in.stateId = rState_.selected >= 0 ? rStates_[rState_.selected].id : 0;
        in.districtId = rDistrict_.selected >= 0 ? rDistricts_[rDistrict_.selected].id : 0;
        in.cityId = rCity_.selected >= 0 ? rCities_[rCity_.selected].id : 0;
        in.categoryCode = rCategory_.selected >= 0 ? svc_.categories()[rCategory_.selected].code : "";
        static const char* pr[] = {"LOW", "NORMAL", "HIGH", "URGENT"};
        in.priority = rPriority_.selected >= 0 ? pr[rPriority_.selected] : "";
        in.impact = rImpact_; in.urgency = rUrgency_;
        try {
            lastSubmit_ = svc_.submitComplaint(*user_, in);     // success ONLY after the DB commit
            submitOpen_ = true;
            rLocality_.field.clear(); rDesc_.field.clear(); rImpact_ = rUrgency_ = 0; rCategory_.selected = -1;
            markDirty();
            refreshNotifications();
        } catch (const ValidationException& e) {
            const std::string& fld = e.field();
            if (fld == "citizenName") rName_.error = e.what();
            else if (fld == "phone") rPhone_.error = e.what();
            else if (fld == "locality") rLocality_.error = e.what();
            else if (fld == "description") rDesc_.error = e.what();
            else if (fld == "state") rState_.error = e.what();
            else if (fld == "district") rDistrict_.error = e.what();
            else if (fld == "city") rCity_.error = e.what();
            else if (fld == "category") rCategory_.error = e.what();
            else if (fld == "impact") rImpactErr_ = e.what();
            else if (fld == "urgency") rUrgencyErr_ = e.what();
            else UI::get().toast(e.what(), ToastKind::ERR);
        } catch (const NotConnectedException& e) {
            dbOnline_ = false;
            UI::get().toast(std::string("Not saved - ") + e.what(), ToastKind::ERR);
        } catch (const AppException& e) {
            UI::get().toast(std::string("Not saved - ") + e.what(), ToastKind::ERR);
        }
    }
    Renderer::text(x + 220, y + 27, "Nothing is shown as submitted unless PostgreSQL commits it.", 12.5f, P().textMuted);

    // side panel: how it works
    Rect side{f.right() + 20, f.y, 340, 470};
    card(side);
    sectionTitle({side.x + 20, side.y + 14, side.w, 30}, "What happens next");
    const char* steps[] = {"Validated field by field", "Severity computed in NASM Assembly", "Stored in a C++ object, linked list and hash table",
                           "Committed to PostgreSQL with history", "Queued (FIFO) for department routing", "You get a notification at every step"};
    float sy = side.y + 64;
    for (int i = 0; i < 6; ++i) {
        Renderer::circle(side.x + 32, sy + 12, 12, P().primarySoft);
        Renderer::textIn({side.x + 20, sy, 24, 24}, num(i + 1), 11.5f, P().primary, Weight::BOLD, Align::CENTER);
        auto ls = Renderer::wrap(steps[i], side.w - 80, 13);
        float ty = sy + 17;
        for (const auto& l : ls) { Renderer::text(side.x + 56, ty, l, 13, P().text); ty += 19; }
        sy += 60;
    }
    aiReportPanel({side.x, side.bottom() + 16, side.w, f.bottom() - side.bottom() - 16});   // AI writing helper
    pageScroll_.end();

    // success modal (only reachable after a committed insert)
    Rect m{(width_ - 560) / 2.0f, (height_ - 520) / 2.0f, 560, 520};
    if (modalBegin(m, "Complaint registered", submitOpen_)) {
        Renderer::circle(m.x + 52, m.y + 110, 26, P().successSoft);
        icon("check", m.x + 52, m.y + 110, 22, P().success);
        Renderer::text(m.x + 92, m.y + 104, lastSubmit_.reference, 22, P().text, Weight::BOLD);
        Renderer::text(m.x + 92, m.y + 128, "Severity " + num(lastSubmit_.severityScore) + " (" + lastSubmit_.severityLevel +
                                                ")  -  position " + num(lastSubmit_.queuePosition) + " in the FIFO queue", 13, P().textMuted);
        float yy = m.y + 168;
        Renderer::text(m.x + 24, yy, "PROCESSING TRACE", 11, P().textFaint, Weight::SEMIBOLD);
        yy += 22;
        for (const auto& s : lastSubmit_.steps) {
            Renderer::textIn({m.x + 24, yy - 14, m.w - 48, 20}, s, 12, P().text);
            yy += 22;
        }
        if (button({m.right() - 180, m.bottom() - 60, 156, 40}, "View complaint", ButtonStyle::PRIMARY)) {
            submitOpen_ = false;
            backFromDetail_ = Screen::COMPLAINTS;
            openDetail(lastSubmit_.id);
        }
        if (button({m.right() - 300, m.bottom() - 60, 108, 40}, "Close", ButtonStyle::SECONDARY)) submitOpen_ = false;
    }
}

// ============================================================ COMPLAINT LIST
void App::loadList() {
    if (!listDirty_ || !user_) return;
    guarded([&] {
        ComplaintFilter f;
        f.scope = svc_.scopeFor(*user_);
        if (fStatus_.selected > 0) f.status = toString(allStatuses()[fStatus_.selected - 1]);
        if (fCategory_.selected > 0) f.categoryCode = svc_.categories()[fCategory_.selected - 1].code;
        f.text = Validator::trim(listSearch_.text());
        f.orderBy = listSort_ == 1 ? "severity" : listSort_ == 2 ? "oldest" : "created";
        listTotal_ = db_.countComplaints(f);
        f.limit = PAGE;
        f.offset = listPage_ * PAGE;
        list_ = db_.getComplaints(f);
        appliedSearch_ = f.text;
    });
    listDirty_ = false;
}

void App::screenComplaints() {
    if (fStatus_.items.empty()) {
        fStatus_.items = {"All statuses"};
        for (Status s : allStatuses()) fStatus_.items.push_back(displayName(s));
        fStatus_.selected = 0;
        fCategory_.items = {"All categories"};
        for (const auto& c : svc_.categories()) fCategory_.items.push_back(c.name);
        fCategory_.selected = 0;
    }
    loadList();
    Rect c = content();
    Rect r{c.x + 28, c.y + 24, c.w - 56, c.h - 48};
    std::string sub = user_->role() == Role::CITIZEN ? "Every complaint you have reported"
                    : user_->role() == Role::DEPARTMENT ? "Complaints assigned to your department"
                    : "Every complaint in the system";
    Renderer::text(r.x, r.y + 14, sub, 13.5f, P().textMuted);
    if (user_->role() == Role::CITIZEN && button({r.right() - 180, r.y - 4, 180, 38}, "Report complaint", ButtonStyle::PRIMARY, true, "plus"))
        go(Screen::REPORT);
    float y = r.y + 34;
    Rect panel{r.x, y, r.w, r.bottom() - y};
    card(panel);
    float fx = panel.x + 16, fy = panel.y + 16;
    // The filter row is laid out from the available width (the old fixed offsets made the Apply button cover the
    // "Oldest" sort option on screens of about 1366 px or less). Fixed parts: two dropdowns, the sort control and
    // Apply; the search box takes the rest (180..300 px).
    const float gap = 12, ddW = 160, segW = 250, applyW = 92;
    float searchW = std::max(180.0f, std::min(300.0f, panel.w - 32 - (2 * ddW + segW + applyW + 4 * gap)));
    float cx = fx;
    listSearch_.draw({cx, fy, searchW, 38}, "Filter by text (Enter)", "search");
    cx += searchW + gap;
    if (fStatus_.draw({cx, fy, ddW, 38}, "Status")) { listPage_ = 0; listDirty_ = true; }
    cx += ddW + gap;
    if (fCategory_.draw({cx, fy, ddW, 38}, "Category")) { listPage_ = 0; listDirty_ = true; }
    cx += ddW + gap;
    if (segmented({cx, fy, segW, 38}, {"Newest", "Severity", "Oldest"}, listSort_)) { listPage_ = 0; listDirty_ = true; }
    cx += segW + gap;
    if (button({std::max(cx, panel.right() - 16 - applyW), fy, applyW, 38}, "Apply", ButtonStyle::SECONDARY)) { listPage_ = 0; listDirty_ = true; }
    Rect tr{panel.x + 1, fy + 54, panel.w - 2, panel.h - 54 - 16 - 60};
    if (list_.empty()) emptyState(tr, "No complaints match", appliedSearch_.empty() ? "Try a different filter" : "Nothing matches '" + appliedSearch_ + "'");
    else complaintTable(tr, list_, listTable_, user_->role() != Role::DEPARTMENT);
    int pages = (listTotal_ + PAGE - 1) / PAGE;
    int np = pager({panel.x + 20, panel.bottom() - 56, panel.w - 40, 38}, listPage_, pages, listTotal_);
    if (np != listPage_) { listPage_ = np; listDirty_ = true; }
}

// ============================================================ DETAIL
void App::openDetail(long long id) {
    if (guarded([&] {
            detail_ = svc_.openComplaint(*user_, id);     // throws AccessDenied for other users' complaints
            history_ = db_.getHistory(id);
            statusRows_ = db_.getStatusHistory(id);
        })) {
        note_.field.clear();
        detailScroll_.offset = 0;
        aiLoadInsight();   // AI recommendations for this complaint (citizens and departments)
        go(Screen::DETAIL);
    }
}

void App::screenDetail() {
    if (!detail_) { go(backFromDetail_); return; }
    Complaint& c = *detail_;
    Rect cr = content();
    // the AI card sits between the action box and the details; work out its height first so the page can size itself
    Status nextForAi = Status::PENDING;
    const bool aiNote = nextStatus(c.status(), nextForAi) && nextForAi != Status::ASSIGNED && user_->canTransition(c, nextForAi);
    float aiH = aiInsight_ ? aiDetailHeight(cr.w - 56, aiNote) : 0;
    float H = 980 + (aiH > 0 ? aiH + 16 : 0);
    float y0 = detailScroll_.begin(cr, H);
    Rect r{cr.x + 28, y0 + 20, cr.w - 56, H - 40};
    if (linkText(r.x, r.y + 14, "\xE2\x86\x90  Back", 13)) { go(backFromDetail_); }

    // header card
    Rect head{r.x, r.y + 30, r.w, 168};
    card(head);
    Renderer::text(head.x + 24, head.y + 40, c.referenceCode(), 22, P().text, Weight::BOLD);
    float bx = head.x + 30 + Renderer::measure(c.referenceCode(), 22, Weight::BOLD);
    bx += badge(bx, head.y + 32, displayName(c.status()), statusColor(c.status(), false), statusColor(c.status(), true));
    badge(bx + 4, head.y + 32, std::string(toString(c.severityLevel())) + " " + num(c.severityScore()), severityColor(c.severityLevel(), false),
          severityColor(c.severityLevel(), true));
    Renderer::text(head.x + 24, head.y + 66, std::string(c.typeName()) + " complaint  \xC2\xB7  " + c.location().locality + ", " +
                                                 c.location().city + ", " + c.location().district + ", " + c.location().state,
                   13.5f, P().textMuted);
    statusStepper({head.x + 60, head.y + 92, head.w - 120, 60}, c.status());

    // actions allowed for THIS user (polymorphic canTransition)
    float ay = head.bottom() + 16;
    Status next;
    bool hasNext = nextStatus(c.status(), next) && next != Status::ASSIGNED && user_->canTransition(c, next);
    Rect act{r.x, ay, r.w, 72};
    card(act);
    if (hasNext) {
        Renderer::text(act.x + 24, act.y + 30, "Next step", 12, P().textMuted);
        Renderer::text(act.x + 24, act.y + 52, std::string("Move to ") + displayName(next), 14, P().text, Weight::SEMIBOLD);
        note_.draw({act.x + 220, act.y + 17, act.w - 220 - 240, 38}, "Add a note for the citizen / department (optional)");
        const char* label = next == Status::IN_PROGRESS ? "Start work" : next == Status::RESOLVED ? "Mark resolved" : "Confirm & close";
        if (button({act.right() - 208, act.y + 16, 184, 40}, label, next == Status::CLOSED ? ButtonStyle::SUCCESS : ButtonStyle::PRIMARY)) {
            confirmTo_ = next;
            confirmOpen_ = true;
        }
    } else {
        std::string why = c.status() == Status::CLOSED ? "This complaint is closed. No further changes are possible."
                        : c.status() == Status::PENDING ? "Waiting in the FIFO queue for department assignment."
                        : user_->role() == Role::CITIZEN && c.status() != Status::RESOLVED ? "The department is working on it - you will be notified of every change."
                        : "No action available for your role at this stage.";
        icon("info", act.x + 32, act.y + 36, 16, P().info);
        Renderer::text(act.x + 52, act.y + 41, why, 13.5f, P().textMuted);
    }

    float y = act.bottom() + 16;
    if (aiH > 0) {   // AI recommendations (citizens and departments)
        aiDetailCard({r.x, y, r.w, aiH}, hasNext);
        y += aiH + 16;
    }
    // details + history
    Rect left{r.x, y, r.w * 0.6f - 8, 600}, right{r.x + r.w * 0.6f + 8, y, r.w * 0.4f - 8, 600};
    card(left);
    sectionTitle({left.x + 24, left.y + 14, left.w, 30}, "Complaint information");
    float ly = left.y + 64;
    auto field = [&](float x, const std::string& k, const std::string& v) {
        Renderer::text(x, ly, k, 11.5f, P().textMuted, Weight::SEMIBOLD);
        Renderer::textIn({x, ly + 6, left.w / 2 - 40, 22}, v, 13.5f, P().text);
    };
    float hx = left.x + 24, hx2 = left.x + left.w / 2;
    field(hx, "CITIZEN", c.citizenName()); field(hx2, "PHONE", c.phone()); ly += 52;
    field(hx, "CITY / DISTRICT", c.location().city + " / " + c.location().district); field(hx2, "STATE", c.location().state); ly += 52;
    field(hx, "DEPARTMENT", c.departmentName().empty() ? "Not assigned yet" : c.departmentName());
    field(hx2, "PRIORITY", toString(c.priority())); ly += 52;
    field(hx, "IMPACT  x  URGENCY", num(c.impact()) + " x " + num(c.urgency()) + "  ->  severity " + num(c.severityScore()));
    field(hx2, "RESOLUTION TARGET (SLA)", num(c.slaHours()) + " hours" + (c.isOverdue(std::time(nullptr)) ? "  -  OVERDUE" : "")); ly += 52;
    field(hx, "REPORTED", fmtTime(c.createdAt())); field(hx2, "LAST UPDATE", fmtTime(c.updatedAt())); ly += 56;
    Renderer::text(hx, ly, "DESCRIPTION", 11.5f, P().textMuted, Weight::SEMIBOLD);
    ly += 22;
    for (const auto& l : Renderer::wrap(c.description(), left.w - 48, 13.5f)) { Renderer::text(hx, ly, l, 13.5f, P().text); ly += 21; }
    ly += 12;
    Rect adv{hx, ly, left.w - 48, 70};
    Renderer::roundRect(adv, 8, c.isPublicSafetyRisk() ? P().dangerSoft : P().infoSoft);
    Renderer::text(adv.x + 14, adv.y + 24, c.isPublicSafetyRisk() ? "Public safety risk - handling advice" : "Handling advice", 12.5f,
                   c.isPublicSafetyRisk() ? P().danger : P().info, Weight::SEMIBOLD);
    Renderer::textIn({adv.x + 14, adv.y + 34, adv.w - 28, 24}, c.handlingAdvice(), 12.5f, P().text);

    card(right);
    sectionTitle({right.x + 20, right.y + 14, right.w, 30}, "Status history stack", "LIFO - latest status on top");
    auto& st = svc_.statusStack();
    auto items = st.topToBottom();
    float sy = right.y + 70;
    Renderer::text(right.x + 24, sy + 4, "TOP", 10.5f, P().primary, Weight::BOLD);
    sy += 12;
    for (std::size_t i = 0; i < items.size(); ++i) {
        Rect b{right.x + 24, sy, right.w - 48, 40};
        Status s = statusFromString(items[i].label);
        Renderer::roundRect(b, 8, i == 0 ? statusColor(s, true) : P().surfaceAlt);
        Renderer::roundRectOutline(b, 8, i == 0 ? statusColor(s, false) : P().border);
        Renderer::text(b.x + 14, b.y + 25, displayName(s), 13.5f, i == 0 ? statusColor(s, false) : P().text, Weight::SEMIBOLD);
        Renderer::textIn({b.x + 120, b.y, b.w - 134, b.h}, items[i].detail, 11.5f, P().textMuted, Weight::REGULAR, Align::RIGHT);
        sy += 46;
    }
    Renderer::text(right.x + 24, sy + 12, "size " + num(st.size()) + "  \xC2\xB7  capacity " + num(st.capacity()) + "  \xC2\xB7  top " +
                                              (st.isEmpty() ? std::string("-") : displayName(statusFromString(st.peek().label))),
                   12, P().textMuted);
    sy += 34;
    Renderer::line(right.x + 20, sy, right.right() - 20, sy, P().border);
    sy += 26;
    Renderer::text(right.x + 24, sy, "Complete history", 13.5f, P().text, Weight::SEMIBOLD);
    sy += 16;
    for (const auto& h : history_) {
        if (sy > right.bottom() - 30) break;
        Renderer::circle(right.x + 30, sy + 12, 4, P().primary);
        Renderer::textIn({right.x + 44, sy, right.w - 64, 20}, h.details, 12.5f, P().text);
        Renderer::text(right.x + 44, sy + 34, h.when + "  \xC2\xB7  " + h.actor, 11, P().textFaint);
        sy += 46;
    }
    detailScroll_.end();

    Rect m{(width_ - 480) / 2.0f, (height_ - 250) / 2.0f, 480, 250};
    if (modalBegin(m, "Confirm status change", confirmOpen_)) {
        auto lines = Renderer::wrap(std::string("Move ") + c.referenceCode() + " from " + displayName(c.status()) + " to " +
                                        displayName(confirmTo_) + "? The change is written to PostgreSQL with history, a notification and an audit log entry.",
                                    m.w - 48, 13.5f);
        float yy = m.y + 92;
        for (const auto& l : lines) { Renderer::text(m.x + 24, yy, l, 13.5f, P().text); yy += 21; }
        if (button({m.right() - 156, m.bottom() - 62, 132, 40}, "Confirm", ButtonStyle::PRIMARY)) {
            long long id = c.id();
            if (guarded([&] { svc_.changeStatus(*user_, id, confirmTo_, note_.text()); })) {
                UI::get().toast(std::string("Status changed to ") + displayName(confirmTo_), ToastKind::SUCCESS);
                markDirty();
                refreshNotifications();
                openDetail(id);
            }
            confirmOpen_ = false;
        }
        if (button({m.right() - 272, m.bottom() - 62, 104, 40}, "Cancel", ButtonStyle::SECONDARY)) confirmOpen_ = false;
    }
}

// ============================================================ NOTIFICATIONS
void App::screenNotifications() {
    Rect c = content();
    Rect r{c.x + 28, c.y + 24, std::min(c.w - 56, 920.0f), c.h - 48};
    Renderer::text(r.x, r.y + 14, num(unread_) + " unread of " + num(notes_.size()), 13.5f, P().textMuted);
    if (button({r.right() - 150, r.y - 4, 150, 38}, "Mark all as read", ButtonStyle::SECONDARY, unread_ > 0, "check")) {
        if (guarded([&] { db_.markAllNotificationsRead(user_->id()); })) refreshNotifications();
    }
    Rect panel{r.x, r.y + 34, r.w, r.h - 34};
    card(panel);
    if (notes_.empty()) { emptyState(panel, "No notifications", "Events about your complaints will appear here"); return; }
    float y0 = notesScroll_.begin(panel.inset(1), notes_.size() * 78.0f);
    UI& u = UI::get();
    for (std::size_t i = 0; i < notes_.size(); ++i) {
        const auto& n = notes_[i];
        Rect row{panel.x + 1, y0 + i * 78, panel.w - 2, 78};
        if (row.bottom() < panel.y || row.y > panel.bottom()) continue;
        bool hov = u.hover(row) && panel.contains(u.mouse.x, u.mouse.y);
        if (!n.read) Renderer::rect(row, P().primarySoft.withAlpha(0.45f));
        if (hov) Renderer::rect(row, P().hover.withAlpha(0.8f));
        const char* ic = n.type == "COMPLAINT_RESOLVED" || n.type == "COMPLAINT_CLOSED" ? "check"
                       : n.type == "COMPLAINT_ASSIGNED" || n.type == "DEPARTMENT_ASSIGNMENT" ? "building"
                       : n.type == "COMPLAINT_CREATED" ? "inbox" : n.type == "ADMIN_ACTION" ? "shield" : "refresh";
        Color ac = n.type == "COMPLAINT_RESOLVED" || n.type == "COMPLAINT_CLOSED" ? P().success : n.type == "COMPLAINT_CREATED" ? P().warning : P().primary;
        Renderer::circle(row.x + 36, row.y + 39, 18, ac.withAlpha(0.12f));
        icon(ic, row.x + 36, row.y + 39, 15, ac);
        Renderer::textIn({row.x + 68, row.y + 14, row.w - 300, 22}, n.title, 14, P().text, n.read ? Weight::REGULAR : Weight::SEMIBOLD);
        Renderer::textIn({row.x + 68, row.y + 38, row.w - 200, 22}, n.message, 12.5f, P().textMuted);
        std::string type = n.type;
        std::replace(type.begin(), type.end(), '_', ' ');
        badge(row.right() - 230, row.y + 26, type, P().textMuted, P().neutralSoft, 10.5f);
        Renderer::textIn({row.right() - 90, row.y + 14, 70, 22}, n.when, 12, P().textFaint, Weight::REGULAR, Align::RIGHT);
        if (!n.read) Renderer::circle(row.right() - 24, row.y + 52, 4, P().primary);
        Renderer::line(row.x, row.bottom() - 0.5f, row.right(), row.bottom() - 0.5f, P().border);
        if (hov) u.wantHand();
        if (hov && u.clicked(row)) {
            long long cid = n.complaintId;
            guarded([&] { db_.markNotificationRead(n.id, user_->id()); });
            refreshNotifications();
            if (cid > 0) { backFromDetail_ = Screen::NOTIFICATIONS; openDetail(cid); }
            break;
        }
    }
    notesScroll_.end();
}

// ============================================================ PROFILE
void App::screenProfile() {
    Rect c = content();
    Rect r{c.x + 28, c.y + 24, std::min(c.w - 56, 720.0f), 330};
    card(r);
    Renderer::circle(r.x + 70, r.y + 76, 40, P().primarySoft);
    Renderer::textIn({r.x + 30, r.y + 36, 80, 80}, user_->initials(), 26, P().primary, Weight::BOLD, Align::CENTER);
    Renderer::text(r.x + 130, r.y + 70, user_->fullName(), 22, P().text, Weight::BOLD);
    badge(r.x + 130, r.y + 92, user_->portalName(), P().primary, P().primarySoft);
    float y = r.y + 150;
    auto f = [&](const std::string& k, const std::string& v) {
        Renderer::text(r.x + 40, y, k, 12, P().textMuted, Weight::SEMIBOLD);
        Renderer::text(r.x + 240, y, v, 14, P().text);
        y += 36;
    };
    f("USERNAME", user_->username());
    f("PHONE", user_->phone());
    f("ROLE", toString(user_->role()));
    if (user_->role() == Role::DEPARTMENT) f("DEPARTMENT", static_cast<DepartmentUser&>(*user_).departmentName());
    f("ACCESS", user_->canAccessAdminData() ? "System-wide (administrator)" : user_->role() == Role::DEPARTMENT ? "Own department only" : "Own complaints only");
}

// ============================================================ SEARCH
void App::screenSearch() {
    Rect c = content();
    Rect r{c.x + 28, c.y + 24, c.w - 56, c.h - 48};
    Renderer::text(r.x, r.y + 14, "Results for \"" + searchedFor_ + "\"", 16, P().text, Weight::SEMIBOLD);
    // algorithm evidence
    Rect m{r.x, r.y + 34, r.w, 92};
    card(m, false);
    float w = r.w / 3;
    auto block = [&](float x, const std::string& t, const std::string& a, const std::string& b) {
        Renderer::text(x + 20, m.y + 28, t, 12, P().textMuted, Weight::SEMIBOLD);
        Renderer::text(x + 20, m.y + 52, a, 14, P().text, Weight::SEMIBOLD);
        Renderer::text(x + 20, m.y + 74, b, 12, P().textFaint);
    };
    block(m.x, "HASH TABLE LOOKUP", search_.hashUsed ? (search_.hashHit ? "Hit - O(1)" : "Miss") : "Not a reference code",
          search_.hashUsed ? num(search_.hashProbes) + " chain probes" : "used for SCMS-... or numeric IDs");
    block(m.x + w, "LINEAR SEARCH (DOUBLY LINKED LIST)", num(search_.linear.matches) + " matches",
          num(search_.linear.comparisons) + " nodes compared via asm_contains_ci");
    block(m.x + 2 * w, "POSTGRESQL (ILIKE, PARAMETERISED)", num(search_.databaseMatches) + " rows", "includes closed complaints");
    Rect t{r.x, m.bottom() + 16, r.w, r.bottom() - m.bottom() - 16};
    card(t);
    if (search_.results.empty()) emptyState(t, "No complaints found", "Search by reference, city, district, state, locality, category, status...");
    else complaintTable({t.x + 1, t.y + 1, t.w - 2, t.h - 2}, search_.results, searchTable_, true);
}
