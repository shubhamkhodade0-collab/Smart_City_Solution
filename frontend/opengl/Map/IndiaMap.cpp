#include "IndiaMap.h"
#include "../../../backend/Exceptions.h"
#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace map {

using ui::Color;
using ui::Renderer;

static const float LAT0 = 23.0f;                                   // projection reference latitude
static const float KX = std::cos(LAT0 * 3.14159265f / 180.0f);     // equirectangular x-compression

Pt IndiaMap::project(double lon, double lat) const { return {static_cast<float>(lon) * KX, -static_cast<float>(lat)}; }

// ------------------------------------------------------------------ loading
void IndiaMap::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw scms::MapDataException("Map file not found: " + path);
    std::string tag;
    int version = 0;
    in >> tag >> version;
    if (tag != "SCMSMAP" || version != 1) throw scms::MapDataException("Malformed map file (bad header): " + path);

    auto expect = [&](const char* want) {
        std::string t;
        if (!(in >> t) || t != want) throw scms::MapDataException(std::string("Malformed map file: expected ") + want);
    };
    auto readName = [&]() {
        std::string rest;
        std::getline(in, rest);
        std::size_t a = rest.find_first_not_of(' ');
        return a == std::string::npos ? std::string() : rest.substr(a);
    };
    auto readPoints = [&](std::vector<Pt>& out) {
        int n;
        if (!(in >> n) || n < 2 || n > 200000) throw scms::MapDataException("Malformed map file: bad point count");
        out.resize(static_cast<std::size_t>(n));
        for (auto& p : out) {
            double lon, lat;
            if (!(in >> lon >> lat) || lon < 60 || lon > 100 || lat < 0 || lat > 40)
                throw scms::MapDataException("Malformed map file: coordinate out of range");
            p = project(lon, lat);
        }
    };

    std::vector<State> states;
    std::vector<District> districts;
    std::vector<std::vector<Pt>> borders;
    int n;
    expect("STATES");
    if (!(in >> n) || n <= 0 || n > 64) throw scms::MapDataException("Malformed map file: state count");
    for (int i = 0; i < n; ++i) {
        expect("S");
        int idx;
        in >> idx;
        State s;
        s.name = readName();
        if (idx != i || s.name.empty()) throw scms::MapDataException("Malformed map file: state record");
        states.push_back(s);
    }
    expect("DISTRICTS");
    if (!(in >> n) || n <= 0) throw scms::MapDataException("Malformed map file: district count");
    for (int i = 0; i < n; ++i) {
        expect("D");
        District d;
        int rings;
        in >> d.state >> rings;
        d.name = readName();
        if (d.state < 0 || d.state >= (int)states.size() || rings <= 0)
            throw scms::MapDataException("Malformed map file: district record");
        for (int k = 0; k < rings; ++k) {
            expect("R");
            std::vector<Pt> ring;
            readPoints(ring);
            d.rings.push_back(std::move(ring));
        }
        districts.push_back(std::move(d));
    }
    expect("BORDERS");
    in >> n;
    for (int i = 0; i < n; ++i) {
        expect("B");
        std::vector<Pt> line;
        readPoints(line);
        borders.push_back(std::move(line));
    }
    expect("END");

    // triangulate + bounding boxes + centroids
    triCount_ = 0;
    wMinX_ = wMinY_ = 1e9f;
    wMaxX_ = wMaxY_ = -1e9f;
    for (std::size_t di = 0; di < districts.size(); ++di) {
        District& d = districts[di];
        d.minX = d.minY = 1e9f;
        d.maxX = d.maxY = -1e9f;
        double cxs = 0, cys = 0, as = 0;
        for (const auto& ring : d.rings) {
            for (const Pt& p : ring) {
                d.minX = std::min(d.minX, p.x); d.maxX = std::max(d.maxX, p.x);
                d.minY = std::min(d.minY, p.y); d.maxY = std::max(d.maxY, p.y);
            }
            auto tris = earClip(ring);
            for (std::size_t t = 0; t + 2 < tris.size(); t += 3) {
                double a = 0.5 * std::fabs((tris[t + 1].x - tris[t].x) * (tris[t + 2].y - tris[t].y) -
                                           (tris[t + 2].x - tris[t].x) * (tris[t + 1].y - tris[t].y));
                cxs += a * (tris[t].x + tris[t + 1].x + tris[t + 2].x) / 3.0;
                cys += a * (tris[t].y + tris[t + 1].y + tris[t + 2].y) / 3.0;
                as += a;
            }
            d.triangles.insert(d.triangles.end(), tris.begin(), tris.end());
        }
        triCount_ += d.triangles.size() / 3;
        d.area = static_cast<float>(as);
        d.centroid = as > 0 ? Pt{float(cxs / as), float(cys / as)} : Pt{(d.minX + d.maxX) / 2, (d.minY + d.maxY) / 2};
        State& s = states[static_cast<std::size_t>(d.state)];
        s.districts.push_back(static_cast<int>(di));
        s.minX = std::min(s.minX, d.minX); s.maxX = std::max(s.maxX, d.maxX);
        s.minY = std::min(s.minY, d.minY); s.maxY = std::max(s.maxY, d.maxY);
        s.labelAt.x += d.centroid.x * d.area;
        s.labelAt.y += d.centroid.y * d.area;
        s.area += d.area;
        wMinX_ = std::min(wMinX_, d.minX); wMaxX_ = std::max(wMaxX_, d.maxX);
        wMinY_ = std::min(wMinY_, d.minY); wMaxY_ = std::max(wMaxY_, d.maxY);
    }
    for (State& s : states)
        if (s.area > 0) { s.labelAt.x /= s.area; s.labelAt.y /= s.area; }
    states_ = std::move(states);
    districts_ = std::move(districts);
    borders_ = std::move(borders);
}

