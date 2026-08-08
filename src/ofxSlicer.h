//
//  ofxSlicer.h
//
//
//  Created by Frikk H Fossdal on 07.05.2018.
//
#include "ofMain.h"
#include "Layer.hpp"
#include "Triangles.hpp"
#include "ofxAssimpModelLoader.h"
#include "vec2key.hpp"
#include <map>
#include <string>


class ofxSlicer : public ofThread{
public:
    //constructor
    ofxSlicer();
    
    //methods
    void slice();
    void loadFile(std::string _pathToFile);
    void buildTriangles();
    void showAssimpModel();
    void showSegments(int _layer);
    void showIntersections(int _layer);
    void showTriangles();
    void cleanSlicer(); 
    void startSlice();
    void stopSlice();
    void threadedFunction();
    
    //variables
    float layerHeight;
    float layerMin;
    float layerMax;
    bool sliceFinished;
    bool isActive;
    bool hasModel;
    bool abortFlag;
    int currentProcessingLayer;
    std::string currentTask; 
    ofxAssimpModelLoader model;
    std::vector<Triangles> allTriangles;
    std::vector<Triangles> activeTriangles;
    std::vector<Layer> layers;
private:
    void sortTriangles();
    void createLayers();
    void findPerim();
    void findIntersectionPoints(std::vector<Layer> &_layers);
    void findJobs(std::vector<Layer> _layers);
    void intersectionCalc(ofVec3f &p0, ofVec3f &p1, ofVec3f &p2, Layer &currentLayer);
    void createContours(Layer &currentLayer);
    void insertHash(std::map<vec2key,std::pair<ofVec3f, ofVec3f>> &_hash, ofVec3f v, ofVec3f u);
    std::vector<ofVec3f> startLoop(std::map<vec2key, std::pair<ofVec3f, ofVec3f>> &_hash);
    void addToLoop(std::vector<ofVec3f> &_currentContour, std::map<vec2key, std::pair<ofVec3f, ofVec3f>> &_hash);
    
};

