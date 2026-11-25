// =====================================================================
// IndiaMap.h - native OpenGL India map from real district geometry
//
// Data  : opengl/data/india.map (generated from Census district polygons by
//         tools/convert_geojson.py). 36 states/UTs, 758 district parts, derived state borders.
// [CO-EVIDENCE] CG-CO2: polygon filling via triangulation (ear clipping), lines, colour
// [CO-EVIDENCE] CG-CO3/CO5: 2-D transformations (translate + scale for pan/zoom, glPushMatrix),
//               window-to-viewport mapping, Cohen-Sutherland line clipping of borders,
//               scissor clipping, picking with inverse transformation + point-in-polygon
// =====================================================================
#pragma once
#include "../UI/Renderer.h"
#include <string>
#include <vector>

namespace map {

struct Pt { float x, y; };

struct District {
    int state = 0;
    std::string name;                       // "~" = unnamed fragment
    std::vector<std::vector<Pt>> rings;     // projected world coordinates
    std::vector<Pt> triangles;              // 3 points per triangle (ear clipping output)
    float minX, minY, maxX, maxY;
    Pt centroid{0, 0};
    float area = 0;
};

struct State {
    std::string name;
    std::vector<int> districts;
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    Pt labelAt{0, 0};
    float area = 0;
};

class IndiaMap {
public:
    void load(const std::string& path);     // throws scms::MapDataException
    bool loaded() const { return !districts_.empty(); }

    // view transform (world -> screen):  s = viewport.origin + offset + world * scale
    void setViewport(const ui::Rect& r);
    void resetView();
    void zoomAt(float sx, float sy, float factor);
    void pan(float dx, float dy);
    float zoom() const { return zoom_; }
    Pt toScreen(float wx, float wy) const;
    Pt toWorld(float sx, float sy) const;
    Pt project(double lon, double lat) const;     // geographic -> world

    int pickDistrict(float sx, float sy) const;   // -1 when outside India

    // stateFill[i] = colour of state i; hover/selected indices or -1
    void render(const std::vector<ui::Color>& stateFill, int hoverState, int selectedState, int hoverDistrict,
                int selectedDistrict, const std::vector<ui::Color>* districtFill = nullptr);
    void renderLabels(const std::vector<std::string>& extra, int selectedState);

    const std::vector<State>& states() const { return states_; }
    const std::vector<District>& districts() const { return districts_; }
    int stateIndex(const std::string& name) const;
    int districtIndex(int state, const std::string& name) const;
    std::size_t triangleCount() const { return triCount_; }
    std::size_t lastClippedSegments() const { return clipped_; }

    // exposed for unit tests
    static bool clipLine(float& x0, float& y0, float& x1, float& y1, float xmin, float ymin, float xmax, float ymax);
    static std::vector<Pt> earClip(const std::vector<Pt>& ring);
    static bool pointInRing(const std::vector<Pt>& ring, float x, float y);

private:
    std::vector<State> states_;
    std::vector<District> districts_;
    std::vector<std::vector<Pt>> borders_;
    float wMinX_ = 0, wMinY_ = 0, wMaxX_ = 1, wMaxY_ = 1;
    ui::Rect vp_;
    float baseScale_ = 1, zoom_ = 1, offX_ = 0, offY_ = 0;
    std::size_t triCount_ = 0;
    mutable std::size_t clipped_ = 0;
};

} // namespace map
