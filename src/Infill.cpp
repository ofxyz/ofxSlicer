//
//  Infill.cpp
//  Infill pattern generator for sliced layers.
//

#include "Infill.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

//--------------------------------------------------------------
// Scanline intersection: find values along the cross-axis where
// a scanline at `lineVal` along the scan-axis crosses the polygon.
//--------------------------------------------------------------
static std::vector<float> findIntersections(
    const std::vector<glm::vec3>& verts,
    float lineVal,
    int scanAxis)
{
    std::vector<float> intersections;
    size_t n = verts.size();
    for (size_t i = 0; i < n; i++) {
        size_t j = (i + 1) % n;
        float v1, v2, c1, c2;
        if (scanAxis == 0) {
            v1 = verts[i].y;  v2 = verts[j].y;
            c1 = verts[i].x;  c2 = verts[j].x;
        } else {
            v1 = verts[i].x;  v2 = verts[j].x;
            c1 = verts[i].y;  c2 = verts[j].y;
        }
        if ((v1 <= lineVal && v2 > lineVal) || (v2 <= lineVal && v1 > lineVal)) {
            float t = (lineVal - v1) / (v2 - v1);
            intersections.push_back(c1 + t * (c2 - c1));
        }
    }
    return intersections;
}

//--------------------------------------------------------------
// Signed area of a 2D polygon (positive = CCW, negative = CW)
//--------------------------------------------------------------
static float signedArea2D(const std::vector<glm::vec3>& verts) {
    float area = 0;
    size_t n = verts.size();
    for (size_t i = 0; i < n; i++) {
        size_t j = (i + 1) % n;
        area += verts[i].x * verts[j].y;
        area -= verts[j].x * verts[i].y;
    }
    return area * 0.5f;
}

//--------------------------------------------------------------
// Scanline fill helper (used by Rectilinear and Grid)
//--------------------------------------------------------------
std::vector<ofPolyline> Infill::scanlineFill(const ofPolyline& contour, float layerZ,
                                              float spacing, bool horizontal)
{
    std::vector<ofPolyline> fillLines;
    auto& verts = contour.getVertices();
    if (verts.size() < 3 || spacing <= 0.0f) return fillLines;

    float minV = FLT_MAX, maxV = -FLT_MAX;
    for (const auto& v : verts) {
        float val = horizontal ? v.y : v.x;
        minV = std::min(minV, val);
        maxV = std::max(maxV, val);
    }

    int scanAxis = horizontal ? 0 : 1;
    for (float lineVal = minV + spacing; lineVal < maxV; lineVal += spacing) {
        auto intersections = findIntersections(verts, lineVal, scanAxis);
        std::sort(intersections.begin(), intersections.end());
        for (size_t i = 0; i + 1 < intersections.size(); i += 2) {
            ofPolyline line;
            if (horizontal) {
                line.addVertex(intersections[i], lineVal, layerZ);
                line.addVertex(intersections[i + 1], lineVal, layerZ);
            } else {
                line.addVertex(lineVal, intersections[i], layerZ);
                line.addVertex(lineVal, intersections[i + 1], layerZ);
            }
            fillLines.push_back(line);
        }
    }
    return fillLines;
}

//--------------------------------------------------------------
// Rectilinear: alternate H/V per layer
//--------------------------------------------------------------
std::vector<ofPolyline> Infill::generateRectilinear(const ofPolyline& contour, int layerIndex, float layerZ) {
    float angle = baseAngle + (layerIndex % 2) * 90.0f;
    bool horizontal = ((int)std::round(angle) % 180 == 0);
    return scanlineFill(contour, layerZ, lineSpacing, horizontal);
}

//--------------------------------------------------------------
// Grid: both H and V on every layer (cross-hatch)
//--------------------------------------------------------------
std::vector<ofPolyline> Infill::generateGrid(const ofPolyline& contour, float layerZ) {
    auto hLines = scanlineFill(contour, layerZ, lineSpacing, true);
    auto vLines = scanlineFill(contour, layerZ, lineSpacing, false);
    hLines.insert(hLines.end(), vLines.begin(), vLines.end());
    return hLines;
}

//--------------------------------------------------------------
// Concentric: inset contour repeatedly
//--------------------------------------------------------------
std::vector<ofPolyline> Infill::generateConcentric(const ofPolyline& contour, float layerZ) {
    std::vector<ofPolyline> fills;
    ofPolyline current = contour;

    for (int shell = 0; shell < 500; shell++) {
        ofPolyline inset = offsetPolygon(current, lineSpacing, layerZ);
        if (inset.size() < 3) break;
        fills.push_back(inset);
        current = inset;
    }
    return fills;
}

