/**
 * @file MeasurementManager.h
 * @brief Profile buffering, double-buffering, and analysis orchestration.
 * 
 * The MeasurementManager is the central coordinator between the scanner hardware
 * and the analysis engine. It handles:
 * - Thread-safe profile buffering from scanner callbacks
 * - Double-buffering for continuous scanning without data loss
 * - Worker thread management for background analysis
 * - Robot synchronization (blocking waits for analysis completion)
 * 
 * ## Architecture:
 * ```
 *                    ┌─────────────────┐
 *   Scanner ────────►│  Write Buffer   │ (active capture)
 *   Callback         └────────┬────────┘
 *                             │ OnSegmentEnd() swaps
 *                             ▼
 *                    ┌─────────────────┐
 *                    │ Process Buffer  │ (analysis in progress)
 *                    └────────┬────────┘
 *                             │ Worker Thread
 *                             ▼
 *                    ┌─────────────────┐
 *                    │ MetrologyEngine │
 *                    └────────┬────────┘
 *                             │
 *                             ▼
 *                    PLY + CSV Output
 * ```
 * 
 * ## Thread Safety:
 * - AddProfile(): Called from scanner callback thread, mutex-protected
 * - ProcessingLoop(): Runs in dedicated worker thread
 * - WaitForAnalysis(): Called from main thread, uses condition variable
 * 
 * ## Typical Usage:
 * @code
 *     MeasurementManager manager(scanner, 1280, scanCONTROL29xx_50);
 *     manager.Start(config);
 *     
 *     // Robot control loop
 *     manager.OnSegmentStart(0);
 *     // ... scanner captures profiles via callback ...
 *     manager.OnSegmentEnd();
 *     manager.WaitForAnalysis(300);  // Blocks until done
 *     
 *     manager.Stop();
 * @endcode
 * 
 * @version 22.2
 * @date 2026-02-05
 * 
 * @see MetrologyEngine for analysis algorithms
 * @see GetProfiles_Callback_Measure.cpp for callback registration
 */

#pragma once

// ===========================================================================
// STANDARD LIBRARY INCLUDES
// ===========================================================================
// Threading types must be included here (not forward-declared) because
// they are used as member variables, not just pointers.

#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

// ===========================================================================
// LOCAL INCLUDES
// ===========================================================================

#include "GetProfiles_Callback_Measure.h"  // SegmentBuffer, Profile structs

// ===========================================================================
// FORWARD DECLARATIONS
// ===========================================================================

/**
 * @brief Forward declaration of scanner driver interface.
 * 
 * Avoids circular includes since InterfaceLLT_2.h is heavy.
 * Only pointer/reference usage is allowed with forward declarations.
 */
class CInterfaceLLT;

// ===========================================================================
// MEASUREMENT MANAGER CLASS
// ===========================================================================

/**
 * @class MeasurementManager
 * @brief Orchestrates profile capture, buffering, and analysis.
 * 
 * Implements a producer-consumer pattern with double-buffering:
 * - **Producer**: Scanner callback adds profiles to write buffer
 * - **Consumer**: Worker thread processes profiles from process buffer
 * - **Swap**: OnSegmentEnd() atomically swaps buffers
 * 
 * ## State Machine:
 * ```
 * IDLE ──OnSegmentStart()──► CAPTURING ──OnSegmentEnd()──► PROCESSING
 *   ▲                                                           │
 *   └────────────────── WaitForAnalysis() ◄─────────────────────┘
 * ```
 * 
 * @note Not copyable or movable due to threading primitives.
 */
class MeasurementManager {
public:
    // =======================================================================
    // ENUMERATIONS
    // =======================================================================
    
    /**
     * @enum AnalysisMode
     * @brief Selects the defect detection algorithm.
     * 
     * | Mode   | Detection Method                          | Use Case              |
     * |--------|-------------------------------------------|-----------------------|
     * | GROOVE | Points below surface (Z < cutPlane)       | Cuts, scratches       |
     * | HOLE   | Missing surface data (gaps in point cloud)| Through-holes, slots  |
     */
    enum class AnalysisMode { 
        GROOVE,  ///< Detect depressions below surface level
        HOLE     ///< Detect gaps/missing data in surface
    };

    // =======================================================================
    // CONFIGURATION STRUCTURE
    // =======================================================================
    
