/**
 * @file MeasurementManager.cpp
 * @brief Implementation of profile buffering, double-buffering, and analysis orchestration.
 * 
 * This file implements the MeasurementManager class which serves as the central
 * coordinator between scanner hardware callbacks and the MetrologyEngine analysis.
 * 
 * ## Key Responsibilities:
 * - Thread-safe profile collection from high-frequency scanner callbacks
 * - Double-buffering to allow continuous scanning during analysis
 * - Worker thread management for background processing
 * - Synchronization with robot control via condition variables
 * 
 * ## Threading Model:
 * ```
 * Main Thread          Callback Thread       Worker Thread
 *     │                      │                     │
 *     │                      │                     │ (waiting)
 *     ├─OnSegmentStart()────►│                     │
 *     │                      ├──AddProfile()──────►│
 *     │                      ├──AddProfile()──────►│
 *     │                      ├──AddProfile()──────►│
 *     ├─OnSegmentEnd()──────►│                     │
 *     │                      │        (swap)───────┤
 *     │                      │                     ├─ProcessingLoop()
 *     │                      │                     │  └─ConvertProfiles
 *     │                      │                     │  └─AnalyzeSegment
 *     ├─WaitForAnalysis()────┼─────────────────────┤
 *     │  (blocking)          │                     ├─notify completion
 *     │◄─────────────────────┼─────────────────────┤
 *     │                      │                     │
 * ```
 * 
 * @version 22.2
 * @date 2026-02-05
 * 
 * @see MeasurementManager.h for class declaration and documentation
 * @see MetrologyEngine for analysis algorithms
 */

#include "MeasurementManager.h"
#include "MetrologyEngine.h"
#include "PLYWriter.h"
#include "InterfaceLLT_2.h"
#include <iostream>

// ===========================================================================
// CONSTRUCTOR / DESTRUCTOR
// ===========================================================================

/**
 * @brief Constructs the MeasurementManager with scanner dependencies.
 * 
 * Initializes double-buffer pointers (A for write, B for process) and
 * sets all state flags to their default values. Does not start the
 * worker thread - call Start() separately.
 * 
 * @param[in] llt   Pointer to scanner driver interface (not owned)
 * @param[in] res   Points per profile (e.g., 1280)
 * @param[in] type  Scanner model identifier for coordinate conversion
 */
MeasurementManager::MeasurementManager(CInterfaceLLT* llt, unsigned int res, int type)
    : _llt(llt), _resolution(res), _scannerType(type) {
    
    // Initialize double-buffer pointers
    // Buffer A starts as write buffer (receives profiles)
    // Buffer B starts as process buffer (analyzed by worker)
    _writeBuffer = &_bufferA;
    _processBuffer = &_bufferB;

    // Explicit initialization of buffer state
    // segmentId = -1 indicates "not active"
    _bufferA.complete = false;
    _bufferA.segmentId = -1;
    _bufferB.complete = false;
    _bufferB.segmentId = -1;

    // Initialize state flags
    _processingFinished = false;
    _stopProcessing = false;
    _scanningDone = false;

    std::cout << "[Manager] Created. Configured Res: " << _resolution << "\n";
}

/**
 * @brief Destructor - ensures clean shutdown of worker thread.
 * 
 * Calls Stop() to signal worker termination and wait for join.
 * Safe even if Stop() was already called.
 */
MeasurementManager::~MeasurementManager() {
    Stop();
    std::cout << "[Manager] Destroyed.\n";
}

// ===========================================================================
// LIFECYCLE METHODS
// ===========================================================================

/**
 * @brief Starts the measurement session and spawns worker thread.
 * 
 * Stores the configuration, resets tracking state, and launches
 * the background ProcessingLoop thread.
 * 
 * @param[in] config  Complete session configuration (copied internally)
 * 
 * @pre Worker thread is not already running.
 * @post Worker thread is running, waiting for data on _processCv.
 */
void MeasurementManager::Start(const ManagerConfig& config) {
    _config = config;
    _stopProcessing = false;
    _scanningDone = false;
    _totalCoveredX = 0.0;
    
    // Spawn worker thread - will block on _processCv until data arrives
    _workerThread = std::thread(&MeasurementManager::ProcessingLoop, this);
}

/**
 * @brief Stops the worker thread and ends the session.
 * 
 * Sets the stop flag and wakes the worker via condition variable.
 * Blocks until worker thread terminates (joins).
 * 
 * @note Safe to call multiple times - checks joinable() before join().
 */
void MeasurementManager::Stop() {
    _stopProcessing = true;
    
    // Wake worker if it's waiting on the condition variable
    _processCv.notify_all();
    
    // Wait for worker to finish
    if (_workerThread.joinable()) {
        _workerThread.join();
    }
}

