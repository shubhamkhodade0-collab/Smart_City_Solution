#include "Renderer.h"
#include <GL/freeglut.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace ui {

namespace {

constexpr float PI = 3.14159265358979f;
const float kSizes[] = {10, 11, 12, 13, 14, 15, 16, 18, 20, 22, 24, 28, 32, 40};
constexpr int kNumSizes = sizeof(kSizes) / sizeof(kSizes[0]);
constexpr int ATLAS_W = 2048, ATLAS_H = 2048;
// extra code points beyond ASCII: · – — • … ← ↑ → ↓ ↔ ₹
const int kExtra[] = {0x00B7, 0x2013, 0x2014, 0x2022, 0x2026, 0x2190, 0x2191, 0x2192, 0x2193, 0x2194, 0x20B9};
constexpr int kNumExtra = sizeof(kExtra) / sizeof(kExtra[0]);

struct BakedFont {
    GLuint tex = 0;
    stbtt_packedchar ascii[kNumSizes][95];
    stbtt_packedchar extra[kNumSizes][kNumExtra];
    float ascent[kNumSizes];
    bool ok = false;
};

BakedFont g_fonts[3];
int g_w = 1, g_h = 1, g_winH = 1;
float g_pixelScale = 1.0f;
std::vector<Rect> g_clip;

int sizeIndex(float size) {
    int best = 0;
    for (int i = 1; i < kNumSizes; ++i)
        if (std::fabs(kSizes[i] - size) < std::fabs(kSizes[best] - size)) best = i;
    return best;
}

bool bake(BakedFont& f, const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    std::fseek(fp, 0, SEEK_END);
    long len = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    std::vector<unsigned char> ttf(static_cast<std::size_t>(len));
    std::size_t got = std::fread(ttf.data(), 1, ttf.size(), fp);
    std::fclose(fp);
    if (got != ttf.size()) return false;

    std::vector<unsigned char> atlas(ATLAS_W * ATLAS_H, 0);
    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, atlas.data(), ATLAS_W, ATLAS_H, 0, 1, nullptr)) return false;
    stbtt_PackSetOversampling(&pc, 2, 1);
    stbtt_fontinfo info;
    stbtt_InitFont(&info, ttf.data(), 0);
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
    for (int i = 0; i < kNumSizes; ++i) {
        stbtt_pack_range ranges[2] = {};
        ranges[0].font_size = kSizes[i];
        ranges[0].first_unicode_codepoint_in_range = 32;
        ranges[0].num_chars = 95;
        ranges[0].chardata_for_range = f.ascii[i];
        ranges[1].font_size = kSizes[i];
        ranges[1].array_of_unicode_codepoints = const_cast<int*>(kExtra);
        ranges[1].num_chars = kNumExtra;
        ranges[1].chardata_for_range = f.extra[i];
        stbtt_PackFontRanges(&pc, ttf.data(), 0, ranges, 2);
        f.ascent[i] = asc * stbtt_ScaleForPixelHeight(&info, kSizes[i]);
    }
    stbtt_PackEnd(&pc);

    glGenTextures(1, &f.tex);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_W, ATLAS_H, 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f.ok = true;
    return true;
}

// UTF-8 decode -> code point, advances i
int nextCodepoint(const std::string& s, std::size_t& i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) { ++i; return c; }
    int n = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : 1;
    int cp = c & (0x3F >> n);
    ++i;
    for (int k = 0; k < n && i < s.size(); ++k, ++i) cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
    return cp;
}

const stbtt_packedchar* glyph(const BakedFont& f, int si, int cp, int& index, bool& isExtra) {
    if (cp >= 32 && cp < 127) { index = cp - 32; isExtra = false; return &f.ascii[si][index]; }
    for (int k = 0; k < kNumExtra; ++k)
        if (kExtra[k] == cp) { index = k; isExtra = true; return &f.extra[si][k]; }
    index = '?' - 32; isExtra = false;
    return &f.ascii[si][index];
}

void setColor(const Color& c) { glColor4f(c.r, c.g, c.b, c.a); }

void applyScissor() {
    if (g_clip.empty()) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    const Rect& r = g_clip.back();
    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<GLint>(std::floor(r.x * g_pixelScale)),
              static_cast<GLint>(std::floor((g_winH - r.y - r.h) * g_pixelScale)),
              static_cast<GLsizei>(std::ceil(r.w < 0 ? 0 : r.w) * g_pixelScale),
              static_cast<GLsizei>(std::ceil(r.h < 0 ? 0 : r.h) * g_pixelScale));
}

// perimeter of a rounded rect: corners TL, TR, BR, BL with the angle increasing
// (screen y points down, so increasing angle walks clockwise on screen)
void roundPerimeter(const Rect& r, float rad, std::vector<float>& out) {
    rad = std::fmin(rad, std::fmin(r.w, r.h) * 0.5f);
    const int seg = 8;
    const float cx[4] = {r.x + rad, r.x + r.w - rad, r.x + r.w - rad, r.x + rad};
    const float cy[4] = {r.y + rad, r.y + rad, r.y + r.h - rad, r.y + r.h - rad};
    const float a0[4] = {PI, 1.5f * PI, 0.0f, 0.5f * PI};
    out.clear();
    for (int k = 0; k < 4; ++k)
        for (int s = 0; s <= seg; ++s) {
            float a = a0[k] + (PI / 2) * s / seg;
            out.push_back(cx[k] + rad * std::cos(a));
            out.push_back(cy[k] + rad * std::sin(a));
        }
}

