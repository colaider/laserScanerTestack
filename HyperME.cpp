#include "HyperME.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#define _USE_MATH_DEFINES
#include <cmath>
using namespace std;
using point = PlyUtilities::point;
using coloredPoint = PlyUtilities::coloredPoint;


vector<point> HyperME::findRefernceLine(float region) {
	float maxZ = -FLT_MAX;
	float minZ = FLT_MAX;
    float treshold = 0.0f;
    float minX = FLT_MAX, maxX = -FLT_MAX; 
    float minY = FLT_MAX, maxY = -FLT_MAX;
	float maxPointZ = 0.0f, minPointZ = 0.0f;

	point maxPoint, minPoint, rMaxPoint, rMinPoint;
	
    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++) {
            if (scan[i][j].z < minZ) minZ = scan[i][j].z;
            if (scan[i][j].z > maxZ) maxZ = scan[i][j].z;
        }

	treshold = minZ + (maxZ - minZ) * region;

	cout << "treshold" << treshold << "\n";
	cout << "minZ: " << minZ << ", maxZ: " << maxZ << "\n";

    for (int i = 0; i < scanLineCount; i++) {
        for (int j = 0; j < lineCounts[i]; j++) {
            if (scan[i][j].z < treshold) continue;
            if (scan[i][j].x < minX) minX = scan[i][j].x;
            if (scan[i][j].y < minY) minY = scan[i][j].y;
            if (scan[i][j].x > maxX) maxX = scan[i][j].x;
            if (scan[i][j].y > maxY) maxY = scan[i][j].y;
        }
    }

    auto ZinRadious = [&](float x, float y, float radious) {
        float maxLocalZ = -FLT_MAX;
        for (int i = 0; i < scanLineCount; i++)
            for (int j = 0; j < lineCounts[i]; j++) {
                float yy = scan[i][j].y - y;
                float xx = scan[i][j].x - x;
                if (sqrt(xx * xx + yy * yy) >= radious) continue;
                if (scan[i][j].z > maxLocalZ) maxLocalZ = scan[i][j].z;
            }
        
		return maxLocalZ;
        };

	maxPoint = { maxX, maxY, ZinRadious(maxX, maxY, 8.0f) };
	minPoint = { minX, minY, ZinRadious(minX, minY, 8.0f) };
    rMaxPoint = { maxX, minY, ZinRadious(maxX, minY, 8.0f) };
    rMinPoint = { minX, maxY, ZinRadious(minX, maxY, 8.0f) };

	cout << "Max Point: (" << maxPoint.x << ", " << maxPoint.y << ", " << maxPoint.z << ")\n";
	cout << "Min Point: (" << minPoint.x << ", " << minPoint.y << ", " << minPoint.z << ")\n";
	cout << "Right Max Point: (" << rMaxPoint.x << ", " << rMaxPoint.y << ", " << rMaxPoint.z << ")\n";
	cout << "Right Min Point: (" << rMinPoint.x << ", " << rMinPoint.y << ", " << rMinPoint.z << ")\n";


    auto line = [&](float t, point p1, point p2 ) {
        return point{
            p1.x + t * (p2.x - p1.x),
            p1.y + t * (p2.y - p1.y),
            p1.z + t * (p2.z - p1.z)
        };
    };

   
    auto getPerpendicular = [&](point p_start, point p0, point p1) -> point {
        float dx = p1.x - p0.x;
        float dy = p1.y - p0.y;
        float dz = p1.z - p0.z;

        float t = ((p_start.x - p0.x) * dx +
            (p_start.y - p0.y) * dy +
            (p_start.z - p0.z) * dz) /
            (dx * dx + dy * dy + dz * dz);

		return line(t, p0, p1);
     };
     

    

    point smalletsPoint;
    float ortoDistance = FLT_MAX;
    for (int i = 0; i < 10; i++) {
		point p_start = line(i / 10.0f, minPoint, maxPoint);
		point p_ortho = getPerpendicular(p_start, rMinPoint, rMaxPoint);
		float dx = p_ortho.x - p_start.x;
		float dy = p_ortho.y - p_start.y;
		float dz = p_ortho.z - p_start.z;
		float dd = sqrt(dx * dx + dy * dy + dz * dz);
		if (dd < ortoDistance) ortoDistance = dd, smalletsPoint = p_start;
    }

	smalletsPoint.z = ZinRadious(smalletsPoint.x, smalletsPoint.y, 5.0f);



	cout << "Smallest Point: (" << smalletsPoint.x << ", " << smalletsPoint.y << ", " << smalletsPoint.z << ")\n";
    cout << "mamxmax z: " << maxZ;
    point pp[5] = { maxPoint, minPoint, rMaxPoint, rMinPoint, smalletsPoint };


	float minHeight = FLT_MAX;
	float maxHeight = -FLT_MAX;
	point minHeightPoint, maxHeightPoint;
    for (int i = 0; i < 5;i++) {
		if (pp[i].z < minHeight) minHeight = pp[i].z, minHeightPoint = pp[i];
		if (pp[i].z > maxHeight) maxHeight = pp[i].z, maxHeightPoint = pp[i];
    }
    vector<point> pts;

    for (int i = 0; i < scanLineCount; i++) {
        for (int j = 0; j < lineCounts[i]; j++) {
            if (scan[i][j].z < minHeightPoint.z - 0.2f) continue;
			pts.push_back(scan[i][j]);
        }
    }
    return pts;
}

