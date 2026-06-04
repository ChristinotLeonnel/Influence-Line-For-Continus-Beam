# Influence Line for Continuous Beam — Complete Documentation

## 📋 Table of Contents
1. [Project Overview](#project-overview)
2. [What is an Influence Line?](#what-is-an-influence-line)
3. [Key Features](#key-features)
4. [Who Should Use This?](#who-should-use-this)
5. [Quick Start](#quick-start)
6. [System Requirements](#system-requirements)
7. [Installation](#installation)
8. [Usage Guide](#usage-guide)
9. [Configuration](#configuration)
10. [Output Interpretation](#output-interpretation)
11. [Architecture & Design](#architecture--design)
12. [Examples](#examples)
13. [Troubleshooting](#troubleshooting)
14. [Contributing](#contributing)

---

## 🎯 Project Overview

**Influence Line for Continuous Beam** is a professional-grade **structural analysis application** written in **C++** that automates the computation of influence lines for statically indeterminate (hyperstatic) continuous beams.

### What It Does
- Analyzes **multi-span continuous beams** with arbitrary spans, materials, and cross-sections
- Computes **influence lines** for bending moment, shear force, deflection, and rotation
- Automatically finds **optimal load positions** to maximize critical effects
- Generates **visualizations**: static PNG plots, MP4 animations, and GIF previews
- Exports all results in **machine-readable JSON format**
- Runs analysis using a **fully parallel pipeline** for speed

### Who Built It
**Tsaraloh A. Christinot** — Structural Engineer  
📧 tsaralohachristinot@gmail.com | 📞 +261 34 30 524 02

### License
**GNU Affero General Public License v3.0 (AGPL-3.0)**

---

## 🌉 What is an Influence Line?

An **influence line** is a graph that shows how a quantity at a specific point in a structure varies as a unit load moves across the span.

### Why They Matter

In civil engineering, influence lines help answer critical questions:
- **"Where should I place a truck load to cause maximum bending moment at midspan?"**
- **"What's the worst-case shear force scenario?"**
- **"If I have a convoy of vehicles, how should I position them?"**

### What This Program Computes

For each span and cross-section of your beam, the program calculates:

| Influence Line | What It Represents |
|---|---|
| **Bending Moment** | How the moment at a section changes as a moving load traverses the beam |
| **Shear Force** | How the shear at a section changes with load position |
| **Deflection** | How the vertical displacement at a point varies with load position |
| **Rotation** | How the slope (angle) at a support varies |
| **Support Moment** | How reaction moments at supports change with load position |

### Example Scenario

**A 3-span highway bridge (20m + 25m + 20m):**
- The program computes 1000s of data points showing how the moment at span 2 changes as a truck moves
- Identifies exactly where to place the truck for **maximum moment** and **maximum shear**
- Shows you the visualized curve so you can see the critical position intuitively

---

## ✨ Key Features

### 1. **Comprehensive Analysis**
- ✅ Three-moment equation solver (hyperstatic method)
- ✅ Arbitrary number of spans with different lengths and materials
- ✅ Variable cross-sections (different EI per span)
- ✅ Combined load scenarios (point loads + distributed loads simultaneously)

### 2. **Intelligent Load Optimization**
- ✅ Automatically positions **point loads** for maximum effect
- ✅ Optimizes **distributed rectangular loads** (e.g., traffic patterns)
- ✅ Combines both load types to find **worst-case global envelope**
- ✅ Critical section analysis (identifies the span and position most at risk)

### 3. **Rich Visualizations**
- ✅ **Static PNG plots**: publication-ready influence lines
- ✅ **MP4 videos**: watch the load move across the influence line in real-time
- ✅ **Animated GIFs**: quick preview of load movement
- ✅ **Envelope plots**: shows optimal load position markers overlaid on influence lines
- ✅ **Fully customizable styling** via JSON config (colors, fonts, grid, nodes)

### 4. **High Performance**
- ✅ **Parallel execution**: uses `std::async` for 4–16 concurrent tasks
- ✅ **Streaming JSON output**: handles large datasets without memory bloat
- ✅ **Typical runtime**: ~23 seconds for a 3-span beam at 1m resolution
- ✅ **Zero external dependencies** for analysis core (nlohmann/json is bundled)

### 5. **Flexible Output**
- ✅ **JSON export**: all influence lines, critical values, optimal positions
- ✅ **Structured directories**: organized by analysis phase
- ✅ **Re-runnable configs**: input file updated with optimal load positions
- ✅ **Human-readable + machine-readable**: plots for engineers, JSON for automation

---

## 👥 Who Should Use This?

### ✅ Ideal Users
- **Structural Engineers**: analyzing bridge designs, platform frames
- **Civil Engineering Students**: learning influence line theory with visual validation
- **Bridge Designers**: optimizing for truck loads, railway loads, live loads
- **Researchers**: computational mechanics, structural optimization
- **CAD/BIM Integrators**: exporting JSON results to downstream tools

### ⚠️ Current Limitations
- **Windows-only**: built for MSVC 2022 on Windows 10/11
- **Continuous beams only**: not for trusses, frames, or cable structures
- **Static analysis only**: no dynamic effects, seismic, or wind
- **Classical beam theory**: Euler-Bernoulli assumption (no shear deformation)

---

## 🚀 Quick Start

### Installation (3 minutes)
```bash
# 1. Clone repository
git clone https://github.com/ChristinotLeonnel/Influence-Line-For-Continus-Beam
cd Influence-Line-For-Continus-Beam

# 2. Install dependencies (Windows + PowerShell)
cd C:\vcpkg
.\vcpkg.exe install opencv4:x64-windows matplotplusplus:x64-windows
winget install gnuplot.gnuplot

# 3. Download gif.h header
curl -o include/gif.h https://raw.githubusercontent.com/charlietangora/gif-h/master/gif.h

# 4. Build with CMake
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### First Run (5 minutes)
```bash
# Run the executable
./x64/Release/InfluenceLineContinuousBeam.exe

# Output directory is created at:
# C:\Users\<YourName>\Documents\Matrix One\Influence Line\
```

### Modify Configuration (5 minutes)
Edit the auto-generated input file at:
```
C:\Users\<YourName>\Documents\Matrix One\Influence Line\structural model input.txt
```

Example for a **3-span bridge**:
```
# Units: meters, kN, kN/m, Pa, m^4
Length: 20 25 20
Steps: 1
Young Modulus: 210e9 210e9 210e9
Moment of Inertia: 1e-6 1e-6 1e-6

# Point load: 6 kN, 12 kN, 12 kN at 2.25m, 4.5m, 1.5m from left
6 12 12 /Point/ 2.25 4.5 1.5 :: Design Truck

# Distributed load: 45 kN/m from 0m to 3m
45 /Distributed/ 0 3 :: Traffic Lane
```

### View Results (2 minutes)
Browse the output directory:
```
Documents/Matrix One/Influence Line/
├── 02_Influence_Lines/     ← JSON data for all curves
├── 05_Output/Plots/        ← PNG images (open in any viewer)
└── 05_Output/Animation/    ← MP4 and GIF files (play in any player)
```

---

## 💻 System Requirements

### Hardware
- **CPU**: Dual-core or better (ideally 4+ cores for parallel analysis)
- **RAM**: 2 GB minimum (4–8 GB recommended for large analyses)
- **Storage**: 50 GB free space (for OpenCV, matplot++, and outputs)
- **GPU**: Optional (OpenCV will use integrated graphics if available)

### Software
| Component | Requirement | Notes |
|---|---|---|
| **OS** | Windows 10/11 | x64 architecture required |
| **Compiler** | Visual Studio 2022 | MSVC v143 or later |
| **C++ Standard** | C++20 | Requires `/std:c++latest` |
| **OpenCV** | 4.12.0+ | Graphics, video encoding |
| **matplot++** | 1.2.1+ | Static plotting (gnuplot backend) |
| **gnuplot** | 6.0+ | Required by matplot++ |
| **CMake** | 3.21+ | For build system |

### Optional but Recommended
- **Git**: for cloning the repository
- **PowerShell 7+**: modern package management
- **Visual Studio Code**: source editing

---

## 📦 Installation

### Step 1: Install Visual Studio 2022
- Download: https://visualstudio.microsoft.com/
- Workload: "Desktop development with C++"
- Must include MSVC v143 toolset

### Step 2: Install vcpkg
```powershell
cd C:\
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg.exe integrate install  # Registers with Visual Studio
```

### Step 3: Install OpenCV & matplot++
```powershell
cd C:\vcpkg
.\vcpkg.exe install opencv4:x64-windows
.\vcpkg.exe install matplotplusplus:x64-windows
```

> ⏱️ **This takes 30–45 minutes.** Go grab a coffee.

### Step 4: Install gnuplot
```powershell
winget install gnuplot.gnuplot
# Verify: gnuplot --version
```

### Step 5: Download gif.h
```powershell
$url = "https://raw.githubusercontent.com/charlietangora/gif-h/master/gif.h"
$out = "D:\Your\Project\Path\include\gif.h"
Invoke-WebRequest -Uri $url -OutFile $out
```

### Step 6: Clone & Build
```powershell
git clone https://github.com/ChristinotLeonnel/Influence-Line-For-Continus-Beam
cd Influence-Line-For-Continus-Beam

mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DCMAKE_BUILD_TYPE=Release

cmake --build . --config Release
```

### Verify Installation
```powershell
# Check DLL files were copied
ls ".\x64\Release\*.dll" | Measure-Object
# Expected: ~29 DLLs

# Run the executable
.\x64\Release\InfluenceLineContinuousBeam.exe
```

---

## 📖 Usage Guide

### 1. Configure Your Structure

**File**: `Documents/Matrix One/Influence Line/structural model input.txt`

#### Syntax
```
# Comments start with #

# Span lengths (meters)
Length: L1 L2 L3 ...

# Analysis resolution (results every N meters)
Steps: 1

# Young's modulus (Pa) — one per span
Young Modulus: E1 E2 E3 ...

# Second moment of area (m^4) — one per span
Moment of Inertia: I1 I2 I3 ...

# Point loads: P1 P2 ... /Point/ x1 x2 ... :: Load Name
6 12 /Point/ 2.5 5.0 :: Truck Load

# Distributed loads: q1 q2 ... /Distributed/ x1 L1 L2 ... :: Load Name
45 30 /Distributed/ 0 5 10 :: Traffic Pattern
```

#### Example: 2-Span Bridge
```
# Simple supported 2-span bridge
# Span 1: 30m, Span 2: 30m
# Both steel beams (EI = 2.1e14 Pa·m⁴)

Length: 30 30
Steps: 1
Young Modulus: 210e9 210e9
Moment of Inertia: 1e-3 1e-3

# Design truck: 6-12-6 axle spacing
6 12 6 /Point/ 0 3 6 :: Design Truck

# Lane loads
45 /Distributed/ 0 30 :: UDL Lane 1
```

### 2. Run Analysis

```powershell
cd "C:\Users\YourName\Documents\Matrix One\Influence Line\"
.\InfluenceLineContinuousBeam.exe
```

**Program steps:**
1. ✅ Loads `structural model input.txt`
2. ✅ Solves three-moment equations (~5 sec)
3. ✅ Computes influence lines (Phase 1, ~10 sec)
4. ✅ Calculates critical values (Phase 2, ~5 sec)
5. ✅ Optimizes load positions (Phase 3, ~3 sec)
6. ✅ Generates visualizations (Phase 4, ~10–30 sec depending on plot complexity)
7. ✅ **Outputs updated config with optimal positions**

Total runtime: **20–60 seconds** depending on beam size and resolution.

### 3. Interpret Results

#### A. JSON Data Files
Location: `02_Influence_Lines/` and `03_Critical_Values/`

**Example**: `bending_moment.json`
```json
{
  "span_0": {
    "section_0": {
      "alpha": [0, 0.1, 0.2, ..., 1.0],
      "values": [0, 1.5, 3.2, ..., 0]
    },
    "section_1": { ... }
  },
  "span_1": { ... }
}
```

**Reading it:**
- `alpha` = fraction of span (0 = left end, 1 = right end)
- `values` = influence line ordinate (effect per unit load)

#### B. PNG Plots
Location: `05_Output/Plots/`

**Three subdirectories:**
- `Maximum/` — influence lines for the critical section
- `All/` — all sections in all spans
- `Envelopes/` — combined load effects with optimal position markers

#### C. Animations
Location: `05_Output/Animation/`

- `Results/GIF/` — watch the load move point-by-point (quick preview)
- `Results/MP4/` — smooth video (20 fps, can be played in any media player)
- `Curvature/` — bonus visualization showing curvature changes

### 4. Customize Visuals (Optional)

**File**: `plot_config.json` in output root

```json
{
  "figure": {
    "width": 1280,
    "height": 720
  },
  "curves": [
    {
      "color": "#1E5FA8",
      "thickness": 2.5,
      "filled": false,
      "fill_alpha": 0.15
    }
  ],
  "legend": {
    "show": true,
    "position": "top-right",
    "font_scale": 0.40
  },
  "grid": {
    "show": true,
    "major_color": "#D8D8D8",
    "background": "#FFFFFF"
  },
  "nodes": {
    "show": true,
    "color": "#CC2222",
    "radius": 5
  },
  "animation": {
    "fps": 20,
    "show_cursor": true,
    "show_point": true
  }
}
```

**Edit → Save → Re-run program (no rebuild needed)**

---

## ⚙️ Configuration

### Input File Format (Detailed)

#### **Line 1–N: Comments**
```
# Any line starting with # is ignored
```

#### **Line: Length**
```
Length: L1 L2 L3 L4 ...
```
- **Required**: YES
- **Type**: space-separated floats (meters)
- **Example**: `Length: 20 25 30` → 3-span beam

#### **Line: Steps**
```
Steps: 1
```
- **Required**: YES
- **Type**: integer
- **Meaning**: Results computed every `Steps` meters
- **Example**: `Steps: 1` → every 1m | `Steps: 0.5` → every 0.5m
- **Trade-off**: Smaller steps = more detail but slower analysis

#### **Line: Young Modulus**
```
Young Modulus: E1 E2 E3 ...
```
- **Required**: YES, must match number of spans
- **Type**: float (Pa, SI units)
- **Common values**:
  - Steel: `210e9` (210 GPa)
  - Concrete: `30e9` (30 GPa)
  - Aluminum: `70e9` (70 GPa)

#### **Line: Moment of Inertia**
```
Moment of Inertia: I1 I2 I3 ...
```
- **Required**: YES, must match number of spans
- **Type**: float (m⁴, SI units)
- **How to compute**:
  - Rectangular section: `I = b*h³/12`
  - I-beam: use design specs or FEA
  - Example: 1m × 0.5m rectangle → `I = 1 × 0.125 / 12 = 0.0104` m⁴

#### **Line: Point Load**
```
P1 P2 P3 ... /Point/ x1 x2 x3 ... :: Load Name
```
- **P values**: magnitudes (kN)
- **x values**: positions from left of beam (m)
- **Must have**: same count of P and x values
- **Example**: `6 12 6 /Point/ 0 3 6 :: Truck` → 3 point loads at 0m, 3m, 6m
- **Optional**: can specify multiple point load lines

#### **Line: Distributed Load**
```
q1 q2 ... /Distributed/ x1 L1 L2 ... :: Load Name
```
- **q values**: intensities (kN/m)
- **x values**: starting position + segment lengths
- **Count**: 1 starting position + N segment lengths for N intensities
- **Example**: `45 30 /Distributed/ 0 10 5` → starts at 0m, first 10m at 45 kN/m, next 5m at 30 kN/m
- **Optional**: can specify multiple distributed load lines

---

## 📊 Output Interpretation

### Directory Structure
```
Documents/Matrix One/Influence Line/
├── 01_Input/
│   └── structural_model.json          # Your input, in machine-readable form
├── 02_Influence_Lines/
│   ├── bending_moment.json            # [span][section][alpha] = influence ordinate
│   ├── shear_force.json
│   ├── deflection.json
│   ├── rotation.json
│   ├── support_moment.json
│   ├── abscissa.json                  # x-coordinates for plotting
│   └── node_lengths.json               # Nodal positions
├── 03_Critical_Values/
│   ├── bending_moment.json            # { "span": X, "section": Y, "alpha": Z, "value": V }
│   ├── shear_force.json               # Same structure for each quantity
│   ├── deflection.json
│   ├── rotation.json
│   └── support_moment.json
├── 04_Load_Envelopes/
│   ├── Global/
│   │   ├── Point_Load/                # Envelopes for point load alone
│   │   ├── Distributed_Load/          # Envelopes for distributed load alone
│   │   └── Combined_Load/             # Envelopes for both loads together
│   └── Critical_Section/              # Same, but only for the most critical location
├── 05_Load_Positioning/
│   ├── Global/
│   │   └── optimal_positions.txt      # Rewritten input config with optimal load positions
│   └── Critical_Section/
└── 05_Output/
    ├── Plots/
    │   ├── Maximum/                   # PNG of influence line at critical section
    │   ├── All/                        # PNGs of all influence lines
    │   └── Envelopes/                 # Load envelope plots with markers
    └── Animation/
        ├── Results/
        │   ├── GIF/                   # Animated GIFs (quick preview)
        │   └── MP4/                   # MP4 videos (full resolution)
        └── Curvature/
            ├── GIF/                   # Curvature animation GIFs
            └── MP4/                   # Curvature animation MP4s
```

### How to Use the Results

#### **For Engineers**
1. Open `05_Output/Plots/Maximum/` → review critical influence lines
2. Open `05_Output/Plots/Envelopes/` → see optimal load positions
3. Open corresponding JSON files in `03_Critical_Values/` → extract max values
4. Share plots with team, import JSON data into CAD/BIM tools

#### **For Students**
1. Open animations in `05_Output/Animation/Results/MP4/` → watch how load affects structure
2. Compare different load positions → understand influence line theory
3. Examine `02_Influence_Lines/*.json` → see raw ordinate values

#### **For Automation**
1. Read `02_Influence_Lines/*.json` → influence line data
2. Read `03_Critical_Values/*.json` → maximum effects
3. Read `04_Load_Envelopes/Global/*.json` → envelope curves
4. Import directly into analysis software, optimization algorithms, etc.

---

## 🏗️ Architecture & Design

### High-Level Flow

```
┌──────────────────────────────────────────────────────────────┐
│ Input: structural_model_input.txt                             │
│ (Spans, materials, loads)                                     │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│ Phase 1: Solve Continuous Beam Equilibrium                   │
│ ├─ Three-moment method (hyperstatic solver)                  │
│ ├─ Unit load superposition                                   │
│ └─ Compute influence lines for all sections [Parallel: 4x]   │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│ Phase 2: Calculate Critical Values                            │
│ ├─ Max/min of each influence line                            │
│ ├─ Location of critical sections                             │
│ └─ Export to 03_Critical_Values/ [Parallel: 5x]              │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│ Phase 3: Optimize Load Positions                              │
│ ├─ Sweep point loads across influence lines                  │
│ ├─ Sweep distributed loads (convoy optimization)             │
│ ├─ Combine both → global envelope                            │
│ └─ Export to 04_Load_Envelopes/ [Parallel: 4x]               │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│ Phase 4: Generate Visualizations (Ploting.lib)               │
│ ├─ Static PNG plots (OpenCV pure)                            │
│ ├─ MP4 animations (OpenCV VideoWriter)                       │
│ ├─ GIF animations (gif.h header)                             │
│ └─ Envelope plots (OpenCV pure) [Parallel: 2–4x]             │
└────────────────┬─────────────────────────────────────────────┘
                 │
                 ▼
┌──────────────────────────────────────────────────────────────┐
│ Output: Complete analysis results                             │
│ ├─ JSON data (02_*, 03_*, 04_*)                              │
│ ├─ PNG plots (05_Output/Plots/)                              │
│ ├─ Animations (05_Output/Animation/)                         │
│ └─ Updated input config (05_Load_Positioning/)               │
└──────────────────────────────────────────────────────────────┘
```

### Core Classes

| Class | File | Purpose |
|---|---|---|
| **Isostatique** | `Isostatique.h/.cpp` | Closed-form influence lines for a single simply-supported span |
| **Hyperstatique** | `Hyperstatique.h/.cpp` | Assembles all spans using three-moment method; hyperstatic solver |
| **Loading** | `Loading.h/.cpp` | Sweeps loads across influence lines to find optimal (worst-case) positions |
| **Output** | `Output.h/.cpp` | Orchestrates phases 1–3; handles JSON export and parallel tasks |
| **UpdatePositions** | `UpdatePositions.h/.cpp` | Rewrites input config with optimal load positions |
| **JsonStreamWriter** | `JsonStreamWriter.h` | Writes large 3D tensors span-by-span to limit RAM usage |
| **Configuration** | `Input.h/.cpp` | Parses input file and stores beam/load data |

### Plotting Modules

| File | Purpose |
|---|---|
| `render_common.hpp` | Shared OpenCV drawing: curves, grid, axes, legend, nodes, annotations |
| `plot_config.hpp` | Visual configuration struct backed by `plot_config.json` |
| `plot_results.hpp` | Static PNG renderer (OpenCV pure) |
| `animate_results.hpp` | MP4 + GIF renderer (OpenCV + gif.h) |
| `envelope_plots.hpp` | Global envelope plots with load position markers |
| `gnuplot_init.hpp` | Initializes PATH for matplot++ / gnuplot |
| `gif_writer.hpp` | Thin wrapper around gif.h for animated GIF output |

### Parallelization Strategy

**Uses `std::async` for independent tasks:**

```cpp
// Phase 1: Compute 4 influence line types in parallel
auto fut1 = std::async(std::launch::async, [](){ /* BM */ });
auto fut2 = std::async(std::launch::async, [](){ /* SF */ });
auto fut3 = std::async(std::launch::async, [](){ /* Def */ });
auto fut4 = std::async(std::launch::async, [](){ /* Rot */ });

fut1.get(); fut2.get(); fut3.get(); fut4.get();  // Wait for all
```

**Result:** ~4x speedup on dual-core, ~16x on high-end CPUs.

---

## 💡 Examples

### Example 1: Simple 2-Span Bridge

**Scenario**: A highway bridge with two 30m spans, both steel I-beams.

**Input File**:
```
# 2-Span Bridge Analysis
# Beam: IPE 400 (I ≈ 23130 cm⁴ = 0.002313 m⁴)

Length: 30 30
Steps: 1
Young Modulus: 210e9 210e9
Moment of Inertia: 0.002313 0.002313

# Standard truck: 6-12-6 kN axles, 3m spacing
6 12 6 /Point/ 0 3 6 :: Design Truck

# Lane load
45 /Distributed/ 0 30 :: Traffic Load
```

**Expected Analysis**:
- Computation: ~15 seconds
- Critical location: likely at interior support or midspan of longer span
- Optimal truck position: somewhere near center of span 1 or span 2 (depending on EI and span lengths)
- Visualization: 2 PNG files (one per span), 2 MP4 animations

### Example 2: 3-Span Continuous Beam with Variable Stiffness

**Scenario**: A building floor with reinforced concrete: center span stiffer than end spans.

**Input File**:
```
# 3-Span RC Building Floor
# EI varies: outer = 2.4e14, middle = 4.0e14 (more reinforcement)

Length: 8 10 8
Steps: 0.5
Young Modulus: 30e9 30e9 30e9
Moment of Inertia: 0.008 0.01333 0.008

# Live load: 5 kN/m (residential floor)
5 /Distributed/ 0 8 10 8 :: Live Load

# Point load (concentrated live load)
2 /Point/ 1 :: Concentrated Load
```

**Expected Analysis**:
- Computation: ~20 seconds (smaller spans, high resolution)
- Influence lines show how stiffness variation affects load distribution
- Optimal load positioning helps with floor design verification

### Example 3: Railway Bridge with Multiple Load Cases

**Scenario**: A railway bridge analyzed for different train configurations.

**Input File**:
```
# Railway Bridge - Bogie Configuration
# Two loading cases: express train (heavier) + local train (lighter)

Length: 25 30 25
Steps: 1
Young Modulus: 210e9 210e9 210e9
Moment of Inertia: 0.004 0.005 0.004

# Express train: 10-15-15-10 kN, standard gauge (1.5m spacing)
10 15 15 10 /Point/ 0 1.5 3 4.5 :: Express Bogie

# Local train: 8-8-8 kN, same spacing
8 8 8 /Point/ 0 1.5 3 :: Local Bogie

# Ballast/track distributed load
20 /Distributed/ 0 80 :: Track Ballast Load
```

**Expected Analysis**:
- Computation: ~18 seconds
- Compare express vs. local effects (critical for design envelope)
- Use `04_Load_Envelopes/` to determine governing load case

---

## 🔧 Troubleshooting

### Build Issues

| Error | Cause | Solution |
|---|---|---|
| `error LNK2001: unresolved external symbol` | Missing OpenCV/matplot++ libs | Verify vcpkg integration: `.\vcpkg.exe integrate install` |
| `fatal error C1083: Cannot open include file: 'opencv2/opencv.hpp'` | Include path missing `opencv4/` subfolder | Check `InfluenceLine.props` has correct includes |
| `error C2220: warning treated as error` | Strict compiler settings | Set `/WX-` in project properties (warnings only, not errors) |
| `Linker error: opencv_world4120.lib not found` | vcpkg builds modular, not monolithic | Use individual libs (`opencv_core4`, `opencv_imgproc4`, etc.) from `.props` |

### Runtime Issues

| Error | Cause | Solution |
|---|---|---|
| `cannot open structural model input.txt` | First run; config not created | Run once to auto-generate; directory is `~/Documents/Matrix One/Influence Line/` |
| `popen() failed: gnuplot not found` | gnuplot not on PATH | `winget install gnuplot.gnuplot` or add to PATH manually |
| `[ERROR] gif.h: No such file` | gif.h not downloaded | Run: `curl -o include/gif.h https://raw.githubusercontent.com/charlietangora/gif-h/master/gif.h` |
| `VideoWriter: cannot open codec` | MSMF backend unavailable on Windows | Requires Windows 8+; should work on Windows 10/11. Check codec support. |
| `[INFO] ONETBB/TBB FAILED` | Optional OpenCV plugins not installed | Harmless warning — OpenCV falls back to built-in threading |
| `Out of memory` | Large beam + small step size | Increase `Steps` (e.g., from 0.5 to 1), reduce span count, or add more RAM |

### Output Issues

| Issue | Cause | Solution |
|---|---|---|
| PNG plots look blank | plot_config.json has wrong color codes | Edit `plot_config.json`: use hex colors (e.g., `#FF0000`), not named colors |
| MP4 is corrupted | Codec mismatch or interrupted write | Re-run program; check disk space; verify OpenCV was built with FFMPEG support |
| JSON file is empty | Analysis crashed silently | Check console output; verify input file syntax; try a smaller beam first |
| Wrong critical values | Influence line computed but load optimization failed | Manually inspect PNG plots; verify load positions are valid; try different load magnitudes |

### Debugging Tips

1. **Verbose output**: Modify `src/Aplication.cpp` to add `std::cout` statements before key phases
2. **Sanity check**: For a simply-supported 1-span beam, influence line at midspan should be triangular
3. **Test case**: Use example values (provided in this doc) to validate installation
4. **Check paths**: Print `ProjectPaths` directory tree; ensure all subdirs exist
5. **Isolate plotting**: Run analysis with plotting disabled (`plotting::run()` commented out) to verify core math

---

## 🤝 Contributing

### Reporting Issues
If you find bugs or have feature requests, please:
1. Check existing issues first
2. Provide:
   - Input file (`structural model input.txt`)
   - Full error message and console output
   - Windows version and Visual Studio version
   - Steps to reproduce

### Code Contributions
This project is licensed under AGPL-3.0. Contributions are welcome:
1. Fork the repository
2. Create a feature branch
3. Make changes with clear commit messages
4. Submit a pull request

### Suggested Enhancements
- [ ] Support for non-rectangular distributed loads (triangular, trapezoidal)
- [ ] Animated load convoy (multiple loads moving together)
- [ ] Linux/macOS port (requires OpenCV/matplot++ cross-platform testing)
- [ ] Stability and buckling analysis
- [ ] Dynamic load factors (moving loads with velocity)
- [ ] Integration with SAP2000, ETABS APIs

---

## 📞 Contact & Support

**Developer**: Tsaraloh A. Christinot  
**Email**: tsaralohachristinot@gmail.com  
**Phone**: +261 34 30 524 02  
**GitHub**: https://github.com/ChristinotLeonnel

**License**: GNU Affero General Public License v3.0 (AGPL-3.0)  
**Third-party**:
- `nlohmann/json` — MIT License
- `gif.h` — MIT License (Charlie Tangora)
- OpenCV — Apache 2.0 License
- matplot++ — MIT License

---

**Last Updated**: June 2026  
**Status**: Active Development  
**Tested On**: Windows 10/11, Visual Studio 2022, OpenCV 4.12.0, matplot++ 1.2.1
