# ofxSlicer

Slices mesh geometry into layers consisting of curves. Written in C++ as an addon for openFrameworks. Includes basic rectilinear infill generation and a bridge to [ofxGCode](https://github.com/ofxyz/ofxGCode) for G-code output.

![current](docs/img/currentOverview.png)


## The Slicer

The slicing algorithm goes something like this:

1. Create a list containing all triangles of the mesh model.
2. Mesh slicing: Calculate triangle intersection points on each plane.
3. Construct contours: Create polygons from the intersection points for each plane.
4. Make sense of the polygons (Clockwise/Counterclockwise).

### Getting the triangles

Getting the triangles was a bit of a struggle in openFrameworks. To import .stl files, we use the [ofxAssimpModelLoader](http://openframeworks.cc/documentation/ofxAssimpModelLoader/ofxAssimpModelLoader/) addon. It took some tweaking to get the triangle faces with their belonging vertices extracted from the assimp class. All the triangles are sorted in ascending order in terms of the lowest point in the triangle. 

> It would probably be easier to use some kind of existing C++ framework for geometry like CGAL.

### Calculate the triangle intersections

Once we have the triangles it's time to calculate the intersection points on each layer. Have a look at this figure.

![triangleInter](docs/img/triangle_slicing-01.png)

There are basically three different situations:
1. The triangle is located on the topside of the layer plane.
2. The triangle is intersecting with the plane.
3. The triangle is underneath the plane. This means that the slicer is finished processing it.


### Active triangles

To improve the speed of the algorithm the triangles that are finished processing are removed from the triangle list that is used in the calculation. This condition applies when the entire triangle is located underneath the layer plane. See figure.

![active triangles](docs/img/triangle_slicing-02.png)

### Generate Polygons

After calculating the intersection points for each layer, the slicer needs to assemble them into closed contour polygons. This is done in `createContours()` using a hash-table approach:

1. **Build an adjacency hash.** Each line segment produced by the triangle intersection step has two endpoints. Both endpoints are inserted into a hash map keyed by their XY(Z) position (`vec2key`). Each key maps to its two neighbouring vertices, forming a doubly-linked chain of edges.

2. **Walk the chains.** Starting from an arbitrary entry in the hash, the algorithm picks a direction and follows the chain of neighbours (`addToLoop`), removing visited entries as it goes. When the walk returns to the starting vertex the contour is closed.

3. **Repeat.** The process repeats until the hash map is empty, producing one `ofPolyline` per closed contour. All contours for a given layer are stored in `layer.contours`.

This approach efficiently reconstructs polygon boundaries from an unordered soup of edge segments, handling both outer perimeters and inner holes.


## Infill Generation

The `Infill` class (`Infill.h` / `Infill.cpp`) generates basic rectilinear fill lines for closed contour polygons.

```cpp
Infill infill;
infill.lineSpacing = 2.0f;          // mm between lines
// or set by density percentage:
infill.setDensity(20.0f);           // 20% infill

auto fillLines = infill.generate(contour, layerIndex);
```

- **Alternating direction**: Even layers produce horizontal lines, odd layers produce vertical lines (offset by `baseAngle`).
- **Scanline clipping**: Lines are clipped to the contour boundary using a scanline intersection algorithm.
- **Safety**: A `lineSpacing <= 0` guard prevents infinite loops.


## G-code Bridge (SlicerToGCode)

The `SlicerToGCode` class (`SlicerToGCode.h` / `SlicerToGCode.cpp`) converts slicer output directly into G-code via [ofxGCode](https://github.com/ofxyz/ofxGCode).

```cpp
SlicerToGCode bridge;
bridge.feedRate       = 600.0f;   // cutting feed rate (mm/min)
bridge.travelFeedRate = 3000.0f;  // rapid travel rate (mm/min)
bridge.safeZ          = 5.0f;     // retract height for travel moves

// Convert layers to an ofxGCode object
ofxGCode gcode = bridge.convert(slicer.layers);
gcode.save3D("output.nc", bridge.safeZ);

// Or get the G-code as a string directly
std::string gcodeStr = bridge.generateGCodeString(slicer.layers);
```

The bridge walks each layer's contours and jobs, converts them to `GLine` segments with per-line Z values, and delegates the actual G-code formatting to `ofxGCode::toGCodeString()`. Travel moves (pen-up / retract to safe Z) are inserted automatically between disconnected contours.


## How to use it

Reusing the slicer in your openFrameworks project should be pretty straightforward:

1. Clone the repo into your local addons folder.
2. Use the openFrameworks projectGenerator to include `ofxSlicer` in your project.
3. Add `ofxAssimpModelLoader` (for STL loading) and optionally `ofxGCode` (for G-code output) to your `addons.make`.
4. Create an `ofxSlicer` object, load an STL, and start slicing:

```cpp
ofxSlicer slicer;
slicer.layerHeight = 0.2;
slicer.loadFile("model.stl");
slicer.startSlice();  // runs in a background thread

// When slicer.sliceFinished == true:
for (auto& layer : slicer.layers) {
    // layer.contours  -- closed perimeter polylines
    // layer.jobs      -- additional toolpaths (infill, etc.)
}
```