void HyperME::fitHyperSurface(vector<point> pts) {

    const int N = 15;  
    double M[N][N] = {};
    double b[N] = {};

    normCx = 0; normCy = 0; normCz = 0;
    normSx = 1; normSy = 1; normSz = 1;

    for (int i = 0; i < N; i++) coeffs[i] = 0;

    for (int i = 0; i < (int)pts.size(); i++) {
		normCx += pts[i].x;
		normCy += pts[i].y;
		normCz += pts[i].z;
    }
	normCx /= pts.size(); normCy /= pts.size(); normCz /= pts.size();


	float maxXs = -FLT_MAX, maxYs = -FLT_MAX, maxZs = -FLT_MAX;

    for (int i = 0; i < (int)pts.size(); i++) {
		if (maxXs < fabs(pts[i].x - normCx)) maxXs = fabs(pts[i].x - normCx);
        if (maxYs < fabs(pts[i].y - normCy)) maxYs = fabs(pts[i].y - normCy);
        if (maxZs < fabs(pts[i].z - normCz)) maxZs = fabs(pts[i].z - normCz);
    }

	normSx = maxXs, normSy = maxYs, normSz = maxZs;
    if (normSx < 1e-6f) normSx = 1.0f;
    if (normSy < 1e-6f) normSy = 1.0f;
    if (normSz < 1e-6f) normSz = 1.0f;



    for (int i = 0; i < (int)pts.size(); i++) {
        double x = (pts[i].x - normCx) / normSx;
        double y = (pts[i].y - normCy) / normSy;
        double z = (pts[i].z - normCz) / normSz;

        double phi[N] = {
            x * x * x * x,
			y * y * y * y,
			x * x * x * y,
			y * y * y * x,
			x * x * y * y,
            x * x * x,    
            y * y * y,    
            x * x * y,    
            x * y * y,    
            x * x,      
            y * y,      
            x * y,      
            x,        
            y,        
            1.0       
        };

        for (int r = 0; r < N; r++) {
            b[r] += phi[r] * z;
            for (int c = r; c < N; c++)
                M[r][c] += phi[r] * phi[c];
        }
    }

    for (int r = 0; r < N; r++)
        for (int c = 0; c < r; c++)
            M[r][c] = M[c][r];

    for (int col = 0; col < N; col++) {
        int maxRow = col;
        for (int row = col + 1; row < N; row++)
            if (fabs(M[row][col]) > fabs(M[maxRow][col])) maxRow = row;
        for (int k = 0; k < N; k++) swap(M[col][k], M[maxRow][k]);
        swap(b[col], b[maxRow]);

        for (int row = col + 1; row < N; row++) {
            double f = M[row][col] / M[col][col];
            for (int k = col; k < N; k++)
                M[row][k] -= f * M[col][k];
            b[row] -= f * b[col];
        }
    }

    for (int row = N - 1; row >= 0; row--) {
        double sum = b[row];
        for (int k = row + 1; k < N; k++)
            sum -= M[row][k] * coeffs[k];
        coeffs[row] = (float)(sum / M[row][row]);
    }

    cout << "Coefficients: ";
    for (int i = 0; i < N; i++) cout << coeffs[i] << " ";
    cout << "\n";
}

