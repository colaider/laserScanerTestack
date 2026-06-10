# Laser Scanner Metrology System

## Overview

This application provides automated 3D surface defect detection and measurement using a Micro-Epsilon scanCONTROL laser line scanner. It integrates with industrial robot controllers via TCP/IP to perform coordinated scanning operations across large workpieces.

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           MEASUREMENT SYSTEM                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐    TCP/IP     ┌──────────────────────────────────────┐   │
│  │    Robot     │◄─────────────►│         RobotServer                  │   │
│  │  Controller  │   Port 5010   │  (Command/Response Protocol)         │   │
│  └──────────────┘               └──────────────────────────────────────┘   │
│                                              │                              │
│                                              ▼                              │
│  ┌──────────────┐   Callback    ┌──────────────────────────────────────┐   │
│  │ scanCONTROL  │──────────────►│      MeasurementManager              │   │
│  │   Scanner    │   Profiles    │  (Double-Buffer, Threading)          │   │
│  └──────────────┘               └──────────────────────────────────────┘   │
│                                              │                              │
│                                              ▼                              │
│                                 ┌──────────────────────────────────────┐   │
│                                 │        MetrologyEngine               │   │
│                                 │  (RANSAC, Clustering, Integration)   │   │
│                                 └──────────────────────────────────────┘   │
│                                              │                              │
│                                    ┌─────────┴─────────┐                   │
│                                    ▼                   ▼                   │
│                            ┌─────────────┐     ┌─────────────┐             │
│                            │  PLYWriter  │     │  CSVWriter  │             │
│                            │ (3D Output) │     │(Measurements)│             │
│                            └─────────────┘     └─────────────┘             │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Features

### Analysis Modes

| Mode   | Detection Method                          | Use Case                      |
|--------|-------------------------------------------|-------------------------------|
| GROOVE | Points below surface (Z < cutPlane)       | Cuts, scratches, depressions  |
| HOLE   | Missing surface data (gaps in point cloud)| Through-holes, slots          |

### Capabilities

- **Profile-Based Surface Leveling**: Corrects for scanner/workpiece tilt and surface warp
- **Morphological Operations**: Erosion and dilation for noise removal
- **Connected-Component Clustering**: Isolates individual defects
- **Trapezoidal Integration**: Accurate area measurement
- **3D Text Labels**: Visual defect identification in PLY output
- **Calibration Correction**: Empirically-tuned measurement accuracy

## File Structure

```
GetProfiles_Callback_Measure/
├── GetProfiles_Callback_Measure.cpp   # Main entry point, robot control loop
├── GetProfiles_Callback_Measure.h     # Core data structures (PointXYZ, ColorPoint, etc.)
├── MeasurementManager.cpp             # Profile buffering, threading, orchestration
├── MeasurementManager.h               # Double-buffer management, worker thread
├── MetrologyEngine.cpp                # Analysis algorithms (RANSAC, clustering, area)
├── MetrologyEngine.h                  # Algorithm constants, AnalysisResult structure
├── RobotServer.cpp                    # TCP/IP server implementation
├── RobotServer.h                      # Robot communication protocol
├── PLYWriter.cpp                      # 3D point cloud export
├── PLYWriter.h                        # PLY format writer
├── CSVWriter.cpp                      # Measurement logging
├── CSVWriter.h                        # CSV format writer
├── stdafx.h                           # Precompiled header
└── README.md                          # This file
```

## Dependencies

- **LLT.dll**: Micro-Epsilon scanner driver library
- **Winsock2**: Windows networking (ws2_32.lib)
- **C++14**: Required compiler standard

## Build Requirements

- Visual Studio 2015 or later
- Windows SDK 10.0 or later
- Micro-Epsilon SDK (LLT.dll in parent directory)

## Robot Communication Protocol

### Connection

The scanner application acts as a TCP server on port 5010. The robot controller connects as a client.

### Commands

| Command     | Description                        | Response              |
|-------------|------------------------------------|-----------------------|
| `READY`     | Robot asks what to do next         | `START <step>` / `BUSY` / `DONE` |
| `SCAN_START`| Begin profile capture              | `OK` / `BUSY` / `DONE`|
| `SCAN_END`  | End capture, trigger analysis      | `OK` / `ERR`          |
| `QUIT`      | Disconnect gracefully              | (none)                |

### Protocol Sequence

```
Robot                          Scanner
  │                               │
  ├─── "READY\n" ────────────────►│
  │◄── "START 10.00>" ────────────┤  (move 10mm and scan)
  │                               │
  ├─── "SCAN_START\n" ───────────►│
  │◄── "OK>" ─────────────────────┤  (capture begins)
  │                               │
  │     (robot moves, scanner captures profiles)
  │                               │
  ├─── "SCAN_END\n" ─────────────►│
  │◄── "OK>" ─────────────────────┤  (analysis complete)
  │                               │
  │     (repeat until object length reached)
  │                               │
  │◄── "DONE>" ───────────────────┤  (job complete)
```

## Configuration Parameters

### User Inputs (at startup)

| Parameter              | Description                                    | Units |
|------------------------|------------------------------------------------|-------|
| Object Length          | Total length of workpiece to scan              | mm    |
| Laser Distance         | Scanner-to-surface distance (baseZ)            | mm    |
| Scan Section Width     | Robot travel per segment (step size)           | mm    |
| End-of-Groove Crop     | Distance to exclude from defect ends (tool entry/exit regions) | mm    |
| Measurement Length     | Window length (0 = full defect length)         | mm    |
| Cut Depth              | Expected groove depth (GROOVE mode only)       | mm    |