// ===========================================================================
// DATA HANDLING
// ===========================================================================

/**
 * @brief Adds a raw profile to the write buffer (thread-safe).
 * 
 * Called from the scanner callback thread at high frequency (up to 4kHz).
 * Copies the raw profile data into the write buffer for later processing.
 * 
 * ## Drop Conditions:
 * - Segment not active (segmentId < 0)
 * - Buffer already marked complete
 * - Buffer full (>10000 profiles) - prevents memory exhaustion
 * - Zero-size data
 * 
 * @param[in] data  Pointer to raw profile binary data
 * @param[in] size  Size of data in bytes
 * 
 * @note Thread-safe: protected by _bufferMutex.
 * @note Must be fast - scanner callback cannot block.
 */
void MeasurementManager::AddProfile(const unsigned char* data, unsigned int size) {
    std::lock_guard<std::mutex> lock(_bufferMutex);
    
    // Reject if segment not active or already complete
    if (_writeBuffer->segmentId < 0 || _writeBuffer->complete) {
        return;
    }

    // Safety limit to prevent memory exhaustion during long scans
    // At 1kHz with 5KB profiles, 10000 profiles = ~50MB
    if (_writeBuffer->profiles.size() > 10000) {
        return;
    }

    // Copy profile data into buffer
    Profile p;
    p.size = size;
    if (size > 0) {
        p.raw.resize(size);
        memcpy(p.raw.data(), data, size);
        _writeBuffer->profiles.push_back(std::move(p));
    }
}

// ===========================================================================
// ROBOT/LOGIC SIGNALING
// ===========================================================================

/**
 * @brief Signals the start of a new scan segment.
 * 
 * Clears the write buffer and prepares it for receiving profiles.
 * Called by RobotControlLoop when SCAN_START command is received.
 * 
 * @param[in] segmentId  Unique identifier for this segment (0-based)
 * 
 * @note Thread-safe: protected by _bufferMutex.
 */
void MeasurementManager::OnSegmentStart(int segmentId) {
    std::lock_guard<std::mutex> lock(_bufferMutex);
    
    // Clear and prepare write buffer
    _writeBuffer->profiles.clear();
    _writeBuffer->profiles.reserve(5000);  // Pre-allocate for typical segment
    _writeBuffer->complete = false;
    _writeBuffer->segmentId = segmentId;
    
    std::cout << ">>> SEGMENT [" << segmentId << "] BEGUN <<<\n";
}

/**
 * @brief Signals the end of the current scan segment.
 * 
 * Marks the write buffer complete and swaps it with the process buffer.
 * This triggers the worker thread to begin analysis.
 * 
 * ## Buffer Swap:
 * ```
 * Before:  writeBuffer -> A (full)    processBuffer -> B (empty)
 * After:   writeBuffer -> B (empty)   processBuffer -> A (full)
 * ```
 * 
 * @note Thread-safe: protected by _bufferMutex.
 * @note Notifies worker via _processCv.
 */
void MeasurementManager::OnSegmentEnd() {
    {
        std::lock_guard<std::mutex> lock(_bufferMutex);
        
        // Mark current write buffer as complete
        _writeBuffer->complete = true;
        
        // Atomic swap - write becomes process, process becomes write
        std::swap(_writeBuffer, _processBuffer);

        // Reset processing flag (worker will set to true when done)
        {
            std::lock_guard<std::mutex> resLock(_resultMutex);
            _processingFinished = false;
        }
    }
    
    // Wake worker thread to process the newly swapped buffer
    _processCv.notify_one();
}

// ===========================================================================
// STATE QUERIES
// ===========================================================================

/**
 * @brief Blocks until current segment analysis completes.
 * 
 * Uses condition variable wait with timeout. Called by RobotControlLoop
 * to ensure analysis is complete before sending OK response to robot.
 * 
 * @param[in] timeoutSeconds  Maximum wait time in seconds
 * 
 * @return true if analysis completed within timeout, false otherwise
 * 
 * @note Thread-safe: uses _resultMutex and _resultCv.
 */
bool MeasurementManager::WaitForAnalysis(int timeoutSeconds) {
    std::cout << "[Manager] Waiting for Analysis...\n";
    
    try {
        std::unique_lock<std::mutex> lock(_resultMutex);
        
        // Wait with timeout for worker to signal completion
        bool result = _resultCv.wait_for(
            lock, 
            std::chrono::seconds(timeoutSeconds), 
            [this] { return _processingFinished; }
        );

        if (result) {
            std::cout << "[Manager] Analysis Complete.\n";
        }
        else {
            std::cout << "[Manager] Analysis TIMEOUT (Worker likely stuck/crashed).\n";
        }

        return result;
    }
    catch (const std::exception& e) {
        std::cerr << "[CRITICAL] Mutex Error: " << e.what() << "\n";
        return false;
    }
}

