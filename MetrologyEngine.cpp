/**
 * @file MetrologyEngine.cpp
 * @brief Core algorithms for 3D point cloud analysis and defect detection.
 * @version 22.11 (Pure Local Envelope for X/Y Warp)
 */

#include "MetrologyEngine.h"
#include "CSVWriter.h"
#include "PLYWriter.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cmath>
#include <random>
#include <iomanip>
#include <queue>
#include <set>
#include <limits> 
#include <map>
#include <algorithm>

 // ===========================================================================
 // FONT DATA 
 // ===========================================================================
const unsigned char FONT_DIGITS[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x11, 0x0E}
};
const unsigned char FONT_DOT[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06 };

// ===========================================================================
// RANSAC PLANE FITTING (Legacy)
// ===========================================================================
void MetrologyEngine::ApplyRANSACLevelling(std::vector<ColorPoint>& points, double baseZ) {
    if (points.empty()) return;
    std::vector<ColorPoint*> candidates;
    candidates.reserve(points.size());
    double searchMin = baseZ - 15.0;
    double searchMax = baseZ + 15.0;

    for (size_t i = 0; i < points.size(); ++i) {
        if (!std::isfinite(points[i].z)) continue;
        if (points[i].z > searchMin && points[i].z < searchMax) {
            candidates.push_back(&points[i]);
        }
    }
    if (candidates.size() < 100) return;

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);

    double bestA = 0, bestB = 0, bestC = 0;
    int bestScore = -1;

    for (int i = 0; i < RANSAC_ITERATIONS; ++i) {
        ColorPoint* p1 = candidates[dist(rng)];
        ColorPoint* p2 = candidates[dist(rng)];
        ColorPoint* p3 = candidates[dist(rng)];

        double v1x = p2->x - p1->x; double v1y = p2->y - p1->y; double v1z = p2->z - p1->z;
        double v2x = p3->x - p1->x; double v2y = p3->y - p1->y; double v2z = p3->z - p1->z;

        double nx = v1y * v2z - v1z * v2y;
        double ny = v1z * v2x - v1x * v2z;
        double nz = v1x * v2y - v1y * v2x;
        double len = sqrt(nx * nx + ny * ny + nz * nz);

        if (len < 1e-6 || abs(nz / len) < 0.1) continue;
        nx /= len; ny /= len; nz /= len;

        double A = -nx / nz; double B = -ny / nz;
        double d = -(nx * p1->x + ny * p1->y + nz * p1->z);
        double C = -d / nz;

        int score = 0;
        for (size_t k = 0; k < candidates.size(); k += 10) {
            double predictedZ = A * candidates[k]->x + B * candidates[k]->y + C;
            if (abs(candidates[k]->z - predictedZ) < RANSAC_THRESHOLD) score++;
        }
        if (score > bestScore) {
            bestScore = score; bestA = A; bestB = B; bestC = C;
        }
    }

    for (auto& p : points) {
        double planeZ = bestA * p.x + bestB * p.y + bestC;
        p.z = static_cast<float>(baseZ + (p.z - planeZ));
    }
}

