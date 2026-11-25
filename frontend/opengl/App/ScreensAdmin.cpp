// Administrator / analytics screens: queue, map, ranking, departments, reports, users, audit,
// DSA engine, Assembly engine, 3-D analytics, settings & system health
#include "App.h"
#include "../../../backend/AsmEngine.h"
#include "../../../backend/Config.h"
#include "../../../backend/Exceptions.h"
#include "../../../backend/Validator.h"
#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace ui;
using namespace scms;

static const Palette& P() { return Theme::p(); }
static std::string num(long long v) { return std::to_string(v); }
static std::string f1(double v, const char* fmt = "%.1f") { char b[48]; std::snprintf(b, sizeof b, fmt, v); return b; }

// ============================================================ PROCESSING QUEUE (FIFO)
void App::screenQueue() {
    Rect c = content();
    Rect r{c.x + 28, c.y + 24, c.w - 56, c.h - 48};
    ComplaintQueue& q = svc_.queue();
    Renderer::text(r.x, r.y + 14, "New complaints wait here in arrival order. The processor always takes the FRONT.", 13.5f, P().textMuted);
    if (button({r.right() - 150, r.y - 4, 150, 38}, "Process next", ButtonStyle::PRIMARY, !q.isEmpty(), "arrow-right")) {
        if (guarded([&] { lastProcess_ = svc_.processNextInQueue(*user_); })) {
            processedOnce_ = true;
            UI::get().toast(lastProcess_.reference + " assigned to " + lastProcess_.department, ToastKind::SUCCESS);
            markDirty();
            refreshNotifications();
        }
    }
    if (button({r.right() - 310, r.y - 4, 150, 38}, "Process 10", ButtonStyle::SECONDARY, !q.isEmpty())) {
        int n = 0;
        while (n < 10 && !q.isEmpty() && guarded([&] { lastProcess_ = svc_.processNextInQueue(*user_); })) ++n;
        processedOnce_ = n > 0;
        UI::get().toast(num(n) + " complaints routed to departments", ToastKind::SUCCESS);
        markDirty();
    }
    if (button({r.right() - 420, r.y - 4, 100, 38}, "Peek", ButtonStyle::GHOST)) {
        guarded([&] { UI::get().toast("Front of queue: " + q.front().reference, ToastKind::INFO); });   // throws on empty
    }
    float y = r.y + 40;
    float w4 = (r.w - 48) / 4;
    std::string front = q.isEmpty() ? "-" : q.front().reference;
    kpiCard({r.x, y, w4, 110}, "QUEUE SIZE", num(q.size()), "pending complaints", P().warning, "queue");
    kpiCard({r.x + (w4 + 16), y, w4, 110}, "FRONT", front, "next to be assigned", P().primary, "arrow-right");
    kpiCard({r.x + 2 * (w4 + 16), y, w4, 110}, "PROCESSED", num(svc_.processedCount()), "this session", P().success, "check");
    kpiCard({r.x + 3 * (w4 + 16), y, w4, 110}, "STATE", processedOnce_ ? "Last: " + svc_.lastProcessed() : "Idle",
            "enqueued " + num(q.totalEnqueued()) + " / dequeued " + num(q.totalDequeued()), P().info, "refresh");
    y += 130;

    // queue visual: FRONT -> REAR
    Rect vis{r.x, y, r.w, 190};
    card(vis);
    sectionTitle({vis.x + 20, vis.y + 14, vis.w, 30}, "FIFO queue (front \xE2\x86\x92 rear)", "enqueue at REAR on submission  \xC2\xB7  dequeue at FRONT by the processor");
    auto items = q.snapshot(9);
    float bx = vis.x + 24, by = vis.y + 84, bw = (vis.w - 48 - 8 * 16) / 9;
    if (items.empty()) emptyState({vis.x, vis.y + 50, vis.w, 130}, "Queue is empty", "Every complaint has been assigned");
    for (std::size_t i = 0; i < items.size(); ++i) {
        Rect b{bx + i * (bw + 16), by, bw, 74};
        const CategoryRow* cat = svc_.categoryByCode(items[i].categoryCode);
        Renderer::roundRect(b, 10, i == 0 ? P().primarySoft : P().surfaceAlt);
        Renderer::roundRectOutline(b, 10, i == 0 ? P().primary : P().border, i == 0 ? 1.5f : 1.0f);
        Renderer::textIn({b.x + 10, b.y + 8, b.w - 20, 20}, items[i].reference.substr(items[i].reference.size() > 6 ? items[i].reference.size() - 6 : 0), 13,
                         P().text, Weight::SEMIBOLD);
        Renderer::textIn({b.x + 10, b.y + 28, b.w - 20, 18}, cat ? cat->name : items[i].categoryCode, 12, P().textMuted);
        Renderer::textIn({b.x + 10, b.y + 48, b.w - 20, 18}, "severity " + num(items[i].severity), 11.5f, P().textFaint);
        if (i + 1 < items.size()) icon("chevron-right", b.right() + 8, b.y + 37, 10, P().textFaint);
        if (UI::get().hover(b)) UI::get().tooltip(items[i].reference + " queued " + fmtTime(items[i].enqueuedAt));
    }
    if (!items.empty()) {
        Renderer::text(bx, by + 96, "FRONT", 11, P().primary, Weight::BOLD);
        if (q.size() > items.size()) Renderer::text(vis.right() - 140, by + 96, "+ " + num(q.size() - items.size()) + " more to REAR", 11, P().textMuted, Weight::SEMIBOLD);
    }
    y += 206;

    // last processing trace + admin action stack
    Rect a{r.x, y, r.w * 0.6f - 8, r.bottom() - y}, b{r.x + r.w * 0.6f + 8, y, r.w * 0.4f - 8, r.bottom() - y};
    card(a);
    sectionTitle({a.x + 20, a.y + 14, a.w, 30}, "Last processing run", "dequeue \xE2\x86\x92 category \xE2\x86\x92 Department object \xE2\x86\x92 assign \xE2\x86\x92 PostgreSQL \xE2\x86\x92 history \xE2\x86\x92 notify");
    float ty = a.y + 76;
    if (!processedOnce_) Renderer::text(a.x + 24, ty, "Press 'Process next' to route the front complaint.", 13, P().textMuted);
    for (const auto& s : lastProcess_.steps) {
        Renderer::circle(a.x + 30, ty - 4, 9, P().successSoft);
        icon("check", a.x + 30, ty - 4, 9, P().success);
        Renderer::textIn({a.x + 48, ty - 15, a.w - 70, 22}, s, 13, P().text);
        ty += 32;
    }
    card(b);
    sectionTitle({b.x + 20, b.y + 14, b.w, 30}, "Recent admin actions (Stack)", "LIFO - most recent action on top");
    auto& st = svc_.adminActions();
    auto acts = st.topToBottom();
    float sy = b.y + 72;
    Renderer::text(b.x + 24, sy, "TOP", 10.5f, P().primary, Weight::BOLD);
    sy += 10;
    for (std::size_t i = 0; i < acts.size() && sy < b.bottom() - 70; ++i) {
        Rect e{b.x + 24, sy, b.w - 48, 34};
        Renderer::roundRect(e, 7, i == 0 ? P().primarySoft : P().surfaceAlt);
        Renderer::textIn({e.x + 12, e.y, e.w - 90, e.h}, acts[i].label, 12.5f, P().text, i == 0 ? Weight::SEMIBOLD : Weight::REGULAR);
        Renderer::textIn({e.right() - 80, e.y, 70, e.h}, acts[i].detail, 11.5f, P().textMuted, Weight::REGULAR, Align::RIGHT);
        sy += 40;
    }
    if (acts.empty()) Renderer::text(b.x + 24, sy + 18, "Stack is empty - no actions yet this session.", 12.5f, P().textMuted);
    Renderer::text(b.x + 24, b.bottom() - 54, "size " + num(st.size()) + " / capacity " + num(st.capacity()), 12, P().textMuted);
    if (button({b.right() - 132, b.bottom() - 70, 108, 34}, "Pop top", ButtonStyle::SECONDARY, true)) {
        guarded([&] { StackEntry e = svc_.adminActions().pop(); UI::get().toast("Popped: " + e.label, ToastKind::INFO); });   // underflow -> error toast
    }
}

// ============================================================ INDIA MAP
// Map and City Ranking are available to every role. The administrator always sees all of India; a citizen or a
// department can switch between "All India" (aggregate counts only, no personal data) and their own scope
// (their complaints / their department's complaints).
Scope App::geoScope() const {
    if (!user_ || user_->role() == Role::ADMIN || viewScope_ == 0) return {};
    return svc_.scopeFor(*user_);
}

void App::scopeToggle(const Rect& r) {
    if (!user_ || user_->role() == Role::ADMIN) return;
    const bool citizen = user_->role() == Role::CITIZEN;
    if (segmented(r, {"All India", citizen ? "My complaints" : "My department"}, viewScope_)) {
        mapLoaded_ = false;
        rankLoaded_ = false;
        selState_ = selDistrict_ = -1;
    }
}

