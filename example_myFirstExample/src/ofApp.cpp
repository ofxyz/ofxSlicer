#include "ofApp.h"

#include <algorithm>

void ofApp::setup()
{
	ofBackground(40);
	ofSetWindowTitle("ofxSlicer — cube slice example");
	ofEnableDepthTest();

	slicer.layerHeight = 1.0f;
	slicer.loadFile(ofToDataPath("cube.obj", true));
	if (!slicer.hasModel) {
		status = "Failed to load bin/data/cube.obj";
		ofLogError("ofxSlicer") << status;
		return;
	}

	status = "Slicing cube.obj …";
	slicer.buildTriangles();
	slicer.slice(); // synchronous for the sample mesh
	status = "Layers: " + ofToString((int)slicer.layers.size())
		+ "  |  arrows = layer, R = re-slice";
	viewLayer = 0;
	ofLogNotice("ofxSlicer") << status;
}

void ofApp::update() {}

void ofApp::draw()
{
	ofDrawBitmapStringHighlight(status, 12, 20);

	cam.begin();
	ofSetColor(180, 180, 200, 40);
	slicer.showAssimpModel();

	if (!slicer.layers.empty()) {
		const int maxL = (int)slicer.layers.size() - 1;
		viewLayer = std::max(0, std::min(viewLayer, maxL));
		ofSetColor(80, 220, 120);
		slicer.showSegments(viewLayer);
		ofSetColor(255, 120, 80);
		slicer.showIntersections(viewLayer);
	}
	cam.end();

	if (!slicer.layers.empty()) {
		ofDrawBitmapStringHighlight(
			"layer " + ofToString(viewLayer + 1) + " / " + ofToString((int)slicer.layers.size()),
			12, 44);
	}
}

void ofApp::keyPressed(int key)
{
	if (key == OF_KEY_RIGHT || key == ']') {
		++viewLayer;
	} else if (key == OF_KEY_LEFT || key == '[') {
		--viewLayer;
	} else if (key == 'r' || key == 'R') {
		if (slicer.hasModel) {
			slicer.cleanSlicer();
			slicer.buildTriangles();
			slicer.slice();
			status = "Re-sliced — layers: " + ofToString((int)slicer.layers.size());
			viewLayer = 0;
		}
	}
	if (!slicer.layers.empty()) {
		const int maxL = (int)slicer.layers.size() - 1;
		viewLayer = std::max(0, std::min(viewLayer, maxL));
	}
}