// ===========================================================================
// PROFILE-BASED LEVELING (UPPER ENVELOPE + SPATIAL ALIGNMENT)
// ===========================================================================
void MetrologyEngine::ApplyProfileLevelling(std::vector<ColorPoint>& points, double baseZ) {
    if (points.empty()) return;

    std::cout << "[Leveling] Applying Pure Local Envelope (85th Percentile) Smoothing...\n";

    double minX = 1e9, maxX = -1e9;
    for (const auto& p : points) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
    }
    minX -= 1.0; maxX += 1.0;
    double rangeX = maxX - minX;

    const int NUM_BINS = 2048;
    // Note: We no longer depend on 'historyGrid' for value clamping, only initialization check

    std::map<int, std::vector<ColorPoint*>> rows;
    for (auto& p : points) {
        rows[p.row].push_back(&p);
    }

    // [FIX] Widened Window Radius to 400.
    // Since we are removing the history clamp, we need a wider bridge to ensure
    // the envelope doesn't fall into wide grooves.
    const int WINDOW_RADIUS = 400;
    const int STEP_SIZE = 5;
    const double PERCENTILE_RANK = 0.85;

    for (auto& rowEntry : rows) {
        std::vector<ColorPoint*>& rowPts = rowEntry.second;

        if (rowPts.size() < (size_t)WINDOW_RADIUS * 2) continue;

        std::sort(rowPts.begin(), rowPts.end(), [](ColorPoint* a, ColorPoint* b) {
            return a->x < b->x;
            });

        size_t n = rowPts.size();
        std::vector<double> windowBuffer;
        windowBuffer.reserve(WINDOW_RADIUS * 2 + 1);

        std::vector<std::pair<int, double>> sparseNodes;
        sparseNodes.reserve((n / STEP_SIZE) + 2);

        for (size_t i = 0; i < n; i += STEP_SIZE) {
            windowBuffer.clear();
            size_t start = (i > (size_t)WINDOW_RADIUS) ? i - WINDOW_RADIUS : 0;
            size_t end = (std::min)(n, i + WINDOW_RADIUS + 1);

            for (size_t j = start; j < end; ++j) {
                windowBuffer.push_back(rowPts[j]->z);
            }

            if (!windowBuffer.empty()) {
                size_t rankIdx = static_cast<size_t>(windowBuffer.size() * PERCENTILE_RANK);
                if (rankIdx >= windowBuffer.size()) rankIdx = windowBuffer.size() - 1;

                std::nth_element(windowBuffer.begin(), windowBuffer.begin() + rankIdx, windowBuffer.end());
                sparseNodes.push_back({ (int)i, windowBuffer[rankIdx] });
            }
        }

        if (sparseNodes.empty()) continue;

        // [FIX] Pure Local Envelope Construction
        // We reconstruct the surface reference ('currentGrid') entirely from the current row's data.
        // This allows the reference plane to bend freely with X and Y warp, 
        // accurately tracking 'walls' that might be physically lower than previous rows.

        std::vector<double> currentGrid(NUM_BINS);

        auto GetNodeX = [&](int nodeIdx) { return rowPts[sparseNodes[nodeIdx].first]->x; };

        int nodeIdx = 0;
        for (int b = 0; b < NUM_BINS; ++b) {
            double binX = minX + ((double)b / (NUM_BINS - 1)) * rangeX;

            while (nodeIdx < sparseNodes.size() - 1 && GetNodeX(nodeIdx + 1) < binX) {
                nodeIdx++;
            }

            double x1 = GetNodeX(nodeIdx);
            double z1 = sparseNodes[nodeIdx].second;

            if (nodeIdx >= sparseNodes.size() - 1) {
                currentGrid[b] = z1;
            }
            else {
                double x2 = GetNodeX(nodeIdx + 1);
                double z2 = sparseNodes[nodeIdx + 1].second;
                double t = (binX - x1) / (x2 - x1);
                t = (t < 0.0) ? 0.0 : (t > 1.0 ? 1.0 : t);
                currentGrid[b] = z1 + t * (z2 - z1);
            }
        }

        // Apply Leveling using the Local Envelope
        for (size_t i = 0; i < n; ++i) {
            double normalizedPos = (rowPts[i]->x - minX) / rangeX;
            int bin = static_cast<int>(normalizedPos * (NUM_BINS - 1));

            if (bin < 0) bin = 0;
            if (bin >= NUM_BINS) bin = NUM_BINS - 1;

            double referenceZ = currentGrid[bin];
            double deviation = rowPts[i]->z - referenceZ;

            rowPts[i]->z = static_cast<float>(baseZ + deviation);
        }
    }
}