void App::loadMap() {
    if (mapLoaded_ || !india_.loaded()) return;
    guarded([&] {
        auto stats = db_.stateStats(geoScope());
        mapStates_.assign(india_.states().size(), RegionStat{});
        stateCount_.assign(india_.states().size(), 0);
        int mx = 0;
        for (const auto& s : stats) {
            int i = india_.stateIndex(s.name);
            if (i < 0) continue;
            mapStates_[i] = s;
            stateCount_[i] = s.complaints;
            mx = std::max(mx, s.complaints);
        }
        // legend thresholds derived from the data so legend == rendering
        t1_ = std::max(1, (mx + 2) / 3);
        t2_ = std::max(t1_ + 1, (2 * mx + 2) / 3);
        if (allCities_.empty()) allCities_ = db_.getAllCities();
        db_.loadCityStats(mapMarkers_, geoScope(), true);   // city bubbles follow the same scope
        mapLoaded_ = true;
        if (selState_ >= 0) {
            mapDistricts_ = db_.districtStats(mapStates_[selState_].id, geoScope());
            districtSort_ = insertionSort(mapDistricts_.data(), mapDistricts_.size(),
                                          [](const RegionStat& a, const RegionStat& b) { return a.complaints > b.complaints; });
        }
    });
}

static Color intensity(int v, int t1, int t2) {
    const Palette& p = Theme::p();
    if (v <= 0) return p.mapNone;
    if (v <= t1) return p.mapLow;
    if (v <= t2) return p.mapMedium;
    return p.mapHigh;
}

