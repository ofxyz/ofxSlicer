//
//  ofxSlicer.cpp
//
//
//  Created by Frikk H Fossdal on 07.05.2018.
//

#include "ofxSlicer.h"
#include <stdexcept>
#include <utility>
#include <cfloat>

ofxSlicer::ofxSlicer(){
    layerHeight = 0.01;
    sliceFinished = false;
    hasModel = false;
    isActive = false;
}
void ofxSlicer::loadFile(std::string _pathToFile){
    //Try/catch not working. Fix it!
    try {
        model.loadModel(_pathToFile);
        hasModel = true;
        
    } catch (std::exception& e) {
        ofLogError("ofxSlicer") << "load failed. Check if file exists at given location: " << e.what();
        hasModel = false; 
    }
}

void ofxSlicer::buildTriangles(){
    int meshIndex = 0;
    ofMesh mesh = model.getMesh(meshIndex);
    
    // Only apply the mesh-node matrix (internal scene-graph transform).
    // Do NOT apply model.getModelMatrix() -- it contains OF-specific
    // transforms (180-deg Z rotation + screen-pixel normalization)
    // that distort the raw geometry we need for slicing.
    ofMatrix4x4 meshMatrix = model.getMeshHelper(meshIndex).matrix;
    
    for(size_t i = 0; i < mesh.getNumVertices(); i++){
        auto vert = mesh.getVertices()[i];
        vert = meshMatrix.preMult(ofVec3f(vert));
        mesh.setVertex(i, vert);
    }

    if (mesh.getNumIndices() > 0) {
        // Indexed mesh: build triangles from index buffer
        for (size_t i = 0; i + 2 < mesh.getNumIndices(); i += 3) {
            ofVec3f v0 = mesh.getVertex(mesh.getIndex(i));
            ofVec3f v1 = mesh.getVertex(mesh.getIndex(i + 1));
            ofVec3f v2 = mesh.getVertex(mesh.getIndex(i + 2));
            allTriangles.push_back(Triangles(v0, v1, v2));
        }
    } else {
        // Non-indexed mesh (common for STL files): consecutive groups of 3 vertices
        for (size_t i = 0; i + 2 < mesh.getNumVertices(); i += 3) {
            ofVec3f v0 = mesh.getVertex(i);
            ofVec3f v1 = mesh.getVertex(i + 1);
            ofVec3f v2 = mesh.getVertex(i + 2);
            allTriangles.push_back(Triangles(v0, v1, v2));
        }
    }

    ofLogNotice("ofxSlicer") << "buildTriangles: " << mesh.getNumVertices()
        << " verts, " << mesh.getNumIndices() << " indices, "
        << allTriangles.size() << " triangles";

    sortTriangles();
}
struct compareVector{
    bool operator()(Triangles &a, Triangles &b)
    {
        if(a.zMin < b.zMin){
            return true;
        }
        else{
            return false;
        }
    }
};
void ofxSlicer::sortTriangles(){
    std::sort(allTriangles.begin(), allTriangles.end(), compareVector());
    findPerim();
}
void ofxSlicer::createLayers(){
    if (layerMax <= layerMin || layerHeight <= 0) {
        ofLogWarning("ofxSlicer") << "createLayers: invalid range or layerHeight ("
            << layerMin << " -> " << layerMax << ", step " << layerHeight << ")";
        return;
    }
    int numberOfLayers = (int)((layerMax - layerMin) / layerHeight);
    static constexpr int kMaxLayers = 5000;
    if (numberOfLayers > kMaxLayers) {
        ofLogWarning("ofxSlicer") << "createLayers: clamping " << numberOfLayers
            << " layers to " << kMaxLayers << " (consider increasing layer height)";
        numberOfLayers = kMaxLayers;
    }
    // Start the first layer half a step above layerMin to avoid slicing exactly
    // at triangle boundaries where intersection calculations are degenerate.
    float startZ = layerMin + layerHeight * 0.5f;
    for (int i = 0; i < numberOfLayers; i++) {
        layers.push_back(Layer(startZ + layerHeight * i));
    }
    ofLogNotice("ofxSlicer") << "createLayers: " << layers.size()
        << " layers from z=" << startZ << " to z=" << (startZ + layerHeight * (numberOfLayers - 1));
}
void ofxSlicer::findIntersectionPoints(std::vector<Layer> &_layers){
    activeTriangles = allTriangles;
    size_t totalContours = 0;
    size_t totalSegments = 0;
    for(size_t l = 0; l < _layers.size(); l++){
        for(auto t = activeTriangles.begin(); t != activeTriangles.end();){
            if(t->zMax < _layers[l].layerHeight){
                t = activeTriangles.erase(t);
            }
            else if(t->zMin > _layers[l].layerHeight){
                ++t;
            }
            else{
                intersectionCalc(t->points[0], t->points[1], t->points[2], _layers[l]);
                ++t;
            }
        }
        totalSegments += _layers[l].segments.size();
        createContours(_layers[l]);
        totalContours += _layers[l].contours.size();
    }
    ofLogNotice("ofxSlicer") << "findIntersectionPoints: " << _layers.size()
        << " layers, " << totalSegments << " segments, " << totalContours << " contours";
}

