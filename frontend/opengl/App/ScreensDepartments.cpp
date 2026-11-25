// =====================================================================
// ScreensDepartments.cpp - the administrator's Departments screen
//
//   * KPI strip      departments, open work, complaints past their SLA, average resolution rate, busiest department
//   * sort control   workload / resolution rate / past SLA / high severity (the project's own Merge Sort)
//   * department cards   status counts, resolution rate, past-SLA and high-severity counts, the officer in charge
//   * click a card   -> detail dialog: officers (phone, last login), status breakdown, SLA / age metrics, the most
//                       urgent open complaints (AI priority), busiest cities and an AI insight; "View complaints"
//                       jumps to the complaint list filtered to that department's category
//   * Export CSV     one row per department in exports/
// =====================================================================
#include "App.h"
#include "../../../backend/Config.h"
#include "../../../backend/Exceptions.h"
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>

using namespace ui;
using namespace scms;

static const Palette& P() { return Theme::p(); }
static std::string num(long long v) { return std::to_string(v); }
static std::string pct(double v) { char b[32]; std::snprintf(b, sizeof b, "%.0f%%", v); return b; }

// ------------------------------------------------------------------ data
void App::loadDepartments() {
    deptLoaded_ = true;
    guarded([&] {
        DepartmentRanking dr = svc_.departmentRanking();
        std::vector<UserRow> users = db_.getUsers();
        deptInfo_.clear();
        for (const Department& d : dr.rows) {
            DeptInfo di;
            di.id = d.id();
            di.name = d.name();
            di.code = d.code();
            if (!d.categories().empty()) di.categoryCode = d.categories()[0];
            for (const auto& cat : svc_.categories())
                if (cat.code == di.categoryCode) { di.categoryName = cat.name; di.slaHours = cat.slaHours; }
            const Workload& w = d.workload();
            di.assigned = w.assigned; di.inProgress = w.inProgress; di.resolved = w.resolved; di.closed = w.closed; di.highOpen = w.highSeverity;
            di.resolutionRate = d.resolutionRate();
            Scope sc;
            sc.departmentId = d.id();
            di.tri = agent_.triageScope(sc, false);
            double sum = 0;
            for (const auto& it : di.tri.items) {
                sum += it.ageHours;
                if (it.ageHours > di.oldestAgeHours) { di.oldestAgeHours = it.ageHours; di.oldestRef = it.reference; }
            }
            if (!di.tri.items.empty()) di.avgAgeHours = sum / di.tri.items.size();
            for (const auto& u : users)
                if (u.role == "DEPARTMENT" && u.department == d.name()) di.officers.push_back(u);
            deptInfo_.push_back(di);
        }
        deptSortMetrics_ = dr.sort;
    });
}

// the AI's reading of one department, from plain facts
ai::Lines App::deptInsight(const DeptInfo& d) const {
    ai::Lines out;
    double avg = 0;
    for (const auto& x : deptInfo_) avg += x.resolutionRate;
    if (!deptInfo_.empty()) avg /= deptInfo_.size();
    const int open = d.tri.open;
    if (open == 0) {
        out.push_back({ai::LineKind::TEXT, "Nothing is waiting for this department right now."});
    } else {
        out.push_back({d.tri.overdue * 2 > open ? ai::LineKind::WARNING : ai::LineKind::TEXT,
                       num(d.tri.overdue) + " of " + num(open) + " open complaints are past their " + num(d.slaHours) + "-hour target" +
                           (d.oldestRef.empty() ? "." : "; the oldest is " + d.oldestRef + " (" + ai::AiAgent::formatHours(d.oldestAgeHours) + ").")});
        if (d.tri.critical > 0) out.push_back({ai::LineKind::BULLET, num(d.tri.critical) + " critical by AI priority - start with the list above."});
        if (d.cities.rows.size() > 0) out.push_back({ai::LineKind::BULLET, "Most complaints come from " + d.cities.rows[0].city + " (" + num(d.cities.rows[0].complaints) + ")."});
        if (d.tri.overdue * 2 > open) out.push_back({ai::LineKind::TIP, "More than half of the open work is overdue: add crew or re-prioritise with the AI triage."});
    }
    out.push_back({ai::LineKind::BULLET, "Resolution rate " + pct(d.resolutionRate) + (d.resolutionRate + 0.5 >= avg ? ", at or above" : ", below") + " the " + pct(avg) + " average of all departments."});
    return out;
}