// ------------------------------------------------------------------ ear clipping triangulation
static float cross(const Pt& a, const Pt& b, const Pt& c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); }

static bool inTriangle(const Pt& p, const Pt& a, const Pt& b, const Pt& c) {
    float d1 = cross(a, b, p), d2 = cross(b, c, p), d3 = cross(c, a, p);
    bool neg = (d1 < 0) || (d2 < 0) || (d3 < 0), pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(neg && pos);
}

std::vector<Pt> IndiaMap::earClip(const std::vector<Pt>& ring) {
    std::vector<Pt> out;
    std::vector<int> idx;
    for (int i = 0; i < (int)ring.size(); ++i) {
        if (!idx.empty() && ring[idx.back()].x == ring[i].x && ring[idx.back()].y == ring[i].y) continue;
        idx.push_back(i);
    }
    if (idx.size() < 3) return out;
    double area2 = 0;
    for (std::size_t i = 0; i < idx.size(); ++i) {
        const Pt &a = ring[idx[i]], &b = ring[idx[(i + 1) % idx.size()]];
        area2 += (double)a.x * b.y - (double)b.x * a.y;
    }
    const float orient = area2 > 0 ? 1.0f : -1.0f;   // convex corners have cross * orient > 0
    int guard = 0;
    while (idx.size() > 3 && guard < 100000) {
        bool clipped = false;
        int n = (int)idx.size();
        for (int i = 0; i < n; ++i) {
            const Pt& a = ring[idx[(i + n - 1) % n]];
            const Pt& b = ring[idx[i]];
            const Pt& c = ring[idx[(i + 1) % n]];
            if (cross(a, b, c) * orient <= 0) continue;           // reflex vertex: not an ear
            bool empty = true;
            for (int k = 0; k < n && empty; ++k) {
                int v = idx[k];
                if (k == i || k == (i + n - 1) % n || k == (i + 1) % n) continue;
                if (inTriangle(ring[v], a, b, c)) empty = false;
            }
            if (!empty) continue;
            out.push_back(a); out.push_back(b); out.push_back(c);
            idx.erase(idx.begin() + i);
            clipped = true;
            break;
        }
        if (!clipped) {   // degenerate (self-touching) ring: fall back to a fan for the remainder
            for (std::size_t k = 1; k + 1 < idx.size(); ++k) {
                out.push_back(ring[idx[0]]); out.push_back(ring[idx[k]]); out.push_back(ring[idx[k + 1]]);
            }
            return out;
        }
        ++guard;
    }
    if (idx.size() == 3) { out.push_back(ring[idx[0]]); out.push_back(ring[idx[1]]); out.push_back(ring[idx[2]]); }
    return out;
}

