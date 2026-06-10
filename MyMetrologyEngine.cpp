#include "stdafx.h"
#include "MyMetrologyEngine.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include "PlyUtilities.h"

using namespace std;
using point = PlyUtilities::point;
using coloredPoint = PlyUtilities::coloredPoint;

MyMetrologyEngine::MyMetrologyEngine(std::string PLYPath, PlyUtilities* plyUtils) {
    this->plyUtils = plyUtils;
    //this->plyUtils->readFromPLY(PLYPath)d
    scan = this->plyUtils->getScan();

    lineCounts = this->plyUtils->getLineCounts();
    scanLineCount = this->plyUtils->getScanLineCount();
    firstLineCount = this->plyUtils->getFirstLineCount();
}

MyMetrologyEngine::~MyMetrologyEngine() {

}

void MyMetrologyEngine::computePlane() {

    //auto avgZ = [&](point& p, int si) {
    //    float sum = 0.0f; int cnt = 0;
    //    for (int j = 0; j < lineCounts[si]; j++) {
    //        float dx = scan[si][j].x - p.x;
    //        float dy = scan[si][j].y - p.y;
    //        if (sqrt(dx * dx + dy * dy) <= 1.0f) { sum += scan[si][j].z; cnt++; }
    //    }
    //    return cnt > 0 ? sum / cnt : p.z;
    //    };

    //platoCount = 2;

    ////point* platosM = getPlatosForLine((int)((scanLineCount/2) + scanLineCount*0.25));

    //int  countM = platoCount;

    //point p1 = platosM[0];
    //p1.z = avgZ(p1, midScanIdx);

    //int lastScanIdx = scanLineCount - scanLineCount / 4;
    //point* platosLast = getPlatosForLine((int)((scanLineCount / 2) - scanLineCount * 0.25));
    //point p2 = platosLast[0];
    //p2.z = avgZ(p2, lastScanIdx);

    //// p3 = last plato on mid scan line
    //point p3 = platosM[countM - 1];
    //p3.z = avgZ(p3, midScanIdx);

    //delete[] platosM;

    //float v1x = p2.x - p1.x, v1y = p2.y - p1.y, v1z = p2.z - p1.z;
    //float v2x = p3.x - p1.x, v2y = p3.y - p1.y, v2z = p3.z - p1.z;

    //float nx = v1y * v2z - v1z * v2y;
    //float ny = v1z * v2x - v1x * v2z;
    //float nz = v1x * v2y - v1y * v2x;
    //float d = nx * p1.x + ny * p1.y + nz * p1.z;

    //planeN[0] = nx; planeN[1] = ny; planeN[2] = nz; planeD = d;

    //std::cout << "p1: " << p1.x << " " << p1.y << " " << p1.z << "\n";
    //std::cout << "p2: " << p2.x << " " << p2.y << " " << p2.z << "\n";
    //std::cout << "p3: " << p3.x << " " << p3.y << " " << p3.z << "\n";
    //std::cout << "Plane normal: (" << nx << ", " << ny << ", " << nz << ")\n";
    //std::cout << "Plane equation: " << nx << "x + " << ny << "y + " << nz << "z = " << d << "\n";
}

void MyMetrologyEngine::colorByPlaneDistance() {

    float a = planeN[0];
    float b = planeN[1];
    float c = planeN[2];
    float d = planeD;

    float mu = 1.0f / sqrt(a * a + b * b + c * c);
    float an = a * mu;
    float bn = b * mu;
    float cn = c * mu;
    float dn = d * mu;

    float cosGamma = cn;

    coloredScan = new coloredPoint * [scanLineCount];

    for (int i = 0; i < scanLineCount; i++) {
        coloredScan[i] = new coloredPoint[lineCounts[i]];

        for (int j = 0; j < lineCounts[i]; j++) {
            float x = scan[i][j].x;
            float y = scan[i][j].y;
            float z = scan[i][j].z;

            float dist = an * x + bn * y + cn * z - dn;
            float distZ = dist * cosGamma;

            coloredScan[i][j].x = x;
            coloredScan[i][j].y = y;
            coloredScan[i][j].z = z;

            if (fabs(distZ) <= 0.2f) {
                coloredScan[i][j].r = 0;
                coloredScan[i][j].g = 255;
                coloredScan[i][j].b = 0;
            }
            else {
                coloredScan[i][j].r = 255;
                coloredScan[i][j].g = 0;
                coloredScan[i][j].b = 0;
            }
        }
    }

}

