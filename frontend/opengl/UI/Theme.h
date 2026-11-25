// Theme.h - design tokens (colours, radius, spacing) for light and dark mode
#pragma once

namespace ui {

struct Color {
    float r, g, b, a;
    Color withAlpha(float na) const { return {r, g, b, na}; }
    Color mix(const Color& o, float t) const {
        return {r + (o.r - r) * t, g + (o.g - g) * t, b + (o.b - b) * t, a + (o.a - a) * t};
    }
};
Color hex(unsigned rgb, float alpha = 1.0f);

struct Palette {
    Color bg, surface, surfaceAlt, sidebar, border, borderStrong, text, textMuted, textFaint;
    Color primary, primarySoft, primaryText, success, successSoft, warning, warningSoft, danger, dangerSoft,
          info, infoSoft, neutralSoft, shadow, overlay, inputBg, hover, selected;
    Color mapNone, mapLow, mapMedium, mapHigh, mapBorder, mapStateBorder, water;
    Color chart[8];
};

class Theme {
public:
    static const Palette& p();
    static bool dark();
    static void setDark(bool d);
    static void toggle();

    // spacing / shape tokens (px)
    static constexpr float radius = 10.0f, radiusSm = 7.0f, radiusLg = 14.0f;
    static constexpr float gap = 16.0f, pad = 24.0f;
    static constexpr float sidebarW = 248.0f, topbarH = 64.0f;
};

} // namespace ui