bool IndiaMap::pointInRing(const std::vector<Pt>& ring, float x, float y) {
    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        if (((ring[i].y > y) != (ring[j].y > y)) &&
            (x < (ring[j].x - ring[i].x) * (y - ring[i].y) / (ring[j].y - ring[i].y) + ring[i].x))
            inside = !inside;
    }
    return inside;
}

// ------------------------------------------------------------------ Cohen-Sutherland line clipping
enum : int { INSIDE = 0, LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8 };
static int outcode(float x, float y, float xmin, float ymin, float xmax, float ymax) {
    int c = INSIDE;
    if (x < xmin) c |= LEFT; else if (x > xmax) c |= RIGHT;
    if (y < ymin) c |= BOTTOM; else if (y > ymax) c |= TOP;
    return c;
}

bool IndiaMap::clipLine(float& x0, float& y0, float& x1, float& y1, float xmin, float ymin, float xmax, float ymax) {
    int c0 = outcode(x0, y0, xmin, ymin, xmax, ymax), c1 = outcode(x1, y1, xmin, ymin, xmax, ymax);
    while (true) {
        if (!(c0 | c1)) return true;          // trivially accept: both endpoints inside
        if (c0 & c1) return false;            // trivially reject: both on the same outside side
        int out = c0 ? c0 : c1;
        float x = 0, y = 0;
        if (out & TOP) { x = x0 + (x1 - x0) * (ymax - y0) / (y1 - y0); y = ymax; }
        else if (out & BOTTOM) { x = x0 + (x1 - x0) * (ymin - y0) / (y1 - y0); y = ymin; }
        else if (out & RIGHT) { y = y0 + (y1 - y0) * (xmax - x0) / (x1 - x0); x = xmax; }
        else { y = y0 + (y1 - y0) * (xmin - x0) / (x1 - x0); x = xmin; }
        if (out == c0) { x0 = x; y0 = y; c0 = outcode(x0, y0, xmin, ymin, xmax, ymax); }
        else { x1 = x; y1 = y; c1 = outcode(x1, y1, xmin, ymin, xmax, ymax); }
    }
}

// ------------------------------------------------------------------ view transform
void IndiaMap::setViewport(const ui::Rect& r) {
    bool first = vp_.w == 0;
    vp_ = r;
    float sx = (r.w - 40) / (wMaxX_ - wMinX_), sy = (r.h - 40) / (wMaxY_ - wMinY_);
    baseScale_ = std::max(1.0f, std::min(sx, sy));
    if (first) resetView();
}

void IndiaMap::resetView() {
    zoom_ = 1;
    float s = baseScale_;
    offX_ = (vp_.w - (wMaxX_ - wMinX_) * s) / 2 - wMinX_ * s;
    offY_ = (vp_.h - (wMaxY_ - wMinY_) * s) / 2 - wMinY_ * s;
}

Pt IndiaMap::toScreen(float wx, float wy) const {
    float s = baseScale_ * zoom_;
    return {vp_.x + offX_ + wx * s, vp_.y + offY_ + wy * s};
}

Pt IndiaMap::toWorld(float sx, float sy) const {   // inverse transformation
    float s = baseScale_ * zoom_;
    return {(sx - vp_.x - offX_) / s, (sy - vp_.y - offY_) / s};
}

