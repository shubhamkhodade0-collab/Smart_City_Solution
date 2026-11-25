#include "Charts.h"
#include "../UI/Widgets.h"
#include <GL/freeglut.h>
#include <GL/glu.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace charts {

using ui::Color;
using ui::Rect;
using ui::Renderer;
using ui::Weight;

static const float PI = 3.14159265358979f;
static const ui::Palette& P() { return ui::Theme::p(); }

static std::string fmt(double v) {
    char b[32];
    if (v >= 10000) std::snprintf(b, sizeof b, "%.1fk", v / 1000.0);
    else std::snprintf(b, sizeof b, "%.0f", v);
    return b;
}

double niceMax(double v) {
    if (v <= 0) return 4;
    double p = std::pow(10.0, std::floor(std::log10(v)));
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0})
        if (v <= m * p) return m * p;
    return 10 * p;
}

void barChart(const Rect& r, const std::vector<std::string>& labels, const std::vector<double>& values,
              const Color& color, bool rotate) {
    if (values.empty()) return;
    double mx = niceMax(*std::max_element(values.begin(), values.end()));
    Rect plot{r.x + 36, r.y + 8, r.w - 44, r.h - (rotate ? 58 : 34)};
    for (int g = 0; g <= 4; ++g) {
        float y = plot.bottom() - plot.h * g / 4.0f;
        Renderer::line(plot.x, y, plot.right(), y, P().border.withAlpha(g == 0 ? 1.0f : 0.6f));
        std::string t = fmt(mx * g / 4.0);
        Renderer::text(plot.x - 8 - Renderer::measure(t, 10.5f), y + 4, t, 10.5f, P().textFaint);
    }
    float slot = plot.w / values.size();
    float bw = std::min(36.0f, slot * 0.58f);
    ui::UI& u = ui::UI::get();
    for (std::size_t i = 0; i < values.size(); ++i) {
        float h = static_cast<float>(plot.h * values[i] / mx);
        Rect bar{plot.x + slot * i + (slot - bw) / 2, plot.bottom() - h, bw, h};
        bool hov = u.hover({plot.x + slot * i, plot.y, slot, plot.h});
        Renderer::roundRect({bar.x, bar.y, bar.w, std::max(0.0f, h)}, std::min(5.0f, h / 2), hov ? color.mix(ui::hex(0x000000), 0.15f) : color);
        if (hov) u.tooltip(labels[i] + ": " + fmt(values[i]));
        std::string lab = Renderer::ellipsize(labels[i], rotate ? 70 : slot - 4, 10.5f);
        if (rotate) {
            glPushMatrix();
            float lx = bar.x + bar.w / 2, ly = plot.bottom() + 10;
            glTranslatef(lx, ly, 0);
            glRotatef(35, 0, 0, 1);          // rotation transform for long axis labels
            Renderer::text(0, 8, lab, 10.5f, P().textMuted);
            glPopMatrix();
        } else {
            float tw = Renderer::measure(lab, 10.5f);
            Renderer::text(bar.x + bar.w / 2 - tw / 2, plot.bottom() + 18, lab, 10.5f, P().textMuted);
        }
    }
}

void hbarChart(const Rect& r, const std::vector<std::string>& labels, const std::vector<double>& values,
               const std::vector<Color>& colors, bool showPercent) {
    if (values.empty()) return;
    double mx = *std::max_element(values.begin(), values.end());
    double total = 0;
    for (double v : values) total += v;
    if (mx <= 0) mx = 1;
    float rowH = std::min(34.0f, r.h / values.size());
    float labelW = std::min(150.0f, r.w * 0.34f);
    ui::UI& u = ui::UI::get();
    for (std::size_t i = 0; i < values.size(); ++i) {
        float y = r.y + i * rowH;
        Renderer::textIn({r.x, y, labelW - 10, rowH}, labels[i], 12.5f, P().text);
        Rect track{r.x + labelW, y + rowH / 2 - 5, r.w - labelW - 70, 10};
        Renderer::roundRect(track, 5, P().neutralSoft);
        float w = static_cast<float>(track.w * values[i] / mx);
        Color c = colors.empty() ? P().primary : colors[i % colors.size()];
        if (w > 0) Renderer::roundRect({track.x, track.y, std::max(10.0f, w), track.h}, 5, c);
        std::string t = fmt(values[i]);
        if (showPercent && total > 0) {
            char b[24];
            std::snprintf(b, sizeof b, "  %.0f%%", 100.0 * values[i] / total);
            t += b;
        }
        Renderer::textIn({track.right() + 10, y, 64, rowH}, t, 12.5f, P().textMuted, Weight::SEMIBOLD, ui::Align::RIGHT);
        if (u.hover({r.x, y, r.w, rowH})) u.tooltip(labels[i] + ": " + fmt(values[i]));
    }
}

