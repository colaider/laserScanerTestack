/**
 * @file PLYWriter.h
 * @brief Static utility class for writing PLY (Polygon File Format) point clouds.
 * 
 * Provides functions to export point cloud data in PLY format, which is widely
 * supported by 3D visualization software (MeshLab, CloudCompare, Blender, etc.).
 * 
 * ## Output File Types:
 * 
 * | Function      | Output               | Use Case                          |
 * |---------------|----------------------|-----------------------------------|
 * | WriteRaw      | XYZ only (white)     | Raw scanner data before analysis  |
 * | WriteAnalyzed | XYZ + RGB + comments | Colorized results with metadata   |
 * 
 * ## PLY Format Overview:
 * ```
 * ply
 * format ascii 1.0
 * comment <optional metadata>
 * element vertex <count>
 * property float x
 * property float y
 * property float z
 * property uchar red      (WriteAnalyzed only)
 * property uchar green    (WriteAnalyzed only)
 * property uchar blue     (WriteAnalyzed only)
 * end_header
 * <x> <y> <z> [<r> <g> <b>]
 * ...
 * ```
 * 
 * ## Typical Usage:
 * @code
 *     // Write raw point cloud
 *     std::vector<PointXYZ> rawPoints = ...;
 *     PLYWriter::WriteRaw("Segment_0_Raw.ply", rawPoints);
 *     
 *     // Write analyzed point cloud with colors and comments
 *     std::vector<ColorPoint> coloredPoints = ...;
 *     std::vector<std::string> comments = {"Groove #1 Area: 12.34 mm^2"};
 *     PLYWriter::WriteAnalyzed("Segment_0_Analyzed.ply", coloredPoints, comments);
 * @endcode
 * 
 * @version 1.0
 * @date 2026-02-05
 * 
 * @see MeasurementManager::ProcessingLoop() - calls WriteRaw()
 * @see MetrologyEngine::AnalyzeSegment() - calls WriteAnalyzed()
 */

#pragma once

#include <string>
#include <vector>
#include "GetProfiles_Callback_Measure.h"  // Required for PointXYZ and ColorPoint structs

/**
 * @class PLYWriter
 * @brief Static utility class for exporting point clouds to PLY format.
 * 
 * All methods are static - no instantiation required. The class serves as
 * a namespace for related file I/O functions.
 * 
 * ## Thread Safety:
 * Not thread-safe for concurrent writes to the same file.
 * Safe for concurrent writes to different files.
 */
class PLYWriter {
public:
    /**
     * @brief Writes a raw XYZ point cloud without colors.
     * 
     * Exports point coordinates only (no RGB data). Used to save the
     * unprocessed scanner output before analysis for debugging and
     * archival purposes.
     * 
     * ## Output Format:
     * - ASCII PLY with X, Y, Z float properties
     * - No color properties
     * - No header comments
     * 
     * @param[in] filepath  Full path to output file (e.g., "Session/Segment_0_Raw.ply")
     * @param[in] points    Vector of PointXYZ coordinates to write
     * 
     * @return true if file was written successfully, false on I/O error
     * 
     * @note Creates/overwrites the file at the specified path.
     * @note Parent directory must exist.
     * 
     * @see MeasurementManager::ProcessingLoop() for typical usage
     */
    static bool WriteRaw(const std::string& filepath,
                         const std::vector<PointXYZ>& points);

    /**
     * @brief Writes a colorized point cloud with RGB values and header comments.
     * 
     * Exports the fully analyzed point cloud with color-coded classifications
     * and measurement metadata embedded in the PLY header as comments.
     * 
     * ## Output Format:
     * - ASCII PLY with X, Y, Z float properties
     * - RGB unsigned char properties (0-255)
     * - Header comments with measurement results
     * 
     * ## Color Coding Convention:
     * | Color  | RGB         | Meaning                              |
     * |--------|-------------|--------------------------------------|
     * | Green  | (0,255,0)   | Valid surface points                 |
     * | Red    | (255,0,0)   | Defect points included in measurement|
     * | Blue   | (0,0,255)   | Defect points excluded (cropped)     |
     * | White  | (255,255,255)| 3D text labels                      |
     * 
     * ## Example Header Comments:
     * ```
     * comment Groove #1 Area: 12.3456 mm^2
     * comment Groove #2 Area: 8.7654 mm^2
     * ```
     * 
     * @param[in] filepath  Full path to output file (e.g., "Session/Segment_0_Analyzed.ply")
     * @param[in] points    Vector of ColorPoint structures with XYZ and RGB data
     * @param[in] comments  Vector of strings to embed as PLY header comments
     * 
     * @return true if file was written successfully, false on I/O error
     * 
     * @note Creates/overwrites the file at the specified path.
     * @note Parent directory must exist.
     * @note Comments are prefixed with "comment " in the PLY header automatically.
     * 
     * @see MetrologyEngine::AnalyzeSegment() for typical usage
     */
    static bool WriteAnalyzed(const std::string& filepath,
                              const std::vector<ColorPoint>& points,
                              const std::vector<std::string>& comments);
};