void App::screenMap() {
    loadMap();
    Rect c = content();
    Rect r{c.x + 24, c.y + 20, c.w - 48, c.h - 40};
    float panelW = 360;
    Rect mapCard{r.x, r.y, r.w - panelW - 16, r.h};
    Rect side{mapCard.right() + 16, r.y, panelW, r.h};
    card(mapCard);
    Renderer::text(mapCard.x + 22, mapCard.y + 34, "India live complaint map", 17, P().text, Weight::SEMIBOLD);
    badge(mapCard.x + 26 + Renderer::measure("India live complaint map", 17, Weight::SEMIBOLD), mapCard.y + 28, "Live PostgreSQL data", P().success, P().successSoft);
    if (!india_.loaded()) {
        emptyState(mapCard, "Map data unavailable", mapError_.empty() ? "opengl/data/india.map missing" : mapError_);
        return;
    }
    Rect view{mapCard.x + 12, mapCard.y + 56, mapCard.w - 24, mapCard.h - 68};
    india_.setViewport(view);
    UI& u = UI::get();

    // map controls (+, -, Reset view) and the legend sit ON TOP of the map. Their rectangles are
    // defined first so the pan / pick logic below leaves clicks on them alone - before this fix the
    // pan handler swallowed every press inside the map area, so the buttons never received a click.
    float cx = view.right() - 46, cy = view.y + 10;
    Rect zoomIn{cx, cy, 36, 34}, zoomOut{cx, cy + 40, 36, 34}, zoomReset{cx - 70, cy + 80, 106, 34};
    Rect lg{view.x + 10, view.bottom() - 128, 210, 118};
    bool onControl = u.hover(zoomIn) || u.hover(zoomOut) || u.hover(zoomReset) || u.hover(lg);

    // interaction: wheel zoom, drag pan, hover, click select
    bool over = u.hover(view);
    bool onMap = over && !onControl;
    if (over && u.mouse.wheel != 0) { india_.zoomAt(u.mouse.x, u.mouse.y, u.mouse.wheel > 0 ? 1.25f : 0.8f); u.mouse.wheel = 0; }
    if (onMap && u.mouse.pressed && u.clickAvailable()) { dragging_ = true; dragX_ = u.mouse.x; dragY_ = u.mouse.y; dragDist_ = 0; u.consumeClick(); }
    if (dragging_ && u.mouse.down) {
        india_.pan(u.mouse.x - dragX_, u.mouse.y - dragY_);
        dragDist_ += std::abs(u.mouse.x - dragX_) + std::abs(u.mouse.y - dragY_);
        dragX_ = u.mouse.x; dragY_ = u.mouse.y;
    }
    hoverDistrict_ = onMap && !dragging_ ? india_.pickDistrict(u.mouse.x, u.mouse.y) : -1;
    if (dragging_ && u.mouse.released) {
        dragging_ = false;
        if (dragDist_ < 4) {   // a click, not a drag
            int d = india_.pickDistrict(u.mouse.x, u.mouse.y);
            if (d >= 0) {
                int s = india_.districts()[d].state;
                if (s != selState_) { selState_ = s; selDistrict_ = -1; mapLoaded_ = false; }
                else selDistrict_ = india_.districts()[d].name == "~" ? -1 : d;
            } else { selState_ = -1; selDistrict_ = -1; }
        }
    }
    int hoverState = hoverDistrict_ >= 0 ? india_.districts()[hoverDistrict_].state : -1;
    if (!mapLoaded_) loadMap();

    std::vector<Color> fill(india_.states().size());
    for (std::size_t i = 0; i < fill.size(); ++i) fill[i] = intensity(i < stateCount_.size() ? stateCount_[i] : 0, t1_, t2_);
    // when a state is selected its districts are coloured by their own counts
    std::vector<Color> dfill;
    if (selState_ >= 0 && !mapDistricts_.empty()) {
        dfill.resize(india_.districts().size());
        for (std::size_t i = 0; i < dfill.size(); ++i) {
            const auto& d = india_.districts()[i];
            dfill[i] = fill[d.state];
            if (d.state == selState_) {
                int v = 0;
                for (const auto& ds : mapDistricts_) if (ds.name == d.name) v = ds.complaints;
                int dmx = 1;
                for (const auto& ds : mapDistricts_) dmx = std::max(dmx, ds.complaints);
                dfill[i] = intensity(v, std::max(1, (dmx + 2) / 3), std::max(2, (2 * dmx + 2) / 3));
            }
        }
    }
    india_.render(fill, hoverState, selState_, hoverDistrict_, selDistrict_, dfill.empty() ? nullptr : &dfill);

    // city markers sized by complaints (from the CityArray loaded via PostgreSQL)
    if (india_.zoom() > 1.4f) {
        Renderer::pushClip(view);
        for (std::size_t i = 0; i < mapMarkers_.size(); ++i) {
            const CityStat& cs = mapMarkers_.at(i);
            for (const auto& ci : allCities_)
                if (ci.cityId == cs.cityId) {
                    map::Pt w = india_.project(ci.lon, ci.lat);
                    map::Pt sp = india_.toScreen(w.x, w.y);
                    float rad = 3.0f + std::min(9.0f, cs.complaints * 0.7f);
                    Renderer::circle(sp.x, sp.y, rad + 1.5f, hex(0xFFFFFF, 0.9f));
                    Renderer::circle(sp.x, sp.y, rad, P().primary.withAlpha(0.85f));
                    if (std::hypot(u.mouse.x - sp.x, u.mouse.y - sp.y) < rad + 2)
                        u.tooltip(cs.city + ", " + cs.state + ": " + num(cs.complaints) + " complaints");
                    break;
                }
        }
        Renderer::popClip();
    }
    std::vector<std::string> extra(india_.states().size());
    for (std::size_t i = 0; i < extra.size() && i < stateCount_.size(); ++i) extra[i] = stateCount_[i] ? num(stateCount_[i]) : "";
    india_.renderLabels(extra, selState_);
    if (hoverDistrict_ >= 0) {
        const auto& d = india_.districts()[hoverDistrict_];
        const auto& s = india_.states()[d.state];
        std::string t = s.name + "  \xC2\xB7  " + num(stateCount_[d.state]) + " complaints";
        if (d.name != "~") t = d.name + ", " + t;
        u.tooltip(t);
        u.wantHand();
    }
    // map controls: zoom about the centre of the view; Reset also clears the state / district selection
    if (button(zoomIn, "+", ButtonStyle::SECONDARY)) india_.zoomAt(view.x + view.w / 2, view.y + view.h / 2, 1.4f);
    if (button(zoomOut, "-", ButtonStyle::SECONDARY)) india_.zoomAt(view.x + view.w / 2, view.y + view.h / 2, 1 / 1.4f);
    if (button(zoomReset, "Reset view", ButtonStyle::SECONDARY)) { india_.resetView(); selState_ = selDistrict_ = -1; mapLoaded_ = false; }
    // legend - thresholds are the exact ones used for the colours above
    Renderer::roundRect(lg, 10, P().surface.withAlpha(0.94f));
    Renderer::roundRectOutline(lg, 10, P().border);
    Renderer::text(lg.x + 14, lg.y + 22, "Complaints per state", 12, P().text, Weight::SEMIBOLD);
    std::pair<Color, std::string> leg[] = {{P().mapNone, "0  No complaints"}, {P().mapLow, "1 - " + num(t1_) + "  Low"},
                                           {P().mapMedium, num(t1_ + 1) + " - " + num(t2_) + "  Medium"}, {P().mapHigh, num(t2_ + 1) + "+  High"}};
    for (int i = 0; i < 4; ++i) {
        Renderer::roundRect({lg.x + 14, lg.y + 34 + i * 20.0f, 14, 12}, 3, leg[i].first);
        Renderer::text(lg.x + 36, lg.y + 45 + i * 20.0f, leg[i].second, 12, P().textMuted);
    }
    Renderer::text(view.x + 232, view.bottom() - 14, "Drag to pan  \xC2\xB7  scroll to zoom  \xC2\xB7  click a state, then a district  \xC2\xB7  " +
                                                          num(india_.triangleCount()) + " triangles, " + num(india_.lastClippedSegments()) + " border segments after clipping",
                   11, P().textFaint);

    // ---- side panel
    card(side);
    float oy = 0;   // citizens and departments get the scope toggle above the search box
    if (user_->role() != Role::ADMIN) { scopeToggle({side.x + 16, side.y + 14, side.w - 32, 32}); oy = 42; }
    mapSearch_.draw({side.x + 16, side.y + 16 + oy, side.w - 32, 38}, "Search state / district / city", "search");
    std::string q = Validator::trim(mapSearch_.text());
    float y = side.y + 72 + oy;
    if (!q.empty()) {   // search results across states, districts and cities
        int shown = 0;
        Renderer::text(side.x + 18, y + 6, "MATCHES", 11, P().textFaint, Weight::SEMIBOLD);
        y += 18;
        for (std::size_t i = 0; i < india_.districts().size() && shown < 12; ++i) {
            const auto& d = india_.districts()[i];
            const auto& sn = india_.states()[d.state].name;
            if (d.name == "~") continue;
            if (!AsmEngine::containsIgnoreCase(d.name, q) && !AsmEngine::containsIgnoreCase(sn, q)) continue;
            Rect row{side.x + 10, y, side.w - 20, 34};
            bool h = u.hover(row);
            if (h) { Renderer::roundRect(row, 7, P().hover); u.wantHand(); }
            Renderer::textIn({row.x + 10, row.y, row.w - 20, row.h}, d.name + ", " + sn, 13, P().text);
            if (u.clicked(row)) { selState_ = d.state; selDistrict_ = (int)i; mapLoaded_ = false; mapSearch_.field.clear(); UI::get().keys.blur(); }
            y += 36;
            ++shown;
        }
        if (shown == 0) Renderer::text(side.x + 18, y + 16, "No state or district matches '" + q + "'", 12.5f, P().textMuted);
        return;
    }
    if (selState_ < 0) {
        const bool mine = viewScope_ == 1 && user_->role() != Role::ADMIN;
        Renderer::text(side.x + 18, y + 10, !mine ? "All India" : user_->role() == Role::CITIZEN ? "My complaints" : "My department", 18, P().text, Weight::SEMIBOLD);
        long long tot = 0, pen = 0, res = 0, high = 0;
        for (const auto& s : mapStates_) { tot += s.complaints; pen += s.pending; res += s.resolved; high += s.highSeverity; }
        y += 30;
        for (auto [k, v] : std::vector<std::pair<const char*, long long>>{{"Complaints", tot}, {"Pending", pen}, {"Resolved", res}, {"High severity", high}}) {
            Renderer::text(side.x + 18, y + 18, k, 13, P().textMuted);
            Renderer::text(side.right() - 18 - Renderer::measure(num(v), 14, Weight::SEMIBOLD), y + 18, num(v), 14, P().text, Weight::SEMIBOLD);
            y += 28;
        }
        y += 16;
        Renderer::text(side.x + 18, y + 6, "TOP STATES", 11, P().textFaint, Weight::SEMIBOLD);
        y += 16;
        std::vector<int> order;
        for (std::size_t i = 0; i < mapStates_.size(); ++i) order.push_back((int)i);
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return mapStates_[a].complaints > mapStates_[b].complaints; });
        float mxv = order.empty() ? 1.0f : std::max(1, mapStates_[order[0]].complaints);
        for (int k = 0; k < 12 && k < (int)order.size(); ++k) {
            int i = order[k];
            if (mapStates_[i].complaints == 0 || y + 34 > side.bottom() - 6) break;
            Rect row{side.x + 10, y, side.w - 20, 34};
            bool h = u.hover(row);
            if (h) { Renderer::roundRect(row, 7, P().hover); u.wantHand(); }
            Renderer::textIn({row.x + 10, row.y, 160, row.h}, india_.states()[i].name, 13, P().text);
            progressBar({row.x + 176, row.y + 14, row.w - 230, 6}, mapStates_[i].complaints / mxv, intensity(mapStates_[i].complaints, t1_, t2_));
            Renderer::textIn({row.right() - 48, row.y, 40, row.h}, num(mapStates_[i].complaints), 13, P().text, Weight::SEMIBOLD, Align::RIGHT);
            if (u.clicked(row)) { selState_ = i; selDistrict_ = -1; mapLoaded_ = false; }
            y += 36;
        }
        return;
    }
    const RegionStat& s = mapStates_[selState_];
    Renderer::text(side.x + 18, y + 4, "SELECTED STATE", 11, P().textFaint, Weight::SEMIBOLD);
    Renderer::text(side.x + 18, y + 30, india_.states()[selState_].name, 19, P().text, Weight::SEMIBOLD);
    y += 44;
    float w2 = (side.w - 46) / 2;
    std::pair<const char*, int> tiles[] = {{"Complaints", s.complaints}, {"Pending", s.pending}, {"Resolved", s.resolved}, {"High severity", s.highSeverity}};
    for (int i = 0; i < 4; ++i) {
        Rect t{side.x + 18 + (i % 2) * (w2 + 10), y + (i / 2) * 64.0f, w2, 56};
        Renderer::roundRect(t, 8, P().surfaceAlt);
        Renderer::text(t.x + 12, t.y + 22, tiles[i].first, 11.5f, P().textMuted);
        Renderer::text(t.x + 12, t.y + 45, num(tiles[i].second), 18, i == 3 ? P().danger : P().text, Weight::BOLD);
    }
    y += 136;
    Renderer::text(side.x + 18, y, "Districts: " + num(s.subRegions) + "  \xC2\xB7  sorted with " + districtSort_.algorithm, 12, P().textMuted);
    y += 12;
    Rect list{side.x + 8, y, side.w - 16, side.bottom() - y - 8};
    float ly = mapListScroll_.begin(list, mapDistricts_.size() * 34.0f);
    float dmx = 1;
    for (const auto& d : mapDistricts_) dmx = std::max<float>(dmx, d.complaints);
    for (std::size_t i = 0; i < mapDistricts_.size(); ++i) {
        const auto& d = mapDistricts_[i];
        Rect row{list.x + 4, ly + i * 34, list.w - 14, 32};
        if (row.bottom() < list.y || row.y > list.bottom()) continue;
        int di = india_.districtIndex(selState_, d.name);
        bool sel = di >= 0 && di == selDistrict_;
        bool h = u.hover(row) && list.contains(u.mouse.x, u.mouse.y);
        if (sel || h) Renderer::roundRect(row, 7, sel ? P().selected : P().hover);
        if (h) u.wantHand();
        Renderer::textIn({row.x + 10, row.y, 150, row.h}, d.name, 13, P().text, sel ? Weight::SEMIBOLD : Weight::REGULAR);
        progressBar({row.x + 166, row.y + 13, row.w - 220, 6}, d.complaints / dmx, P().primary);
        Renderer::textIn({row.right() - 46, row.y, 38, row.h}, num(d.complaints), 13, P().text, Weight::SEMIBOLD, Align::RIGHT);
        if (h && u.clicked(row)) selDistrict_ = di;
    }
    mapListScroll_.end();
}

// ============================================================ CITY RANKING
void App::loadRanking() {
    if (rankLoaded_) return;
    int top = rankTop_ == 0 ? 10 : rankTop_ == 1 ? 20 : 0;
    guarded([&] { ranking_ = svc_.cityRanking(rankOrder_ == 0, top, rankSearch_.text(), geoScope()); });
    rankLoaded_ = true;
}