void MyMetrologyEngine::alignXAxis() {
    float minY = FLT_MAX; float maxY = -FLT_MAX;


    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++) {
            if (coloredScan[i][j].g == 255 && coloredScan[i][j].r == 0) {
                if (coloredScan[i][j].y < minY) minY = coloredScan[i][j].y;
                if (coloredScan[i][j].y > maxY) maxY = coloredScan[i][j].y;
            }
        }
;

    coloredPoint* pLeftMin = nullptr; coloredPoint* pRightMin = nullptr;
    coloredPoint* pLeftMax = nullptr; coloredPoint* pRightMax = nullptr;

    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++) {
            coloredPoint& p = coloredScan[i][j];
            if (p.g == 255 && p.r == 0) {
                if (fabs(p.y - minY) < 1.0f) {
                    if (!pLeftMin || p.x < pLeftMin->x)  pLeftMin = &p;
                    if (!pRightMin || p.x > pRightMin->x) pRightMin = &p;
                }
            }
        }

    if (!pLeftMin || !pRightMin) {
        cout << "No green points found\n"; return;
    }

    float mFirst = 0, bFirst = 0;
    if (fabs(pRightMin->x - pLeftMin->x) > 0.0001f) {
        mFirst = (pRightMin->y - pLeftMin->y) / (pRightMin->x - pLeftMin->x);
        bFirst = pLeftMin->y - mFirst * pLeftMin->x;
    }

    auto refineRegression = [&](float& m, float& b) {
        vector<pair<float, float>> pts;
        for (int i = 0; i < scanLineCount; i++)
            for (int j = 0; j < lineCounts[i]; j++) {
                if (coloredScan[i][j].r == 255 || coloredScan[i][j].g != 255) continue;
                float x = coloredScan[i][j].x;
                float y = coloredScan[i][j].y;


                if (y <= (m*x + b) + 0.1f)
                    pts.push_back({ x, y });
            }
        if (pts.size() < 2) return;
        float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
        int   n = (int)pts.size();
        for (auto& p : pts) {
            sumX += p.first;  sumY += p.second;
            sumXY += p.first * p.second;
            sumX2 += p.first * p.first;
        }
        m = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
        b = (sumY - m * sumX) / n;
        };

    for (int i = 0; i < 50; i++)
        refineRegression(mFirst, bFirst);
      

    cout << "First line: y = " << mFirst << "*x + " << bFirst << "\n";

    float angle = rotTeta = atan(mFirst);
    float cosA = cos(-angle);
    float sinA = sin(-angle);
    dYScaling = cosA;

    cout << "Rotation angle: " << angle * 180.0f / 3.14159f << " degrees\n";
    

    for (int i = 0; i < scanLineCount; i++) {
        for (int j = 0; j < lineCounts[i]; j++) {
            float x = coloredScan[i][j].x;
            float y = coloredScan[i][j].y;

            float xNew = x * cosA - y * sinA;
            float yNew = x * sinA + y * cosA;

            coloredScan[i][j].x = xNew;  // modify in place
            coloredScan[i][j].y = yNew;

            if (fabs(y - (mFirst * x + bFirst)) <= 0.3f) {
                coloredScan[i][j].r = 255;
                coloredScan[i][j].g = 105;
                coloredScan[i][j].b = 180;
            }
        }
    }
    cout << "Rotated and clipped point cloud\n";

    plyUtils->setCounts(lineCounts, scanLineCount);
}

