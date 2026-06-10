# Quick Start Guide - Laser Scanner Metrology System

## Get Scanning in 10 Minutes

This guide will help you perform your first scan quickly. For detailed explanations, see the Full Reference Manual.

---

## Prerequisites Checklist

Before starting, ensure you have:

- [ ] scanCONTROL laser scanner with 24V power supply
- [ ] Ethernet cable connected between scanner and laptop
- [ ] `GetProfiles_Callback_Measure.exe` and `GetProfiles_Callback_Measure.pdb` in your working folder
- [ ] `LLT.dll` in the folder **above** your executable
- [ ] scanCONTROL Configuration Tools installed
- [ ] CloudCompare installed (for viewing results)
- [ ] Robot program loaded and ready

---

## Step-by-Step Procedure

### Step 1: Power On the Scanner (2 min)

1. Connect the 24V power supply to the scanner
2. Wait for the scanner's status LED to indicate ready (solid green)
3. Connect the Ethernet cable from the scanner to your laptop

---

### Step 2: Align the Laser (3 min)

1. Open **scanCONTROL Configuration Tools**
2. Connect to the scanner
3. Navigate to the **Groove Width/Depth** measurement view
4. Move the robot arm until:
   - The **middle of the laser line** is aligned with the **centre of your test section**
   - The laser distance reads approximately **95mm** (this is the optimal working distance)
   - The test section width is **under 50mm**
5. **Disconnect** from the scanner in Configuration Tools (important!)
6. Close Configuration Tools

> ⚠️ **Important**: You must disconnect from Configuration Tools before launching the measurement application.

---

### Step 3: Launch the Application (1 min)

1. Navigate to your application folder
2. Double-click `GetProfiles_Callback_Measure.exe`
3. A console window will open

---

### Step 4: Enter Parameters (2 min)

Enter the following when prompted:

```
Enter object length (mm): [Total length to scan, e.g., 80]
Enter the distance of the laser scanner from the object (mm): 95
Enter the width needed for each scan section (mm): [e.g., 15]
Enter End-of-Groove Crop (mm): [e.g., 2]
Enter Measurement Window Length (mm, 0 for full): 0

--- Analysis Selection ---
1. Groove Analysis (Standard)
2. Surface Hole Analysis (Automatic Boundary)
Selection: [1 for grooves, 2 for holes]

Enter the depth of the cut (mm): [e.g., 1.5]  (GROOVE mode only)
```

**Example for a typical groove scan:**
```
Object length: 80
Laser distance: 95
Section width: 15
Crop: 2
Measurement length: 0
Mode: 1
Cut depth: 1.5
```

---

### Step 5: Run the Scan (2 min)

1. The application displays: `>>> WAITING FOR ROBOT CONNECTION <<<`
2. Start the robot program
3. The scan runs automatically
4. When complete, you'll see: `Press Enter to exit...`
5. Press **Enter** to close the application

---

### Step 6: View Your Results

Your results are saved in a timestamped folder (e.g., `Session_2026-02-10_14-30-45/`) in the same directory as the executable.

**Quick view in CloudCompare:**
1. Open CloudCompare
2. Drag and drop `Segment_X_Analyzed.ply` into the window
3. Results are colour-coded:
   - 🟢 **Green** = Good surface
   - 🔴 **Red** = Detected defect (measured)
   - 🔵 **Blue** = Defect points (excluded/cropped)
   - ⚪ **White** = Measurement labels

---

## Troubleshooting Quick Fixes

| Problem | Solution |
|---------|----------|
| Connection initialisation fails | Restart the application - this is normal and will work on retry |
| No defects detected | Check your Cut Depth value - it may be too shallow |
| Scanner not found | Ensure you disconnected from Configuration Tools first |

---

## Next Steps

- Read the **Full Reference Manual** for detailed parameter explanations
- Review `Summary.csv` for measurement data
- Compare multiple `_Analyzed.ply` files to track consistency