// ===========================================================================
// MORPHOLOGICAL OPERATIONS
// ===========================================================================
void MetrologyEngine::ErodeGrid(std::vector<std::vector<GridCell>>& grid, int w, int h) {
    std::vector<std::pair<int, int>> toRemove;
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            if (grid[x][y].active) {
                int neighborCount = 0;
                for (int nx = x - 1; nx <= x + 1; ++nx) {
                    for (int ny = y - 1; ny <= y + 1; ++ny) {
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h && !(nx == x && ny == y)) {
                            if (grid[nx][ny].active) neighborCount++;
                        }
                    }
                }
                if (neighborCount < 8) toRemove.push_back({ x, y });
            }
        }
    }
    for (const auto& pos : toRemove) grid[pos.first][pos.second].active = false;
}

void MetrologyEngine::DilateGrid(std::vector<std::vector<GridCell>>& grid, int w, int h) {
    std::vector<std::pair<int, int>> toAdd;
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y) {
            if (grid[x][y].active) {
                for (int nx = x - 1; nx <= x + 1; ++nx) {
                    for (int ny = y - 1; ny <= y + 1; ++ny) {
                        if (nx >= 0 && nx < w && ny >= 0 && ny < h && !grid[nx][ny].active) {
                            toAdd.push_back({ nx, ny });
                        }
                    }
                }
            }
        }
    }
    for (const auto& pos : toAdd) grid[pos.first][pos.second].active = true;
}

// ===========================================================================
// VISUALIZATION
// ===========================================================================
void MetrologyEngine::Generate3DText(std::string text, float centerX, float centerY, float centerZ, std::vector<ColorPoint>& outPoints) {
    float pixelSize = 0.25f; float density = 0.04f;
    float charWidth = 5.0f * pixelSize; float charHeight = 7.0f * pixelSize; float spacing = 0.25f * pixelSize;
    float totalWidth = (float)text.length() * charWidth + ((float)text.length() - 1.0f) * spacing;
    float cursorX = centerX - (totalWidth / 2.0f); float topY = centerY + (charHeight / 2.0f);

    for (char c : text) {
        const unsigned char* currentMap = nullptr;
        if (c >= '0' && c <= '9') currentMap = FONT_DIGITS[c - '0'];
        else if (c == '.') currentMap = FONT_DOT;
        else { cursorX += (charWidth + spacing); continue; }

        for (int r = 0; r < 7; r++) {
            unsigned char rowByte = currentMap[r];
            float pixY = topY - (r * pixelSize);
            for (int col = 0; col < 5; col++) {
                if ((rowByte >> (4 - col)) & 0x01) {
                    float pixX = cursorX + (col * pixelSize);
                    for (float dx = 0; dx < pixelSize; dx += density) {
                        for (float dy = 0; dy < pixelSize; dy += density) {
                            ColorPoint p; p.x = pixX + dx; p.y = pixY - dy; p.z = centerZ;
                            p.r = 255; p.g = 255; p.b = 255; p.isGroove = false;
                            outPoints.push_back(p);
                        }
                    }
                }
            }
        }
        cursorX += (charWidth + spacing);
    }
}

