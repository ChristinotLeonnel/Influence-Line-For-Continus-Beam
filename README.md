# Structural Analysis — Continuous Beam (Influence Lines)

A C++ application for the **hyperstatique (statically indeterminate) analysis of continuous beams**. It computes influence lines for bending moment, shear force, deflection, and rotation, then determines optimal load placements and exports all results as JSON.

---

## Features

- Three-moment equation solver for continuous multi-span beams
- Influence line computation for: bending moment, shear force, deflection, rotation, and support moments
- Optimal load positioning for point loads and distributed (rectangular) loads
- Combined load envelopes (point + distributed simultaneously)
- Critical section analysis (worst-case span and section)
- Fully parallel pipeline using `std::async`
- All output in JSON — no external plotting dependencies

---

## Project Structure

```
.
├── CMakeLists.txt
├── path.json                        # Auto-generated on first run
├── doc/
│   ├── Architecture_Optimisation.docx
│   └── Steps.txt
├── headers/
│   ├── Hyperstatique.h              # Continuous beam solver
│   ├── Input.h                      # Configuration parser
│   ├── Isostatique.h                # Single-span influence lines
│   ├── JsonStreamWriter.h           # Streaming JSON writer (low RAM)
│   ├── Loading.h                    # Load positioning & envelopes
│   ├── Output.h                     # Full pipeline orchestrator
│   ├── ProjectPaths.h               # Centralised path registry
│   ├── SpanResult.h                 # Per-span result struct
│   ├── UpdatePositions.h            # Rewrites input with optimal positions
│   ├── Utils.h                      # Shared types, math, JSON helpers
│   └── nlohmann/
│       ├── json.hpp                 # nlohmann/json v3.11.3 (bundled)
│       └── json_fwd.hpp
└── src/
    ├── Aplication.cpp               # Entry point (main)
    ├── Hyperstatique.cpp
    ├── Input.cpp
    ├── Isostatique.cpp
    ├── Loading.cpp
    ├── Output.cpp
    └── UpdatePositions.cpp
```

---

## Output Directory Layout

All results are written under a configurable root (default: `~/Documents/Matrix One/Influence Line/`):

```
<root>/
├── 01_Input/
│   └── structural_model.json        # spans, E, I, step, node count
├── 02_Influence_Lines/
│   ├── bending_moment.json          # [span][section][alpha]
│   ├── shear_force.json
│   ├── deflection.json
│   ├── rotation.json
│   ├── support_moment.json          # [support][alpha]
│   ├── abscissa.json                # global x-coordinates
│   ├── shear_abscissa.json          # x-coordinates with SF discontinuities
│   └── node_lengths.json            # cumulative span lengths
├── 03_Critical_Values/
│   ├── bending_moment.json          # { span, section, alpha, value }
│   ├── shear_force.json
│   ├── deflection.json
│   ├── rotation.json
│   └── support_moment.json
├── 04_Load_Envelopes/
│   ├── Global/
│   │   ├── Point_Load/              # optimal load for entire beam
│   │   ├── Distributed_Load/
│   │   └── Combined_Load/
│   └── Critical_Section/
│       ├── Point_Load/              # optimal load for critical span only
│       ├── Distributed_Load/
│       └── Combined_Load/
└── 05_Load_Positioning/
    ├── Global/                      # input config rewritten with optimal positions (.txt)
    └── Critical_Section/
```

---

## Configuration File

On first run, a template is generated at `<root>/structural model input.txt`. Units: **m**, **kN**, **kN/m**, **Pa**, **m⁴**.

```
# STRUCTURAL ANALYSIS CONFIGURATION FILE
# Units: Length(m), Force(kN), Distributed(kN/m), E(Pa), I(m^4)

Length: 20 25 20
Steps: 1
Young Modulus: 210e9 210e9 210e9
Moment of Inertia: 1e-6 1e-6 1e-6

# Point loads:        intensities /Point/ offsets :: name
6 12 12 6 12 12 /Point/ 2.25 4.5 1.5 5 4.5 1.5 2.5 :: BC 1

# Distributed loads:  intensities /Distributed/ offsets :: name
45 /Distributed/ 0 3 :: UDL 1
```

**Point load syntax:** `intensity1 intensity2 ... /Point/ offset1 offset2 ... :: load name`
**Distributed load syntax:** `intensity1 intensity2 ... /Distributed/ initial_offset length1 length2 ... :: load name`

---

## Building

The project requires a C++20-compatible compiler. Recommended: MSVC 2022, GCC 12+, or Clang 15+.

### With CMake (recommended)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### With MSVC (manual)

```
cl /std:c++20 /O2 /EHsc /utf-8 ^
   src\Aplication.cpp src\Input.cpp src\Isostatique.cpp ^
   src\Hyperstatique.cpp src\Loading.cpp src\Output.cpp src\UpdatePositions.cpp ^
   /I headers ^
   /Fe:InfluenceLineContinuousBeam.exe
```

No external libraries are needed — `nlohmann/json` is bundled under `headers/nlohmann/`.

---

## Usage

```bash
./InfluenceLineContinuousBeam
```

On startup the program:
1. Reads `structural model input.txt` (created automatically on first run)
2. Runs the full structural analysis
3. Exports all JSON results to the output directories
4. Rewrites the input file with optimal load positions in `05_Load_Positioning/`

The console reports the analysis duration in milliseconds and the output root path.

---

## Architecture

```
main()
 └── Configuration::loadFromFile()      # parse input
 └── Output::Output()
      ├── Phase 1  — BM / SF / Def / Rot        [4 async tasks]
      ├── Phase 2a — Critical values JSON        [5 async tasks]
      ├── Phase 2b — Influence line JSON         [8 async tasks]
      ├── Phase 3a — Global load envelopes       [4 async tasks]
      └── Phase 3b — Critical section envelopes  [4 async tasks]
 └── UpdatePositions::run()             # rewrite input with optimal positions
```

### Key Classes

| Class | Role |
|---|---|
| `Isostatique` | Closed-form influence lines for a single simply-supported span |
| `Hyperstatique` | Assembles all spans using the three-moment method; applies unit-load superposition |
| `Loading` | Sweeps a load convoy across the influence line to find the worst position |
| `Output` | Orchestrates the full pipeline and JSON export |
| `UpdatePositions` | Post-processes the load envelope JSON to rewrite the input config |
| `JsonStreamWriter` | Writes large 3D tensors to disk span-by-span to limit peak RAM |

---

## Dependencies

| Dependency | Version | Bundled |
|---|---|---|
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | `headers/nlohmann/json.hpp` |
| C++ standard library (`<future>`, `<filesystem>`) | C++20 | — |

No other external dependencies.

---

## License

Copyright © Tsaraloh A. Christinot — All rights reserved.

This software is licensed under the **GNU Affero General Public License v3.0 (AGPL-3.0)**.
You may use, modify, and distribute this software under the terms of the AGPL-3.0. Any modified version made available over a network must also be released under the same license.

See the full license text at: https://www.gnu.org/licenses/agpl-3.0.html

`json.hpp` is distributed under the MIT License — © 2013–2023 Niels Lohmann.

---

## Contact

**Tsaraloh A. Christinot**
✉️ tsaralohachristinot@gmail.com
📞 +261 34 30 524 02