void App::exportDepartmentsCsv() {
    std::string dir = Config::instance().get("EXPORT_DIR", "exports");
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::time_t now = std::time(nullptr);
    char stamp[32];
    std::strftime(stamp, sizeof stamp, "%Y%m%d_%H%M%S", std::localtime(&now));
    std::string path = dir + "/departments_" + stamp + ".csv";
    std::ofstream out(path);
    if (!out) { UI::get().toast("Could not write " + path, ToastKind::ERR); return; }
    out << "Department,Code,Category,SLA hours,Officers,Assigned,In progress,Resolved,Closed,Resolution rate %,Open,Past SLA,Close to SLA,Critical (AI),Open high severity\n";
    for (const auto& d : deptInfo_) {
        std::string officers;
        for (const auto& o : d.officers) officers += (officers.empty() ? "" : "; ") + o.fullName;
        out << '"' << d.name << "\"," << d.code << ',' << d.categoryName << ',' << d.slaHours << ",\"" << officers << "\"," << d.assigned << ',' << d.inProgress << ',' << d.resolved
            << ',' << d.closed << ',' << static_cast<int>(d.resolutionRate + 0.5) << ',' << d.tri.open << ',' << d.tri.overdue << ',' << d.tri.atRisk << ',' << d.tri.critical << ',' << d.highOpen << "\n";
    }
    UI::get().toast("Saved " + path, ToastKind::SUCCESS);
}

