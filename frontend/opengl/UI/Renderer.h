// =====================================================================
// Renderer.h - 2-D drawing primitives on top of fixed-function OpenGL
//
// Everything is drawn in window pixel coordinates (glOrtho, origin top-left).
// Rounded rectangles = triangle fans around quarter-circle corners; shadows =
// stacked translucent rounded rects; text = textured quads from a stb_truetype
// atlas (Segoe UI). Clipping uses glScissor with a stack.
//
// [CO-EVIDENCE] CG-CO2: OpenGL primitives (GL_TRIANGLE_FAN, GL_LINES, GL_LINE_STRIP,
//               GL_QUADS, GL_TRIANGLES), circles from parametric points, colour + alpha blending
// =====================================================================
#pragma once
#include "Theme.h"
#include <string>
#include <vector>

namespace ui {

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    Rect inset(float d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
    Rect inset(float dx, float dy) const { return {x + dx, y + dy, w - 2 * dx, h - 2 * dy}; }
    float right() const { return x + w; }
    float bottom() const { return y + h; }
};

enum class Weight { REGULAR = 0, SEMIBOLD = 1, BOLD = 2 };
enum class Align { LEFT, CENTER, RIGHT };

class Renderer {
public:
    static bool init(const std::string& regular, const std::string& semibold, const std::string& bold);
    // windowHeight = real height of the OpenGL window (0 = same as height). It is smaller than the layout height only on
    // very small screens; the layout is then anchored to the top-left corner of the window.
    static void beginFrame(int width, int height, int windowHeight = 0, float pixelScale = 1.0f);
    static float pixelScale();
    static int width();
    static int height();
    static int windowHeight();

    static void rect(const Rect& r, const Color& c);
    static void roundRect(const Rect& r, float radius, const Color& c);
    static void roundRectOutline(const Rect& r, float radius, const Color& c, float thickness = 1.0f);
    static void shadow(const Rect& r, float radius, float spread = 10.0f, float strength = 1.0f);
    static void circle(float cx, float cy, float radius, const Color& c, int segments = 40);
    static void ring(float cx, float cy, float r0, float r1, float a0, float a1, const Color& c);   // donut slice
    static void line(float x0, float y0, float x1, float y1, const Color& c, float width = 1.0f);
    static void polyline(const std::vector<float>& xy, const Color& c, float width = 1.0f);
    static void triangle(float x0, float y0, float x1, float y1, float x2, float y2, const Color& c);
    static void hgradient(const Rect& r, const Color& left, const Color& right);
    static void vgradient(const Rect& r, const Color& top, const Color& bottom);

    // text
    static float text(float x, float baselineY, const std::string& s, float size, const Color& c,
                      Weight w = Weight::REGULAR);
    static void textIn(const Rect& r, const std::string& s, float size, const Color& c, Weight w = Weight::REGULAR,
                       Align align = Align::LEFT);   // vertically centred, clipped with ellipsis
    static float measure(const std::string& s, float size, Weight w = Weight::REGULAR);
    static std::string ellipsize(const std::string& s, float maxWidth, float size, Weight w = Weight::REGULAR);
    static std::vector<std::string> wrap(const std::string& s, float maxWidth, float size, Weight w = Weight::REGULAR);
    static float lineHeight(float size) { return size * 1.45f; }
    static bool fontsLoaded();

    // clipping (scissor stack)
    static void pushClip(const Rect& r);
    static void popClip();
};

} // namespace ui
