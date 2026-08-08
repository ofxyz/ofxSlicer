#pragma once

#include "ofMain.h"
#include "ofxSlicer.h"

class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void keyPressed(int key) override;

private:
	ofxSlicer slicer;
	ofEasyCam cam;
	int viewLayer {0};
	std::string status;
};
