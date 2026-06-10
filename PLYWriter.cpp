/**
 * @file PLYWriter.cpp
 * @brief Implementation of PLY (Polygon File Format) point cloud export functions.
 *
 * This module provides file I/O for exporting point cloud data to PLY format,
 * a widely supported 3D file format readable by MeshLab, CloudCompare, Blender,
 * and most point cloud processing software.
 *
 * ## PLY Format Details:
 * - **Format**: ASCII 1.0 (human-readable, larger file size)
 * - **Precision**: 4 decimal places for coordinates
 * - **Colors**: 8-bit unsigned integers (0-255) for RGB
 *
 * ## Output Files:
 * | File Pattern              | Function      | Contents                    |
 * |---------------------------|---------------|-----------------------------|
 * | Segment_X_Raw.ply         | WriteRaw      | XYZ coordinates only        |
 * | Segment_X_Analyzed.ply    | WriteAnalyzed | XYZ + RGB + measurement data|
 *
 * @version 1.0
 * @date 2026-02-05
 *
 * @see PLYWriter.h for function declarations
 * @see https://en.wikipedia.org/wiki/PLY_(file_format) for format specification
 */

#include "PLYWriter.h"
#include <fstream>
#include <iomanip>
#include <iostream>

 // ===========================================================================
 // RAW POINT CLOUD EXPORT
 // ===========================================================================

 /**
  * @brief Writes a raw XYZ point cloud without colors.
  *
  * Exports unprocessed scanner data for debugging and archival.
  * Creates an ASCII PLY file with X, Y, Z float properties only.
  *
  * ## Output Example:
  * ```
  * ply
  * format ascii 1.0
  * element vertex 12345
  * property float x
  * property float y
  * property float z
  * end_header
  * -5.1234 0.0000 95.1234
  * -5.1194 0.0400 95.1198
  * ...
  * ```
  *
  * @param[in] filepath  Full path to output file
  * @param[in] points    Vector of PointXYZ coordinates to write
  *
  * @return true if file was written successfully, false on I/O error
  *
  * @note File is opened with truncation - existing content is overwritten.
  * @note Coordinates are written with 4 decimal places precision.
  */
bool PLYWriter::WriteRaw(const std::string& filepath, const std::vector<PointXYZ>& points) {
    // Open file for writing (truncate if exists)
    std::ofstream out_(filepath, std::ios::out | std::ios::trunc);
    if (!out_.is_open()) {
        return false;
    }

    // -----------------------------------------------------------------------
    // PLY HEADER GENERATION
    // -----------------------------------------------------------------------
    // Standard PLY header declaring ASCII format and vertex properties

    out_ << "ply\n";
    out_ << "format ascii 1.0\n";
    out_ << "element vertex " << points.size() << "\n";
    out_ << "property float x\n";
    out_ << "property float y\n";
    out_ << "property float z\n";
    out_ << "end_header\n";

    // -----------------------------------------------------------------------
    // VERTEX DATA WRITING
    // -----------------------------------------------------------------------
    // Write each point as space-separated X Y Z values
    // Fixed precision (4 decimals) provides ~0.1 micron resolution

    out_ << std::fixed << std::setprecision(4);

    for (const auto& p : points) {
        out_ << p.x << " " << p.y << " " << p.z << "\n";
    }

    // Close and verify no errors occurred during writing
    out_.close();
    return !out_.fail();
}

// ===========================================================================
// ANALYZED POINT CLOUD EXPORT
// ===========================================================================

/**
 * @brief Writes a colorized point cloud with RGB values and header comments.
 *
 * Exports the fully analyzed point cloud with color-coded classifications
 * and measurement metadata embedded in the PLY header.
 *
 * ## Output Example:
 * ```
 * ply
 * format ascii 1.0
 * comment Groove #1 Area: 12.3456 mm^2
 * comment Groove #2 Area: 8.7654 mm^2
 * element vertex 54321
 * property float x
 * property float y
 * property float z
 * property uchar red
 * property uchar green
 * property uchar blue
 * end_header
 * -5.1234 0.0000 95.1234 0 255 0
 * -5.1194 0.0400 94.8000 255 0 0
 * ...
 * ```
 *
 * ## Color Encoding:
 * RGB values are written as integers (0-255), not raw bytes.
 * This is required because PLY ASCII format expects decimal representation.
 *
 * @param[in] filepath  Full path to output file
 * @param[in] points    Vector of ColorPoint structures with XYZ and RGB data
 * @param[in] comments  Vector of strings to embed as PLY header comments
 *
 * @return true if file was written successfully, false on I/O error
 *
 * @note File is opened with truncation - existing content is overwritten.
 * @note RGB values are cast to int to ensure decimal output (not ASCII characters).
 */
bool PLYWriter::WriteAnalyzed(const std::string& filepath,
    const std::vector<ColorPoint>& points,
    const std::vector<std::string>& comments) {

    // Open file for writing (truncate if exists)
    std::ofstream out_(filepath, std::ios::out | std::ios::trunc);
    if (!out_.is_open()) {
        return false;
    }

    // -----------------------------------------------------------------------
    // PLY HEADER GENERATION
    // -----------------------------------------------------------------------

    out_ << "ply\n";
    out_ << "format ascii 1.0\n";

    // Inject measurement results as header comments
    // These appear in PLY viewers and can be parsed by post-processing tools
    for (const auto& c : comments) {
        out_ << "comment " << c << "\n";
    }

    // Vertex element declaration with XYZ + RGB properties
    out_ << "element vertex " << points.size() << "\n";
    out_ << "property float x\n";
    out_ << "property float y\n";
    out_ << "property float z\n";
    out_ << "property uchar red\n";
    out_ << "property uchar green\n";
    out_ << "property uchar blue\n";
    out_ << "end_header\n";

    // -----------------------------------------------------------------------
    // VERTEX DATA WRITING
    // -----------------------------------------------------------------------
    // Write each point as: X Y Z R G B
    // 
    // IMPORTANT: RGB values must be cast to int before output.
    // Without the cast, unsigned char values are written as ASCII characters
    // (e.g., 255 would output as 'ÿ' instead of "255").

    out_ << std::fixed << std::setprecision(4);

    for (const auto& p : points) {
        out_ << p.x << " " << p.y << " " << p.z << " "
            << static_cast<int>(p.r) << " "
            << static_cast<int>(p.g) << " "
            << static_cast<int>(p.b) << "\n";
    }

    // Close and verify no errors occurred during writing
    out_.close();
    return !out_.fail();
}