/**
 * @file CSVWriter.h
 * @brief Static utility class for logging defect measurements to CSV files.
 * 
 * Provides simple, session-based CSV logging for recording groove and hole
 * measurements. Designed for append-based operation where each defect
 * detection adds a new row to the summary file.
 * 
 * ## Output Format:
 * ```
 * Segment_ID,Groove_Index,Area_mm2,Point_Count
 * 0,1,12.3456,5000
 * 0,2,8.7654,3200
 * 1,1,15.0000,6100
 * ```
 * 
 * ## Typical Usage:
 * @code
 *     // At session start (once)
 *     CSVWriter::Initialize("Session_2026-02-05/Summary.csv");
 *     
 *     // After each defect is analyzed (multiple times)
 *     CSVWriter::LogGroove(summaryPath, segmentId, idx, area, pointCount);
 * @endcode
 * 
 * ## Thread Safety:
 * Not thread-safe. If multiple threads need to log concurrently,
 * external synchronization is required.
 * 
 * @version 1.0
 * @date 2026-02-05
 * 
 * @see MetrologyEngine::AnalyzeSegment() - primary caller of LogGroove()
 * @see CSVWriter.cpp for implementation details
 */

#pragma once

#include <string>

/**
 * @class CSVWriter
 * @brief Static utility class for CSV-based measurement logging.
 * 
 * All methods are static - no instantiation required. The class serves
 * as a namespace for related file I/O functions rather than representing
 * an object with state.
 * 
 * ## Design Decisions:
 * - **Append mode**: Each LogGroove() call opens/closes the file to ensure
 *   data is flushed immediately (crash safety over performance).
 * - **No buffering**: Prioritizes data integrity for long-running sessions
 *   where a crash could lose accumulated measurements.
 * - **Simple format**: Plain CSV with no quoting (all fields are numeric).
 */
class CSVWriter {
public:
    /**
     * @brief Creates a new CSV file and writes the header row.
     * 
     * Must be called once at the start of each measurement session before
     * any calls to LogGroove(). Creates the file if it doesn't exist, or
     * overwrites it if it does.
     * 
     * @param[in] filepath  Full path to the CSV file to create
     * 
     * @return true if file was created and header written successfully,
     *         false if file could not be opened for writing
     * 
     * @warning Overwrites any existing file at the specified path.
     * 
     * @note Header format: "Segment_ID,Groove_Index,Area_mm2,Point_Count"
     * 
     * @see LogGroove() for appending measurement data after initialization
     */
    static bool Initialize(const std::string& filepath);

    /**
     * @brief Appends a single defect measurement row to the CSV file.
     * 
     * Opens the file in append mode, writes one row, and closes immediately.
     * This ensures data is persisted to disk after each measurement,
     * protecting against data loss from crashes during long sessions.
     * 
     * @param[in] filepath    Path to the CSV file (must exist via Initialize())
     * @param[in] segmentId   Scan segment identifier (0-based, matches robot pass)
     * @param[in] grooveIdx   Defect index within segment (1-based)
     * @param[in] area        Measured defect area in mm² (calibration-corrected)
     * @param[in] pointCount  Number of points in measurement window (quality metric)
     * 
     * @return true if row was written successfully, false on I/O error
     * 
     * @pre Initialize() must have been called to create the file with headers.
     * 
     * @note The function name is "LogGroove" for historical reasons but is
     *       used for both groove and hole measurements.
     * 
     * @par Example:
     * @code
     *     // Log defect #2 from segment 0 with area 12.5mm² and 4500 points
     *     CSVWriter::LogGroove("Summary.csv", 0, 2, 12.5, 4500);
     *     // Appends: "0,2,12.5,4500\n"
     * @endcode
     */
    static bool LogGroove(const std::string& filepath,
                          int segmentId,
                          int grooveIdx,
                          double area,
                          int pointCount);
};