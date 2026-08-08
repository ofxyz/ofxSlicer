#pragma once
//
//  SlicerToGCode.h
//  Bridges ofxSlicer output (Layer contours) to G-code string generation.
//  Generates proper 3D G-code directly, supporting both CNC and 3D printing modes.
//

#include "Layer.hpp"
#include <vector>
#include <string>
#include <sstream>

class SlicerToGCode {
public:
    // Feed rates in mm/min
    float feedRate      = 1200.0f;   // XY cutting / printing speed
    float travelFeedRate = 3000.0f;  // Rapid travel speed
    float plungeFeedRate = 300.0f;   // Z plunge speed (CNC mode)

    // Z behavior
    float safeZ         = 5.0f;      // Safe retract height for rapid moves
    float retractHeight = 2.0f;      // Z lift above current layer for travel

    // Mode
    bool cncMode = true;  // true = CNC router (retract Z for travel)
                          // false = 3D printer (extrusion-based)

    // 3D printing parameters (only used when cncMode == false)
    float nozzleDiameter      = 0.4f;
    float filamentDiameter    = 1.75f;
    float printLayerHeight    = 0.2f;   // Layer thickness for extrusion calculation
    float extrusionMultiplier = 1.0f;

    /// Generate the complete G-code string directly from slicer layers.
    /// Produces proper 3-axis G-code with feed rates, layer comments, and
    /// optional extrusion values for 3D printing mode.
    std::string generateGCodeString(const std::vector<Layer>& layers);

private:
    /// Calculate filament length to extrude for a given XY move length.
    float computeExtrusionLength(float moveLength, float layerHeight) const;

    /// Emit G-code for a single contour or infill polyline at the given Z.
    void emitContour(std::ostringstream& out, const ofPolyline& contour,
                     float z, float& currentE, glm::vec3& currentPos, bool& atSafeZ);
};