void roundFan(const Rect& r, float rad) {
    static std::vector<float> pts;
    roundPerimeter(r, rad, pts);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
    for (std::size_t i = 0; i < pts.size(); i += 2) glVertex2f(pts[i], pts[i + 1]);
    glVertex2f(pts[0], pts[1]);
    glEnd();
}

} // namespace

bool Renderer::init(const std::string& regular, const std::string& semibold, const std::string& bold) {
    bool a = bake(g_fonts[0], regular);
    bool b = bake(g_fonts[1], semibold) || bake(g_fonts[1], regular);
    bool c = bake(g_fonts[2], bold) || bake(g_fonts[2], semibold);
    return a && b && c;
}

bool Renderer::fontsLoaded() { return g_fonts[0].ok; }

void Renderer::beginFrame(int width, int height, int windowHeight, float pixelScale) {
    g_w = width;
    g_h = height;
    g_winH = windowHeight > 0 ? windowHeight : height;
    g_pixelScale = std::max(1.0f, pixelScale);
    g_clip.clear();
    glViewport(0, static_cast<GLint>(std::min(0, g_winH - height) * g_pixelScale),
               static_cast<GLsizei>(width * g_pixelScale), static_cast<GLsizei>(height * g_pixelScale));
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

int Renderer::width() { return g_w; }
int Renderer::height() { return g_h; }
int Renderer::windowHeight() { return g_winH; }
float Renderer::pixelScale() { return g_pixelScale; }

void Renderer::rect(const Rect& r, const Color& c) {
    setColor(c);
    glBegin(GL_QUADS);
    glVertex2f(r.x, r.y);
    glVertex2f(r.x + r.w, r.y);
    glVertex2f(r.x + r.w, r.y + r.h);
    glVertex2f(r.x, r.y + r.h);
    glEnd();
}

void Renderer::roundRect(const Rect& r, float radius, const Color& c) {
    if (r.w <= 0 || r.h <= 0) return;
    setColor(c);
    if (radius <= 0.5f) { rect(r, c); return; }
    roundFan(r, radius);
}

void Renderer::roundRectOutline(const Rect& r, float rad, const Color& c, float thickness) {
    if (r.w <= 0 || r.h <= 0) return;
    static std::vector<float> pts;
    roundPerimeter(Rect{r.x + 0.5f, r.y + 0.5f, r.w - 1.0f, r.h - 1.0f}, rad, pts);
    setColor(c);
    glLineWidth(thickness);
    glBegin(GL_LINE_LOOP);
    for (std::size_t i = 0; i < pts.size(); i += 2) glVertex2f(pts[i], pts[i + 1]);
    glEnd();
    glLineWidth(1.0f);
}

void Renderer::shadow(const Rect& r, float radius, float spread, float strength) {
    const Color& s = Theme::p().shadow;
    const int layers = 6;
    for (int i = layers; i >= 1; --i) {
        float t = static_cast<float>(i) / layers;
        float grow = spread * t;
        Rect e{r.x - grow * 0.6f, r.y - grow * 0.3f + spread * 0.25f, r.w + grow * 1.2f, r.h + grow * 1.2f};
        roundRect(e, radius + grow, s.withAlpha(s.a * strength * (1.0f - t) * 0.9f));
    }
}

void Renderer::circle(float cx, float cy, float radius, const Color& c, int segments) {
    setColor(c);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; ++i) {
        float a = 2 * PI * i / segments;
        glVertex2f(cx + radius * std::cos(a), cy + radius * std::sin(a));
    }
    glEnd();
}

void Renderer::ring(float cx, float cy, float r0, float r1, float a0, float a1, const Color& c) {
    setColor(c);
    int seg = static_cast<int>(std::fmax(4.0f, (a1 - a0) * 40));
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= seg; ++i) {
        float a = a0 + (a1 - a0) * i / seg;
        glVertex2f(cx + r1 * std::cos(a), cy + r1 * std::sin(a));
        glVertex2f(cx + r0 * std::cos(a), cy + r0 * std::sin(a));
    }
    glEnd();
}

void Renderer::line(float x0, float y0, float x1, float y1, const Color& c, float width) {
    setColor(c);
    glLineWidth(width);
    glBegin(GL_LINES);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glEnd();
    glLineWidth(1.0f);
}

void Renderer::polyline(const std::vector<float>& xy, const Color& c, float width) {
    if (xy.size() < 4) return;
    setColor(c);
    glLineWidth(width);
    glBegin(GL_LINE_STRIP);
    for (std::size_t i = 0; i + 1 < xy.size(); i += 2) glVertex2f(xy[i], xy[i + 1]);
    glEnd();
    glLineWidth(1.0f);
}