void App::screenRanking() {
    loadRanking();
    Rect c = content();
    Rect r{c.x + 28, c.y + 24, c.w - 56, c.h - 48};
    if (user_->role() != Role::ADMIN) {   // citizens / departments: choose what the ranking counts
        scopeToggle({r.x, r.y, 300, 34});
        const bool mine = viewScope_ == 1;
        Renderer::text(r.x + 316, r.y + 22, mine ? (user_->role() == Role::CITIZEN ? "Counting only the complaints you reported" : "Counting only your department's complaints")
                                                 : "Counting every complaint in India (totals only, no personal details)",
                       12, P().textMuted);
        r.y += 46;
        r.h -= 46;
    }
    if (segmented({r.x, r.y, 300, 38}, {"Top 10", "Top 20", "All cities"}, rankTop_)) rankLoaded_ = false;
    if (segmented({r.x + 316, r.y, 220, 38}, {"Descending", "Ascending"}, rankOrder_)) rankLoaded_ = false;
    rankSearch_.draw({r.x + 552, r.y, 240, 38}, "Find a city (Binary Search)", "search");
    if (button({r.x + 804, r.y, 90, 38}, "Find", ButtonStyle::SECONDARY)) rankLoaded_ = false;
    float y = r.y + 54;
    Rect m{r.x, y, r.w, 86};
    card(m, false);
    float w = r.w / 4;
    auto blk = [&](int i, const std::string& k, const std::string& v) {
        Renderer::text(m.x + 20 + i * w, m.y + 32, k, 12, P().textMuted, Weight::SEMIBOLD);
        Renderer::text(m.x + 20 + i * w, m.y + 58, v, 14.5f, P().text, Weight::SEMIBOLD);
    };
    blk(0, "ALGORITHM", ranking_.sort.algorithm + "  (" + ranking_.sort.complexity + ")");
    blk(1, "SOURCE", "PostgreSQL \xE2\x86\x92 CityArray (" + num(ranking_.sort.n) + " cities)");
    blk(2, "COMPARISONS / SWAPS", num(ranking_.sort.comparisons) + " / " + num(ranking_.sort.swaps));
    if (!Validator::trim(rankSearch_.text()).empty())
        blk(3, "BINARY SEARCH '" + ranking_.binary.target + "'", ranking_.foundRank > 0 ? "Rank #" + num(ranking_.foundRank) + " in " + num(ranking_.binary.comparisons) + " comparisons"
                                                                                 : "Not found (" + num(ranking_.binary.comparisons) + " comparisons)");
    else blk(3, "TIME", f1(ranking_.sort.microseconds, "%.1f microseconds"));
    y += 102;
    Rect t{r.x, y, r.w, r.bottom() - y};
    card(t);
    std::vector<Column> cols{{"RANK", 70}, {"CITY", 1.0f}, {"DISTRICT", 1.0f}, {"STATE", 1.0f}, {"COMPLAINTS", 110, Align::RIGHT},
                             {"PENDING", 90, Align::RIGHT}, {"RESOLVED", 90, Align::RIGHT}, {"HIGH", 70, Align::RIGHT}, {"SHARE", 90, Align::RIGHT}};
    std::vector<std::vector<Cell>> rows;
    int sel = -1;
    for (std::size_t i = 0; i < ranking_.rows.size(); ++i) {
        const auto& cs = ranking_.rows[i];
        if (ranking_.foundRank == (long long)i + 1) sel = (int)i;
        Cell rank = textCell("#" + num(i + 1), Weight::BOLD);
        if (i < 3 && rankOrder_ == 0) rank.fg = i == 0 ? P().warning : P().textMuted;
        Cell high = textCell(num(cs.highSeverity));
        if (cs.highSeverity) high.fg = P().danger;
        rows.push_back({rank, textCell(cs.city, Weight::SEMIBOLD), mutedCell(cs.district), mutedCell(cs.state), textCell(num(cs.complaints), Weight::SEMIBOLD),
                        textCell(num(cs.pending)), textCell(num(cs.resolved)), high, mutedCell(percent(cs.complaints, ranking_.total))});
    }
    rankTable_.draw(t.inset(1), cols, rows, false, sel);
}

// ============================================================ REPORTS
void App::loadReport() {
    if (reportLoaded_) return;
    guarded([&] { report_ = reports_.generate(ReportService::types()[reportSel_].first, svc_.scopeFor(*user_)); });
    reportLoaded_ = true;
}

void App::screenReports() {
    loadReport();
    Rect c = content();
    Rect r{c.x + 28, c.y + 16, c.w - 56, c.h - 32};
    std::vector<std::string> names;
    for (const auto& t : ReportService::types()) names.push_back(t.second);
    if (tabs({r.x, r.y, r.w - 170, 44}, names, reportSel_)) { reportLoaded_ = false; reportTable_.scroll.offset = 0; }
    if (button({r.right() - 158, r.y + 2, 158, 38}, "Export CSV", ButtonStyle::PRIMARY, !report_.rows.empty(), "download")) {
        std::string path;
        if (guarded([&] { path = reports_.exportCsv(report_, Config::instance().get("EXPORT_DIR", "exports"), user_->id()); }))
            UI::get().toast("Exported " + num(report_.rows.size()) + " rows to " + path, ToastKind::SUCCESS);
    }
    float y = r.y + 60;
    Renderer::text(r.x, y + 10, report_.title, 18, P().text, Weight::SEMIBOLD);
    Renderer::text(r.x, y + 32, report_.subtitle + (report_.algorithmNote.empty() ? "" : "   \xC2\xB7   " + report_.algorithmNote), 12.5f, P().textMuted);
    y += 50;
    Rect ch{r.x, y, r.w, 260};
    card(ch);
    if (report_.chartValues.empty()) emptyState(ch, "No data", "This report has no rows yet");
    else if (report_.type == ReportType::MONTHLY) {
        charts::lineChart({ch.x + 16, ch.y + 16, ch.w - 32, ch.h - 24}, report_.chartLabels, report_.chartValues, {}, "Created", "");
    } else if (report_.type == ReportType::SUMMARY || report_.type == ReportType::RESOLUTION || report_.type == ReportType::SEVERITY) {
        std::vector<Color> cols(P().chart, P().chart + 8);
        if (report_.type == ReportType::SEVERITY) cols = {P().success, P().info, P().warning, P().danger};
        charts::donut({ch.x + 30, ch.y + 16, ch.w - 60, ch.h - 32}, report_.chartLabels, report_.chartValues, cols, "total");
    } else {
        charts::barChart({ch.x + 16, ch.y + 20, ch.w - 32, ch.h - 30}, report_.chartLabels, report_.chartValues, P().chart[0], report_.chartLabels.size() > 6);
    }
    y += 276;
    Rect t{r.x, y, r.w, r.bottom() - y};
    card(t);
    std::vector<Column> cols;
    for (std::size_t i = 0; i < report_.columns.size(); ++i)
        cols.push_back({report_.columns[i], i == 0 ? 1.0f : 0.6f, i == 0 ? Align::LEFT : Align::RIGHT});
    std::vector<std::vector<Cell>> rows;
    for (const auto& row : report_.rows) {
        std::vector<Cell> cells;
        for (std::size_t i = 0; i < row.size(); ++i) cells.push_back(i == 0 ? textCell(row[i], Weight::SEMIBOLD) : textCell(row[i]));
        rows.push_back(cells);
    }
    reportTable_.draw(t.inset(1), cols, rows);
}