void donut(const Rect& r, const std::vector<std::string>& labels, const std::vector<double>& values,
           const std::vector<Color>& colors, const std::string& center) {
    double total = 0;
    for (double v : values) total += v;
    float rad = std::min(r.h, r.w * 0.45f) * 0.46f;
    float cy = r.y + r.h / 2;
    float labelW = 0, valueW = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        labelW = std::max(labelW, Renderer::measure(labels[i], 12.5f));
        valueW = std::max(valueW, Renderer::measure(fmt(values[i]), 12.5f, Weight::SEMIBOLD));
    }
    labelW = std::min(labelW, std::max(0.0f, r.w - (2 * rad + 10 + 28 + 10 + 8 + 18 + valueW)));
    float legendW = 10 + 8 + labelW + 18 + valueW;
    float groupW = 2 * rad + 10 + 28 + legendW;
    float groupX = r.x + (r.w - groupW) / 2;
    float cx = groupX + rad + 10;
    ui::UI& u = ui::UI::get();
    float a = -PI / 2;
    if (total <= 0) Renderer::ring(cx, cy, rad * 0.64f, rad, 0, 2 * PI, P().neutralSoft);
    for (std::size_t i = 0; i < values.size() && total > 0; ++i) {
        float sweep = static_cast<float>(2 * PI * values[i] / total);
        if (sweep <= 0) continue;
        float mid = a + sweep / 2;
        float dx = u.mouse.x - cx, dy = u.mouse.y - cy, dist = std::sqrt(dx * dx + dy * dy);
        float ang = std::atan2(dy, dx);
        while (ang < -PI / 2) ang += 2 * PI;
        bool hov = dist > rad * 0.64f && dist < rad && ang >= a && ang < a + sweep && u.hover(r);
        float grow = hov ? 4.0f : 0.0f;
        Renderer::ring(cx + std::cos(mid) * grow * 0.5f, cy + std::sin(mid) * grow * 0.5f, rad * 0.64f, rad + grow,
                       a + 0.012f, a + sweep - 0.012f, colors[i % colors.size()]);
        if (hov) {
            char b[64];
            std::snprintf(b, sizeof b, ": %.0f (%.1f%%)", values[i], 100.0 * values[i] / total);
            u.tooltip(labels[i] + b);
        }
        a += sweep;
    }
    float tw = Renderer::measure(fmt(total), 22, Weight::BOLD);
    Renderer::text(cx - tw / 2, cy + 4, fmt(total), 22, P().text, Weight::BOLD);
    float lw = Renderer::measure(center, 11);
    Renderer::text(cx - lw / 2, cy + 22, center, 11, P().textMuted);
    // legend
    float lx = cx + rad + 28, ly = cy - (static_cast<float>(values.size()) - 1) * 13.0f;
    float labelX = lx + 18, valueRight = lx + legendW;
    for (std::size_t i = 0; i < values.size(); ++i) {
        Renderer::roundRect({lx, ly + i * 26 - 5, 10, 10}, 3, colors[i % colors.size()]);
        Renderer::text(labelX, ly + i * 26 + 4, Renderer::ellipsize(labels[i], labelW, 12.5f), 12.5f, P().text);
        std::string v = fmt(values[i]);
        Renderer::text(valueRight - Renderer::measure(v, 12.5f, Weight::SEMIBOLD), ly + i * 26 + 4, v, 12.5f,
                       P().textMuted, Weight::SEMIBOLD);
    }
}