void MyMetrologyEngine::colorEndRegions(float blueRegion, float keepRegion) {

    float yMin = FLT_MAX;
    float yMax = -FLT_MAX;

	this->blueRegion = blueRegion;
	this->redRegion = keepRegion;

    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++) {
            if (coloredScan[i][j].y < yMin) yMin = coloredScan[i][j].y;
            if (coloredScan[i][j].y > yMax) yMax = coloredScan[i][j].y;
        }

    float zoneEnd = yMax - blueRegion;
    float zoneKeep = zoneEnd - keepRegion;

    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++) {
            coloredPoint& p = coloredScan[i][j];
            if (p.r != 255 || p.g != 0) continue;  

            if (p.y >= zoneEnd || p.y <= zoneKeep) {
                p.r = 0; p.g = 0; p.b = 255;  
            }
        }
}

vector<MyMetrologyEngine::GrooveData> MyMetrologyEngine::getGrooveAreas(float minArea) {

    const float deltaY = (float)plyUtils->getScanPerMM() * dYScaling;
    coloredPoint gp0; coloredPoint gp1;
    vector<GrooveData> out;
    int jj = 0;
    bool corupted = false;


    float minY = FLT_MAX, maxY = -FLT_MAX, midY = 0.0f;
    int si = scanLineCount, fi = 0;

    for (int i = 0;i < scanLineCount;i++) {
        for (int j = 0;j < lineCounts[i];j++) {
            if (coloredScan[i][j].y < minY) minY = coloredScan[i][j].y;
            if (coloredScan[i][j].y > maxY) maxY = coloredScan[i][j].y;
        }
    }

    float blueStartY = maxY - blueRegion;
    float redEnd = blueStartY - redRegion;


    auto hasNoGreenBetween = [&](int scanLine, int j0, int j1) {
        for (int idx = j0 + 1; idx < j1; idx++)
            if (coloredScan[scanLine][idx].g == 255 && coloredScan[scanLine][idx].r == 0)
                return false;  
        return true;  
        };


    midY = maxY - (blueRegion + redRegion) / 2.0f;

    // Find single best scan line closest to midY
    int midLine = 0;
    float minDist = FLT_MAX;
    for (int i = 0; i < scanLineCount; i++) {
        float d = fabs(coloredScan[i][0].y - midY);
        if (d < minDist) { minDist = d; midLine = i; }
    }

    // Detect grooves on that single line
    for (int j = 0; j < lineCounts[midLine]; j++) {
        if (coloredScan[midLine][j].g != 255 || coloredScan[midLine][j].r != 0) continue;
        gp0 = coloredScan[midLine][j];
        for (int k = j + 1; k < lineCounts[midLine]; k++) {
            if (coloredScan[midLine][k].g != 255 || coloredScan[midLine][k].r != 0) continue;
            gp1 = coloredScan[midLine][k];
            if (fabs(gp1.x - gp0.x) < 1.0f) continue;
            if (!hasNoGreenBetween(midLine, j, k)) continue;
            out.push_back({ 0.0f, gp0.x + (gp1.x - gp0.x) / 2.0f, midY });
            j = k;
            break;
        }
    }


    for (int i = 0; i < scanLineCount && si == scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++)
            if (fabs(coloredScan[i][j].y - blueStartY) < 0.08f) { si = i; break; }

    for (int i = 0; i < scanLineCount && fi == 0; i++)
        for (int j = 0; j < lineCounts[i]; j++)
            if (fabs(coloredScan[i][j].y - redEnd) < 0.08f) { fi = i; break; }


	
    corupted = false;
    noPointLeft = 0.0f; noPointRight = 0.0f;
    for (int g = 0; g < (int)out.size(); g++) {       
        
        float area = 0.0f;
        float prevW = -1.0f;
        for (int i = si - 1; i >= fi; i--) {
            float leftX = FLT_MAX, minDist = FLT_MAX, rightX = -FLT_MAX;
            int   centerJ = 0;

            for (int j = 0; j < lineCounts[i]; j++) {
                float d = fabs(coloredScan[i][j].x - out[g].centerX);
                if (d < minDist) { minDist = d; centerJ = j; }
            }

            for (int k = centerJ - 1; k >= 0; k--) {
                if (coloredScan[i][k].g == 255 && coloredScan[i][k].r == 0) { leftX = coloredScan[i][k].x; break; }
                if(k == 0) { noPointLeft = coloredScan[i][k].x; }
            }
            for (int k = centerJ + 1; k < lineCounts[i]; k++) {
                if (coloredScan[i][k].g == 255 && coloredScan[i][k].r == 0) { rightX = coloredScan[i][k].x; break; }
                if (k == lineCounts[i] - 1) { noPointRight = coloredScan[i][k].x; }

            }
            if (leftX == FLT_MAX || rightX == -FLT_MAX) { prevW = -1.0f; continue; }

            float currW = fabs(rightX - leftX);

            if (noPointLeft != 0.0f) {
				noPointLeft = 1.0f;
				corupted = true;
				area = 0.0f;
                break;
				
            } else if (noPointRight != 0.0f && corupted == false) {
				noPointRight = 1.0f;
				corupted = true;
				area = 0.0f;
                break;
            }

            if (prevW >= 0.0f)
                area += (prevW + currW) / 2.0f * deltaY;

            prevW = currW;
        }

        if (area > minArea) {
            std::cout << "Groove " << g << " centerX=" << out[g].centerX << " si=" << si << " fi=" << fi << "\n";
            out[g].area = area;
        } else {
            out.erase(out.begin() + g);
            g--;
        }
    }
    
    std::cout << "minY=" << minY << " maxY=" << maxY << "\n";
    std::cout << "blueStartY=" << blueStartY << " redEnd=" << redEnd << "\n";
    std::cout << "midY=" << midY << " jj=" << jj << "\n";

    std::cout << "Found " << out.size() << " grooves:\n";
    for (int i = 0; i < (int)out.size(); i++)
        std::cout << "Groove " << i << ": area=" << out[i].area
        << " mm2  cx=" << out[i].centerX
        << " cy=" << out[i].centerY << "\n";

    return out;
}