void Renderer::triangle(float x0, float y0, float x1, float y1, float x2, float y2, const Color& c) {
    setColor(c);
    glBegin(GL_TRIANGLES);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void Renderer::hgradient(const Rect& r, const Color& a, const Color& b) {
    glBegin(GL_QUADS);
    setColor(a); glVertex2f(r.x, r.y);
    setColor(b); glVertex2f(r.x + r.w, r.y);
    setColor(b); glVertex2f(r.x + r.w, r.y + r.h);
    setColor(a); glVertex2f(r.x, r.y + r.h);
    glEnd();
}

void Renderer::vgradient(const Rect& r, const Color& a, const Color& b) {
    glBegin(GL_QUADS);
    setColor(a); glVertex2f(r.x, r.y);
    setColor(a); glVertex2f(r.x + r.w, r.y);
    setColor(b); glVertex2f(r.x + r.w, r.y + r.h);
    setColor(b); glVertex2f(r.x, r.y + r.h);
    glEnd();
}

float Renderer::measure(const std::string& s, float size, Weight w) {
    if (size < 10.0f) size = 12.0f;
    const BakedFont& f = g_fonts[static_cast<int>(w)];
    if (!f.ok) return s.size() * size * 0.55f;
    int si = sizeIndex(size);
    float x = 0;
    for (std::size_t i = 0; i < s.size();) {
        int cp = nextCodepoint(s, i), idx;
        bool ex;
        x += glyph(f, si, cp, idx, ex)->xadvance;
    }
    return x;
}

float Renderer::text(float x, float baseline, const std::string& s, float size, const Color& c, Weight w) {
    if (size < 10.0f) size = 12.0f;
    const BakedFont& f = g_fonts[static_cast<int>(w)];
    if (!f.ok) {   // fallback: GLUT bitmap font (only if Segoe UI is missing)
        setColor(c);
        glRasterPos2f(x, baseline);
        for (char ch : s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, ch);
        return x + s.size() * 7.0f;
    }
    int si = sizeIndex(size);
    float px = std::floor(x), py = std::floor(baseline + 0.5f);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, f.tex);
    setColor(c);
    glBegin(GL_QUADS);
    for (std::size_t i = 0; i < s.size();) {
        int cp = nextCodepoint(s, i), idx;
        bool ex;
        glyph(f, si, cp, idx, ex);
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(ex ? f.extra[si] : f.ascii[si], ATLAS_W, ATLAS_H, idx, &px, &py, &q, 0);
        glTexCoord2f(q.s0, q.t0); glVertex2f(q.x0, q.y0);
        glTexCoord2f(q.s1, q.t0); glVertex2f(q.x1, q.y0);
        glTexCoord2f(q.s1, q.t1); glVertex2f(q.x1, q.y1);
        glTexCoord2f(q.s0, q.t1); glVertex2f(q.x0, q.y1);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    return px;
}

std::string Renderer::ellipsize(const std::string& s, float maxWidth, float size, Weight w) {
    if (measure(s, size, w) <= maxWidth) return s;
    std::string out = s;
    const std::string dots = "\xE2\x80\xA6";   // …
    while (!out.empty() && measure(out + dots, size, w) > maxWidth) {
        out.pop_back();
        while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80) out.pop_back();
    }
    return out + dots;
}

void Renderer::textIn(const Rect& r, const std::string& s, float size, const Color& c, Weight w, Align align) {
    std::string t = ellipsize(s, r.w, size, w);
    float tw = measure(t, size, w);
    float x = r.x;
    if (align == Align::CENTER) x = r.x + (r.w - tw) * 0.5f;
    else if (align == Align::RIGHT) x = r.x + r.w - tw;
    float baseline = r.y + r.h * 0.5f + size * 0.36f;
    text(x, baseline, t, size, c, w);
}

std::vector<std::string> Renderer::wrap(const std::string& s, float maxWidth, float size, Weight w) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string candidate = line.empty() ? word : line + " " + word;
        if (measure(candidate, size, w) <= maxWidth || line.empty()) line = candidate;
        else {
            lines.push_back(line);
            line = word;
        }
        word.clear();
    };
    for (char ch : s) {
        if (ch == '\n') { flushWord(); lines.push_back(line); line.clear(); }
        else if (ch == ' ') flushWord();
        else word += ch;
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

void Renderer::pushClip(const Rect& r) {
    Rect c = r;
    if (!g_clip.empty()) {   // intersect with the parent clip
        const Rect& p = g_clip.back();
        float x0 = std::fmax(c.x, p.x), y0 = std::fmax(c.y, p.y);
        float x1 = std::fmin(c.x + c.w, p.x + p.w), y1 = std::fmin(c.y + c.h, p.y + p.h);
        c = {x0, y0, std::fmax(0.0f, x1 - x0), std::fmax(0.0f, y1 - y0)};
    }
    g_clip.push_back(c);
    applyScissor();
}

void Renderer::popClip() {
    if (!g_clip.empty()) g_clip.pop_back();
    applyScissor();
}

} // namespace ui