    /**
     * @struct ManagerConfig
     * @brief Complete configuration for a measurement session.
     * 
     * Collected from user input at startup and passed to Start().
     * Contains all parameters needed for capture, filtering, and analysis.
     */
    struct ManagerConfig {
        // --- Analysis Settings ---
        AnalysisMode mode = AnalysisMode::GROOVE;  ///< Detection algorithm
        
        // --- Geometry ---
        double fixedRobotStepMm;    ///< Robot travel distance per segment (mm)
        double objectLengthMm;      ///< Total object length to scan (mm)
        double laserDistanceMm;     ///< Scanner-to-surface distance (mm, = baseZ)
        double cutDepthMm;          ///< Expected groove depth (mm, GROOVE mode only)
        
        // --- Z Filtering ---
        double minValidZ;           ///< Minimum valid Z value (mm, rejects noise below)
        double maxValidZ;           ///< Maximum valid Z value (mm, rejects noise above)
        
        // --- X Cropping ---
        double cropMinX;            ///< Left X boundary (mm, typically -stepSize/2)
        double cropMaxX;            ///< Right X boundary (mm, typically +stepSize/2)
        
        // --- Measurement Window ---
        double measurementLength = 0;  ///< Length to measure (mm, 0 = full length)
        double grooveCropEnd;          ///< Distance to crop from defect end (mm)
        
        // --- Output Paths ---
        std::string sessionFolder;  ///< Directory for output files
        std::string summaryPath;    ///< Path to Summary.csv
    };

    // =======================================================================
    // CONSTRUCTOR / DESTRUCTOR
    // =======================================================================
    
    /**
     * @brief Constructs a MeasurementManager with scanner dependencies.
     * 
     * Initializes double-buffer pointers and sets up threading primitives.
     * Does not start the worker thread - call Start() separately.
     * 
     * @param[in] llt         Pointer to scanner driver interface
     * @param[in] resolution  Points per profile (e.g., 1280)
     * @param[in] scannerType Scanner model identifier (e.g., scanCONTROL29xx_50)
     * 
     * @note Does not take ownership of `llt` - caller must manage lifetime.
     */
    MeasurementManager(class CInterfaceLLT* llt, unsigned int resolution, int scannerType);
    
    /**
     * @brief Destructor - ensures worker thread is stopped.
     * 
     * Calls Stop() if not already called to cleanly shutdown the worker.
     */
    ~MeasurementManager();

    // =======================================================================
    // LIFECYCLE METHODS
    // =======================================================================
    
    /**
     * @brief Starts the measurement session and worker thread.
     * 
     * Stores configuration and spawns the background processing thread.
     * Must be called before OnSegmentStart().
     * 
     * @param[in] config  Complete session configuration
     * 
     * @pre Worker thread is not already running.
     * @post Worker thread is running and waiting for data.
     * 
     * @see Stop() to end the session
     */
    void Start(const ManagerConfig& config);
    
    /**
     * @brief Stops the worker thread and ends the session.
     * 
     * Signals the worker to exit and waits for it to join.
     * Safe to call multiple times.
     * 
     * @post Worker thread has terminated.
     * @post No further processing will occur.
     */
    void Stop();

    // =======================================================================
    // DATA HANDLING
    // =======================================================================
    
    /**
     * @brief Adds a raw profile to the write buffer.
     * 
     * Called from the scanner callback at high frequency (up to 4kHz).
     * Must be fast and thread-safe.
     * 
     * @param[in] data  Pointer to raw profile binary data
     * @param[in] size  Size of data in bytes
     * 
     * @note Thread-safe: protected by _bufferMutex.
     * @note Profiles are dropped if buffer is full (>10000) or segment not active.
     * 
     * @see NewProfile() callback in GetProfiles_Callback_Measure.cpp
     */
    void AddProfile(const unsigned char* data, unsigned int size);

    // =======================================================================
    // ROBOT/LOGIC SIGNALING
    // =======================================================================
    
    /**
     * @brief Signals the start of a new scan segment.
     * 
     * Clears the write buffer and prepares for profile capture.
     * Called when robot begins motion for a new segment.
     * 
     * @param[in] segmentId  Unique identifier for this segment (0-based)
     * 
     * @pre Previous segment has been processed (not busy).
     * @post Write buffer is empty and ready for profiles.
     * 
     * @see OnSegmentEnd() to complete the segment
     */
    void OnSegmentStart(int segmentId);
    
    /**
     * @brief Signals the end of the current scan segment.
     * 
     * Marks the write buffer complete and swaps it with the process buffer.
     * This triggers the worker thread to begin analysis.
     * 
     * @post Write buffer becomes process buffer (and vice versa).
     * @post Worker thread is notified to process.
     * 
     * @see WaitForAnalysis() to block until processing completes
     */
    void OnSegmentEnd();