// ===========================================================================
// MAIN ANALYSIS PIPELINE
// ===========================================================================
MetrologyEngine::AnalysisResult MetrologyEngine::AnalyzeSegment(
    int segmentId,
    const std::vector<PointXYZ>& inputPoints,
    const std::string& outputFile,
    double xRes,
    double yRes,
    double minZ,
    double maxZ,
    double baseZ,
    double measurementLength,
    double grooveCropEnd,
    const std::string& sessionFolder,
    const std::string& summaryPath,
    MeasurementManager::AnalysisMode mode) {

    std::cout << "\n--- DEBUG: Starting Analysis Segment " << segmentId << " ---\n";
    std::cout << "Config: BaseZ=" << baseZ << " CropEnd=" << grooveCropEnd << " Length=" << measurementLength << "\n";

    const double SURFACE_TOLERANCE = 0.5;
    const double BOUNDARY_BUFFER = 0.1;

    // STEP 1: CONVERSION & FILTERING
    std::vector<ColorPoint> points;
    points.reserve(inputPoints.size() + 200000);
    double minX = 1e6, maxX = -1e6, minY = 1e6, maxY = -1e6;

    for (const auto& ip : inputPoints) {
        if (!std::isfinite(ip.x) || !std::isfinite(ip.y) || !std::isfinite(ip.z)) continue;
        if (ip.z > minZ && ip.z < maxZ) {
            ColorPoint cp; cp.x = ip.x; cp.y = ip.y; cp.z = ip.z;
            cp.r = 255; cp.g = 255; cp.b = 255; cp.isGroove = false;
            cp.row = static_cast<int>(ip.y / yRes);
            points.push_back(cp);
            if (ip.x < minX) minX = ip.x; if (ip.x > maxX) maxX = ip.x;
            if (ip.y < minY) minY = ip.y; if (ip.y > maxY) maxY = ip.y;
        }
    }
    if (points.empty()) return AnalysisResult();

    // STEP 2: PROFILE LEVELING (MEDIAN FILTER)
    MetrologyEngine::ApplyProfileLevelling(points, baseZ);

    // STEP 3: BUILD SURFACE INDEX (for HOLE mode)
    std::map<int, std::vector<float>> rowSurfaceX;
    // [USER REQUEST] Enforce tight 0.2mm tolerance for valid surface points used in gap detection
    const double Z_FILTER_THRESHOLD = 0.2;

    int surfacePointCount = 0;
    for (const auto& p : points) {
        if (std::abs(p.z - baseZ) <= Z_FILTER_THRESHOLD) {
            rowSurfaceX[p.row].push_back(p.x);
            surfacePointCount++;
        }
    }
    // IMPORTANT: Sort the surface points X-coordinates for gap detection
    for (auto& row : rowSurfaceX) std::sort(row.second.begin(), row.second.end());

    std::cout << "[DEBUG] Surface Index Built. Valid Surface Points: " << surfacePointCount << "\n";

    // STEP 4: BOUNDARY DETECTION
    double surfXMin = 1e6, surfXMax = -1e6, surfYMin = 1e6, surfYMax = -1e6;
    bool surfaceDetected = false;
    for (const auto& p : points) {
        if (std::abs(p.z - baseZ) <= SURFACE_TOLERANCE) {
            if (p.x < surfXMin) surfXMin = p.x; if (p.x > surfXMax) surfXMax = p.x;
            if (p.y < surfYMin) surfYMin = p.y; if (p.y > surfYMax) surfYMax = p.y;
            surfaceDetected = true;
        }
    }
    surfXMin += BOUNDARY_BUFFER; surfXMax -= BOUNDARY_BUFFER;
    surfYMin += BOUNDARY_BUFFER; surfYMax -= BOUNDARY_BUFFER;

    // STEP 5: GRID INIT
    int gridW = static_cast<int>((maxX - minX) / GRID_CELL_SIZE) + 5;
    int gridH = static_cast<int>((maxY - minY) / GRID_CELL_SIZE) + 5;
    std::vector<std::vector<bool>> isSurfaceMap(gridW, std::vector<bool>(gridH, false));
    std::vector<std::vector<GridCell>> grid(gridW, std::vector<GridCell>(gridH));
    double cutPlaneZ = baseZ - PLANE_DEPTH_OFFSET;

    // STEP 6: GRID POPULATION
    for (auto& p : points) {
        p.r = 0; p.g = 255; p.b = 0; // Default Surface Green
        int gx = static_cast<int>((p.x - minX) / GRID_CELL_SIZE);
        int gy = static_cast<int>((p.y - minY) / GRID_CELL_SIZE);

        if (gx >= 0 && gx < gridW && gy >= 0 && gy < gridH) {
            if (mode == MeasurementManager::AnalysisMode::GROOVE) {
                if (p.z < cutPlaneZ) {
                    grid[gx][gy].ptCount++;
                    grid[gx][gy].points.push_back(&p);
                    if (grid[gx][gy].ptCount >= MIN_POINTS_PER_CELL) grid[gx][gy].active = true;
                }
            }
            else if (mode == MeasurementManager::AnalysisMode::HOLE) {
                // [FIX] Relaxed Wall Detection:
                // We use a deeper threshold (-2.0mm) here to detect walls between holes
                // even if they are warped or low. This prevents holes from merging into one blob.
                if (p.z >= (baseZ - 2.0)) isSurfaceMap[gx][gy] = true;
            }
        }
    }

    // --- HOLE FILLING (Still needed for localization, but not measurement) ---
    if (mode == MeasurementManager::AnalysisMode::HOLE && surfaceDetected) {
        for (int x = 0; x < gridW; ++x) {
            for (int y = 0; y < gridH; ++y) {
                double cellStartX = minX + (x * GRID_CELL_SIZE);
                double cellStartY = minY + (y * GRID_CELL_SIZE);
                double centerX = cellStartX + (GRID_CELL_SIZE / 2.0);
                double centerY = cellStartY + (GRID_CELL_SIZE / 2.0);
                if (centerX > surfXMin && centerX < surfXMax && centerY > surfYMin && centerY < surfYMax) {
                    if (!isSurfaceMap[x][y]) {
                        grid[x][y].active = true;
                        // Fill grid cell with synthetic points at exact scan resolution
                        for (double sx = cellStartX; sx < cellStartX + GRID_CELL_SIZE; sx += xRes) {
                            for (double sy = cellStartY; sy < cellStartY + GRID_CELL_SIZE; sy += yRes) {
                                ColorPoint* synth = new ColorPoint();
                                synth->x = (float)sx; synth->y = (float)sy; synth->z = (float)baseZ;
                                synth->r = 255; synth->g = 0; synth->b = 0;
                                synth->row = static_cast<int>(sy / yRes);
                                synth->isGroove = true;
                                grid[x][y].points.push_back(synth);
                            }
                        }
                    }
                }
            }
        }
    }

    // STEP 7: CLUSTERING
    // [FIX] Separation Passes:
    // We use 3 morphological passes (Erode+Dilate) to cleanly separate touching blobs.
    // This snaps the "thin bridges" seen in the image.
    const int MORPH_ITERATIONS = 3;

    for (int i = 0; i < MORPH_ITERATIONS; ++i) MetrologyEngine::ErodeGrid(grid, gridW, gridH);

    int currentClusterId = 1;
    for (int x = 0; x < gridW; ++x) {
        for (int y = 0; y < gridH; ++y) {
            if (grid[x][y].active && grid[x][y].clusterId == 0) {
                std::queue<std::pair<int, int>> q; q.push({ x, y });
                grid[x][y].clusterId = currentClusterId;
                while (!q.empty()) {
                    auto curr = q.front(); q.pop();
                    for (int nx = curr.first - 1; nx <= curr.first + 1; ++nx) {
                        for (int ny = curr.second - 1; ny <= curr.second + 1; ++ny) {
                            if (nx >= 0 && nx < gridW && ny >= 0 && ny < gridH &&
                                grid[nx][ny].active && grid[nx][ny].clusterId == 0) {
                                grid[nx][ny].clusterId = currentClusterId; q.push({ nx, ny });
                            }
                        }
                    }
                }
                currentClusterId++;
            }
        }
    }
    // Dilation restores the size of the blobs after separation
    for (int i = 0; i < MORPH_ITERATIONS; ++i) MetrologyEngine::DilateGrid(grid, gridW, gridH);
    for (int i = 0; i < MORPH_ITERATIONS; ++i) { // Heal
        for (int x = 0; x < gridW; ++x) {
            for (int y = 0; y < gridH; ++y) {
                if (grid[x][y].active && grid[x][y].clusterId == 0) {
                    for (int nx = x - 1; nx <= x + 1; ++nx) {
                        for (int ny = y - 1; ny <= y + 1; ++ny) {
                            if (nx >= 0 && nx < gridW && ny >= 0 && ny < gridH && grid[nx][ny].clusterId > 0) {
                                grid[x][y].clusterId = grid[nx][ny].clusterId; goto healed;
                            }
                        }
                    }
                }
            healed:;
            }
        }
    }

    // STEP 8: DATA GATHERING
    std::map<int, double> clusterStartY, clusterEndY;
    std::map<int, std::map<int, std::pair<double, double>>> sliceBounds;
    std::map<int, Centroid> clusterCentroids;
    std::set<int> validClusters;
    std::map<int, int> clusterSizes;
    int minCells = static_cast<int>(MIN_GROOVE_AREA_MM2 / (GRID_CELL_SIZE * GRID_CELL_SIZE));

    for (int x = 0; x < gridW; ++x) {
        for (int y = 0; y < gridH; ++y) {
            int id = grid[x][y].clusterId;
            if (id > 0) clusterSizes[id]++;
        }
    }
    for (auto const& entry : clusterSizes) {
        if (entry.second >= minCells) validClusters.insert(entry.first);
    }

    for (int x = 0; x < gridW; ++x) {
        for (int y = 0; y < gridH; ++y) {
            int cId = grid[x][y].clusterId;
            if (cId > 0 && validClusters.count(cId)) {
                for (auto* p : grid[x][y].points) {
                    if (clusterStartY.find(cId) == clusterStartY.end() || p->y < clusterStartY[cId]) clusterStartY[cId] = p->y;
                    if (clusterEndY.find(cId) == clusterEndY.end() || p->y > clusterEndY[cId]) clusterEndY[cId] = p->y;
                }
            }
        }
    }

    for (int x = 0; x < gridW; ++x) {
        for (int y = 0; y < gridH; ++y) {
            int cId = grid[x][y].clusterId;
            if (cId > 0 && validClusters.count(cId)) {
                double endCropY = clusterEndY[cId] - grooveCropEnd;
                double startCropY = clusterStartY[cId];
                if (measurementLength > 0.001) {
                    startCropY = endCropY - measurementLength;
                    if (startCropY < clusterStartY[cId]) startCropY = clusterStartY[cId];
                }

                for (auto* p : grid[x][y].points) {
                    p->isGroove = true;
                    if (p->y < startCropY || p->y > endCropY) { p->r = 0; p->g = 0; p->b = 255; }
                    else {
                        p->r = 255; p->g = 0; p->b = 0;
                        clusterCentroids[cId].sumX += p->x;
                        clusterCentroids[cId].sumY += p->y;
                        clusterCentroids[cId].sumZ += p->z;
                        clusterCentroids[cId].count++;
                        if (sliceBounds[cId].find(p->row) == sliceBounds[cId].end()) sliceBounds[cId][p->row] = { p->x, p->x };
                        else {
                            sliceBounds[cId][p->row].first = std::min<double>(sliceBounds[cId][p->row].first, p->x);
                            sliceBounds[cId][p->row].second = std::max<double>(sliceBounds[cId][p->row].second, p->x);
                        }
                    }
                }
            }
        }
    }

    // STEP 9: AREA CALCULATION
    std::map<int, double> finalAreas;
    const double MASTER_SLOPE = 1.0097;
    const double MASTER_INTERCEPT = -0.0284;

    for (auto const& cluster : sliceBounds) {
        int cId = cluster.first;
        double area = 0.0;
        double endCropY = clusterEndY[cId] - grooveCropEnd;
        double startCropY = clusterStartY[cId];
        if (measurementLength > 0.001) {
            startCropY = endCropY - measurementLength;
            if (startCropY < clusterStartY[cId]) startCropY = clusterStartY[cId];
        }

        double prevWidth = 0.0; double prevY = 0.0; bool hasPrev = false;

        // --- TRAPEZOIDAL AREA CALCULATION ---
        for (auto const& row : cluster.second) {
            double y = static_cast<double>(row.first) * yRes;
            double w = 0.0;

            if (mode == MeasurementManager::AnalysisMode::HOLE) {
                // [MAXIMUM GAP METHOD]
                // 1. We know the rough bounds of the hole from the red dots (row.second.first/second)
                // 2. We expand that search window SIGNIFICANTLY (2.5x) to ensure we catch the true edge
                //    even if the synthetic grid fill was conservative or the hole is irregular.
                // 3. We iterate through the SURFACE points in that window and find the LARGEST GAP.

                double searchMinX = row.second.first - (GRID_CELL_SIZE * 2.5);
                double searchMaxX = row.second.second + (GRID_CELL_SIZE * 2.5);

                double maxGap = 0.0;
                // Fallback width (if no surface found)
                double syntheticWidth = (row.second.second - row.second.first) + xRes;

                if (rowSurfaceX.count(row.first)) {
                    auto& sRow = rowSurfaceX[row.first];

                    // Find iterator to the first point >= searchMinX
                    auto it = std::lower_bound(sRow.begin(), sRow.end(), searchMinX);

                    // Iterate until we pass searchMaxX
                    if (it != sRow.end()) {
                        double prevX = *it;
                        ++it;
                        while (it != sRow.end() && *it <= searchMaxX) {
                            double currX = *it;
                            double gap = currX - prevX;
                            if (gap > maxGap) maxGap = gap;
                            prevX = currX;
                            ++it;
                        }
                    }
                }

                // If a significant gap was found (e.g., > 1mm), we use it. 
                // Otherwise we fallback to synthetic if the surface was missing/noisy.
                if (maxGap > 1.0) {
                    w = maxGap;
                }
                else {
                    w = syntheticWidth;
                }
            }
            else {
                // GROOVE MODE: Use synthetic width directly as grooves are filled with points
                w = (row.second.second - row.second.first) + xRes;
            }

            if (y >= startCropY && y <= endCropY) {
                if (hasPrev && (y - prevY) < 0.15) area += ((w + prevWidth) / 2.0) * (y - prevY);
                prevY = y; prevWidth = w; hasPrev = true;
            }
            else hasPrev = false;
        }

        finalAreas[cId] = (area - MASTER_INTERCEPT) / MASTER_SLOPE;
    }

    // STEP 10: REPORTING
    std::vector<std::string> plyComments;
    int idx = 1;
    for (auto const& areaEntry : finalAreas) {
        int id = areaEntry.first; double area = areaEntry.second;
        std::stringstream ss; std::string type = (mode == MeasurementManager::AnalysisMode::HOLE) ? "Hole" : "Groove";
        ss << type << " #" << idx << " Area: " << std::fixed << std::setprecision(4) << area << " mm^2";
        plyComments.push_back(ss.str());
        CSVWriter::LogGroove(summaryPath, segmentId, idx, area, clusterCentroids[id].count);
        if (clusterCentroids[id].count > 0) {
            auto& c = clusterCentroids[id];
            std::string label = std::to_string(segmentId) + "." + std::to_string(idx);
            float tx = (float)(c.sumX / c.count); float ty = (float)(clusterStartY[id] - 5.0); float tz = (float)(c.sumZ / c.count + 5.0);
            MetrologyEngine::Generate3DText(label, tx, ty, tz, points);
        }
        idx++;
    }

    // For holes, we need to push the synthetic points into the main buffer for saving
    if (mode == MeasurementManager::AnalysisMode::HOLE) {
        for (int x = 0; x < gridW; ++x) {
            for (int y = 0; y < gridH; ++y) {
                if (grid[x][y].active && grid[x][y].clusterId > 0) {
                    for (auto* p : grid[x][y].points) { points.push_back(*p); delete p; }
                }
            }
        }
    }

    std::string fullPath = sessionFolder + "\\" + outputFile;
    PLYWriter::WriteAnalyzed(fullPath, points, plyComments);

    AnalysisResult finalResult;
    finalResult.grooveAreas = finalAreas;
    finalResult.centroids = clusterCentroids;
    finalResult.clusterStartY = clusterStartY;
    finalResult.grooveCount = static_cast<int>(finalAreas.size());
    return finalResult;
}