/**
 * @brief Checks if a segment is currently being captured or processed.
 * 
 * @return true if either buffer is marked complete (capture done, or pending processing)
 * 
 * @note Thread-safe: uses _bufferMutex.
 */
bool MeasurementManager::IsBusy() const {
    std::lock_guard<std::mutex> lock(_bufferMutex);
    return _writeBuffer->complete || _processBuffer->complete;
}

// ===========================================================================
// COORDINATE COMPUTATION
// ===========================================================================

/**
 * @brief Computes Y coordinate from profile timestamp.
 * 
 * Uses the scanner's internal timestamp to calculate elapsed time,
 * then converts to distance using assumed robot speed.
 * 
 * @param[in]     Timestamp  Pointer to 16-byte timestamp in profile data
 * @param[in,out] baseTime   Reference timestamp (0 on first call, updated each call)
 * @param[in,out] Ymm        Accumulated Y position in mm (incremented each call)
 * 
 * @note GlobalSpeed (1.2 mm/s) is hardcoded - should match robot configuration.
 * @note Timestamp format is scanner-specific, decoded by driver.
 */
void MeasurementManager::YCompute(unsigned char* Timestamp, double& baseTime, double& Ymm) {
    double O;           // Oscillator value (unused)
    double C;           // Counter value (current time)
    double R;           // Delta time since last profile
    unsigned int P;     // Profile number (unused)
    
    // Robot travel speed in mm per time unit
    // TODO: Make configurable via ManagerConfig
    const double GlobalSpeed = 1.2;
    
    // Decode timestamp using scanner driver
    _llt->Timestamp2TimeAndCount(Timestamp, &O, &C, &P);
    
    // First profile establishes baseline
    if (baseTime == 0.0) {
        baseTime = C;
        R = 0.0;
    }
    else {
        R = C - baseTime;
        if (R < 0) R = 0.0;  // Guard against timestamp wraparound
    }
    
    // Update for next iteration
    baseTime = C;
    
    // Convert time to distance
    Ymm += GlobalSpeed * R;
}

// ===========================================================================
// WORKER THREAD
// ===========================================================================

/**
 * @brief Background processing thread main loop.
 * 
 * Waits for process buffer to be marked complete, then:
 * 1. Moves data to local buffer (releases lock quickly)
 * 2. Converts raw profiles to XYZ point cloud
 * 3. Writes raw PLY file
 * 4. Calls MetrologyEngine for analysis
 * 5. Updates coverage tracking
 * 6. Signals completion to waiting threads
 * 
 * ## Processing Pipeline:
 * ```
 * Raw Profiles ──► ConvertProfile2Values ──► PointXYZ vector
 *                                                   │
 *                                                   ├──► Raw PLY file
 *                                                   │
 *                                                   └──► MetrologyEngine
 *                                                              │
 *                                                              ├──► Analyzed PLY
 *                                                              └──► CSV log
 * ```
 * 
 * @note Runs until _stopProcessing is set true.
 * @note Exception-safe: catches all exceptions to prevent thread termination.
 * @note Always signals completion, even on error (prevents deadlock).
 */