void lineChart(const Rect& r, const std::vector<std::string>& labels, const std::vector<double>& a,
               const std::vector<double>& b, const std::string& nameA, const std::string& nameB) {
    if (a.size() < 2) return;
    double mx = 0;
    for (double v : a) mx = std::max(mx, v);
    for (double v : b) mx = std::max(mx, v);
    mx = niceMax(mx);
    Rect plot{r.x + 36, r.y + 28, r.w - 48, r.h - 56};
    for (int g = 0; g <= 4; ++g) {
        float y = plot.bottom() - plot.h * g / 4.0f;
        Renderer::line(plot.x, y, plot.right(), y, P().border.withAlpha(g == 0 ? 1.0f : 0.6f));
        std::string t = fmt(mx * g / 4.0);
        Renderer::text(plot.x - 8 - Renderer::measure(t, 10.5f), y + 4, t, 10.5f, P().textFaint);
    }
    auto px = [&](std::size_t i) { return plot.x + plot.w * i / (a.size() - 1); };
    auto py = [&](double v) { return static_cast<float>(plot.bottom() - plot.h * v / mx); };
    auto series = [&](const std::vector<double>& s, const Color& c, bool area) {
        if (area) {
            glBegin(GL_TRIANGLE_STRIP);
            for (std::size_t i = 0; i < s.size(); ++i) {
                glColor4f(c.r, c.g, c.b, 0.16f); glVertex2f(px(i), py(s[i]));
                glColor4f(c.r, c.g, c.b, 0.0f); glVertex2f(px(i), plot.bottom());
            }
            glEnd();
        }
        std::vector<float> pts;
        for (std::size_t i = 0; i < s.size(); ++i) { pts.push_back(px(i)); pts.push_back(py(s[i])); }
        Renderer::polyline(pts, c, 2.2f);
        for (std::size_t i = 0; i < s.size(); ++i) {
            Renderer::circle(px(i), py(s[i]), 3.6f, P().surface);
            Renderer::circle(px(i), py(s[i]), 2.6f, c);
        }
    };
    series(a, P().chart[0], true);
    if (!b.empty()) series(b, P().chart[1], false);
    ui::UI& u = ui::UI::get();
    for (std::size_t i = 0; i < a.size(); ++i) {
        std::string lab = labels[i];
        float tw = Renderer::measure(lab, 10.5f);
        if (a.size() <= 12 || i % 2 == 0) Renderer::text(px(i) - tw / 2, plot.bottom() + 18, lab, 10.5f, P().textMuted);
        Rect hit{px(i) - plot.w / (2 * (a.size() - 1)), plot.y, plot.w / (a.size() - 1), plot.h};
        if (u.hover(hit)) {
            Renderer::line(px(i), plot.y, px(i), plot.bottom(), P().borderStrong);
            u.tooltip(lab + "  -  " + nameA + ": " + fmt(a[i]) + (b.empty() ? "" : "   " + nameB + ": " + fmt(b[i])));
        }
    }
    // legend
    Renderer::roundRect({r.x + 36, r.y + 4, 10, 10}, 3, P().chart[0]);
    float lx = Renderer::text(r.x + 52, r.y + 13, nameA, 12, P().textMuted) + 18;
    if (!b.empty()) {
        Renderer::roundRect({lx, r.y + 4, 10, 10}, 3, P().chart[1]);
        Renderer::text(lx + 16, r.y + 13, nameB, 12, P().textMuted);
    }
}

// ================================================================== 3-D bar chart
static void box(float x, float z, float w, float d, float h) {
    // six faces with normals for lighting
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);  glVertex3f(x, h, z); glVertex3f(x + w, h, z); glVertex3f(x + w, h, z + d); glVertex3f(x, h, z + d);
    glNormal3f(0, 0, 1);  glVertex3f(x, 0, z + d); glVertex3f(x + w, 0, z + d); glVertex3f(x + w, h, z + d); glVertex3f(x, h, z + d);
    glNormal3f(0, 0, -1); glVertex3f(x, 0, z); glVertex3f(x, h, z); glVertex3f(x + w, h, z); glVertex3f(x + w, 0, z);
    glNormal3f(1, 0, 0);  glVertex3f(x + w, 0, z); glVertex3f(x + w, h, z); glVertex3f(x + w, h, z + d); glVertex3f(x + w, 0, z + d);
    glNormal3f(-1, 0, 0); glVertex3f(x, 0, z); glVertex3f(x, 0, z + d); glVertex3f(x, h, z + d); glVertex3f(x, h, z);
    glEnd();
}

void Chart3D::handleMouse(const Rect& r) {
    ui::UI& u = ui::UI::get();
    if (u.hover(r) && u.mouse.pressed) { dragging = true; autoRotate = false; lastX = u.mouse.x; lastY = u.mouse.y; u.consumeClick(); }
    if (!u.mouse.down) dragging = false;
    if (dragging) {
        yaw += (u.mouse.x - lastX) * 0.4f;
        pitch = std::clamp(pitch + (u.mouse.y - lastY) * 0.3f, 8.0f, 75.0f);
        lastX = u.mouse.x; lastY = u.mouse.y;
    }
    if (u.hover(r) && u.mouse.wheel != 0) {
        distance = std::clamp(distance * (u.mouse.wheel > 0 ? 0.9f : 1.1f), 0.55f, 2.2f);
        u.mouse.wheel = 0;
    }
    if (autoRotate) yaw += 0.12f;
}