// ============================================================ USERS
void App::screenUsers() {
    if (!usersLoaded_) { guarded([&] { users_ = db_.getUsers(); }); usersLoaded_ = true; }
    Rect c = content();
    Rect r{c.x + 28, c.y + 24, c.w - 56, c.h - 48};
    int counts[3] = {0, 0, 0};
    for (const auto& u : users_) counts[u.role == "CITIZEN" ? 0 : u.role == "DEPARTMENT" ? 1 : 2]++;
    Renderer::text(r.x, r.y + 14, num(users_.size()) + " accounts  \xC2\xB7  " + num(counts[0]) + " citizens, " + num(counts[1]) + " department officers, " +
                                      num(counts[2]) + " administrators", 13.5f, P().textMuted);
    if (button({r.right() - 150, r.y - 4, 150, 38}, "Create user", ButtonStyle::PRIMARY, true, "plus")) {
        userModal_ = true;
        uRole_.items = {"Department officer", "Administrator"};
        uRole_.selected = 0;
        uDept_.items.clear();
        for (const auto& d : svc_.departments()) uDept_.items.push_back(d.name());
    }
    Rect t{r.x, r.y + 34, r.w, r.h - 34};
    card(t);
    std::vector<Column> cols{{"USERNAME", 0.75f}, {"FULL NAME", 0.9f}, {"ROLE", 130}, {"DEPARTMENT", 1.0f}, {"PHONE", 120}, {"STATUS", 100}, {"LAST LOGIN", 150}};
    std::vector<std::vector<Cell>> rows;
    for (const auto& u : users_) {
        Color rc = u.role == "ADMIN" ? P().danger : u.role == "DEPARTMENT" ? P().info : P().success;
        rows.push_back({textCell(u.username, Weight::SEMIBOLD), textCell(u.fullName), badgeCell(u.role, rc, rc.withAlpha(0.12f)),
                        mutedCell(u.department.empty() ? "-" : u.department), mutedCell(u.phone),
                        badgeCell(u.active ? "Active" : "Disabled", u.active ? P().success : P().textMuted, u.active ? P().successSoft : P().neutralSoft),
                        mutedCell(u.lastLogin)});
    }
    usersTable_.draw(t.inset(1), cols, rows);

    Rect m{(width_ - 560) / 2.0f, (height_ - 560) / 2.0f, 560, 560};
    if (modalBegin(m, "Create staff account", userModal_)) {
        float x = m.x + 24, y = m.y + 84, cw = (m.w - 68) / 2;
        auto label = [&](float lx, float ly, const char* t) { Renderer::text(lx, ly, t, 12.5f, P().text, Weight::SEMIBOLD); };
        label(x, y, "Full name"); uName_.draw({x, y + 8, cw, 40}, "Officer name");
        label(x + cw + 20, y, "Username"); uUser_.draw({x + cw + 20, y + 8, cw, 40}, "e.g. roads.officer2");
        y += 84;
        label(x, y, "Phone"); uPhone_.draw({x, y + 8, cw, 40}, "10-digit mobile");
        label(x + cw + 20, y, "Role"); uRole_.draw({x + cw + 20, y + 8, cw, 40}, "Role");
        y += 84;
        label(x, y, "Department"); uDept_.draw({x, y + 8, m.w - 48, 40}, uRole_.selected == 0 ? "Select department" : "Not needed for administrators");
        y += 84;
        label(x, y, "Password"); uPass_.draw({x, y + 8, cw, 40}, "Min 8, Aa1");
        label(x + cw + 20, y, "Confirm"); uConfirm_.draw({x + cw + 20, y + 8, cw, 40}, "Repeat password");
        if (button({m.right() - 164, m.bottom() - 64, 140, 40}, "Create", ButtonStyle::PRIMARY)) {
            for (TextInput* t2 : {&uName_, &uUser_, &uPhone_, &uPass_, &uConfirm_}) t2->error.clear();
            uDept_.error.clear();
            NewUser nu;
            nu.fullName = uName_.text(); nu.username = uUser_.text(); nu.phone = uPhone_.text(); nu.password = uPass_.field.text();
            nu.role = uRole_.selected == 1 ? Role::ADMIN : Role::DEPARTMENT;
            nu.departmentId = uDept_.selected >= 0 ? svc_.departments()[uDept_.selected].id() : 0;
            try {
                svc_.createStaffUser(*user_, nu, uConfirm_.field.text());
                UI::get().toast("User " + nu.username + " created (password stored as bcrypt hash)", ToastKind::SUCCESS);
                userModal_ = false;
                usersLoaded_ = auditLoaded_ = false;
                for (TextInput* t2 : {&uName_, &uUser_, &uPhone_, &uPass_, &uConfirm_}) t2->field.clear();
                refreshNotifications();
            } catch (const ValidationException& e) {
                std::string f = e.field();
                if (f == "department") uDept_.error = e.what();
                else (f == "fullName" ? uName_ : f == "username" ? uUser_ : f == "phone" ? uPhone_ : f == "confirm" ? uConfirm_ : uPass_).error = e.what();
            } catch (const AppException& e) {
                UI::get().toast(e.what(), ToastKind::ERR);
            }
        }
        if (button({m.right() - 268, m.bottom() - 64, 92, 40}, "Cancel", ButtonStyle::SECONDARY)) userModal_ = false;
    }
}

// ============================================================ AUDIT
void App::screenAudit() {
    if (auditFilter_.items.empty()) {
        auditFilter_.items = {"All actions", "LOGIN", "LOGOUT", "LOGIN_FAILED", "REGISTER", "COMPLAINT_CREATED", "COMPLAINT_ASSIGNED", "STATUS_CHANGED",
                              "COMPLAINT_RESOLVED", "COMPLAINT_CLOSED", "USER_CREATED", "ADMIN_ACTION", "REPORT_EXPORTED"};
        auditFilter_.selected = 0;
    }
    if (!auditLoaded_) {
        guarded([&] { audit_ = db_.getAuditLogs(300, auditFilter_.selected > 0 ? auditFilter_.value() : ""); });
        auditLoaded_ = true;
    }
    Rect c = content();
    Rect r{c.x + 28, c.y + 24, c.w - 56, c.h - 48};
    Renderer::text(r.x, r.y + 14, "Every login, complaint action, assignment and administrative change (latest 300).", 13.5f, P().textMuted);
    if (auditFilter_.draw({r.right() - 330, r.y - 4, 210, 38}, "Action")) auditLoaded_ = false;
    if (button({r.right() - 108, r.y - 4, 108, 38}, "Refresh", ButtonStyle::SECONDARY, true, "refresh")) auditLoaded_ = false;
    Rect t{r.x, r.y + 34, r.w, r.h - 34};
    card(t);
    std::vector<Column> cols{{"TIME", 170}, {"USER", 150}, {"ACTION", 190}, {"ENTITY", 110}, {"REFERENCE", 170}, {"DETAILS", 1.0f}};
    std::vector<std::vector<Cell>> rows;
    for (const auto& a : audit_) {
        Color ac = a.action.find("FAILED") != std::string::npos ? P().danger : a.action.rfind("COMPLAINT", 0) == 0 ? P().primary
                 : a.action.rfind("LOG", 0) == 0 ? P().textMuted : P().info;
        rows.push_back({mutedCell(a.when), textCell(a.user, Weight::SEMIBOLD), badgeCell(a.action, ac, ac.withAlpha(0.12f)), mutedCell(a.entityType),
                        textCell(a.entityId), mutedCell(a.details)});
    }
    auditTable_.draw(t.inset(1), cols, rows);
}

