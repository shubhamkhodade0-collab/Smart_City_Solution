#include "Theme.h"

namespace ui {

Color hex(unsigned rgb, float alpha) {
    return {((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, alpha};
}

static Palette makeLight() {
    Palette p{};
    p.bg = hex(0xF5F6F8); p.surface = hex(0xFFFFFF); p.surfaceAlt = hex(0xF9FAFB); p.sidebar = hex(0xFFFFFF);
    p.border = hex(0xE6E8EC); p.borderStrong = hex(0xD0D5DD);
    p.text = hex(0x101828); p.textMuted = hex(0x5D6676); p.textFaint = hex(0x98A2B3);
    p.primary = hex(0x4F46E5); p.primarySoft = hex(0xEEF0FF); p.primaryText = hex(0x3730A3);
    p.success = hex(0x0E9F6E); p.successSoft = hex(0xE7F7F0);
    p.warning = hex(0xDC8A0C); p.warningSoft = hex(0xFEF4E2);
    p.danger = hex(0xE5484D); p.dangerSoft = hex(0xFDECEC);
    p.info = hex(0x2E78D2); p.infoSoft = hex(0xE8F1FC);
    p.neutralSoft = hex(0xF0F2F5);
    p.shadow = hex(0x101828, 0.06f); p.overlay = hex(0x0B1220, 0.45f);
    p.inputBg = hex(0xFFFFFF); p.hover = hex(0xF3F4F7); p.selected = hex(0xEEF0FF);
    p.mapNone = hex(0xE9ECF1); p.mapLow = hex(0x5CC08F); p.mapMedium = hex(0xF2B23A); p.mapHigh = hex(0xE5484D);
    p.mapBorder = hex(0xFFFFFF, 0.55f); p.mapStateBorder = hex(0x475467, 0.85f); p.water = hex(0xF5F6F8);
    const unsigned c[8] = {0x4F46E5, 0x0E9F6E, 0xDC8A0C, 0x2E78D2, 0xE5484D, 0x8B5CF6, 0x14B8A6, 0x64748B};
    for (int i = 0; i < 8; ++i) p.chart[i] = hex(c[i]);
    return p;
}

static Palette makeDark() {
    Palette p{};
    p.bg = hex(0x0B0F17); p.surface = hex(0x131A26); p.surfaceAlt = hex(0x172030); p.sidebar = hex(0x0F1520);
    p.border = hex(0x232D3D); p.borderStrong = hex(0x34405A);
    p.text = hex(0xE6E9EF); p.textMuted = hex(0x9AA4B5); p.textFaint = hex(0x667085);
    p.primary = hex(0x7C78F2); p.primarySoft = hex(0x232552); p.primaryText = hex(0xC7C5FF);
    p.success = hex(0x34C38F); p.successSoft = hex(0x12302A);
    p.warning = hex(0xF0B04A); p.warningSoft = hex(0x3A2C12);
    p.danger = hex(0xF26B70); p.dangerSoft = hex(0x3B1A1E);
    p.info = hex(0x5AA2F0); p.infoSoft = hex(0x152A44);
    p.neutralSoft = hex(0x1B2433);
    p.shadow = hex(0x000000, 0.35f); p.overlay = hex(0x000000, 0.6f);
    p.inputBg = hex(0x0F1520); p.hover = hex(0x1A2332); p.selected = hex(0x232552);
    p.mapNone = hex(0x222C3C); p.mapLow = hex(0x2FA877); p.mapMedium = hex(0xD99A2B); p.mapHigh = hex(0xE0555A);
    p.mapBorder = hex(0x0B0F17, 0.5f); p.mapStateBorder = hex(0xCBD5E1, 0.7f); p.water = hex(0x0B0F17);
    const unsigned c[8] = {0x7C78F2, 0x34C38F, 0xF0B04A, 0x5AA2F0, 0xF26B70, 0xA78BFA, 0x2DD4BF, 0x94A3B8};
    for (int i = 0; i < 8; ++i) p.chart[i] = hex(c[i]);
    return p;
}

static bool g_dark = false;

const Palette& Theme::p() {
    static const Palette light = makeLight(), darkP = makeDark();
    return g_dark ? darkP : light;
}
bool Theme::dark() { return g_dark; }
void Theme::setDark(bool d) { g_dark = d; }
void Theme::toggle() { g_dark = !g_dark; }

} // namespace ui