void MeasurementManager::ProcessingLoop() {
    // Pre-allocate coordinate buffers (sized for maximum profile width)
    std::vector<double> valX(16384);
    std::vector<double> valZ(16384);

    while (!_stopProcessing) {
        SegmentBuffer local;
        
        // -------------------------------------------------------------------
        // PHASE 1: Wait for data and acquire buffer
        // -------------------------------------------------------------------
        {
            std::unique_lock<std::mutex> lock(_bufferMutex);
            
            // Block until either:
            // - Process buffer is ready (complete == true)
            // - Stop signal received
            _processCv.wait(lock, [this] { 
                return _stopProcessing || _processBuffer->complete; 
            });

            // Exit if stopped and no pending work
            if (_stopProcessing && !_processBuffer->complete) {
                break;
            }

            // Move data to local buffer (swap is O(1))
            // This releases the mutex quickly for the callback thread
            local.segmentId = _processBuffer->segmentId;
            local.profiles.swap(_processBuffer->profiles);
            _processBuffer->complete = false;
        }

        // -------------------------------------------------------------------
        // PHASE 2: Handle empty segments
        // -------------------------------------------------------------------
        // Can occur if robot moves without scanning, or on quick stop
        if (local.profiles.empty()) {
            std::cout << "[Manager] Segment " << local.segmentId << " empty. Skipping.\n";
            
            // Must still signal completion to prevent deadlock
            {
                std::lock_guard<std::mutex> resLock(_resultMutex);
                _processingFinished = true;
            }
            _resultCv.notify_one();
            continue;
        }

        // -------------------------------------------------------------------
        // PHASE 3: Process profiles with exception handling
        // -------------------------------------------------------------------
        try {
            std::vector<PointXYZ> points;
            points.reserve(local.profiles.size() * _resolution);

            // Calculate X offset for this segment (stitching)
            double xOffset = static_cast<double>(local.segmentId) * _config.fixedRobotStepMm;
            
            // Y coordinate tracking (computed from timestamps)
            double yMm = 0.0;
            double baseTime = 0.0;

            // ---------------------------------------------------------------
            // Convert each raw profile to XYZ coordinates
            // ---------------------------------------------------------------
            for (auto& p : local.profiles) {
                // Skip malformed profiles (minimum viable size check)
                if (p.size < 64) continue;

                // Convert binary profile to X/Z coordinate arrays
                // NULL pointers for unused outputs (intensity, width, etc.)
                _llt->ConvertProfile2Values(
                    p.raw.data(),           // Raw profile data
                    _resolution,            // Points per profile
                    PROFILE,                // Profile format
                    (TScannerType)_scannerType,  // Scanner model
                    0,                      // Reflection filter
                    true,                   // Convert to mm
                    NULL, NULL, NULL,       // Unused: x_um, z_um, width
                    valX.data(),            // X coordinates (mm)
                    valZ.data(),            // Z coordinates (mm)
                    NULL, NULL              // Unused: intensity, threshold
                );

                // Extract timestamp from end of profile data (last 16 bytes)
                if (p.size >= 16) {
                    YCompute(p.raw.data() + (p.size - 16), baseTime, yMm);
                }

                // ---------------------------------------------------------------
                // Filter and accumulate valid points
                // ---------------------------------------------------------------
                size_t loopMax = (_resolution < valX.size()) ? _resolution : valX.size();
                
                for (size_t i = 0; i < loopMax; ++i) {
                    // Apply X cropping (scan width) and Z validity (noise rejection)
                    if (valX[i] >= _config.cropMinX && 
                        valX[i] <= _config.cropMaxX && 
                        valZ[i] > 10.0) {  // Z > 10mm rejects invalid/noise points
                        
                        points.push_back({ 
                            (float)(valX[i] + xOffset),  // X with segment offset
                            (float)yMm,                   // Y from timestamp
                            (float)valZ[i]                // Z (distance)
                        });
                    }
                }
            }

            // ---------------------------------------------------------------
            // Write output files and run analysis
            // ---------------------------------------------------------------
            if (!points.empty()) {
                // Write raw point cloud (before analysis/colorization)
                std::string rawPath = _config.sessionFolder + "\\Segment_" 
                                    + std::to_string(local.segmentId) + "_Raw.ply";
                PLYWriter::WriteRaw(rawPath, points);

                // Run defect detection and measurement
                MetrologyEngine::AnalyzeSegment(
                    local.segmentId,
                    points,
                    "Segment_" + std::to_string(local.segmentId) + "_Analyzed.ply",
                    0.04,                       // X resolution (mm)
                    0.04,                       // Y resolution (mm)
                    _config.minValidZ,
                    _config.maxValidZ,
                    _config.laserDistanceMm,    // baseZ
                    _config.measurementLength,
                    _config.grooveCropEnd,
                    _config.sessionFolder,
                    _config.summaryPath,
                    _config.mode
                );
            }

            // ---------------------------------------------------------------
            // Update coverage tracking
            // ---------------------------------------------------------------
            _totalCoveredX += _config.fixedRobotStepMm;
            
            // Check if we've scanned the full object length
            if (_config.objectLengthMm > 0 && _totalCoveredX >= _config.objectLengthMm) {
                _scanningDone = true;
            }
        }
        catch (const std::exception& e) {
            // Log but don't terminate - worker must keep running
            std::cerr << "\n[CRITICAL ERROR IN WORKER]: " << e.what() << "\n";
        }
        catch (...) {
            // Catch-all for unknown exceptions
            std::cerr << "\n[CRITICAL UNKNOWN CRASH IN WORKER]\n";
        }

        // -------------------------------------------------------------------
        // PHASE 4: Signal completion (ALWAYS, even on error)
        // -------------------------------------------------------------------
        // This is critical - WaitForAnalysis() will deadlock if not signaled
        {
            std::lock_guard<std::mutex> resLock(_resultMutex);
            _processingFinished = true;
        }
        _resultCv.notify_one();
    }
}