// ============================================================ DSA ENGINE
void App::screenDsa() {
    if (!dsaLoaded_) {
        guarded([&] { svc_.loadRuntime(*user_); });
        dsaLoaded_ = true;
    }
    Rect c = content();
    float y0 = pageScroll_.begin(c, 1180);
    Rect r{c.x + 28, y0 + 24, c.w - 56, 1130};
    float y = r.y;
    // ---- doubly linked list
    DoublyLinkedList& dl = svc_.activeList();
    Rect a{r.x, y, r.w, 190};
    card(a);
    sectionTitle({a.x + 20, a.y + 14, a.w, 30}, "Doubly linked list - active complaints",
                 "size " + num(dl.size()) + "  \xC2\xB7  owns the Complaint objects  \xC2\xB7  O(1) insert at tail, O(1) unlink when CLOSED");
    auto nodes = dl.toVector(false);
    float nx = a.x + 24, nw = (a.w - 48 - 5 * 46) / 6;
    for (std::size_t i = 0; i < nodes.size() && i < 6; ++i) {
        Rect b{nx + i * (nw + 46), a.y + 82, nw, 70};
        Renderer::roundRect(b, 9, P().surfaceAlt);
        Renderer::roundRectOutline(b, 9, P().border);
        Renderer::rect({b.x, b.y, 18, b.h}, P().primarySoft);
        Renderer::rect({b.right() - 18, b.y, 18, b.h}, P().primarySoft);
        Renderer::textIn({b.x + 22, b.y + 10, b.w - 44, 20}, nodes[i]->referenceCode(), 12.5f, P().text, Weight::SEMIBOLD, Align::CENTER);
        Renderer::textIn({b.x + 22, b.y + 32, b.w - 44, 18}, std::string(nodes[i]->typeName()) + " \xC2\xB7 " + displayName(nodes[i]->status()), 11, P().textMuted, Weight::REGULAR, Align::CENTER);
        Renderer::textIn({b.x, b.y + 50, 18, 16}, "p", 10, P().primary, Weight::BOLD, Align::CENTER);
        Renderer::textIn({b.right() - 18, b.y + 50, 18, 16}, "n", 10, P().primary, Weight::BOLD, Align::CENTER);
        if (i + 1 < nodes.size() && i < 5) {
            float ax = b.right() + 6, ay = b.y + 28;
            Renderer::line(ax, ay, ax + 34, ay, P().primary, 1.5f);
            Renderer::triangle(ax + 34, ay, ax + 28, ay - 4, ax + 28, ay + 4, P().primary);
            Renderer::line(ax, ay + 14, ax + 34, ay + 14, P().textFaint, 1.5f);
            Renderer::triangle(ax, ay + 14, ax + 6, ay + 10, ax + 6, ay + 18, P().textFaint);
        }
    }
    Renderer::text(a.x + 24, a.y + 174, "HEAD = " + (dl.head() ? dl.head()->data->referenceCode() : std::string("null")) + "      TAIL = " +
                                             (dl.tail() ? dl.tail()->data->referenceCode() : std::string("null")) + (nodes.size() > 6 ? "      (" + num(nodes.size() - 6) + " more nodes)" : ""),
                   12, P().textMuted);
    y += 206;

    // ---- hash table
    HashTable& ht = svc_.index();
    auto hs = ht.stats();
    Rect h{r.x, y, r.w * 0.55f - 8, 300};
    card(h);
    sectionTitle({h.x + 20, h.y + 14, h.w, 30}, "Hash table - complaint index", "separate chaining  \xC2\xB7  bucket = asm_hash_djb2(key) mod m  \xC2\xB7  keys: reference code + id");
    std::pair<const char*, std::string> hstat[] = {{"Records", num(hs.records)}, {"Buckets (m)", num(hs.buckets)}, {"Load factor", f1(hs.loadFactor, "%.3f")},
                                                   {"Collisions", num(hs.collisions)}, {"Longest chain", num(hs.longestChain)}, {"Rehashes", num(hs.rehashes)},
                                                   {"Successful searches", num(hs.successfulSearches)}, {"Failed searches", num(hs.failedSearches)}};
    for (int i = 0; i < 8; ++i) {
        float sx = h.x + 20 + (i % 4) * (h.w - 40) / 4, sy = h.y + 80 + (i / 4) * 50.0f;
        Renderer::text(sx, sy, hstat[i].first, 11.5f, P().textMuted);
        Renderer::text(sx, sy + 22, hstat[i].second, 16, P().text, Weight::BOLD);
    }
    auto chains = ht.chainLengths();
    Rect hist{h.x + 20, h.y + 186, h.w - 40, 90};
    Renderer::text(hist.x, hist.y - 6, "Chain length of the first " + num(std::min<std::size_t>(chains.size(), 96)) + " buckets", 11.5f, P().textMuted);
    std::size_t nb = std::min<std::size_t>(chains.size(), 96);
    float bw = hist.w / std::max<std::size_t>(nb, 1);
    std::size_t cmax = 1;
    for (std::size_t i = 0; i < nb; ++i) cmax = std::max(cmax, chains[i]);
    for (std::size_t i = 0; i < nb; ++i) {
        float bh = (hist.h - 10) * chains[i] / cmax;
        Renderer::rect({hist.x + i * bw + 1, hist.bottom() - bh, std::max(1.0f, bw - 2), bh}, chains[i] > 1 ? P().warning : P().primary);
    }
    // ---- interactive search
    Rect s{r.x + r.w * 0.55f + 8, y, r.w * 0.45f - 8, 300};
    card(s);
    sectionTitle({s.x + 20, s.y + 14, s.w, 30}, "Search lab", "Hash lookup  \xC2\xB7  Linear search (DLL)  \xC2\xB7  Binary search (cities)");
    dsaKey_.draw({s.x + 20, s.y + 66, s.w - 150, 38}, "SCMS-2026-001042, an id, or a city", "search");
    if (button({s.right() - 116, s.y + 66, 96, 38}, "Run", ButtonStyle::PRIMARY)) {
        std::string key = Validator::trim(dsaKey_.text());
        guarded([&] {
            Complaint* hit = ht.search(key);
            unsigned long long probes = ht.stats().lastProbes;
            dsaResult_ = hit ? "Hash hit: " + hit->referenceCode() + " (" + hit->location().city + ") after " + num(probes) + " probe(s)"
                             : "Hash miss after " + num(probes) + " probe(s) in bucket " + num(ht.bucketOf(key));
            std::vector<Complaint*> v = dl.toVector();
            std::vector<std::size_t> found;
            dsaLinear_ = linearSearch(v, key, [&](Complaint* c2) { return c2->matches(key); }, found);
            CityRanking cr = svc_.cityRanking(true, 0, key, {});
            dsaBinary_ = cr.binary;
            dsaSearched_ = true;
        });
    }
    if (dsaSearched_) {
        float ry = s.y + 136;
        Renderer::text(s.x + 20, ry, "HASH TABLE", 11, P().textFaint, Weight::SEMIBOLD);
        Renderer::textIn({s.x + 20, ry + 6, s.w - 40, 22}, dsaResult_, 13, P().text);
        ry += 50;
        Renderer::text(s.x + 20, ry, "LINEAR SEARCH", 11, P().textFaint, Weight::SEMIBOLD);
        Renderer::text(s.x + 20, ry + 22, "target '" + dsaLinear_.target + "'  \xC2\xB7  " + num(dsaLinear_.matches) + " matches  \xC2\xB7  " + num(dsaLinear_.comparisons) +
                                              " comparisons (n = " + num(dsaLinear_.n) + ")", 13, P().text);
        ry += 50;
        Renderer::text(s.x + 20, ry, "BINARY SEARCH (cities sorted by name)", 11, P().textFaint, Weight::SEMIBOLD);
        Renderer::text(s.x + 20, ry + 22, dsaBinary_.target.empty() ? std::string("-") : (dsaBinary_.resultIndex >= 0 ? "found at index " + num(dsaBinary_.resultIndex) : std::string("not found")) +
                                              "  \xC2\xB7  " + num(dsaBinary_.comparisons) + " comparisons (n = " + num(dsaBinary_.n) + ")", 13, P().text);
    } else Renderer::text(s.x + 20, s.y + 150, "Type a key and press Run to compare the three techniques.", 12.5f, P().textMuted);
    y += 316;

    // ---- sorting benchmark on real city data
    Rect b{r.x, y, r.w, 250};
    card(b);
    sectionTitle({b.x + 20, b.y + 14, b.w, 30}, "Sorting benchmark", "Same PostgreSQL city dataset, ranked by complaint count with each algorithm (std::sort is never used)");
    if (button({b.right() - 150, b.y + 18, 130, 36}, "Run benchmark", ButtonStyle::PRIMARY)) {
        guarded([&] {
            CityArray arr;
            db_.loadCityStats(arr, {}, false);
            auto less = [](const CityStat& x, const CityStat& z) { return x.complaints > z.complaints; };
            std::vector<CityStat> base(arr.data(), arr.data() + arr.size());
            sortBench_.clear();
            { auto v = base; sortBench_.push_back(quickSort(v.data(), v.size(), less)); }
            { auto v = base; sortBench_.push_back(mergeSort(v, less)); }
            { auto v = base; sortBench_.push_back(insertionSort(v.data(), v.size(), less)); }
            { auto v = base; sortBench_.push_back(bubbleSort(v.data(), v.size(), less)); }
        });
    }
    std::vector<Column> cols{{"ALGORITHM", 0.85f}, {"COMPLEXITY", 1.0f}, {"N", 90, Align::RIGHT}, {"COMPARISONS", 140, Align::RIGHT}, {"SWAPS / MOVES", 140, Align::RIGHT}, {"TIME (us)", 120, Align::RIGHT}};
    std::vector<std::vector<Cell>> rows;
    for (const auto& m : sortBench_)
        rows.push_back({textCell(m.algorithm, Weight::SEMIBOLD), mutedCell(m.complexity), textCell(num(m.n)), textCell(num(m.comparisons)), textCell(num(m.swaps)), textCell(f1(m.microseconds))});
    static Table bench;
    if (rows.empty()) Renderer::text(b.x + 24, b.y + 110, "Press 'Run benchmark' - Quick, Merge, Insertion and Bubble sort run on every city in PostgreSQL.", 12.5f, P().textMuted);
    else bench.draw({b.x + 1, b.y + 64, b.w - 2, b.h - 65}, cols, rows);
    y += 266;

    // ---- queue + stack summary
    Rect q{r.x, y, r.w * 0.5f - 8, 150}, st{r.x + r.w * 0.5f + 8, y, r.w * 0.5f - 8, 150};
    card(q);
    sectionTitle({q.x + 20, q.y + 14, q.w, 30}, "FIFO queue", "linked list, enqueue at rear / dequeue at front");
    Renderer::text(q.x + 20, q.y + 92, "size " + num(svc_.queue().size()) + "   front " + (svc_.queue().isEmpty() ? std::string("-") : svc_.queue().front().reference) +
                                          "   processed " + num(svc_.processedCount()), 13.5f, P().text);
    if (linkText(q.x + 20, q.y + 124, "Open processing queue", 13)) go(Screen::QUEUE);
    card(st);
    sectionTitle({st.x + 20, st.y + 14, st.w, 30}, "Stacks", "array-based, fixed capacity, overflow/underflow exceptions");
    Renderer::text(st.x + 20, st.y + 92, "admin actions " + num(svc_.adminActions().size()) + "/" + num(svc_.adminActions().capacity()) + "   status history " +
                                           num(svc_.statusStack().size()) + "/" + num(svc_.statusStack().capacity()), 13.5f, P().text);
    Renderer::text(st.x + 20, st.y + 122, "Open any complaint to see its STATUS HISTORY STACK.", 12.5f, P().textMuted);
    pageScroll_.end();
}