// ------------------------------------------------------------------ the screen
void App::screenDepartments() {
    if (!deptLoaded_) loadDepartments();
    UI& u = UI::get();
    Rect c = content();
    const float cardH = 250, gap = 16;
    int rows = static_cast<int>((deptInfo_.size() + 2) / 3);
    float H = 24 + 44 + 14 + 92 + 22 + rows * (cardH + gap) + 40;
    float y0 = pageScroll_.begin(c, std::max(H, c.h));
    Rect r{c.x + 28, y0 + 24, c.w - 56, H - 40};

    // ---- order of the cards (the project's Merge Sort, never std::sort)
    struct Key { int idx; double key; };
    std::vector<Key> keys;
    for (std::size_t i = 0; i < deptInfo_.size(); ++i) {
        const DeptInfo& d = deptInfo_[i];
        double k = deptSortSel_ == 0 ? d.assigned + d.inProgress : deptSortSel_ == 1 ? d.resolutionRate : deptSortSel_ == 2 ? d.tri.overdue : d.highOpen;
        keys.push_back({static_cast<int>(i), k});
    }
    SortMetrics sm = mergeSort(keys, [](const Key& a, const Key& b) { return a.key > b.key; });

    // ---- controls
    static const char* sortNames[] = {"Open workload", "Resolution rate", "Past SLA", "High severity"};
    Renderer::text(r.x, r.y + 14, "Sort by", 12.5f, P().textMuted);
    segmented({r.x + 56, r.y - 4, 470, 38}, {sortNames[0], sortNames[1], sortNames[2], sortNames[3]}, deptSortSel_);
    Renderer::text(r.x + 540, r.y + 14, "Merge Sort  \xC2\xB7  " + num(sm.comparisons) + " comparisons on " + num(sm.n) + " departments", 12, P().textFaint);
    if (button({r.right() - 120, r.y - 4, 120, 38}, "Refresh", ButtonStyle::SECONDARY, true, "refresh")) deptLoaded_ = false;
    if (button({r.right() - 120 - 12 - 130, r.y - 4, 130, 38}, "Export CSV", ButtonStyle::SECONDARY, true, "download")) exportDepartmentsCsv();

    // ---- KPI strip
    int openSum = 0, overSum = 0, atRiskSum = 0;
    double rateSum = 0;
    const DeptInfo* busiest = nullptr;
    for (const auto& d : deptInfo_) {
        openSum += d.tri.open; overSum += d.tri.overdue; atRiskSum += d.tri.atRisk; rateSum += d.resolutionRate;
        if (!busiest || d.tri.open > busiest->tri.open) busiest = &d;
    }
    double avgRate = deptInfo_.empty() ? 0 : rateSum / deptInfo_.size();
    float ky = r.y + 44 + 14;
    float kw = (r.w - 4 * 12) / 5;
    struct Kpi { const char* label; std::string value, sub; Color color; const char* ic; };
    Kpi kpis[] = {
        {"DEPARTMENTS", num(deptInfo_.size()), "one per service area", P().primary, "building"},
        {"OPEN WORK", num(openSum), "assigned + in progress", P().info, "list"},
        {"PAST SLA", num(overSum), num(atRiskSum) + " more close to it", overSum ? P().danger : P().success, "clock"},
        {"AVG RESOLUTION", pct(avgRate), "resolved + closed / total", P().success, "check"},
        {"BUSIEST", busiest ? busiest->code : "-", busiest ? num(busiest->tri.open) + " open complaints" : "", P().warning, "trophy"},
    };
    for (int i = 0; i < 5; ++i) {
        Rect k{r.x + i * (kw + 12), ky, kw, 92};
        card(k);
        Renderer::text(k.x + 18, k.y + 26, kpis[i].label, 11, P().textMuted, Weight::SEMIBOLD);
        Renderer::text(k.x + 18, k.y + 58, kpis[i].value, 24, P().text, Weight::BOLD);
        Renderer::textIn({k.x + 18, k.y + 66, k.w - 36, 18}, kpis[i].sub, 11.5f, P().textFaint);
        Renderer::circle(k.right() - 30, k.y + 30, 16, kpis[i].color.withAlpha(0.14f));
        icon(kpis[i].ic, k.right() - 30, k.y + 30, 15, kpis[i].color);
    }

    // ---- department cards
    float cy = ky + 92 + 22;
    float w3 = (r.w - 32) / 3;
    for (std::size_t pos = 0; pos < keys.size(); ++pos) {
        const DeptInfo& d = deptInfo_[keys[pos].idx];
        Rect b{r.x + (pos % 3) * (w3 + 16), cy + (pos / 3) * (cardH + gap), w3, cardH};
        bool hov = !deptDialog_ && u.hover(b);
        card(b, hov);
        if (hov) u.wantHand();
        Color col = P().chart[keys[pos].idx % 8];
        Renderer::circle(b.x + 36, b.y + 38, 18, col.withAlpha(0.14f));
        Renderer::textIn({b.x + 18, b.y + 20, 36, 36}, "#" + num(pos + 1), 12, col, Weight::BOLD, Align::CENTER);
        Renderer::textIn({b.x + 66, b.y + 22, b.w - 86, 20}, d.name, 14.5f, P().text, Weight::SEMIBOLD);
        Renderer::textIn({b.x + 66, b.y + 42, b.w - 86, 18}, d.code + "  \xC2\xB7  handles " + (d.categoryCode.empty() ? "-" : d.categoryCode), 12, P().textMuted);
        float yy = b.y + 84;
        std::pair<const char*, int> stats[] = {{"Assigned", d.assigned}, {"In progress", d.inProgress}, {"Resolved", d.resolved}, {"Closed", d.closed}};
        for (int k = 0; k < 4; ++k) {
            float sx = b.x + 20 + k * (b.w - 40) / 4;
            Renderer::text(sx, yy, num(stats[k].second), 18, P().text, Weight::BOLD);
            Renderer::text(sx, yy + 20, stats[k].first, 11.5f, P().textMuted);
        }
        yy += 52;
        Renderer::text(b.x + 20, yy, "Resolution rate", 12, P().textMuted);
        Renderer::text(b.right() - 20 - Renderer::measure(pct(d.resolutionRate), 12, Weight::SEMIBOLD), yy, pct(d.resolutionRate), 12, P().text, Weight::SEMIBOLD);
        progressBar({b.x + 20, yy + 10, b.w - 40, 8}, static_cast<float>(d.resolutionRate / 100.0), P().success);
        yy += 38;
        Renderer::text(b.x + 20, yy, "Past SLA: " + num(d.tri.overdue), 12, d.tri.overdue ? P().danger : P().textMuted, Weight::SEMIBOLD);
        Renderer::text(b.x + 130, yy, "Open high-severity: " + num(d.highOpen), 12, d.highOpen ? P().danger : P().textMuted, Weight::SEMIBOLD);
        yy += 24;
        std::string off = d.officers.empty() ? "No officer account" : d.officers[0].fullName + (d.officers.size() > 1 ? "  +" + num(d.officers.size() - 1) : "");
        icon("user", b.x + 28, yy - 4, 12, P().textFaint);
        Renderer::textIn({b.x + 42, yy - 12, b.w - 140, 18}, off, 12, P().textMuted);
        Renderer::text(b.right() - 20 - Renderer::measure("Details", 12, Weight::SEMIBOLD), yy, "Details", 12, hov ? P().primary : P().textFaint, Weight::SEMIBOLD);
        if (hov && u.clicked(b)) {
            deptOpenIdx_ = keys[pos].idx;
            deptDialog_ = true;
            DeptInfo& dd = deptInfo_[deptOpenIdx_];
            if (!dd.citiesLoaded) {
                dd.citiesLoaded = true;
                guarded([&] { Scope sc; sc.departmentId = dd.id; dd.cities = svc_.cityRanking(true, 5, "", sc); });
            }
        }
    }
    pageScroll_.end();
    if (deptDialog_) deptDetailDialog();
}