void ofxSlicer::findPerim(){
    if (allTriangles.empty()) {
        layerMin = 0;
        layerMax = 0;
        ofLogWarning("ofxSlicer") << "findPerim: no triangles, nothing to slice";
        return;
    }
    Triangles lastTriangle = allTriangles.back();
    Triangles firstTriangle = allTriangles.front();
    layerMax = lastTriangle.zMax;
    layerMin = firstTriangle.zMin;
    ofLogNotice("ofxSlicer") << "findPerim: zMin=" << layerMin << " zMax=" << layerMax;
}
void ofxSlicer::showAssimpModel(){
    ofSetColor(255, 15);
    model.drawWireframe();
}
void ofxSlicer::showIntersections(int _layer){
    layers[_layer].showIntersections();
}

void ofxSlicer::showSegments(int _layer){
    layers[_layer].show(); 
    //draw all segments for each layer
}
void ofxSlicer::showTriangles(){
    for(auto it = allTriangles.begin(); it != allTriangles.end(); it++)
    {
        ofSetColor(255, 0, 0, 15);
        ofDrawTriangle(it->points[0].x, it->points[0].y, it->points[0].z, it->points[1].x, it->points[1].y, it->points[1].z, it->points[2].x, it->points[2].y, it->points[2].z);
    }
}
void ofxSlicer::cleanSlicer(){
    allTriangles.clear();
    activeTriangles.clear(); 
    layers.clear();
}
void ofxSlicer::intersectionCalc(ofVec3f &p0, ofVec3f&p1, ofVec3f &p2, Layer &currentLayer){
    std::vector<ofVec3f> intersectionPoints;
    //t for P0P1
    float t0 = (currentLayer.layerHeight - p0.z) / (p1.z-p0.z);
    //t for P0P2
    float t1 = (currentLayer.layerHeight - p0.z) / (p2.z-p0.z);
    //t for P1P2
    float t2 = (currentLayer.layerHeight - p1.z) / (p2.z-p1.z);
    
    //test t values and calculate coordinates for intersection
    if(t0 < 1 && t0 > 0){
        float x = p0.x + t0*(p1.x-p0.x);
        float y = p0.y + t0*(p1.y-p0.y);
        intersectionPoints.push_back(ofVec3f(x,y,currentLayer.layerHeight));
        currentLayer.intersectionpoints.push_back(ofVec3f(x,y,currentLayer.layerHeight));
    }
    if(t1 < 1 && t1 > 0){
        float x = p0.x + t1*(p2.x-p0.x);
        float y = p0.y + t1*(p2.y-p0.y);
        intersectionPoints.push_back(ofVec3f(x,y,currentLayer.layerHeight));
        currentLayer.intersectionpoints.push_back(ofVec3f(x,y,currentLayer.layerHeight));
    }
    if(t2 < 1 && t2 > 0){
        float x = p1.x + t2*(p2.x-p1.x);
        float y = p1.y + t2*(p2.y-p1.y);
        intersectionPoints.push_back(ofVec3f(x,y,currentLayer.layerHeight));
        currentLayer.intersectionpoints.push_back(ofVec3f(x,y,currentLayer.layerHeight));
    }
    //Create a polyline segment
    if(intersectionPoints.size() > 1){
        ofPolyline line;
        line.begin();
        line.addVertex(intersectionPoints[0].x, intersectionPoints[0].y,intersectionPoints[0].z);
        line.addVertex(intersectionPoints[1].x, intersectionPoints[1].y,intersectionPoints[1].z);
        line.end();
        currentLayer.segments.push_back(line);
    }
}