void IndiaMap::zoomAt(float sx, float sy, float factor) {
    Pt w = toWorld(sx, sy);
    zoom_ = std::clamp(zoom_ * factor, 0.6f, 40.0f);
    float s = baseScale_ * zoom_;
    offX_ = sx - vp_.x - w.x * s;     // keep the world point under the cursor fixed
    offY_ = sy - vp_.y - w.y * s;
}

void IndiaMap::pan(float dx, float dy) {
    offX_ += dx;
    offY_ += dy;
}

int IndiaMap::pickDistrict(float sx, float sy) const {
    if (!vp_.contains(sx, sy)) return -1;
    Pt w = toWorld(sx, sy);
    for (std::size_t i = 0; i < districts_.size(); ++i) {
        const District& d = districts_[i];
        if (w.x < d.minX || w.x > d.maxX || w.y < d.minY || w.y > d.maxY) continue;
        for (const auto& ring : d.rings)
            if (pointInRing(ring, w.x, w.y)) return static_cast<int>(i);
    }
    return -1;
}

int IndiaMap::stateIndex(const std::string& name) const {
    for (std::size_t i = 0; i < states_.size(); ++i)
        if (states_[i].name == name) return static_cast<int>(i);
    return -1;
}

int IndiaMap::districtIndex(int state, const std::string& name) const {
    for (std::size_t i = 0; i < districts_.size(); ++i)
        if (districts_[i].state == state && districts_[i].name == name) return static_cast<int>(i);
    return -1;
}

// ------------------------------------------------------------------ rendering
void IndiaMap::render(const std::vector<Color>& fill, int hoverState, int selState, int hoverDistrict, int selDistrict,
                      const std::vector<Color>* districtFill) {
    const ui::Palette& P = ui::Theme::p();
    Renderer::pushClip(vp_);
    glPushMatrix();
    // window-to-viewport: translate to the viewport origin + pan offset, then scale world units
    glTranslatef(vp_.x + offX_, vp_.y + offY_, 0);
    glScalef(baseScale_ * zoom_, baseScale_ * zoom_, 1);

    // visible world rectangle (used to cull districts and clip borders)
    Pt a = toWorld(vp_.x, vp_.y), b = toWorld(vp_.right(), vp_.bottom());

    glBegin(GL_TRIANGLES);
    for (std::size_t i = 0; i < districts_.size(); ++i) {
        const District& d = districts_[i];
        if (d.maxX < a.x || d.minX > b.x || d.maxY < a.y || d.minY > b.y) continue;
        Color c = districtFill && i < districtFill->size() ? (*districtFill)[i]
                                                           : (d.state < (int)fill.size() ? fill[d.state] : P.mapNone);
        if (d.state == selState) c = c.mix(P.primary, 0.18f);
        if ((int)i == selDistrict) c = c.mix(P.primary, 0.45f);
        else if ((int)i == hoverDistrict && hoverDistrict >= 0) c = c.mix(ui::hex(0xFFFFFF), 0.28f);
        else if (d.state == hoverState) c = c.mix(ui::hex(0xFFFFFF), ui::Theme::dark() ? 0.08f : 0.18f);
        glColor4f(c.r, c.g, c.b, c.a);
        for (const Pt& p : d.triangles) glVertex2f(p.x, p.y);
    }
    glEnd();
    glPopMatrix();

    // district outlines (thin) - drawn in screen space so the width stays 1px at any zoom
    float s = baseScale_ * zoom_;
    auto drawRing = [&](const std::vector<Pt>& ring, const Color& c, float w) {
        glColor4f(c.r, c.g, c.b, c.a);
        glLineWidth(w);
        glBegin(GL_LINE_LOOP);
        for (const Pt& p : ring) glVertex2f(vp_.x + offX_ + p.x * s, vp_.y + offY_ + p.y * s);
        glEnd();
    };
    if (zoom_ > 1.6f || selState >= 0) {
        for (const District& d : districts_) {
            if (d.maxX < a.x || d.minX > b.x || d.maxY < a.y || d.minY > b.y) continue;
            if (zoom_ <= 1.6f && d.state != selState) continue;
            for (const auto& ring : d.rings) drawRing(ring, P.mapBorder, 1.0f);
        }
    }

    // state borders: Cohen-Sutherland clip each segment against the visible world rect
    clipped_ = 0;
    glColor4f(P.mapStateBorder.r, P.mapStateBorder.g, P.mapStateBorder.b, P.mapStateBorder.a);
    glLineWidth(zoom_ > 3 ? 1.6f : 1.1f);
    glBegin(GL_LINES);
    for (const auto& line : borders_) {
        for (std::size_t i = 0; i + 1 < line.size(); ++i) {
            float x0 = line[i].x, y0 = line[i].y, x1 = line[i + 1].x, y1 = line[i + 1].y;
            if (!clipLine(x0, y0, x1, y1, a.x, a.y, b.x, b.y)) continue;
            ++clipped_;
            glVertex2f(vp_.x + offX_ + x0 * s, vp_.y + offY_ + y0 * s);
            glVertex2f(vp_.x + offX_ + x1 * s, vp_.y + offY_ + y1 * s);
        }
    }
    glEnd();

    // selection outlines on top
    if (selState >= 0 && selState < (int)states_.size())
        for (int di : states_[selState].districts)
            for (const auto& ring : districts_[di].rings) drawRing(ring, P.primary.withAlpha(0.55f), 1.2f);
    if (hoverDistrict >= 0 && hoverDistrict < (int)districts_.size())
        for (const auto& ring : districts_[hoverDistrict].rings) drawRing(ring, P.text.withAlpha(0.7f), 1.6f);
    if (selDistrict >= 0 && selDistrict < (int)districts_.size())
        for (const auto& ring : districts_[selDistrict].rings) drawRing(ring, P.primary, 2.2f);
    glLineWidth(1.0f);
    Renderer::popClip();
}