float HyperME::eval(float x, float y) {
    float xn = (x - normCx) / normSx;
    float yn = (y - normCy) / normSy;

    float zn = coeffs[0] * xn * xn * xn * xn + coeffs[1] * yn * yn * yn * yn +
        coeffs[2] * xn * xn * xn * yn + coeffs[3] * xn * yn * yn * yn +
        coeffs[4] * xn * xn * yn * yn +
        coeffs[5] * xn * xn * xn + coeffs[6] * yn * yn * yn +
        coeffs[7] * xn * xn * yn + coeffs[8] * xn * yn * yn +
        coeffs[9] * xn * xn + coeffs[10] * yn * yn +
        coeffs[11] * xn * yn + coeffs[12] * xn +
        coeffs[13] * yn + coeffs[14];

    return zn * normSz + normCz;
}

vector<point> HyperME::getSubCloud(float zMin, float zMax) {
	vector<point> subCloud;
    float maxY = -FLT_MAX;
    float minY = FLT_MAX;
    float maxZ = -FLT_MAX;
    float minZ = FLT_MAX;
    float treshold = 0.0f;

    point maxPoint, minPoint, rMaxPoint, rMinPoint;

    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++) {
            if (scan[i][j].z < minZ) minZ = scan[i][j].z;
            if (scan[i][j].z > maxZ) maxZ = scan[i][j].z;
        }


    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++) {
            if (scan[i][j].z <= treshold) continue;
            if (scan[i][j].y < minY) minY = scan[i][j].y;
            if (scan[i][j].y > maxY) maxY = scan[i][j].y;
        }


    for (int i = 0; i < scanLineCount; i++) {
        for (int j = 0; j < lineCounts[i]; j++) {
            float x = scan[i][j].x;
            float y = scan[i][j].y;
            float z = scan[i][j].z;
            float zSurface = eval(x, y);
            float dist = z - zSurface;
			float yHat = (y - minY) / (maxY - minY);
			float group = zMin + (zMax - zMin) * yHat;

            if (dist < -group) continue;
            subCloud.push_back(scan[i][j]);
        }
    }
	return subCloud;
}