//Creates contour from intersection points
//review this function and its sub-function. Something is fishy...
void ofxSlicer::createContours(Layer &currentLayer){
    //create the an initial hash table
    typedef std::pair<ofVec3f, ofVec3f> vec_pair;
    std::map<vec2key, vec_pair> hash;
    
    for(auto s = currentLayer.segments.begin(); s != currentLayer.segments.end(); s++){
        //fill the hash table with segments and one blank space seg(u,v) -> hash(key = u, value {v, *} and  hash(key = v, value {u, *}
        ofPolyline q = *s;
        if(glm::distance(glm::vec3(q[0]), glm::vec3(q[1])) > 0.0001f){
            insertHash(hash, q[0], q[1]);
            insertHash(hash, q[1], q[0]);
        }
    }
    //loop through hash and build contours
    while(!hash.empty()){
        std::vector<ofVec3f> newContour = startLoop(hash);
        addToLoop(newContour, hash);
        
        if (newContour.size() < 3) continue; // skip degenerate fragments
        
        ofPolyline p;
        for(const auto& v : newContour){
            p.addVertex(ofVec3f(v.x, v.y, v.z));
        }
        
        // If the walker closed the loop, the last vertex duplicates the first.
        // Remove the duplicate and mark the polyline as closed instead.
        if (p.size() >= 2) {
            auto& verts = p.getVertices();
            if (glm::distance(glm::vec3(verts.front()), glm::vec3(verts.back())) < 0.001f) {
                verts.pop_back();
            }
            p.close();
        }
        
        currentLayer.contours.push_back(p);
    }
}
void ofxSlicer::insertHash(std::map<vec2key,std::pair<ofVec3f, ofVec3f>> &_hash, ofVec3f u, ofVec3f v){
    auto search = _hash.find(vec2key(u.x, u.y,u.z));
    if(search == _hash.end()){
        //key does not exist. Make it
        _hash.insert(std::make_pair(vec2key(u.x,u.y,u.z), std::make_pair(v, ofVec3f(0))));
    }
    else{
        //key exists, add second point to hash with current index
        (*search).second.second = v;
    }
}
std::vector<ofVec3f> ofxSlicer::startLoop(std::map<vec2key, std::pair<ofVec3f, ofVec3f> > &_hash){
    std::vector<ofVec3f> p;
    auto it = _hash.begin();
    p.push_back(ofVec3f(it->first.x,it->first.y,it->first.z));
    p.push_back(ofVec3f(it->second.first.x,it->second.first.y,it->second.first.z));
    _hash.erase(it);
    return p;
}

void ofxSlicer::addToLoop(std::vector<ofVec3f> &p ,std::map<vec2key, std::pair<ofVec3f, ofVec3f> > &_hash){
    ofVec3f first = p.front();
    ofVec3f current = p.back();
    
    while(true){
        auto it = _hash.find(vec2key(current.x, current.y, current.z));
        if(it == _hash.end()){
            break;
        }
        
        ofVec3f n1 = it->second.first;
        ofVec3f n2 = it->second.second;
        
        // Determine previous vertex so we don't backtrack
        ofVec3f prev = (p.size() >= 2) ? p[p.size() - 2] : ofVec3f(FLT_MAX);
        
        // Choose the neighbor that is NOT the vertex we came from
        ofVec3f next;
        if (n1 == prev) {
            next = n2;
        } else {
            next = n1;
        }
        
        _hash.erase(it);
        
        // Sentinel check: uninitialized second neighbor is ofVec3f(0).
        // Since all intersection points have z == layerHeight != 0,
        // the sentinel (0,0,0) will never collide with a valid point.
        static const ofVec3f kSentinel(0, 0, 0);
        if (next == kSentinel) {
            break;
        }
        
        p.push_back(next);
        
        if (next == first) {
            break;
        }
        current = next;
    }
}

// ---------------------THREADING-------------------------
void ofxSlicer::startSlice(){
    startThread();
    isActive = true;
}
void ofxSlicer::stopSlice(){
    stopThread();
    isActive = false; 
}
void ofxSlicer::threadedFunction(){
    while(isThreadRunning())
    {
        sliceFinished = false;
        currentTask = "slicer initiating";
        ofLogNotice("ofxSlicer") << "slicer thread started";
        //Do slicing and put information into each layer
        currentTask = "cleaning memory";
        cleanSlicer();
        currentTask = "building triangles";
        buildTriangles();
        currentTask = "creating layers";
        createLayers();
        currentTask = "locating intersection points";
        findIntersectionPoints(layers);
        stopSlice();
        sliceFinished = true;
        ofLogNotice("ofxSlicer") << "slicing complete";
        currentTask = ""; 
        //Run slicer animation and update relevant GUI.
    }
}




