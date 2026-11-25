// =====================================================================
// Charts.h - 2-D charts and the 3-D bar chart
//
// [CO-EVIDENCE] CG-CO2: primitives for bars, polylines, filled areas, donut (circle) slices
// [CO-EVIDENCE] CG-CO3/CO5: dynamic graph viewport (glViewport per chart card), 3-D object
//               manipulation (rotation / scaling / translation of the bar grid), perspective
//               camera (gluPerspective + gluLookAt), depth test, gluProject for labels
// [CO-EVIDENCE] CG-CO6 (preview): GL_LIGHTING with a positional light + material colours
// =====================================================================
#pragma once
#include "../UI/Renderer.h"
#include <string>
#include <vector>

namespace charts {

void barChart(const ui::Rect& r, const std::vector<std::string>& labels, const std::vector<double>& values,
              const ui::Color& color, bool rotateLabels = false);
void hbarChart(const ui::Rect& r, const std::vector<std::string>& labels, const std::vector<double>& values,
               const std::vector<ui::Color>& colors, bool showPercent = false);
void donut(const ui::Rect& r, const std::vector<std::string>& labels, const std::vector<double>& values,
           const std::vector<ui::Color>& colors, const std::string& centerLabel);
void lineChart(const ui::Rect& r, const std::vector<std::string>& labels, const std::vector<double>& a,
               const std::vector<double>& b, const std::string& nameA, const std::string& nameB);
double niceMax(double v);

class Chart3D {
public:
    float yaw = -38, pitch = 32, distance = 1.0f;
    bool dragging = false;
    float lastX = 0, lastY = 0;
    bool autoRotate = true;
    // rows x cols grid of values; rowLabels along depth, colLabels along width
    void draw(const ui::Rect& r, const std::vector<std::string>& rowLabels, const std::vector<std::string>& colLabels,
              const std::vector<std::vector<int>>& values);
    void handleMouse(const ui::Rect& r);
};

} // namespace charts
