# Full Reference Manual
## Laser Scanner Metrology System

**Version:** 22.2  
**Date:** February 2026  
**Audience:** Test Engineers

---

# Table of Contents

1. [Introduction](#1-introduction)
2. [System Requirements](#2-system-requirements)
3. [Hardware Setup](#3-hardware-setup)
4. [Software Installation](#4-software-installation)
5. [Laser Alignment Procedure](#5-laser-alignment-procedure)
6. [Running a Measurement Session](#6-running-a-measurement-session)
7. [Configuration Parameters Explained](#7-configuration-parameters-explained)
8. [Understanding Analysis Modes](#8-understanding-analysis-modes)
9. [Output Files and Data](#9-output-files-and-data)
10. [Viewing Results in CloudCompare](#10-viewing-results-in-cloudcompare)
11. [Interpreting Measurements](#11-interpreting-measurements)
12. [Troubleshooting](#12-troubleshooting)
13. [Glossary](#13-glossary)

---

# 1. Introduction

## 1.1 Purpose

The Laser Scanner Metrology System is an automated 3D surface inspection tool designed to detect and measure surface defects on test specimens. It works in coordination with a robot controller to scan workpieces and generate detailed measurement reports.

## 1.2 What This System Does

- **Detects** surface defects such as grooves, cuts, scratches, and holes
- **Measures** the area of each detected defect in square millimetres (mm²)
- **Generates** 3D point cloud files for visual inspection
- **Logs** all measurements to CSV files for data analysis

## 1.3 System Overview

```
┌─────────────────┐                       ┌─────────────────┐
│   24V Power     │                       │     Robot       │
│    Supply       │                       │   Controller    │
└────────┬────────┘                       └────────┬────────┘
         │                                         │
         ▼                                         │
┌─────────────────────────────────────────┐        │
│         scanCONTROL Laser Scanner       │        │
└─────────────────────────────────────────┘        │
                    │                              │
                    ▼                              ▼
         ┌─────────────────────────────────────────────────┐
         │              Laptop / PC                        │
         │  ┌─────────────────────────────────────────┐   │
         │  │   GetProfiles_Callback_Measure.exe      │   │
         │  └─────────────────────────────────────────┘   │
         └─────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │  Output Files   │
                    │  (.ply, .csv)   │
                    └─────────────────┘
```

---

# 2. System Requirements

## 2.1 Hardware Requirements

| Component | Specification |
|-----------|---------------|
| Laser Scanner | Micro-Epsilon scanCONTROL series |
| Power Supply | 24V DC for scanner |
| Computer | Windows 10/11 PC with Ethernet port |
| Network | Direct Ethernet connection to scanner |
| Robot | Industrial robot with TCP/IP communication capability |

## 2.2 Software Requirements

| Software | Purpose | Where to Get It |
|----------|---------|-----------------|
| scanCONTROL Configuration Tools | Laser alignment and setup | Micro-Epsilon website |
| GetProfiles_Callback_Measure.exe | Main measurement application | Provided with this system |
| GetProfiles_Callback_Measure.pdb | Debug symbols (keep with .exe) | Provided with this system |
| LLT.dll | Scanner driver library | Micro-Epsilon SDK |
| CloudCompare | 3D visualisation | https://www.cloudcompare.org/ |

## 2.3 File Organisation

Your folder structure should look like this:

```
SDK42/
├── LLT.dll                          ← Scanner driver (MUST be here)
│
└── examples/
    └── GetProfiles_Callback_Measure/
        ├── GetProfiles_Callback_Measure.exe    ← Main application
        ├── GetProfiles_Callback_Measure.pdb    ← Debug symbols
        └── Session_YYYY-MM-DD_HH-MM-SS/        ← Output folders (created automatically)
```

> ⚠️ **Critical**: The `LLT.dll` file must be in the **parent folder** of the executable, not in the same folder.

---

# 3. Hardware Setup

## 3.1 Scanner Power Connection

1. Locate the 24V DC power supply
2. Connect the power supply to the scanner's power input
3. Connect the power supply to mains electricity
4. Verify the scanner powers on (check status LED)

## 3.2 Network Connection

1. Connect an Ethernet cable from the scanner to the switchbox
2. Connect an Ethernet cable from the robot controller to the switchbox
3. Connect an Ethernet cable from the switchbox to your laptop
4. The devices will auto-configure their network settings

## 3.3 Robot Connection

The robot controller connects to the measurement application via TCP/IP on **port 5010**. Ensure:
- The laptop and robot are on the same network (or robot connects to laptop's IP)
- Port 5010 is not blocked by firewall
- The robot program is configured with the laptop's IP address

---

# 4. Software Installation

## 4.1 Installing scanCONTROL Configuration Tools

1. Download from the Micro-Epsilon website
2. Run the installer and follow the prompts
3. Restart your computer if prompted

## 4.2 Installing CloudCompare

1. Visit https://www.cloudcompare.org/
2. Download the Windows installer
3. Run the installer and follow the prompts

## 4.3 Setting Up the Measurement Application

1. Ensure `LLT.dll` is in the parent folder of your executable
2. Place `GetProfiles_Callback_Measure.exe` and `.pdb` in your working folder
3. No installation required - the application runs directly

---

# 5. Laser Alignment Procedure

Proper laser alignment is **critical** for accurate measurements. This procedure ensures the laser is correctly positioned over your test area.

## 5.1 Opening Configuration Tools

1. Ensure the scanner is powered on and connected via Ethernet
2. Launch **scanCONTROL Configuration Tools**
3. The software should auto-detect your scanner
4. Click **Connect** to establish communication

## 5.2 Selecting the Measurement View

1. In Configuration Tools, navigate to the measurement modes
2. Select **Groove Width/Depth** measurement
3. This view shows a live profile of what the laser sees

## 5.3 Aligning the Laser

With the live view active, adjust the robot arm position:

### Height Alignment (Z-axis)
- **Target Distance**: Approximately **95mm** from laser to surface
- This is the **median range** of the laser where accuracy is optimal
- The flat section of your test board should be at this distance
- **Tip**: Keep the flat surface distance slightly above 95mm (e.g., 96-97mm). This ensures minor surface deviations remain above the reference plane and are not mistakenly classified as grooves or holes.

### Lateral Alignment (X/Y-axis)
- Position the laser so the **centre of the laser line** is over the **centre of your test section**
- The test section should be **under 50mm wide** to fit within the scanner's field of view

```
        Laser Scanner
             │
             │ ~95mm
             ▼
    ┌────────────────────┐
    │    Test Section    │  ← Should be < 50mm wide
    │  ┌              ┐  │
    │  │   Defects    │  │  ← Centre of laser here
    │  └──────────────┘  │
    └                    ┘
```

## 5.4 Disconnecting Before Measurement

> ⚠️ **Critical Step**: You MUST disconnect from Configuration Tools before running the measurement application.

1. Click **Disconnect** in Configuration Tools
2. Only then launch the measurement application

**Why?** The scanner can only communicate with one application at a time. If Configuration Tools is still connected, the measurement application will fail to initialise.

---

# 6. Running a Measurement Session

## 6.1 Launching the Application

1. Navigate to the folder containing `GetProfiles_Callback_Measure.exe`
2. Double-click to run, or open a command prompt and type:
   ```
   GetProfiles_Callback_Measure.exe
   ```
3. A console window will appear

## 6.2 Entering Configuration Parameters

The application will prompt you for several parameters. Enter each value and press **Enter**:

```
Enter object length (mm): _
Enter the distance of the laser scanner from the object (mm): _
Enter the width needed for each scan section (mm): _
Enter End-of-Groove Crop (mm): _
Enter Measurement Window Length (mm, 0 for full): _

--- Analysis Selection ---
1. Groove Analysis (Standard)
2. Surface Hole Analysis (Automatic Boundary)
Selection: _

Enter the depth of the cut (mm): _    ← Only for GROOVE mode
```

See [Section 7](#7-configuration-parameters-explained) for detailed explanations of each parameter.

## 6.3 Waiting for Robot Connection

After entering parameters, you'll see:

```
>>> WAITING FOR ROBOT CONNECTION <<<
Waiting for Robot on port 5010...
```

At this point:
1. Start your robot program
2. The robot will connect to the application
3. You'll see: `Robot Connected!`

## 6.4 During the Scan

The scan runs automatically:
- The robot moves to each segment position
- The scanner captures profile data
- The application analyses each segment
- Progress information is displayed in the console

**Do not** close the application or disconnect the scanner during scanning.

## 6.5 Completing the Session

When the scan is complete:
1. You'll see: `Press Enter to exit...`
2. Press **Enter** to close the application
3. Your results are saved in a `Session_YYYY-MM-DD_HH-MM-SS` folder

---

# 7. Configuration Parameters Explained

## 7.1 Object Length (mm)

**What it is**: The total length of the workpiece to be scanned.

**How it's used**: The system divides this by the Section Width to determine how many scan segments are needed.

**Example**: If your test board is 80mm long, enter `80`.

---

## 7.2 Laser Distance (mm)

**What it is**: The distance from the scanner to the surface being measured.

**Recommended value**: **95mm** (the median range of the scanner)

**Why 95mm?** This is the optimal working distance where:
- The laser has maximum resolution
- Measurement accuracy is highest
- The field of view is appropriate for typical test sections

**How to verify**: Use Configuration Tools to check the measured distance to the flat surface of your test piece.

---

## 7.3 Scan Section Width (mm)

**What it is**: How far the robot moves between scan segments.

**How it works**: 
- This value is sent to the robot controller
- The robot moves this distance along the X-axis for each new segment
- Smaller values = more overlap and detail, but longer scan time

**Example**: For a 80mm object with 15mm section width:
- Number of segments = 80 ÷ 15 ≈ 6 segments

---

## 7.4 End-of-Groove Crop (mm)

**What it is**: Distance to exclude from the ends of detected defects.

**Why it's needed**: The ends of grooves represent where the cutting tool enters and exits the test material. These entry/exit regions do not represent the full depth of the cut. By cropping these ends, we measure only the portion where the tool was fully submerged in the material.

**Typical value**: 1-3mm depending on groove characteristics

**Visual representation**:
```
    Groove Profile (Side View)
    
    Tool Entry              Tool Exit
        ↘                      ↙
┌────────────────────────────────────┐
│        ╲                  ╱        │  ← Surface
│         ╲────────────────╱         │
│         │← Full Depth  →│          │  ← Measured Area
│         │   (measured)  │          │
└─────────┴────────────────┴─────────┘
          ↑                ↑
       Cropped          Cropped
    (partial depth)  (partial depth)
```

---

## 7.5 Measurement Window Length (mm)

**What it is**: Specific length to measure within each defect.

**Values**:
- `0` = Measure the full length of each detected defect
- `>0` = Measure only this specific length (centred on defect)

**When to use non-zero**: When you need consistent measurement lengths across variable-sized defects.

---

## 7.6 Cut Depth (mm) - GROOVE Mode Only

**What it is**: The expected depth of grooves/cuts in your test piece.

**How it's used**: 
- Sets a "floor" for the analysis
- Points below this depth are excluded
- Prevents erroneous data from affecting measurements

**Important**: This should be slightly deeper than your actual expected groove depth to ensure all valid points are included.

**Example**: If your grooves are approximately 1.2mm deep, enter `1.5mm` to provide margin.

---

## 7.7 Analysis Mode Selection

### Mode 1: Groove Analysis (Standard)

**Use for**: Surface depressions, cuts, scratches, grooves

**Detection method**: Identifies points that are **below** the reference surface plane

**Best for**:
- Machined grooves
- Scratch marks
- Surface depressions
- Any defect where material has been removed

### Mode 2: Surface Hole Analysis

**Use for**: Through-holes, slots, gaps in the surface

**Detection method**: Identifies **missing data** (gaps) in the point cloud

**Best for**:
- Drilled holes
- Slots
- Any defect where the laser cannot see a surface (passes through)

---

# 8. Understanding Analysis Modes

## 8.1 Groove Analysis Mode

In Groove mode, the system:

1. **Captures** a 3D point cloud of the surface
2. **Levels** the data to correct for any tilt
3. **Identifies** points below the surface plane
4. **Clusters** connected low points into individual defects
5. **Measures** the area of each defect cluster

```
Cross-Section View:
                    Surface Level
────────────────────────────────────────────
                    ╲         ╱
                     ╲───────╱  ← Groove (detected)
                     
Points below this line ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─
                              ← Cut Depth Floor (excluded below here)
```

## 8.2 Hole Analysis Mode

In Hole mode, the system:

1. **Captures** a 3D point cloud of the surface
2. **Identifies** areas where no surface data exists
3. **Maps** the boundaries of these gaps
4. **Measures** the area of each gap

```
Top-Down View:
┌─────────────────────────────────────┐
│  Surface (data present)             │
│         ┌───────────┐               │
│         │           │               │
│         │   HOLE    │  ← No data    │
│         │  (gap)    │     detected  │
│         └───────────┘               │
│                                     │
└─────────────────────────────────────┘
```

---

# 9. Output Files and Data

## 9.1 Session Folder

Each measurement session creates a new folder with a timestamp:

```
Session_2026-02-10_14-30-45/
├── Summary.csv              ← All measurements in tabular format
├── Segment_0_Raw.ply        ← Raw point cloud (before processing)
├── Segment_0_Analyzed.ply   ← Processed point cloud (colour-coded)
├── Segment_1_Raw.ply
├── Segment_1_Analyzed.ply
└── ...
```

## 9.2 Summary.csv File

This file contains all measurements in a format suitable for spreadsheet analysis.

**Format**:
```csv
Segment_ID,Groove_Index,Area_mm2,Point_Count
0,1,12.3456,5000
0,2,8.7654,3200
1,1,15.0000,6100
```

**Columns explained**:

| Column | Description |
|--------|-------------|
| Segment_ID | Which scan segment (0, 1, 2, ...) |
| Groove_Index | Defect number within that segment (1, 2, 3, ...) |
| Area_mm2 | Measured defect area in square millimetres |
| Point_Count | Number of 3D points in this defect |

**Opening in Excel**:
1. Open Excel
2. Go to File → Open
3. Select the `Summary.csv` file
4. The data will import into columns automatically

## 9.3 PLY Files

PLY (Polygon File Format) files contain 3D point cloud data.

**Two types per segment**:

| File | Contents | Use |
|------|----------|-----|
| `Segment_X_Raw.ply` | Original captured points (no colour) | Debugging, re-analysis |
| `Segment_X_Analyzed.ply` | Processed points with colour coding | Visual inspection |

---

# 10. Viewing Results in CloudCompare

## 10.1 Opening a PLY File

1. Launch **CloudCompare**
2. Go to **File → Open** (or drag-and-drop the file)
3. Select your `_Analyzed.ply` file
4. Click **Open**

## 10.2 Understanding the Colour Coding

The analysed point cloud uses colours to indicate different classifications:

| Colour | RGB Value | Meaning |
|--------|-----------|---------|
| 🟢 **Green** | (0, 255, 0) | Valid surface points - normal surface area |
| 🔴 **Red** | (255, 0, 0) | Defect points **included** in measurement |
| 🔵 **Blue** | (0, 0, 255) | Defect points **excluded** (cropped region) |
| ⚪ **White** | (255, 255, 255) | 3D text labels showing measurements |

```
Colour Map Visual:

    Green = Good Surface
    ████████████████████████████████████████
    ████████████████████████████████████████
    ████     Blue      Red      Blue    ████
    ████   (cropped) (measured)(cropped)████
    ████████████████████████████████████████
              ↑                    ↑
           Excluded            Excluded
           from area           from area
```

## 10.3 Navigation Controls

| Action | Mouse/Keyboard |
|--------|----------------|
| Rotate view | Left-click + drag |
| Pan view | Right-click + drag (or Shift + left-click + drag) |
| Zoom | Scroll wheel |
| Reset view | Press **1** on keyboard |
| Top-down view | Press **8** on keyboard |

## 10.4 Adjusting Point Display

If points appear too small or too large:

1. Select your point cloud in the **DB Tree** panel (left side)
2. In the **Properties** panel (typically bottom-left):
   - Find **Point size**
   - Adjust the slider (try 2-5 for most displays)

## 10.5 Reading 3D Labels

The white 3D text labels show:
- Defect number
- Measured area value

To read labels clearly:
1. Zoom in to the area of interest
2. Rotate to view labels face-on
3. Labels are positioned at the centroid of each defect

---

# 11. Interpreting Measurements

## 11.1 What the Area Measurement Means

The **Area_mm2** value represents the **projected surface area** of the defect, measured in square millimetres.

For grooves, this is the area of the groove as viewed from above (bird's-eye view), not the internal surface area of the groove walls.

## 11.2 Factors Affecting Measurements

Several factors can influence measurement accuracy:

| Factor | Effect | Mitigation |
|--------|--------|------------|
| Scanner distance | Affects resolution | Keep at ~95mm |
| Surface reflectivity | May cause missing points | Clean surface before scanning |
| Groove depth | Very shallow grooves harder to detect | Ensure Cut Depth is set correctly |
| Scan speed | Fast motion may reduce point density | Use appropriate robot speed |

## 11.3 Measurement Consistency

For reliable comparisons:
- Use the same parameters for all scans in a test series
- Verify laser alignment before each session
- Check that the 95mm working distance is maintained

## 11.4 Typical Values

Measurement values vary greatly depending on your specific defects. As a general guide:

| Defect Type | Typical Area Range |
|-------------|-------------------|
| Fine scratches | 0.5 - 2 mm² |
| Small grooves | 2 - 10 mm² |
| Large cuts | 10 - 50+ mm² |

> **Note**: These are examples only. Your actual values will depend on your specific test specimens.

---

# 12. Troubleshooting

## 12.1 Connection Issues

### "Scanner initialisation failed"

**Cause**: The scanner couldn't be connected.

**Solutions**:
1. **Restart the application** - This is common and usually works on the second attempt
2. Ensure Configuration Tools is completely closed
3. Check the Ethernet cable connection
4. Verify the scanner is powered on

### "Robot connection timeout"

**Cause**: The robot didn't connect within the expected time.

**Solutions**:
1. Check the robot program is running
2. Verify the robot is configured with the correct laptop IP address
3. Check that port 5010 is not blocked by firewall

## 12.2 No Defects Detected

**Possible causes and solutions**:

| Possible Cause | Solution |
|----------------|----------|
| Cut Depth too shallow | Increase the Cut Depth value |
| Wrong analysis mode | Use GROOVE mode for depressions, HOLE mode for through-holes |
| Surface too reflective | Clean the surface to reduce specular reflections |
| Scanner not aligned | Re-run the alignment procedure |

## 12.3 Unexpected Area Values

### Areas too small or zero

- Check that defects are within the scan area
- Verify the Cut Depth setting isn't too deep
- Ensure the laser distance is approximately 95mm

### Areas too large

- May be measuring multiple defects as one
- Check the End-of-Groove Crop setting
- Verify surface levelling is working correctly (check Raw vs Analyzed PLY)

## 12.4 Recovery from Errors

If an error occurs during scanning:

1. **Close the application** (press Ctrl+C or close the window)
2. **Stop the robot program**
3. **Re-home the robot** to the starting position
4. **Restart the application** from the beginning
5. **Re-enter your parameters** and begin a new session

> **Note**: Partial sessions cannot be resumed. You must restart the entire scan if an error occurs.

## 12.5 Console Error Messages

| Message | Meaning | Action |
|---------|---------|--------|
| `Initialisation failed` | Scanner connection error | Restart application |
| `BUSY` responses | Previous segment still processing | Wait - this is normal |
| `ERR` response | Analysis error occurred | Check console for details |
| `Timeout` | Communication delay | Check network connection |

---

# 13. Glossary

| Term | Definition |
|------|------------|
| **Cluster** | A group of connected defect points identified as a single defect |
| **Cropping** | Excluding edge regions of defects from measurement |
| **Cut Depth** | The expected maximum depth of grooves below the surface |
| **Median Range** | The optimal scanner working distance (95mm) |
| **PLY** | Polygon File Format - a 3D point cloud file format |
| **Point Cloud** | A collection of 3D points representing a scanned surface |
| **Profile** | A single line of 3D points captured by the laser scanner |
| **RANSAC** | Algorithm used to level the surface data |
| **Segment** | One section of the total scan area |
| **Session** | A complete measurement run from start to finish |

---

# Appendix A: Quick Reference Card

## Recommended Settings

| Parameter | Typical Value |
|-----------|---------------|
| Laser Distance | 95mm |
| Section Width | 10-20mm |
| End-of-Groove Crop | 2mm |
| Measurement Length | 0 (full) |

## Colour Code Summary

| Colour | Meaning |
|--------|---------|
| 🟢 Green | Good surface |
| 🔴 Red | Measured defect |
| 🔵 Blue | Excluded points |
| ⚪ White | Labels |

## File Locations

| What | Where |
|------|-------|
| Application | Your working folder |
| LLT.dll | Parent folder of application |
| Results | `Session_YYYY-MM-DD_HH-MM-SS/` subfolder |

---

# Appendix B: Example Session Walkthrough

## Scenario: Measuring 3 Grooves on a Test Board

**Test piece**: 80mm board with 3 machined grooves (~1.2mm deep)

### Step 1: Setup
```
- Power on scanner (24V)
- Connect Ethernet
- Open Configuration Tools
- Align laser to 95mm, centred on test area
- Disconnect and close Configuration Tools
```

### Step 2: Launch and Configure
```
Enter object length (mm): 80
Enter the distance of the laser scanner from the object (mm): 95
Enter the width needed for each scan section (mm): 15
Enter End-of-Groove Crop (mm): 2
Enter Measurement Window Length (mm, 0 for full): 0
Selection: 1
Enter the depth of the cut (mm): 1.5
```

### Step 3: Run Scan
```
- Start robot program
- Wait for "Robot Connected!"
- Scan runs automatically (~2-3 minutes)
- "Press Enter to exit..."
```

### Step 4: Review Results
```
Open: Session_2026-02-10_14-30-45/

Summary.csv shows:
Segment_ID,Groove_Index,Area_mm2,Point_Count
0,1,8.234,3200
2,1,7.891,3100
4,1,8.102,3150

View Segment_2_Analyzed.ply in CloudCompare:
- Green surface with red groove visible
- White label showing "1: 7.89 mm²"
```

---

**End of User Manual**

*For technical support or questions about this system, contact the development team.*