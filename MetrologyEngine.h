/**
 * @file MetrologyEngine.h
 * @brief Core analysis engine for 3D surface defect detection and measurement.
 * * This header defines the MetrologyEngine class, which provides the computational
 * algorithms for analyzing laser scanner point cloud data. The engine supports
 * two primary analysis modes:
 * * - **GROOVE Mode**: Detects and measures surface depressions (cuts, scratches, grooves)
 * - **HOLE Mode**: Detects and measures through-holes by identifying gaps in surface data
 * * ## Key Capabilities:
 * - Profile-based leveling (Median Filter) to handle warped/stepped surfaces
 * - Grid-based morphological operations for noise removal
 * - Connected-component clustering for defect isolation
 * - Trapezoidal integration for accurate area measurement
 * - 3D text label generation for visual identification
 * * ## Usage:
 * All methods are static - no instantiation required:
 * @code
 * auto result = MetrologyEngine::AnalyzeSegment(segmentId, points, ...);
 * std::cout << "Found " << result.grooveCount << " defects\n";
 * @endcode
 * * @version 22.3 (Median Leveling)
 * @date 2026-02-06
 * * @see MetrologyEngine.cpp for implementation details
 * @see MeasurementManager for high-level orchestration
 */

#pragma once

#include <vector>
#include <string>
#include <map>
#include "GetProfiles_Callback_Measure.h"  // For ColorPoint, GridCell, PointXYZ, Centroid
#include "MeasurementManager.h"            // For MeasurementManager::AnalysisMode enum

 /**
  * @class MetrologyEngine
  * @brief Static utility class containing all metrology algorithms.
  */
class MetrologyEngine {
public:
    // =======================================================================
    // ALGORITHM TUNING CONSTANTS
    // =======================================================================

    static const int RANSAC_ITERATIONS = 1500;
    static constexpr double RANSAC_THRESHOLD = 0.05;

    /**
     * @brief Depth offset below surface to define the "cut plane" (mm).
     * Points with Z < (baseZ - PLANE_DEPTH_OFFSET) are classified as defects.
     */
    static constexpr double PLANE_DEPTH_OFFSET = 0.22;

    static const int EROSION_ITERATIONS = 2;
    static constexpr double MIN_GROOVE_AREA_MM2 = 0.1;
    static constexpr double GRID_CELL_SIZE = 0.1;
    static const int MIN_POINTS_PER_CELL = 1;

    // =======================================================================
    // DATA STRUCTURES
    // =======================================================================

    struct AnalysisResult {
        std::map<int, double> grooveAreas;
        std::map<int, Centroid> centroids;
        std::map<int, double> clusterStartY;
        int grooveCount;
    };

    // =======================================================================
    // SURFACE CORRECTION
    // =======================================================================

    /**
     * @brief Corrects surface tilt using RANSAC (Legacy method).
     * Kept for reference or flat parts.
     */
    static void ApplyRANSACLevelling(std::vector<ColorPoint>& points, double baseZ);

    /**
     * @brief Levels warped, curved, or stepped surfaces using a Strided Median Filter.
     * * Unlike RANSAC (which fits a flat global plane), this method calculates
     * a local reference surface for every point by looking at its neighbors.
     * * ## Capabilities:
     * - **Warped Parts**: Tracks curves (like bones) perfectly.
     * - **Stepped Edges**: Preserves sharp drops/jumps without smoothing them.
     * - **Grooves**: Bridges over defects using median logic (ignores depth).
     * * @param[in,out] points  Vector of ColorPoints to level (Z modified in-place)
     * @param[in]     baseZ   Target Z-height
     */
    static void ApplyProfileLevelling(std::vector<ColorPoint>& points, double baseZ);

    // =======================================================================
    // MORPHOLOGICAL OPERATIONS
    // =======================================================================

    static void ErodeGrid(std::vector<std::vector<GridCell>>& grid, int w, int h);
    static void DilateGrid(std::vector<std::vector<GridCell>>& grid, int w, int h);

    // =======================================================================
    // VISUALIZATION
    // =======================================================================

    static void Generate3DText(std::string text, float centerX, float centerY, float centerZ, std::vector<ColorPoint>& outPoints);

    // =======================================================================
    // MAIN ANALYSIS PIPELINE
    // =======================================================================

    static AnalysisResult AnalyzeSegment(
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
        MeasurementManager::AnalysisMode mode);
};