### Algorithm Constants (MetrologyEngine.h)

| Constant              | Value   | Description                              |
|-----------------------|---------|------------------------------------------|
| `RANSAC_ITERATIONS`   | 1500    | Plane fitting iterations                 |
| `RANSAC_THRESHOLD`    | 0.05 mm | Inlier distance threshold                |
| `PLANE_DEPTH_OFFSET`  | 0.22 mm | Surface-to-groove cut plane offset       |
| `EROSION_ITERATIONS`  | 2       | Morphological cleanup passes             |
| `MIN_GROOVE_AREA_MM2` | 0.1 mm² | Minimum defect size threshold            |
| `GRID_CELL_SIZE`      | 0.1 mm  | Analysis grid resolution                 |

## Output Files

Each session creates a timestamped folder:

```
Session_2026-02-05_14-30-45/
├── Summary.csv                 # Measurement log (all defects)
├── Segment_0_Raw.ply           # Raw point cloud (before analysis)
├── Segment_0_Analyzed.ply      # Colorized result with 3D labels
├── Segment_1_Raw.ply
├── Segment_1_Analyzed.ply
└── ...
```

### PLY Color Coding

| Color  | RGB           | Meaning                                |
|--------|---------------|----------------------------------------|
| Green  | (0, 255, 0)   | Valid surface points                   |
| Red    | (255, 0, 0)   | Defect points included in measurement  |
| Blue   | (0, 0, 255)   | Defect points excluded (cropped)       |
| White  | (255, 255, 255)| 3D text labels                        |

### CSV Format

```csv
Segment_ID,Groove_Index,Area_mm2,Point_Count
0,1,12.3456,5000
0,2,8.7654,3200
1,1,15.0000,6100
```

## Analysis Pipeline

The MetrologyEngine processes each segment through 10 steps:

1. **Point Filtering**: Remove points outside Z validity range
2. **Profile Leveling**: Correct for scanner/surface tilt using upper envelope method
3. **Surface Index**: Build row-indexed surface point map (HOLE mode)
4. **Boundary Detection**: Find surface extents
5. **Grid Initialization**: Create 2D analysis grid (0.1mm cells)
6. **Grid Population**: Mark active cells based on mode
7. **Clustering**: Erode → Label → Dilate → Heal IDs
8. **Data Gathering**: Collect cluster extents and centroids
9. **Area Calculation**: Trapezoidal integration with calibration
10. **Reporting**: Generate PLY, CSV, and 3D labels

## Usage

### Starting the Application

1. Connect scanner to network
2. Run the application
3. Enter configuration parameters when prompted
4. Application waits for robot connection on port 5010

### Example Session

```
Enter object length (mm): 500
Enter the distance of the laser scanner from the object (mm): 95
Enter the width needed for each scan section (mm): 10
Enter End-of-Groove Crop (mm): 2
Enter Measurement Window Length (mm, 0 for full): 0

--- Analysis Selection ---
1. Groove Analysis (Standard)
2. Surface Hole Analysis (Automatic Boundary)
Selection: 1

Enter the depth of the cut (mm): 1.5

>>> WAITING FOR ROBOT CONNECTION <<<
Waiting for Robot on port 5010...
Robot Connected!
```

## Threading Model

```
Main Thread              Callback Thread         Worker Thread
    │                          │                       │
    │                          │                       │ (waiting)
    ├─OnSegmentStart()────────►│                       │
    │                          ├──AddProfile()────────►│
    │                          ├──AddProfile()────────►│
    │                          ├──AddProfile()────────►│
    ├─OnSegmentEnd()──────────►│                       │
    │                          │        (swap)─────────┤
    │                          │                       ├─ProcessingLoop()
    │                          │                       │  └─AnalyzeSegment()
    ├─WaitForAnalysis()────────┼───────────────────────┤
    │  (blocking)              │                       ├─notify completion
    │◄─────────────────────────┼───────────────────────┤
```

## Troubleshooting

### Common Issues

| Problem                        | Possible Cause                    | Solution                          |
|--------------------------------|-----------------------------------|-----------------------------------|
| Area always ~0.028 mm²         | Calibration on zero area          | Check MIN_GROOVE_AREA threshold   |
| No defects detected            | PLANE_DEPTH_OFFSET too large      | Reduce offset or check cut depth  |
| Points incorrectly classified  | SURFACE_TOLERANCE too wide        | Reduce tolerance (default 0.15mm) |
| RANSAC skipped                 | < 100 candidate points            | Check Z filter range              |
| Robot timeout                  | Analysis taking too long          | Increase WaitForAnalysis timeout  |

### Debug Output

The application prints debug information during analysis:

```
--- DEBUG: Starting Analysis Segment 0 ---
Config: BaseZ=95 CropEnd=2 Length=0
[DEBUG] Raw Points: 125000 | Bounds X: -5.12 to 4.88
Done. (Tilt X: 0.0012)
[DEBUG] Surface Index Built. Valid Surface Points: 98000
[DEBUG] Found 2 valid clusters.
[DEBUG] Processing Cluster 1 | Centroid Count: 3500
[DEBUG] Raw Area Calculated: 12.45
```

## Version History

| Version | Date       | Changes                                    |
|---------|------------|--------------------------------------------|
| 22.2    | 2026-02-05 | Added HOLE mode, comprehensive documentation |
| 22.0    | 2026-01-15 | Initial integrated version                 |

## License

Proprietary - Internal use only.

## Contact

For technical support or questions about this system, contact the development team.
