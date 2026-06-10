#pragma once
#include <iostream>
#include "HyperME.h"
#include <vector>
#include "PlyUtilities.h"
#include "DataGrouper.h"
#include "structs.h"


class MetrologyFFT : public HyperME {
public:
    MetrologyFFT(std::string PLYPath, PlyUtilities* plyUtils) : HyperME(PLYPath, plyUtils) {
    }

    std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>> getLines(std::vector<GrooveData> grooveData, float sectionY);
    void biasLines(std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>>& grooves, bool XZ);
    std::vector<std::vector<FTD>> amplitudeFrequency(std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>> grooves, float strF, float endF, bool XZ);
    std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>> getContourLine(std::vector<GrooveData> grooveData);

};
