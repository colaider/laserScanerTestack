#pragma once
#include <string>
#include <map>
#include <queue>
#include <vector>
#include "PlyUtilities.h"

class MyMetrologyEngine {
protected:
    PlyUtilities::point** scan = nullptr;
    PlyUtilities::coloredPoint** coloredScan = nullptr;
    PlyUtilities* plyUtils = nullptr;


    float range = 0.0f;
    int   scanLineCount = 0;
    int   firstLineCount = 0;
    int   midScanIdx = 0;
    int   platoCount = 0;
    int*  lineCounts = nullptr;

    float planeN[3] = { 0, 0, 0 };
    float planeD = 0;

	float dYScaling = 1.0f;
	float blueRegion = 0.0f; float redRegion = 0.0f;

    float rotTeta = 0.0f;
	

    const double PI = 3.141592653589793;

public:
    float noPointLeft = 0.0f, noPointRight = 0.0f;

    MyMetrologyEngine(std::string PLYPath, PlyUtilities* plyUtils);
    ~MyMetrologyEngine();
    struct GrooveData { float area; float centerX; float centerY; };

    //virtual PlyUtilities::point*         getPlatosForLine(int scanIdx);
    void                         computePlane();
    virtual void                 colorByPlaneDistance();
    void                         flattenToPlane(float groveLen);
    void                         removeLowestPoints();
    virtual void alignXAxis();
    void colorEndRegions(float blueRegion, float keepRegion);
    void populate2DGroove();
	void shiftInX(float shift);
    void writeTextToCloud(std::string text, float cx, float cy);
    void removeBottomPoints(float percent);


    int                          getPlatoCount() { return this->platoCount; }
    std::vector<float>           getRedAreas(float minArea = 1.0f);
    std::vector<GrooveData> getGrooveAreas(float minArea = 1.0f);
    std::vector<GrooveData> getHoleAreas(float minArea = 1.0f);
    PlyUtilities::coloredPoint** getCloloredScan() { return coloredScan; }
	PlyUtilities * getPlyUtils() { return plyUtils; }
	float setRange(float range) { this->range = range; return range; }
	float getRotTeta() { return rotTeta; }
	float getDYScaling() { return dYScaling; }
	void setRotTeta(float t) { rotTeta = t; }
	void setDYScaling(float s) { dYScaling = s; }
	float getPI(){ return PI; }

};