//--------------------------------------------------------------
// Offset a closed polygon inward by `distance`.
//--------------------------------------------------------------
ofPolyline Infill::offsetPolygon(const ofPolyline& poly, float distance, float z) {
    auto& verts = poly.getVertices();
    size_t n = verts.size();
    if (n < 3) return ofPolyline();

    // Determine winding: positive area = CCW
    float area = signedArea2D(verts);
    if (std::abs(area) < 1e-6f) return ofPolyline();

    // For CCW polygons, inward offset uses the left-hand normal.
    // For CW polygons, inward offset uses the right-hand normal.
    float sign = (area > 0) ? 1.0f : -1.0f;

    ofPolyline result;
    for (size_t i = 0; i < n; i++) {
        size_t prev = (i + n - 1) % n;
        size_t next = (i + 1) % n;

        glm::vec2 e0 = glm::vec2(verts[i].x - verts[prev].x, verts[i].y - verts[prev].y);
        glm::vec2 e1 = glm::vec2(verts[next].x - verts[i].x, verts[next].y - verts[i].y);

        float len0 = glm::length(e0);
        float len1 = glm::length(e1);
        if (len0 < 1e-8f || len1 < 1e-8f) continue;
        e0 /= len0;
        e1 /= len1;

        // Inward normals (rotate 90 degrees)
        glm::vec2 n0(-e0.y * sign, e0.x * sign);
        glm::vec2 n1(-e1.y * sign, e1.x * sign);

        // Bisector direction
        glm::vec2 bisector = n0 + n1;
        float bisLen = glm::length(bisector);
        if (bisLen < 1e-8f) continue;
        bisector /= bisLen;

        // Scale so the actual perpendicular distance from each edge is `distance`
        float dot = glm::dot(bisector, n0);
        if (std::abs(dot) < 0.01f) continue; // nearly parallel edges (spike)
        float offset = distance / dot;

        // Clamp to avoid extreme spikes on sharp angles
        float maxOffset = distance * 4.0f;
        offset = std::clamp(offset, -maxOffset, maxOffset);

        result.addVertex(verts[i].x + bisector.x * offset,
                         verts[i].y + bisector.y * offset,
                         z);
    }

    if (result.size() < 3) return ofPolyline();

    // Validate: inset polygon area must be smaller than original and same sign
    float newArea = signedArea2D(result.getVertices());
    if ((area > 0 && newArea <= 0) || (area < 0 && newArea >= 0)) {
        return ofPolyline(); // polygon collapsed or inverted
    }
    if (std::abs(newArea) > std::abs(area)) {
        return ofPolyline(); // expanded instead of shrunk (shouldn't happen)
    }

    result.close();
    return result;
}

//--------------------------------------------------------------
// Generate perimeter shells
//--------------------------------------------------------------
std::vector<ofPolyline> Infill::generatePerimeters(const ofPolyline& contour, float layerZ,
                                                    ofPolyline& innerContour)
{
    std::vector<ofPolyline> perimeters;
    ofPolyline current = contour;

    for (int i = 0; i < perimeterCount; i++) {
        if (i == 0) {
            // First perimeter is the original contour itself
            perimeters.push_back(current);
        } else {
            ofPolyline inset = offsetPolygon(current, nozzleWidth, layerZ);
            if (inset.size() < 3) break;
            perimeters.push_back(inset);
            current = inset;
        }
    }

    // The innermost perimeter (or original if perimeterCount <= 1)
    // offset by one more nozzle width for infill boundary
    if (perimeterCount > 0) {
        innerContour = offsetPolygon(current, nozzleWidth * 0.5f, layerZ);
        if (innerContour.size() < 3) {
            innerContour = current; // fallback to last perimeter
        }
    } else {
        innerContour = contour;
    }

    return perimeters;
}

//--------------------------------------------------------------
// Main entry point: dispatch based on pattern
//--------------------------------------------------------------
std::vector<ofPolyline> Infill::generate(const ofPolyline& contour, int layerIndex, float layerZ) {
    auto& verts = contour.getVertices();
    if (verts.size() < 3) return {};
    if (lineSpacing <= 0.0f) return {};

    switch (pattern) {
        case InfillPattern::Rectilinear:
            return generateRectilinear(contour, layerIndex, layerZ);
        case InfillPattern::Grid:
            return generateGrid(contour, layerZ);
        case InfillPattern::Concentric:
            return generateConcentric(contour, layerZ);
        case InfillPattern::None:
        default:
            return {};
    }
}

//--------------------------------------------------------------
void Infill::setDensity(float percent) {
    percent = std::clamp(percent, 0.0f, 100.0f);
    if (percent < 1.0f) {
        lineSpacing = 999999.0f; // effectively no infill
        return;
    }
    lineSpacing = nozzleWidth / (percent / 100.0f);
}