    // =======================================================================
    // STATE QUERIES
    // =======================================================================
    
    /**
     * @brief Checks if a segment is currently being captured or processed.
     * 
     * @return true if write buffer is capturing OR process buffer is pending
     * 
     * @note Thread-safe: uses mutex lock.
     */
    bool IsBusy() const;
    
    /**
     * @brief Checks if the full object length has been scanned.
     * 
     * @return true if total X coverage >= objectLengthMm
     * 
     * @note Thread-safe: uses atomic boolean.
     */
    bool IsScanningDone() const { return _scanningDone.load(); }
    
    /**
     * @brief Blocks until the current segment analysis completes.
     * 
     * Used by the robot control loop to ensure analysis finishes
     * before sending OK response.
     * 
     * @param[in] timeoutSeconds  Maximum time to wait (seconds)
     * 
     * @return true if analysis completed, false if timeout occurred
     * 
     * @note Thread-safe: uses condition variable wait.
     */
    bool WaitForAnalysis(int timeoutSeconds);

private:
    // =======================================================================
    // PRIVATE METHODS
    // =======================================================================
    
    /**
     * @brief Worker thread main loop.
     * 
     * Waits for process buffer to be ready, then:
     * 1. Converts raw profiles to PointXYZ
     * 2. Calls MetrologyEngine::AnalyzeSegment()
     * 3. Signals completion via condition variable
     * 
     * Runs until _stopProcessing is set.
     */
    void ProcessingLoop();
    
    /**
     * @brief Extracts Y coordinate from profile timestamp.
     * 
     * Uses scanner timestamp to compute position along travel direction.
     * Based on robot speed and elapsed time.
     * 
     * @param[in]     Timestamp  Pointer to timestamp bytes in profile
     * @param[in,out] baseTime   Reference time for delta calculation
     * @param[in,out] Ymm        Accumulated Y position (mm)
     */
    void YCompute(unsigned char* Timestamp, double& baseTime, double& Ymm);

    // =======================================================================
    // MEMBER VARIABLES
    // =======================================================================
    
    // -----------------------------------------------------------------------
    // Dependencies (not owned)
    // -----------------------------------------------------------------------
    
    class CInterfaceLLT* _llt;  ///< Scanner driver interface (external ownership)
    unsigned int _resolution;   ///< Points per profile
    int _scannerType;           ///< Scanner model identifier
    ManagerConfig _config;      ///< Session configuration (copied)

    // -----------------------------------------------------------------------
    // Double Buffers
    // -----------------------------------------------------------------------
    
    SegmentBuffer _bufferA;     ///< First segment buffer
    SegmentBuffer _bufferB;     ///< Second segment buffer
    SegmentBuffer* _writeBuffer;   ///< Currently receiving profiles (points to A or B)
    SegmentBuffer* _processBuffer; ///< Currently being analyzed (points to A or B)

    // -----------------------------------------------------------------------
    // Threading Primitives
    // -----------------------------------------------------------------------
    
    /**
     * @brief Mutex protecting buffer access.
     * 
     * Guards _writeBuffer, _processBuffer, and their contents.
     * Mutable to allow locking in const methods (IsBusy).
     */
    mutable std::mutex _bufferMutex;
    
    /**
     * @brief Condition variable for worker thread wake-up.
     * 
     * Signaled by OnSegmentEnd() when process buffer is ready.
     */
    std::condition_variable _processCv;

    /**
     * @brief Mutex protecting result signaling.
     */
    std::mutex _resultMutex;
    
    /**
     * @brief Condition variable for analysis completion.
     * 
     * Signaled by worker when analysis finishes.
     * Waited on by WaitForAnalysis().
     */
    std::condition_variable _resultCv;

    // -----------------------------------------------------------------------
    // State Flags
    // -----------------------------------------------------------------------
    
    bool _processingFinished = false;       ///< True when current analysis is done
    std::atomic<bool> _stopProcessing{ false };  ///< Signal to terminate worker
    std::atomic<bool> _scanningDone{ false };    ///< True when object fully scanned

    // -----------------------------------------------------------------------
    // Tracking
    // -----------------------------------------------------------------------
    
    double _totalCoveredX = 0.0;  ///< Accumulated X coverage (mm)
    std::thread _workerThread;    ///< Background processing thread
};