vector<MyMetrologyEngine::GrooveData> MyMetrologyEngine::getHoleAreas(float minArea) {
    const float deltaY = (float)plyUtils->getScanPerMM() * dYScaling;
    vector<GrooveData> out;
    float minHoleDiameter = 0.1;



    struct HoleCandidate {
        int scanLineTop;
        int scanLineBottom;
		float radius;
        point pC;
        float area;
    };


    std::vector<HoleCandidate> candidates;
    auto preventDublicates = [&](float x, float y) {
        for (const auto& c : candidates) {
            if (c.area == -1) continue;

            float dx = c.pC.x - x;
            float dy = c.pC.y - y;
            if (sqrt(dx * dx + dy * dy) < c.radius) return true;
        }
        return false;

        };

    for (int i = 0; i < scanLineCount;i++) {
        for (int j = 0; j < lineCounts[i]; j++) {
            if (coloredScan[i][j].g != 255 || coloredScan[i][j].r != 0) continue;
            if (j + 1 >= lineCounts[i]) break;
            if (fabs(coloredScan[i][j].x - coloredScan[i][j + 1].x) < minHoleDiameter) continue;
           
            HoleCandidate hole; 
			hole.pC.x = (coloredScan[i][j].x + coloredScan[i][j + 1].x) / 2.0f;
			hole.pC.y = coloredScan[i][j].y;
            hole.scanLineTop = 0;
            hole.scanLineBottom = 0; 
            hole.area = -1;
			candidates.push_back(hole);
        }
    }

    for (auto& c : candidates) {
        float closestTopY = -FLT_MAX;  
        float closestBottomY = FLT_MAX;  
        int   topLine = 0;
        int   bottomLine = 0;

        for (int i = 0; i < scanLineCount; i++) {
            for (int j = 0; j < lineCounts[i]; j++) {
                float lineY = coloredScan[i][j].y;
                if (coloredScan[i][j].g != 255 || coloredScan[i][j].r != 0) continue;
                if (fabs(coloredScan[i][j].x - c.pC.x) >= 0.1) continue;

                if (lineY < c.pC.y && lineY > closestTopY) {
                    closestTopY = lineY;
                    topLine = i;
                }
                if (lineY > c.pC.y && lineY < closestBottomY) {
                    closestBottomY = lineY;
                    bottomLine = i;
                }
            }
        }

        c.scanLineTop = topLine;
        c.scanLineBottom = bottomLine;
		c.pC.y = (closestTopY + closestBottomY) / 2.0f;
		c.radius = max(closestBottomY - c.pC.y, c.pC.y - closestTopY);
    }   

    for (auto& c : candidates) {
        if (preventDublicates(c.pC.x, c.pC.y)) continue;

        float area = 0.0f;
        float prevW = -1.0f;

        for (int i = c.scanLineTop; i <= c.scanLineBottom; i++) {
            float leftX = FLT_MAX;
            float rightX = -FLT_MAX;
            float minDist = FLT_MAX;
            int   centerJ = 0;

            for (int j = 0; j < lineCounts[i]; j++) {
                float d = fabs(coloredScan[i][j].x - c.pC.x);
                if (d < minDist) { minDist = d; centerJ = j; }
            }

            for (int k = centerJ - 1; k >= 0; k--)
                if (coloredScan[i][k].g == 255 && coloredScan[i][k].r == 0) { leftX = coloredScan[i][k].x; break; }

            for (int k = centerJ + 1; k < lineCounts[i]; k++)
                if (coloredScan[i][k].g == 255 && coloredScan[i][k].r == 0){ rightX = coloredScan[i][k].x; break; }

            if (leftX == FLT_MAX || rightX == -FLT_MAX) { prevW = -1.0f; continue; }

            float currW = fabs(rightX - leftX);
            
            if (prevW >= 0.0f)
                area += (prevW + currW) / 2.0f * deltaY;

            prevW = currW;
        }

        if (area < minArea) continue;

        c.area = area; GrooveData gd; gd.area = area;
        gd.centerX = c.pC.x; gd.centerY = c.pC.y;
        out.push_back(gd);
    }

    std::cout << "Found " << out.size() << " holes:\n";
    for (int i = 0; i < (int)out.size(); i++)
        std::cout << "Hole " << i << ": area=" << out[i].area
        << " mm2  cx=" << out[i].centerX
        << " cy=" << out[i].centerY << "\n";


    return out;
}



