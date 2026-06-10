#pragma once
#include "MyMetrologyEngine.h"

class HyperME : public MyMetrologyEngine {
    float normCx = 0, normCy = 0, normCz = 0;
    float normSx = 1, normSy = 1, normSz = 1;
    float coeffs[15] = { 0 };

public:
    HyperME(std::string PLYPath, PlyUtilities* plyUtils)
        : MyMetrologyEngine(PLYPath, plyUtils) {
    }

    void   fitHyperSurface(std::vector<PlyUtilities::point> pts);
    float  eval(float x, float y);
    void   colorByPlaneDistance() override;
    std::vector<PlyUtilities::point> findRefernceLine(float region);
    bool   isPointNearPlane(PlyUtilities::point p, float threshold);
    std::vector<PlyUtilities::point> getSubCloud(float zMin, float zMax);
    void flattenPointCloud();
    float scanDisplacementCorrrection(float range);
    float scanDisplacementCorrrectionHoles();
	void alignYAxis();
	void applyRotation(float teta);
    void alignByEigenVector();
    void alignXAxis();
};        