void HyperME::colorByPlaneDistance() {
	fitHyperSurface(findRefernceLine(0.3f));
    for (int i = 1; i <= 2; i++)
            fitHyperSurface(getSubCloud(0.4/i, 0.6/i));

    coloredScan = new PlyUtilities::coloredPoint * [scanLineCount];

    for (int i = 0; i < scanLineCount; i++) {
        coloredScan[i] = new PlyUtilities::coloredPoint[lineCounts[i]];

        for (int j = 0; j < lineCounts[i]; j++) {
            float x = scan[i][j].x;
            float y = scan[i][j].y;
            float z = scan[i][j].z;

            float zSurface = eval(x, y);

            float dist = fabs(z - zSurface);

            coloredScan[i][j].x = x;
            coloredScan[i][j].y = y;
            coloredScan[i][j].z = z;

            if (dist <= 0.3f) {
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

void HyperME::flattenPointCloud() {
    for (int i = 0; i < scanLineCount; i++) {
        for (int j = 0; j < lineCounts[i]; j++) {
            float x = coloredScan[i][j].x;
            float y = coloredScan[i][j].y;

            float zSurface = eval(x, y);
			coloredScan[i][j].z = coloredScan[i][j].z - zSurface;
            
        }
    }
}

float HyperME::scanDisplacementCorrrection(float range) {

    float globalMinX = FLT_MAX;
    float lastLineMinX = FLT_MAX;
    float maxY = -FLT_MAX, minY = FLT_MAX;

    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++)
            if (coloredScan[i][j].g == 255 && coloredScan[i][j].r == 0) {
                if (coloredScan[i][j].x < globalMinX) globalMinX = coloredScan[i][j].x;
                if (coloredScan[i][j].y > maxY)       maxY = coloredScan[i][j].y;
                if (coloredScan[i][j].y < minY)       minY = coloredScan[i][j].y;
            }

    float targetY = maxY - blueRegion - 0.5f;

    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++)
            if (coloredScan[i][j].g == 255 && coloredScan[i][j].r == 0)
                if (fabs(coloredScan[i][j].y - targetY) < 0.5f)
                    if (coloredScan[i][j].x < lastLineMinX) lastLineMinX = coloredScan[i][j].x;

    std::cout << "globalMinX=" << globalMinX << " lastLineMinX=" << lastLineMinX << "\n";

    if (globalMinX == FLT_MAX || lastLineMinX == FLT_MAX) return 0.0f;
    float diff = lastLineMinX - globalMinX;

    for (int i = 0; i < scanLineCount;i++)
        for (int j = 0;j < lineCounts[i];j++)
            coloredScan[i][j].y -= minY;

    if (fabs(diff) >= 0.05f) return fabs(diff) * cos(rotTeta);
    return 0.0f;
}


float HyperME::scanDisplacementCorrrectionHoles() {
    // Step 1 - global rightmost green point
    float globalMinX = FLT_MAX;
    for (int i = 0; i < scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++)
            if (coloredScan[i][j].g == 255 && coloredScan[i][j].r == 0)
                if (coloredScan[i][j].x < globalMinX) globalMinX = coloredScan[i][j].x;

    // Find middle 60% scan line range
    int startLine = (int)(scanLineCount * 0.2f);
    int endLine = (int)(scanLineCount * 0.8f);

    // Step 2 - per scan line smallest X green point in middle 60%
    // Step 3 - find furthest from globalMinX
    float maxDiff = 0.0f;

    for (int i = startLine; i < endLine; i++) {
        float lineMinX = FLT_MAX;
        for (int j = 0; j < lineCounts[i]; j++)
            if (coloredScan[i][j].g == 255 && coloredScan[i][j].r == 0)
                if (coloredScan[i][j].x < lineMinX) lineMinX = coloredScan[i][j].x;

        if (lineMinX == FLT_MAX) continue;

        float diff = fabs(lineMinX - globalMinX);
        if (diff > maxDiff) maxDiff = diff;
    }

    cout << "globalMinX=" << globalMinX << " maxDiff=" << maxDiff
        << " startLine=" << startLine << " endLine=" << endLine << "\n";

    if (maxDiff < 0.05f) return 0.0f;
    return maxDiff * cos(rotTeta);
}


void HyperME::alignYAxis() {

    float maxY = -FLT_MAX, minY = FLT_MAX;

    for(int i=0; i<scanLineCount; i++)
        for (int j = 0; j < lineCounts[i]; j++) {
            if (coloredScan[i][j].g == 0 || coloredScan[i][j].r == 255) continue;
            if (coloredScan[i][j].y > maxY && coloredScan[i][j].g) maxY = coloredScan[i][j].y;
            if (coloredScan[i][j].y < minY) minY = coloredScan[i][j].y;
        }

    float bottomTreshold = minY + (maxY - minY) * 0.15;
	float topTreshold = maxY - (maxY - minY) * 0.15;

    vector<coloredPoint> rightPoints;
     
    for (int i = 0; i < scanLineCount; i++) {
		coloredPoint maxX; maxX.x = -FLT_MAX;
        for(int j=0; j<lineCounts[i]; j++) 
            if (coloredScan[i][j].x > maxX.x && coloredScan[i][j].g == 255 && coloredScan[i][j].r == 0){
                if(coloredScan[i][j].y < topTreshold && coloredScan[i][j].y > bottomTreshold)
                    maxX = coloredScan[i][j];
			}
        if (maxX.x == -FLT_MAX) continue;
		//cout << "Line " << i << ": maxX = (" << maxX.x << ", " << maxX.y << ", " << maxX.z << ")\n";
		rightPoints.push_back(maxX);
    }


    if (rightPoints.size() < 4) {
		cout << "Not enough points to align Y axis\n"; return;
    }

	int quter = (int)(rightPoints.size() / 4);
    vector<coloredPoint> filterd;

    for (int i = 0; i < quter; i++){
        float max = -FLT_MAX; int maxJ = 0;
        for (int j = 0; j <  rightPoints.size(); j++) 
			if (rightPoints[j].x > max) { max = rightPoints[j].x; maxJ = j; }
            
        filterd.push_back(rightPoints[maxJ]);
        rightPoints.erase(rightPoints.begin() + maxJ);
    }



    float sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int   n = (int)filterd.size();

    for (auto& p : filterd) {
        sumX += p.x;
        sumY += p.y;
        sumXY += p.x * p.y;
        sumX2 += p.x * p.x;
    }

    float m = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    float b = (sumY - m * sumX) / n;

	cout << "Reference line: y = " << m << "*x + " << b << "\n";

    float angle = rotTeta = (PI/2.0f)+atan(m);
    float cosA = cos(-angle);
    float sinA = sin(-angle);
    dYScaling = cosA;
    float denom = sqrt(m * m + 1.0f);

    cout << "Rotation angle: " << angle * 180.0f / 3.14159f << " degrees\n";


    for (int i = 0; i < scanLineCount; i++) {
        for (int j = 0; j < lineCounts[i]; j++) {
            float x = coloredScan[i][j].x;
            float y = coloredScan[i][j].y;

            float xNew = x * cosA - y * sinA;
            float yNew = x * sinA + y * cosA;

            coloredScan[i][j].x = xNew;  // modify in place
            coloredScan[i][j].y = yNew;

            if (fabs(y - m * x - b) / denom <= 0.2f) {
                coloredScan[i][j].r = 255;
                coloredScan[i][j].g = 105;
                coloredScan[i][j].b = 180;
            }
        }
    }
    cout << "Rotated and clipped point cloud\n";

    plyUtils->setCounts(lineCounts, scanLineCount);
}


void HyperME::applyRotation(float teta) {
    float cosA = cos(-teta);
    float sinA = sin(-teta);
    dYScaling = cosA;
	rotTeta = teta;

    cout << "Rotation angle: " << teta * 180.0f / 3.14159f << " degrees\n";

    for (int i = 0; i < scanLineCount; i++) {
        for (int j = 0; j < lineCounts[i]; j++) {
            float x = coloredScan[i][j].x;
            float y = coloredScan[i][j].y;

            float xNew = x * cosA - y * sinA;
            float yNew = x * sinA + y * cosA;

            coloredScan[i][j].x = xNew;  
            coloredScan[i][j].y = yNew;
        }
    }
    cout << "Rotated and clipped point cloud\n";

    plyUtils->setCounts(lineCounts, scanLineCount);
}

void HyperME::alignByEigenVector() {
    float xC = 0.0f, yC = 0.0f; int n = 0;
    float sXsX = 0.0f, sYsY = 0.0f, sXsY = 0.0f;

    for (int i = 0;i < scanLineCount;i++) 
        for (int j = 0;j < lineCounts[i];j++) { 
            if (coloredScan[i][j].x < 10.0f ||(coloredScan[i][j].g == 0)) continue;
            xC += coloredScan[i][j].x; 
            yC += coloredScan[i][j].y;n++; 
        }

    xC /= n; yC /= n;


    for (int i = 0;i < scanLineCount;i++)
        for (int j = 0;j < lineCounts[i];j++){
            if (coloredScan[i][j].x < 10.0f || (coloredScan[i][j].g == 0)) continue;
			sXsX += (coloredScan[i][j].x - xC) * (coloredScan[i][j].x - xC);
            sYsY += (coloredScan[i][j].y - yC) * (coloredScan[i][j].y - yC);
            sXsY += (coloredScan[i][j].y - yC) * (coloredScan[i][j].x - xC);
         }
    sXsX /= n; sYsY /= n; sXsY /= n;

    vector<vector<float>> C;
    vector<float> cRow1; vector<float> cRow2;
	cRow1.push_back(sXsX); cRow1.push_back(sXsY); C.push_back(cRow1);
	cRow2.push_back(sXsY); cRow2.push_back(sYsY); C.push_back(cRow2);
    float a = 1.0f, b = -(sXsX + sYsY), c = sXsX * sYsY - sXsY * sXsY;

    float D = b * b - 4 * a * c;
    float l1 = (-b + sqrt(D)) / (2 * a), l2 = (-b - sqrt(D)) / (2 * a);

    float m = (l1 < l2) ? (sXsY / (l1 - sXsX)) : (sXsY / (l2 - sXsX)); b = 0;


    float angle = rotTeta = atan(1.0f / m);
    float cosA = cos(-angle);
    float sinA = sin(-angle);
    dYScaling = cosA;
    float denom = sqrt(m * m + 1.0f);

    cout << "Rotation angle: " << angle * 180.0f / 3.14159f << " degrees\n";

    for (int i = 0; i < scanLineCount; i++) {
        for (int j = 0; j < lineCounts[i]; j++) {
            float x = coloredScan[i][j].x;
            float y = coloredScan[i][j].y;

            float xNew = x * cosA - y * sinA;
            float yNew = x * sinA + y * cosA;

            coloredScan[i][j].x = xNew;  // modify in place
            coloredScan[i][j].y = yNew;

            if (fabs(y - m * x - b) / denom <= 0.2f) {
                coloredScan[i][j].r = 255;
                coloredScan[i][j].g = 105;
                coloredScan[i][j].b = 180;
            }
        }
    }
    cout << "Rotated and clipped point cloud\n";

    plyUtils->setCounts(lineCounts, scanLineCount);
}

void HyperME::alignXAxis() {
    float miX = FLT_MAX; float minY0 = FLT_MAX; float minYX = FLT_MAX;

    for (int i = 0; i < scanLineCount;i++)
        for (int j = 0;j < lineCounts[i];j++) {
            if (coloredScan[i][j].g != 255 || coloredScan[i][j].r != 0) continue;
            if (coloredScan[i][j].x < miX) miX = coloredScan[i][j].x;
        }
            
    for (int i = 0;i < scanLineCount;i++) {
        for (int j = 0;j < lineCounts[i]; j++) {
            if (coloredScan[i][j].g != 255 || coloredScan[i][j].r != 0) continue;
            if(fabs(coloredScan[i][j].x - miX) < 0.2f)
                if(coloredScan[i][j].y < minYX) minYX = coloredScan[i][j].y;
            if(fabs(coloredScan[i][j].x) < 0.2f)
                if(coloredScan[i][j].y < minY0) minY0 = coloredScan[i][j].y;    
        }
    }

    auto fit = [&](float& m, float& b,  float r) {
        float sumOfY = 0.0f, sumOfX = 0.0f, sumOfX2 = 0.0f, sumOfXY = 0.0f;
        int n = 0;
        for (int i = 0; i < scanLineCount;i++) {
            for (int j = 0; j < lineCounts[i]; j++) {
                if (coloredScan[i][j].g != 255 || coloredScan[i][j].r != 0) continue;
                if (coloredScan[i][j].y <= (coloredScan[i][j].x * m + b) + r) {
                    sumOfY += coloredScan[i][j].y; sumOfX += coloredScan[i][j].x;
                    sumOfX2 += coloredScan[i][j].x * coloredScan[i][j].x;
                    sumOfXY += coloredScan[i][j].x * coloredScan[i][j].y;
                    n++;
                }
            }
        }
        m = (n * sumOfXY - sumOfX * sumOfY) / (n * sumOfX2 - sumOfX * sumOfX);
        b = (sumOfY * sumOfX2 - sumOfX * sumOfXY) / (n * sumOfX2 - sumOfX * sumOfX);

        };

    float m = (minY0 - minYX) / (0 - miX); float b = minY0;

    for (float r : {1.0f, 0.8f, 0.5f, 0.1f, 0.08f, 0.06f, 0.04f, 0.02f}) fit(m, b, r);


    float angle = rotTeta = atan(m);
    float cosA = cos(-angle);
    float sinA = sin(-angle);
    dYScaling = cosA;

    std::cout << "Rotation angle: " << angle * 180.0f / 3.14159f << " degrees\n";


    for (int i = 0; i < scanLineCount; i++) {
        for (int j = 0; j < lineCounts[i]; j++) {
            float x = coloredScan[i][j].x;
            float y = coloredScan[i][j].y;

            float xNew = x * cosA - y * sinA;
            float yNew = x * sinA + y * cosA;

            coloredScan[i][j].x = xNew;  // modify in place
            coloredScan[i][j].y = yNew;

            if (fabs(y - (m * x + b)) <= 0.3f) {
                coloredScan[i][j].r = 255;
                coloredScan[i][j].g = 105;
                coloredScan[i][j].b = 180;
            }
        }
    }
    std::cout << "Rotated and clipped point cloud\n";

    plyUtils->setCounts(lineCounts, scanLineCount);
}