void IndiaMap::renderLabels(const std::vector<std::string>& extra, int selState) {
    const ui::Palette& P = ui::Theme::p();
    Renderer::pushClip(vp_);
    float s = baseScale_ * zoom_;
    for (std::size_t i = 0; i < states_.size(); ++i) {
        const State& st = states_[i];
        float wpx = (st.maxX - st.minX) * s;
        std::string label = st.name;
        float tw = Renderer::measure(label, 11, ui::Weight::SEMIBOLD);
        if (wpx < tw * 0.9f && (int)i != selState) continue;      // too small at this zoom
        Pt c = toScreen(st.labelAt.x, st.labelAt.y);
        Color fg = ui::Theme::dark() ? ui::hex(0xFFFFFF, 0.92f) : ui::hex(0x101828, 0.85f);
        Renderer::text(c.x - tw / 2, c.y + 3, label, 11, fg, ui::Weight::SEMIBOLD);
        if (i < extra.size() && !extra[i].empty()) {
            float ew = Renderer::measure(extra[i], 10);
            Renderer::text(c.x - ew / 2, c.y + 16, extra[i], 10, fg.withAlpha(0.75f));
        }
    }
    if (zoom_ > 3.0f) {   // district names when zoomed in
        for (const District& d : districts_) {
            if (d.name == "~") continue;
            float wpx = (d.maxX - d.minX) * s;
            float tw = Renderer::measure(d.name, 10);
            if (wpx < tw * 1.1f) continue;
            Pt c = toScreen(d.centroid.x, d.centroid.y);
            if (!vp_.contains(c.x, c.y)) continue;
            Renderer::text(c.x - tw / 2, c.y + 3, d.name, 10, P.text.withAlpha(0.65f));
        }
    }
    Renderer::popClip();
}

} // namespace map