// ============================================================ ASSEMBLY ENGINE
void App::screenAssembly() {
    Rect c = content();
    float y0 = pageScroll_.begin(c, 980);
    Rect r{c.x + 28, y0 + 24, c.w - 56, 940};
    float y = r.y;
    auto stats = AsmEngine::stats();
    Rect hero{r.x, y, r.w, 120};
    card(hero);
    Renderer::circle(hero.x + 56, hero.y + 60, 30, (asmOnline_ ? P().success : P().danger).withAlpha(0.12f));
    icon("cpu", hero.x + 56, hero.y + 60, 26, asmOnline_ ? P().success : P().danger);
    Renderer::text(hero.x + 104, hero.y + 42, "ASSEMBLY ENGINE", 12, P().textMuted, Weight::SEMIBOLD);
    Renderer::text(hero.x + 104, hero.y + 70, std::string("Status: ") + (asmOnline_ ? "ONLINE" : "FAULT"), 20, asmOnline_ ? P().success : P().danger, Weight::BOLD);
    Renderer::text(hero.x + 104, hero.y + 96, "Routine: Severity Calculator (calculateSeverityAsm)  \xC2\xB7  NASM x86-64, Win64 ABI  \xC2\xB7  self-test passed at start-up", 12.5f, P().textMuted);
    Renderer::text(hero.right() - 220, hero.y + 52, "Total calls", 12, P().textMuted);
    Renderer::text(hero.right() - 220, hero.y + 84, num(AsmEngine::totalCalls()), 28, P().text, Weight::BOLD);
    y += 136;
    float w6 = (r.w - 80) / 6;
    std::pair<const char*, unsigned long long> calls[] = {{"calculateSeverityAsm", stats.severityCalls}, {"classifySeverityAsm", stats.classifyCalls},
                                                          {"asm_hash_djb2", stats.hashCalls}, {"asm_contains_ci", stats.containsCalls},
                                                          {"multiply (shift/succ.)", stats.multiplyCalls}, {"asm_strlen / to_lower", stats.stringCalls}};
    for (int i = 0; i < 6; ++i) {
        Rect b{r.x + i * (w6 + 16), y, w6, 84};
        card(b, false);
        Renderer::textIn({b.x + 14, b.y + 12, b.w - 28, 20}, calls[i].first, 11.5f, P().textMuted, Weight::SEMIBOLD);
        Renderer::text(b.x + 14, b.y + 62, num(calls[i].second), 22, P().text, Weight::BOLD);
    }
    y += 100;

    // ---- interactive severity calculator with register trace
    if (asmCat_.items.empty()) { for (const auto& cat : svc_.categories()) asmCat_.items.push_back(cat.name); asmCat_.selected = 2; }
    Rect calc{r.x, y, r.w * 0.5f - 8, 470};
    card(calc);
    sectionTitle({calc.x + 20, calc.y + 14, calc.w, 30}, "Severity calculator", "Each change makes ONE real call into NASM");
    float cx = calc.x + 20, cy = calc.y + 70;
    Renderer::text(cx, cy, "Category (RCX)", 12, P().text, Weight::SEMIBOLD);
    asmCat_.draw({cx, cy + 8, calc.w - 40, 38}, "Category");
    Renderer::text(cx, cy + 70, "Impact (RDX)", 12, P().text, Weight::SEMIBOLD);
    segmented({cx, cy + 78, (calc.w - 52) / 2, 36}, {"1", "2", "3", "4", "5"}, asmImpact_);
    Renderer::text(cx + (calc.w - 52) / 2 + 12, cy + 70, "Urgency (R8)", 12, P().text, Weight::SEMIBOLD);
    segmented({cx + (calc.w - 52) / 2 + 12, cy + 78, (calc.w - 52) / 2, 36}, {"1", "2", "3", "4", "5"}, asmUrgency_);
    int cat = asmCat_.selected >= 0 ? svc_.categories()[asmCat_.selected].asmCode : 0;
    int imp = asmImpact_ + 1, urg = asmUrgency_ + 1;
    static int lastKey = -1, score = 0, level = 1;
    int key = cat * 100 + imp * 10 + urg;
    if (key != lastKey) { score = AsmEngine::severityScore(cat, imp, urg); level = AsmEngine::classify(score); lastKey = key; }
    const int weights[8] = {14, 10, 22, 18, 12, 16, 10, 10};
    float ty = cy + 140;
    Renderer::roundRect({cx, ty, calc.w - 40, 190}, 8, Theme::dark() ? hex(0x0B0F17) : hex(0x101828));
    auto reg = [&](float yy, const std::string& n, const std::string& v, const std::string& note) {
        Renderer::text(cx + 16, yy, n, 12.5f, hex(0x93C5FD), Weight::SEMIBOLD);
        Renderer::text(cx + 70, yy, v, 12.5f, hex(0xF9FAFB));
        Renderer::text(cx + 170, yy, note, 12, hex(0x9CA3AF));
    };
    reg(ty + 26, "RCX", num(cat), "category code -> index into weight_table");
    reg(ty + 50, "R10", num(weights[cat]), "base = [weight_table + RCX*8]");
    reg(ty + 74, "RDX", num(imp), "impact (clamped 1..5 with CMP/JL/JG)");
    reg(ty + 98, "R8", num(urg), "urgency (clamped 1..5)");
    reg(ty + 122, "R11", num(imp * urg), "impact x urgency via add-and-shift loop");
    reg(ty + 146, "RAX", num(score), "(R11 << 1) + R11 + R10, saturated at 100");
    reg(ty + 170, "level", num(level), "classifySeverityAsm: CMP 80/55/30 + JGE");
    auto lvl = static_cast<SeverityLevel>(level);
    Renderer::text(cx, ty + 222, "Result", 12, P().textMuted);
    Renderer::text(cx + 60, ty + 224, num(score) + " / 100", 20, P().text, Weight::BOLD);
    badge(cx + 170, ty + 217, toString(lvl), severityColor(lvl, false), severityColor(lvl, true));

    // ---- add-and-shift multiplier vs successive addition
    Rect mul{r.x + r.w * 0.5f + 8, y, r.w * 0.5f - 8, 470};
    card(mul);
    sectionTitle({mul.x + 20, mul.y + 14, mul.w, 30}, "64-bit multiplication (COA CO3)", "asm_multiply_shift_add vs asm_multiply_successive");
    float mx = mul.x + 20, my = mul.y + 70;
    if (mulA_.text().empty()) { mulA_.field.setText("1234"); mulB_.field.setText("567"); }
    Renderer::text(mx, my, "Multiplicand", 12, P().text, Weight::SEMIBOLD);
    mulA_.draw({mx, my + 8, (mul.w - 52) / 2, 38}, "a");
    Renderer::text(mx + (mul.w - 52) / 2 + 12, my, "Multiplier", 12, P().text, Weight::SEMIBOLD);
    mulB_.draw({mx + (mul.w - 52) / 2 + 12, my + 8, (mul.w - 52) / 2, 38}, "b");
    unsigned long long A = mulA_.text().empty() ? 0 : std::stoull(mulA_.text()), B = mulB_.text().empty() ? 0 : std::stoull(mulB_.text());
    static unsigned long long lastA = ~0ull, lastB = ~0ull, p1 = 0, p2 = 0;
    if (A != lastA || B != lastB) { p1 = AsmEngine::multiplyShiftAdd(A, B); p2 = AsmEngine::multiplySuccessive(A, B % 2000000); lastA = A; lastB = B; }
    float ry = my + 76;
    Renderer::text(mx, ry, "Add-and-shift result: " + num(p1), 15, P().text, Weight::SEMIBOLD);
    Renderer::text(mx, ry + 24, std::string("Successive addition: ") + (B < 2000000 ? num(p2) : std::string("(skipped, multiplier too large)")) +
                                    (B < 2000000 && p1 == p2 ? "   \xE2\x9C\x93 equal" : ""), 13, P().textMuted);
    Renderer::text(mx, ry + 48, "Iterations: add-and-shift = " + num([&] { unsigned long long b = B; int n = 0; while (b) { b >>= 1; ++n; } return n; }()) +
                                    "   successive = " + num(B), 13, P().textMuted);
    ry += 76;
    Renderer::text(mx, ry, "ADD-AND-SHIFT TRACE (first 10 steps)", 11, P().textFaint, Weight::SEMIBOLD);
    ry += 8;
    unsigned long long a = A, b2 = B, prod = 0;
    for (int i = 0; i < 10 && b2; ++i) {
        bool add = b2 & 1;
        if (add) prod += a;
        char line[160];
        std::snprintf(line, sizeof line, "step %d  LSB=%d  %s  product=%llu  multiplicand<<1=%llu  multiplier>>1=%llu", i + 1, add ? 1 : 0,
                      add ? "ADD " : "skip", prod, a << 1, b2 >> 1);
        Renderer::text(mx, ry + 20 + i * 20, line, 11.5f, add ? P().text : P().textMuted);
        a <<= 1;
        b2 >>= 1;
    }
    y += 486;
    Rect doc{r.x, y, r.w, 150};
    card(doc);
    sectionTitle({doc.x + 20, doc.y + 14, doc.w, 30}, "Integration", "");
    const char* lines[] = {"Build: nasm -f win64 assembly/severity.asm -o build/obj/asm_severity.o  (Makefile rule $(OBJ)/asm_%.o)",
                           "Link:  the .o files are passed to g++ together with the C++ objects - no DLL, the routines live in the executable",
                           "C++:   extern \"C\" declarations in backend/AsmEngine.h; called from Complaint::computeSeverity() during every complaint submission",
                           "ABI:   Win64 - args RCX, RDX, R8, R9; return RAX; only volatile registers (RAX RCX RDX R8-R11) are used, so no PUSH/POP needed"};
    for (int i = 0; i < 4; ++i) Renderer::text(doc.x + 20, doc.y + 64 + i * 22.0f, lines[i], 12.5f, P().textMuted);
    pageScroll_.end();
}

