/**
 * @file CSVWriter.cpp
 * @brief Utility for logging defect measurement data to CSV files.
 * 
 * This module provides simple, append-based CSV logging for recording
 * groove and hole measurements during analysis sessions. Each scan segment
 * can log multiple defects, with one row per defect.
 * 
 * ## Output Format:
 * ```
 * Segment_ID,Groove_Index,Area_mm2,Point_Count
 * 0,1,12.3456,5000
 * 0,2,8.7654,3200
 * 1,1,15.0000,6100
 * ```
 * 
 * ## Usage:
 * @code
 *     // At session start
 *     CSVWriter::Initialize("Session_2026-02-05/Summary.csv");
 *     
 *     // After each defect is measured
 *     CSVWriter::LogGroove(summaryPath, segmentId, grooveIdx, area, pointCount);
 * @endcode
 * 
 * @version 1.0
 * @date 2026-02-05
 * 
 * @see MetrologyEngine::AnalyzeSegment() - primary caller
 */

#include "CSVWriter.h"
#include <fstream>
#include <iostream>

/**
 * @brief Creates a new CSV file with the standard header row.
 * 
 * This function should be called once at the beginning of each measurement
 * session to create (or overwrite) the summary file. Subsequent measurements
 * are appended using LogGroove().
 * 
 * @param[in] filepath  Full path to the CSV file to create
 * 
 * @return true if file was created successfully, false on I/O error
 * 
 * @warning This will overwrite any existing file at the specified path.
 *          Call only once per session, not per segment.
 * 
 * @note The header format is: "Segment_ID,Groove_Index,Area_mm2,Point_Count"
 * 
 * @see LogGroove() for appending measurement data
 */
bool CSVWriter::Initialize(const std::string& filepath) {
    // Create/Overwrite file (default mode truncates existing content)
    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    // Write the standard header row
    // Columns:
    //   Segment_ID   - Which scan segment (0-based, corresponds to robot passes)
    //   Groove_Index - Which defect within the segment (1-based)
    //   Area_mm2     - Calculated area in square millimeters
    //   Point_Count  - Number of points in the measurement window (quality indicator)
    file << "Segment_ID,Groove_Index,Area_mm2,Point_Count\n";

    file.close();
    return true;
}

/**
 * @brief Appends a single defect measurement row to the CSV file.
 * 
 * Each call adds one row representing a detected groove or hole.
 * Multiple defects per segment result in multiple rows with the same
 * Segment_ID but different Groove_Index values.
 * 
 * @param[in] filepath    Path to the CSV file (must already exist via Initialize())
 * @param[in] segmentId   Scan segment identifier (0-based)
 * @param[in] grooveIdx   Defect index within the segment (1-based)
 * @param[in] area        Measured area in mm² (after calibration correction)
 * @param[in] pointCount  Number of points contributing to the measurement
 * 
 * @return true if row was written successfully, false on I/O error
 * 
 * @note File is opened in append mode - existing data is preserved.
 * @note File is closed after each write to ensure data is flushed to disk.
 *       This is intentional for crash safety during long measurement sessions.
 * 
 * @par Example Output Row:
 * @code
 * 0,1,12.3456,5000
 * @endcode
 * 
 * @see Initialize() must be called first to create the file with headers
 */
bool CSVWriter::LogGroove(const std::string& filepath,
                          int segmentId,
                          int grooveIdx,
                          double area,
                          int pointCount) {

    // Open in append mode to preserve existing data
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        return false;
    }

    // Write comma-separated values (no quoting needed for numeric data)
    file << segmentId << ","
         << grooveIdx << ","
         << area << ","
         << pointCount << "\n";

    // Explicit close to flush data immediately
    // This ensures data is written even if the program crashes later
    file.close();
    
    // Return success status based on stream state
    return !file.fail();
}