// ------------------------------------------------------------------ detail dialog
void App::deptDetailDialog() {
    if (deptOpenIdx_ < 0 || deptOpenIdx_ >= static_cast<int>(deptInfo_.size())) { deptDialog_ = false; return; }
    UI& u = UI::get();
    const DeptInfo& d = deptInfo_[deptOpenIdx_];
    Rect m{(width_ - 920) / 2.0f, std::max(16.0f, (height_ - 664) / 2.0f), 920, 664};
    if (!modalBegin(m, d.name, deptDialog_)) return;
    float x0 = m.x + 28, leftW = 410, rightX = x0 + leftW + 30, rightW = m.right() - 28 - rightX;
    float y = m.y + 66;
    Renderer::text(x0, y + 10, d.code + "  \xC2\xB7  handles " + d.categoryName + "  \xC2\xB7  resolution target " + num(d.slaHours) + " hours", 12.5f, P().textMuted);
    y += 30;
    const float top = y;

    // ---- left column: officers, status breakdown, metrics
    Renderer::text(x0, y + 12, "OFFICERS", 11, P().primary, Weight::BOLD);
    y += 22;
    if (d.officers.empty()) { Renderer::text(x0, y + 14, "No officer account is assigned to this department.", 12.5f, P().textFaint); y += 26; }
    for (std::size_t i = 0; i < d.officers.size() && i < 3; ++i) {
        const UserRow& o = d.officers[i];
        Renderer::circle(x0 + 16, y + 20, 16, P().primarySoft);
        icon("user", x0 + 16, y + 20, 14, P().primary);
        Renderer::text(x0 + 42, y + 16, o.fullName, 13.5f, P().text, Weight::SEMIBOLD);
        badge(x0 + leftW - 70, y + 6, o.active ? "Active" : "Disabled", o.active ? P().success : P().textMuted, o.active ? P().successSoft : P().neutralSoft, 11);
        Renderer::textIn({x0 + 42, y + 22, leftW - 50, 18}, o.username + "  \xC2\xB7  " + o.phone + "  \xC2\xB7  last login " + (o.lastLogin.empty() ? "never" : o.lastLogin), 11.5f, P().textMuted);
        y += 46;
    }
    y += 8;
    Renderer::text(x0, y + 12, "STATUS BREAKDOWN", 11, P().primary, Weight::BOLD);
    y += 22;
    {
        int total = std::max(1, d.assigned + d.inProgress + d.resolved + d.closed);
        int vals[] = {d.assigned, d.inProgress, d.resolved, d.closed};
        const char* names[] = {"Assigned", "In progress", "Resolved", "Closed"};
        Color cols[] = {P().info, P().primary, P().success, P().textFaint};
        float bx = x0;
        Renderer::roundRect({x0, y, leftW, 16}, 8, P().neutralSoft);
        Renderer::pushClip({x0, y, leftW, 16});
        for (int k = 0; k < 4; ++k) {
            float bw = leftW * vals[k] / total;
            Renderer::rect({bx, y, bw, 16}, cols[k]);
            bx += bw;
        }
        Renderer::popClip();
        for (int k = 0; k < 4; ++k) {
            float lx = x0 + (k % 2) * (leftW / 2), ly = y + 30 + (k / 2) * 20;
            Renderer::circle(lx + 5, ly + 5, 4.5f, cols[k]);
            Renderer::text(lx + 16, ly + 10, std::string(names[k]) + "  " + num(vals[k]), 12, P().text);
        }
    }
    y += 78;
    Renderer::text(x0, y + 12, "WORKLOAD NOW", 11, P().primary, Weight::BOLD);
    y += 22;
    {
        struct M { const char* label; std::string value; Color color; };
        M ms[] = {
            {"Open", num(d.tri.open), P().text},
            {"Past SLA", num(d.tri.overdue), d.tri.overdue ? P().danger : P().text},
            {"Close to SLA", num(d.tri.atRisk), d.tri.atRisk ? P().warning : P().text},
            {"Critical (AI)", num(d.tri.critical), d.tri.critical ? P().danger : P().text},
            {"Average age", d.tri.items.empty() ? "-" : ai::AiAgent::formatHours(d.avgAgeHours), P().text},
            {"Oldest open", d.oldestRef.empty() ? "-" : ai::AiAgent::formatHours(d.oldestAgeHours), P().text},
        };
        float bw = (leftW - 2 * 10) / 3.0f;
        for (int k = 0; k < 6; ++k) {
            Rect b{x0 + (k % 3) * (bw + 10), y + (k / 3) * 66, bw, 56};
            Renderer::roundRect(b, 8, P().surfaceAlt);
            Renderer::text(b.x + 12, b.y + 22, ms[k].value, 17, ms[k].color, Weight::BOLD);
            Renderer::text(b.x + 12, b.y + 42, ms[k].label, 11, P().textMuted);
        }
    }

    // ---- right column: urgent complaints, busiest cities, AI insight
    float ry = top;
    Renderer::text(rightX, ry + 12, "MOST URGENT OPEN COMPLAINTS  (AI priority)", 11, P().primary, Weight::BOLD);
    ry += 22;
    if (d.tri.items.empty()) { Renderer::text(rightX, ry + 14, "No open complaints.", 12.5f, P().textFaint); ry += 26; }
    for (std::size_t i = 0; i < d.tri.items.size() && i < 3; ++i) {
        const ai::TriageItem& it = d.tri.items[i];
        Rect row{rightX - 6, ry, rightW + 12, 42};
        bool hov = u.hover(row);
        if (hov) { Renderer::roundRect(row, 8, P().hover); u.wantHand(); }
        Color fg = P().textMuted, bg = P().neutralSoft;
        if (it.label == "Critical") { fg = P().danger; bg = P().dangerSoft; }
        else if (it.label == "Urgent") { fg = P().warning; bg = P().warningSoft; }
        else if (it.label == "Elevated") { fg = P().info; bg = P().infoSoft; }
        else if (it.label == "Routine") { fg = P().success; bg = P().successSoft; }
        Renderer::text(rightX + 2, ry + 17, it.reference, 12.5f, P().text, Weight::SEMIBOLD);
        std::string lab = it.label + " " + num(it.priority);
        badge(rightX + rightW - Renderer::measure(lab, 11, Weight::SEMIBOLD) - 16, ry + 8, lab, fg, bg, 11);
        Renderer::textIn({rightX + 2, ry + 22, rightW - 4, 16}, it.city + "  \xC2\xB7  " + it.reason, 11.5f, P().textMuted);
        if (hov && u.clicked(row)) {
            deptDialog_ = false;
            backFromDetail_ = Screen::DEPARTMENTS;
            openDetail(it.id);
            return;
        }
        ry += 46;
    }
    ry += 6;
    Renderer::text(rightX, ry + 12, "BUSIEST CITIES", 11, P().primary, Weight::BOLD);
    ry += 22;
    if (d.cities.rows.empty()) { Renderer::text(rightX, ry + 14, "No complaints yet.", 12.5f, P().textFaint); ry += 24; }
    int maxC = 1;
    for (const auto& cr : d.cities.rows) maxC = std::max(maxC, cr.complaints);
    for (std::size_t i = 0; i < d.cities.rows.size() && i < 4; ++i) {
        const auto& cr = d.cities.rows[i];
        Renderer::textIn({rightX, ry, 150, 20}, num(i + 1) + ".  " + cr.city, 12.5f, P().text);
        progressBar({rightX + 156, ry + 5, rightW - 156 - 40, 8}, static_cast<float>(cr.complaints) / maxC, P().chart[i % 8]);
        Renderer::text(rightX + rightW - 28, ry + 14, num(cr.complaints), 12.5f, P().text, Weight::SEMIBOLD);
        ry += 24;
    }
    ry += 8;
    Renderer::text(rightX, ry + 12, "AI INSIGHT", 11, P().primary, Weight::BOLD);
    ry += 24;
    aiLines(deptInsight(d), rightX, ry, rightW, true);

    // ---- buttons
    float by = m.bottom() - 62;
    if (button({m.right() - 28 - 96, by, 96, 40}, "Close", ButtonStyle::SECONDARY)) deptDialog_ = false;
    if (button({x0, by, 190, 40}, "View complaints", ButtonStyle::PRIMARY, true, "list")) {
        if (fCategory_.items.empty()) {   // same items the complaint list builds
            fCategory_.items = {"All categories"};
            for (const auto& cat : svc_.categories()) fCategory_.items.push_back(cat.name);
        }
        int sel = 0;
        for (std::size_t i = 0; i < svc_.categories().size(); ++i)
            if (svc_.categories()[i].code == d.categoryCode) sel = static_cast<int>(i) + 1;
        fCategory_.selected = sel;
        fStatus_.selected = 0;
        listSearch_.field.clear();
        listPage_ = 0;
        listDirty_ = true;
        deptDialog_ = false;
        go(Screen::COMPLAINTS);
    }
}