// ============================================================ 3-D ANALYTICS
void App::screen3D() {
    if (!gridLoaded_) { guarded([&] { db_.loadStateCategoryGrid(grid_, gridStates_, gridCats_, 8); }); gridLoaded_ = true; }
    Rect c = content();
    Rect r{c.x + 28, c.y + 24, c.w - 56, c.h - 48};
    Renderer::text(r.x, r.y + 14, "Top 8 states x 6 categories - bar height = complaints (PostgreSQL \xE2\x86\x92 StatGrid 2-D array). Drag to rotate, scroll to zoom.", 13.5f, P().textMuted);
    if (button({r.right() - 150, r.y - 4, 150, 38}, chart3d_.autoRotate ? "Stop rotation" : "Auto-rotate", ButtonStyle::SECONDARY)) chart3d_.autoRotate = !chart3d_.autoRotate;
    Rect ch{r.x, r.y + 34, r.w - 276, r.h - 34};
    card(ch);
    std::vector<std::vector<int>> vals;
    for (std::size_t s = 0; s < gridStates_.size(); ++s) {
        std::vector<int> row;
        for (std::size_t k = 0; k < gridCats_.size(); ++k) row.push_back(grid_.get((int)s, (int)k));
        vals.push_back(row);
    }
    if (vals.empty()) emptyState(ch, "No data", "Complaints are needed to build the 3-D view");
    else chart3d_.draw(ch.inset(2), gridStates_, gridCats_, vals);
    Rect side{ch.right() + 16, r.y + 34, 260, r.h - 34};
    card(side);
    sectionTitle({side.x + 20, side.y + 14, side.w, 30}, "Legend");
    for (std::size_t k = 0; k < gridCats_.size(); ++k) {
        Renderer::roundRect({side.x + 20, side.y + 62 + k * 30.0f, 14, 14}, 3, P().chart[k % 8]);
        Renderer::text(side.x + 44, side.y + 74 + k * 30.0f, gridCats_[k], 13, P().text);
        Renderer::text(side.right() - 20 - Renderer::measure(num(grid_.columnTotal((int)k)), 13, Weight::SEMIBOLD), side.y + 74 + k * 30.0f,
                       num(grid_.columnTotal((int)k)), 13, P().textMuted, Weight::SEMIBOLD);
    }
    float y = side.y + 80 + gridCats_.size() * 30.0f;
    Renderer::text(side.x + 20, y, "CAMERA", 11, P().textFaint, Weight::SEMIBOLD);
    Renderer::text(side.x + 20, y + 24, "yaw " + f1(std::fmod(chart3d_.yaw, 360.0f), "%.0f") + " deg   pitch " + f1(chart3d_.pitch, "%.0f") + " deg", 12.5f, P().text);
    Renderer::text(side.x + 20, y + 46, "zoom " + f1(1.0 / chart3d_.distance, "%.2fx"), 12.5f, P().text);
    Renderer::text(side.x + 20, y + 80, "gluPerspective + gluLookAt,", 12, P().textMuted);
    Renderer::text(side.x + 20, y + 98, "glRotate / glTranslate / glScale,", 12, P().textMuted);
    Renderer::text(side.x + 20, y + 116, "depth test, GL_LIGHT0 lighting,", 12, P().textMuted);
    Renderer::text(side.x + 20, y + 134, "own glViewport inside the card.", 12, P().textMuted);
}

// ============================================================ SETTINGS & SYSTEM HEALTH
void App::screenSettings() {
    Rect c = content();
    float y0 = pageScroll_.begin(c, 900);
    Rect r{c.x + 28, y0 + 24, std::min(c.w - 56, 980.0f), 860};
    float y = r.y;
    Rect ap{r.x, y, r.w, 110};
    card(ap);
    sectionTitle({ap.x + 20, ap.y + 14, ap.w, 30}, "Appearance", "Applies to every screen, chart and the map");
    int mode = Theme::dark() ? 1 : 0;
    if (segmented({ap.x + 20, ap.y + 58, 260, 38}, {"Light mode", "Dark mode"}, mode)) Theme::setDark(mode == 1);
    y += 126;
    Rect db{r.x, y, r.w * 0.5f - 8, 300};
    card(db);
    sectionTitle({db.x + 20, db.y + 14, db.w, 30}, "Database");
    float yy = db.y + 66;
    auto row = [&](const Rect& box, const std::string& k, const std::string& v, const Color* col = nullptr) {
        Renderer::text(box.x + 20, yy, k, 12.5f, P().textMuted);
        Renderer::textIn({box.x + box.w * 0.42f, yy - 14, box.w * 0.58f - 20, 20}, v, 13, col ? *col : P().text, Weight::SEMIBOLD);
        yy += 30;
    };
    static double ping = -1;
    Color ok = dbOnline_ ? P().success : P().danger;
    row(db, "Status", dbOnline_ ? "Connected" : "Disconnected", &ok);
    row(db, "Server", "PostgreSQL " + db_.serverVersion());
    row(db, "Connection", db_.connectionInfo());
    row(db, "Queries this session", num(db_.queryCount()));
    row(db, "Round trip", ping < 0 ? "press Test" : f1(ping, "%.2f ms"));
    row(db, "Driver", "libpq (PQexecParams, parameterised)");
    if (button({db.x + 20, db.bottom() - 54, 110, 36}, "Test", ButtonStyle::SECONDARY)) { ping = db_.pingMs(); dbOnline_ = ping >= 0; }
    if (button({db.x + 140, db.bottom() - 54, 130, 36}, "Reconnect", ButtonStyle::SECONDARY)) {
        dbOnline_ = db_.connect();
        UI::get().toast(dbOnline_ ? "Reconnected to PostgreSQL" : "Reconnect failed: " + db_.lastError(), dbOnline_ ? ToastKind::SUCCESS : ToastKind::ERR);
        if (dbOnline_) markDirty();
    }
    Rect sys{r.x + r.w * 0.5f + 8, y, r.w * 0.5f - 8, 300};
    card(sys);
    sectionTitle({sys.x + 20, sys.y + 14, sys.w, 30}, "System health");
    yy = sys.y + 66;
    Color aok = asmOnline_ ? P().success : P().danger;
    row(sys, "Assembly engine", std::string(asmOnline_ ? "ONLINE" : "FAULT") + " (" + num(AsmEngine::totalCalls()) + " calls)", &aok);
    row(sys, "Complaint objects", num(Complaint::liveInstances()) + " live in memory");
    row(sys, "Active list (DLL)", num(svc_.activeList().size()) + " nodes");
    row(sys, "Hash table", num(svc_.index().size()) + " keys, load " + f1(svc_.index().stats().loadFactor, "%.2f"));
    row(sys, "FIFO queue", num(svc_.queue().size()) + " waiting");
    row(sys, "Map", india_.loaded() ? num(india_.states().size()) + " states, " + num(india_.triangleCount()) + " triangles" : "unavailable: " + mapError_);
    row(sys, "Renderer", std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
    y += 316;
    Rect ab{r.x, y, r.w, 220};
    card(ab);
    sectionTitle({ab.x + 20, ab.y + 14, ab.w, 30}, "About", "Smart City Complaint Monitoring & Management System  \xC2\xB7  version 1.0.0");
    const char* lines[] = {"Backend: C++17 (OOP), PostgreSQL via libpq, NASM x86-64 Assembly severity + string engine",
                           "Data structures: doubly linked list, hash table (chaining), FIFO queue, stack, city array + 2-D grid",
                           "Algorithms: Quick Sort, Bubble Sort, Merge Sort, Insertion Sort, Linear and Binary Search",
                           "Frontend: native OpenGL + FreeGLUT, Segoe UI via stb_truetype, real India district geometry",
                           "Keyboard: printable keys always go to the focused text field; there are no letter or digit shortcuts"};
    for (int i = 0; i < 5; ++i) Renderer::text(ab.x + 20, ab.y + 70 + i * 26.0f, lines[i], 13, P().textMuted);
    pageScroll_.end();
}
