#include "MetrologyFFT.h"
#include "structs.h"

std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>>  MetrologyFFT::getLines(std::vector<GrooveData> grooveData, float sectionY) {
	std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>>  out;
	std::vector<std::vector<PlyUtilities::coloredPoint>> outGroove;
	std::vector<PlyUtilities::coloredPoint> outLine;

	auto closestPoint = [&](float x, int lIdx) {
		PlyUtilities::coloredPoint closest; closest.x = 0; closest.y = 0; closest.z = 0; closest.r = 0; closest.g = 0; closest.b = 0;
		float minDist = FLT_MAX;

		for (int j = 0; j < lineCounts[lIdx]; j++) {
			float d = fabs(coloredScan[lIdx][j].x - x);
			if (d < minDist) { 
				minDist = d; closest = coloredScan[lIdx][j];
			}
		}
		return closest;
		};

	int gIndex = 0;
	for (auto& groove : grooveData) {
		float avgW30p = (groove.area / sectionY) / 2.5f;
		outGroove.clear();
		
		for(auto r: {groove.centerX - avgW30p, groove.centerX, groove.centerX + avgW30p}) {
			outLine.clear();
			for (int i = 0; i < scanLineCount; i++) {
				
				PlyUtilities::coloredPoint p = closestPoint(r, i);
				if(p.r != 0 && p.g == 0 && p.b == 0 && p.z != 0)
					outLine.push_back(p);


				for(int j=0; j<lineCounts[i]; j++) {
					if (fabs(coloredScan[i][j].x - r) < 0.08f && coloredScan[i][j].r != 0) {
						coloredScan[i][j].r = 30.0; coloredScan[i][j].g = 30;
					}
				}
			}
			outGroove.push_back(outLine);
		}

		out.push_back(outGroove);
	}
	return out;
}


void MetrologyFFT::biasLines(std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>>& grooves, bool XZ) {
	auto fit = [&](std::vector<PlyUtilities::coloredPoint> line, float& m, float& b) {
		float sumOfZ = 0.0f, sumOfY = 0.0f, sumOfY2 = 0.0f, sumOfYZ = 0.0f;
		int n = 0;
		for (auto& p : line) {
			sumOfZ += (XZ ? p.x : p.z); sumOfY += p.y;
			sumOfY2 += p.y * p.y;
			sumOfYZ += p.y * (XZ ? p.x : p.z);
			n++;
		}
		m = (n * sumOfYZ - sumOfY * sumOfZ) / (n * sumOfY2 - sumOfY * sumOfY);
		b = (sumOfZ * sumOfY2 - sumOfY * sumOfYZ) / (n * sumOfY2 - sumOfY * sumOfY);

		};

	for (auto& groove : grooves) {
		int lI = 0;
		for (auto& line : groove) {
			float m = 0.0, b = 0.0;
			if (lI != 1){
				fit(line, m, b);
				for (auto& p : line)
					(XZ ? p.x : p.z) -= m * p.y + b;
			}
			lI++;
		}
		
	}

}


std::vector<std::vector<FTD>> MetrologyFFT::amplitudeFrequency(std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>> grooves, float strF, float endF, bool XZ) {
	std::vector<std::vector<FTD>> out;
	std::vector<FTD> groove;
	FTD ftd;
	float deltaF = 0.001;
		
	for (auto& g : grooves) {
		groove.clear();
		int lIdx = 0;
		for (auto& l : g) {
			float a = 0.0f; float b = 0.0f;
			if (lIdx != 1) {
				ftd.amplitude.clear(); ftd.phase.clear(); ftd.f.clear();
				int kS = (int)((l.size() * strF * (fabs(l[1].y - l[0].y))) / (2 * PI));
				kS = kS == 0 ? 1 : kS;
				int kE = (int)((l.size() * endF * (fabs(l[1].y - l[0].y))) / (2 * PI));
				//for (int k = kS; k < kE ; k++) ftd.f.push_back((2*PI*k)/(l.size()* fabs(l[1].y - l[0].y)));
				for (int i = 0; i < (fabs(strF - endF) / deltaF); i++) { ftd.f.push_back(strF + (i * deltaF)); }
				for (auto& f : ftd.f) {
					b = 0.0f, a = 0.0f;
					for (auto& p : l) {
						a += (XZ ? p.x : p.z) * cos(f * p.y);
						b += (XZ ? p.x : p.z) * sin(f * p.y);
					}
					ftd.phase.push_back(atan2(b, a));
					ftd.amplitude.push_back(sqrt(a * a + b * b) / l.size());
				}
				groove.push_back(ftd);
			}lIdx++;
		}
		out.push_back(groove);
	}

	return out;
}

std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>> MetrologyFFT::getContourLine(std::vector<GrooveData> grooveData) {
	float minY = FLT_MAX, maxY = -FLT_MAX, midY = 0.0f;
	int si = scanLineCount, fi = 0;
	std::vector<std::vector<std::vector<PlyUtilities::coloredPoint>>> out;
	std::vector<std::vector<PlyUtilities::coloredPoint>> groove;
	std::vector<PlyUtilities::coloredPoint> line;

	for (int i = 0;i < scanLineCount;i++) {
		for (int j = 0;j < lineCounts[i];j++) {
			if (coloredScan[i][j].y < minY) minY = coloredScan[i][j].y;
			if (coloredScan[i][j].y > maxY) maxY = coloredScan[i][j].y;
		}
	}

	float blueStartY = maxY - blueRegion;
	float redEnd = blueStartY - redRegion;


	for (int i = 0; i < scanLineCount && si == scanLineCount; i++)
		for (int j = 0; j < lineCounts[i]; j++)
			if (fabs(coloredScan[i][j].y - blueStartY) < 0.8f) { si = i; break; }

	for (int i = 0; i < scanLineCount && fi == 0; i++)
		for (int j = 0; j < lineCounts[i]; j++)
			if (fabs(coloredScan[i][j].y - redEnd) < 0.8f) { fi = i; break; }


	for (auto& g : grooveData) {
		groove.clear();
		line.clear();
		for (int i = si - 1; i >= fi; i--) {
			PlyUtilities::coloredPoint p; float minDist = FLT_MAX, rightX = -FLT_MAX;
			p.x = 0.0f; p.y = 0.0f; p.z = 0.0f;
			int centerJ = 0;

			for (int j = 0; j < lineCounts[i]; j++) {
				float d = fabs(coloredScan[i][j].x - g.centerX);
				if (d < minDist) { minDist = d; centerJ = j; }
			}

			for (int k = centerJ - 1; k >= 0; k--)
				if (coloredScan[i][k].g == 255 && coloredScan[i][k].r == 0) {
					p = coloredScan[i][k]; 
					coloredScan[i][k].r = 0.0f; coloredScan[i][k].g = 0.0f; coloredScan[i][k].b = 255.0f; break;
				}

			line.push_back(p);
			

		}
		groove.push_back(line);
		out.push_back(groove);
	}
	return out;
}