void Chart3D::draw(const Rect& r, const std::vector<std::string>& rows, const std::vector<std::string>& cols,
                   const std::vector<std::vector<int>>& v) {
    handleMouse(r);
    int H = Renderer::windowHeight();   // glViewport works in window coordinates
    int mx = 1;
    for (const auto& row : v) for (int x : row) mx = std::max(mx, x);
    const float cell = 1.0f, bar = 0.62f;
    float W = cols.size() * cell, D = rows.size() * cell;

    // ---- dedicated viewport for the chart (dynamic graph viewport)
    const float pixelScale = Renderer::pixelScale();
    glViewport(static_cast<GLint>(r.x * pixelScale), static_cast<GLint>((H - r.y - r.h) * pixelScale),
               static_cast<GLsizei>(r.w * pixelScale), static_cast<GLsizei>(r.h * pixelScale));
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluPerspective(38.0, r.w / r.h, 0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    float dist = std::max(W, D) * 1.9f * distance + 4;
    gluLookAt(0, 0, dist, 0, 0, 0, 0, 1, 0);          // camera
    glRotatef(pitch, 1, 0, 0);                           // 3-D rotation (camera orbit)
    glRotatef(yaw, 0, 1, 0);
    glTranslatef(-W / 2, -1.2f, -D / 2);                 // centre the grid at the origin
    glScalef(1.0f, 3.2f / mx, 1.0f);                     // scale heights to the data range

    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    // floor grid
    glDisable(GL_LIGHTING);
    Color g = P().border;
    glColor4f(g.r, g.g, g.b, 1);
    glBegin(GL_LINES);
    for (std::size_t i = 0; i <= cols.size(); ++i) { glVertex3f(i * cell, 0, 0); glVertex3f(i * cell, 0, D); }
    for (std::size_t j = 0; j <= rows.size(); ++j) { glVertex3f(0, 0, j * cell); glVertex3f(W, 0, j * cell); }
    glEnd();

    // lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    GLfloat pos[] = {W * 0.2f, static_cast<float>(mx) * 2.0f, D * 1.6f, 1.0f};
    GLfloat amb[] = {0.38f, 0.38f, 0.42f, 1}, dif[] = {0.75f, 0.75f, 0.72f, 1};
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    for (std::size_t j = 0; j < rows.size(); ++j)
        for (std::size_t i = 0; i < cols.size() && i < v[j].size(); ++i) {
            if (v[j][i] <= 0) continue;
            Color c = P().chart[i % 8];
            glColor4f(c.r, c.g, c.b, 1);
            box(i * cell + (cell - bar) / 2, j * cell + (cell - bar) / 2, bar, bar, static_cast<float>(v[j][i]));
        }
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);

    // project label anchors to window coordinates before restoring matrices
    GLdouble model[16], proj[16];
    GLint view[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, model);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, view);
    struct L { float x, y; std::string t; bool depthRow; };
    std::vector<L> labels;
    for (std::size_t j = 0; j < rows.size(); ++j) {
        GLdouble wx, wy, wz;
        if (gluProject(-0.25, 0, j * cell + cell / 2, model, proj, view, &wx, &wy, &wz))
            labels.push_back({static_cast<float>(wx / pixelScale), static_cast<float>(H - wy / pixelScale), rows[j], true});
    }
    for (std::size_t i = 0; i < cols.size(); ++i) {
        GLdouble wx, wy, wz;
        if (gluProject(i * cell + cell / 2, 0, D + 0.35, model, proj, view, &wx, &wy, &wz))
            labels.push_back({static_cast<float>(wx / pixelScale), static_cast<float>(H - wy / pixelScale), cols[i], false});
    }
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    Renderer::beginFrame(Renderer::width(), Renderer::height(), Renderer::windowHeight(), pixelScale);

    Renderer::pushClip(r);
    for (const auto& l : labels) {
        std::string t = Renderer::ellipsize(l.t, 110, 11);
        float tw = Renderer::measure(t, 11);
        Renderer::text(l.depthRow ? l.x - tw - 4 : l.x - tw / 2, l.y + 4, t, 11, l.depthRow ? P().textMuted : P().text,
                       l.depthRow ? Weight::REGULAR : Weight::SEMIBOLD);
    }
    Renderer::popClip();
}

} // namespace charts