void MyMetrologyEngine::shiftInX(float shift) {
    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++)
			coloredScan[i][j].x -= shift;
}

void MyMetrologyEngine::writeTextToCloud(string text, float cx, float cy){

    map<char, vector<string>> font = {
        {'0', {"01110","10001","10001","10001","10001","10001","01110"}},
        {'1', {"00100","01100","00100","00100","00100","00100","01110"}},
        {'2', {"01110","10001","00001","00010","00100","01000","11111"}},
        {'3', {"11111","00010","00100","00010","00001","10001","01110"}},
        {'4', {"00010","00110","01010","10010","11111","00010","00010"}},
        {'5', {"11111","10000","11110","00001","00001","10001","01110"}},
        {'6', {"01110","10000","10000","11110","10001","10001","01110"}},
        {'7', {"11111","00001","00010","00100","01000","01000","01000"}},
        {'8', {"01110","10001","10001","01110","10001","10001","01110"}},
        {'9', {"01110","10001","10001","01111","00001","00001","01110"}},
        {'.', {"00000","00000","00000","00000","00000","00000","00100"}},
        {' ', {"00000","00000","00000","00000","00000","00000","00000"}},
        {'-', {"00000","00000","00000","11111","00000","00000","00000"}},
    };

    const float charW = 15.0f * 0.044f;
    const float charH = 30.0f * (float)plyUtils->getScanPerMM() * dYScaling;
    const float ptW = charW / 5.0f;
    const float ptH = charH / 7.0f;
    const float spacing = ptW;
    const float rectW = text.size() * (charW + spacing) + spacing;
    const float rectH = charH + spacing;
    const float stepX = 0.04f;
    const float stepY = 0.04f;  
    const float lineWidth = 2; 

    float zMax = -FLT_MAX;
    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++)
            if (coloredScan[i][j].z > zMax) zMax = coloredScan[i][j].z;

    float startX = cx - rectW / 2.0f;
    float startY = cy - rectH / 2.0f;

    // Build text pixel map
    map<pair<int, int>, bool> textPixels;
    for (int ci = 0; ci < (int)text.size(); ci++) {
        char c = text[ci];
        if (font.find(c) == font.end()) c = ' ';
        auto& rows = font[c];
        for (int row = 0; row < (int)rows.size(); row++)
            for (int col = 0; col < (int)rows[row].size(); col++)
                if (rows[row][col] == '1') {
                    float tx = startX + spacing / 2.0f + ci * (charW + spacing) + (4 - col) * ptW;
                    float ty = startY + spacing / 2.0f + row * ptH;
                    int   ix = (int)round((tx - startX) / stepX);
                    int   iy = (int)round((ty - startY) / stepY);

                    for (int dx = -lineWidth; dx <= lineWidth; dx++)
                        for (int dy = -lineWidth; dy <= lineWidth; dy++)
                            textPixels[{ix + dx, iy + dy}] = true;
                }
    }

    vector<coloredPoint> rectPts;
    for (float x = startX; x <= startX + rectW; x += stepX) {
        for (float y = startY; y <= startY + rectH; y += stepY) {
            int ix = (int)round((x - startX) / stepX);
            int iy = (int)round((y - startY) / stepY);

            coloredPoint p;
            p.x = x; p.y = y; p.z = zMax;

            if (textPixels.count({ ix, iy })) { p.r = 0; p.g = 0; p.b = 0; }
            else { p.r = 255; p.g = 255; p.b = 255; }
            rectPts.push_back(p);
        }
    }

    if (rectPts.empty()) return;

    coloredPoint** newScan = new coloredPoint * [scanLineCount + 1];
    int* newCounts = new int[scanLineCount + 1];

    for (int i = 0; i < scanLineCount; i++) {
        newScan[i] = coloredScan[i];
        newCounts[i] = lineCounts[i];
    }

    newScan[scanLineCount] = new coloredPoint[rectPts.size()];
    newCounts[scanLineCount] = (int)rectPts.size();
    for (int i = 0; i < (int)rectPts.size(); i++)
        newScan[scanLineCount][i] = rectPts[i];

    coloredScan = newScan;
    lineCounts = newCounts;
    scanLineCount++;
    plyUtils->setCounts(lineCounts, scanLineCount);
}

void MyMetrologyEngine::removeBottomPoints(float percent) {

    float minZ = FLT_MAX;
    float maxZ = -FLT_MAX;

    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++) {
            if (coloredScan[i][j].z < minZ) minZ = coloredScan[i][j].z;
            if (coloredScan[i][j].z > maxZ) maxZ = coloredScan[i][j].z;
        }

    float threshold = minZ + (maxZ - minZ) * percent;

    for (int i = 0; i < scanLineCount; i++) {
        int newCount = 0;
        for (int j = 0; j < lineCounts[i]; j++) {
            if (coloredScan[i][j].z < threshold) continue;
            coloredScan[i][newCount++] = coloredScan[i][j];
        }
        lineCounts[i] = newCount;
    }
    plyUtils->setCounts(lineCounts, scanLineCount);
}
