#pragma once
//
//  Infill.h
//  Infill pattern generator for sliced layers.
//  Supports rectilinear, grid, concentric, and no-fill patterns,
//  plus perimeter shell generation via polygon offsetting.
//

#include "ofMain.h"
#include <vector>

/// Available infill patterns
enum class InfillPattern {
    None,          // No infill -- perimeters only
    Rectilinear,   // Alternating H/V scanlines (default)
    Grid,          // Cross-hatch: both H and V on every layer
    Concentric     // Concentric insets of the contour
};

class Infill {
public:
    // Pattern
    InfillPattern pattern = InfillPattern::Rectilinear;

    // Spacing between infill lines (mm) -- set via setDensity()
    float lineSpacing = 2.0f;

    // Angle offset for alternating layers (degrees)
    float baseAngle = 0.0f;

    // Nozzle width (mm) -- used for perimeter offset and density calc
    float nozzleWidth = 0.4f;

    // Number of perimeter shells (walls).  0 = none, 1 = outline only, 2+ = multiple walls.
    int perimeterCount = 2;

    /// Generate infill lines for a closed contour according to the current pattern.
    /// layerIndex is used for alternation (rectilinear) and angle rotation.
    /// layerZ sets the Z height for the generated fill vertices.
    std::vector<ofPolyline> generate(const ofPolyline& contour, int layerIndex = 0, float layerZ = 0.0f);

    /// Generate perimeter shells (inward offsets of the contour).
    /// Returns perimeterCount polylines, from outermost to innermost.
    /// Also returns the innermost contour in `innerContour` for infill clipping.
    std::vector<ofPolyline> generatePerimeters(const ofPolyline& contour, float layerZ,
                                                ofPolyline& innerContour);

    /// Set infill density as a percentage (0-100).
    /// Adjusts lineSpacing based on the current nozzleWidth.
    void setDensity(float percent);

    /// Offset a closed polygon inward by `distance`.
    /// Returns an empty polyline if the polygon collapses.
    static ofPolyline offsetPolygon(const ofPolyline& poly, float distance, float z);

private:
    std::vector<ofPolyline> generateRectilinear(const ofPolyline& contour, int layerIndex, float layerZ);
    std::vector<ofPolyline> generateGrid(const ofPolyline& contour, float layerZ);
    std::vector<ofPolyline> generateConcentric(const ofPolyline& contour, float layerZ);

    /// Scanline fill helper: clip a set of scanlines against a contour.
    static std::vector<ofPolyline> scanlineFill(const ofPolyline& contour, float layerZ,
                                                 float spacing, bool horizontal);
};
