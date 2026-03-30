#pragma once
#ifndef __PROJECT_PATHS__
#define __PROJECT_PATHS__

// =============================================================================
//  ProjectPaths.h  —  Registre centralisé de tous les chemins du projet
// =============================================================================
//
//  Arborescence complète (100% JSON) :
//
//   <root>/
//    ├── 01_Input/
//    │   └── structural_model.json
//    │
//    ├── 02_Influence_Lines/
//    │   ├── bending_moment.json        ← [S][N_span][N_total]
//    │   ├── shear_force.json
//    │   ├── deflection.json
//    │   ├── rotation.json
//    │   ├── support_moment.json        ← [S+1][N_total]
//    │   ├── abscissa.json              ← [N_total]
//    │   ├── shear_abscissa.json        ← [S][N_span][N_total+disc]
//    │   └── node_lengths.json          ← [S+1]
//    │
//    ├── 03_Critical_Values/
//    │   ├── bending_moment.json        ← { span, section, alpha, value }
//    │   ├── shear_force.json
//    │   ├── deflection.json
//    │   ├── rotation.json
//    │   └── support_moment.json
//    │
//    ├── 04_Load_Envelopes/
//    │   ├── Global/
//    │   │   ├── Point_Load/            ← { maximum, span, section, load }
//    │   │   │   ├── bending_moment.json
//    │   │   │   ├── shear_force.json
//    │   │   │   ├── deflection.json
//    │   │   │   └── rotation.json
//    │   │   ├── Distributed_Load/  (idem)
//    │   │   └── Combined_Load/     (idem)
//    │   └── Critical_Section/
//    │       ├── Point_Load/        (idem)
//    │       ├── Distributed_Load/  (idem)
//    │       └── Combined_Load/     (idem)
//    │
//    └── 05_Load_Positioning/
//        ├── Global/
//        │   ├── Point_Load/            ← fichiers .txt mis à jour
//        │   ├── Distributed_Load/
//        │   └── Combined_Load/
//        └── Critical_Section/
//            ├── Point_Load/
//            ├── Distributed_Load/
//            └── Combined_Load/
// =============================================================================

#include "utils/Utils.h"
#include <filesystem>

struct ProjectPaths
{
    std::filesystem::path root;

    // Niveau 1
    std::filesystem::path input;
    std::filesystem::path influence_lines;
    std::filesystem::path critical_values;
    std::filesystem::path load_envelopes;
    std::filesystem::path load_positioning;

    // 04_Load_Envelopes
    std::filesystem::path env_global;
    std::filesystem::path env_global_point;
    std::filesystem::path env_global_dist;
    std::filesystem::path env_global_combined;

    std::filesystem::path env_critical;
    std::filesystem::path env_critical_point;
    std::filesystem::path env_critical_dist;
    std::filesystem::path env_critical_combined;

    // 05_Load_Positioning
    std::filesystem::path pos_global;
    std::filesystem::path pos_global_point;
    std::filesystem::path pos_global_dist;
    std::filesystem::path pos_global_combined;

    std::filesystem::path pos_critical;
    std::filesystem::path pos_critical_point;
    std::filesystem::path pos_critical_dist;
    std::filesystem::path pos_critical_combined;

    explicit ProjectPaths(std::filesystem::path r = {})
        : root(r.empty() ? getConfigPath() : std::move(r))
    {
        input            = root / "01_Input";
        influence_lines  = root / "02_Influence_Lines";
        critical_values  = root / "03_Critical_Values";
        load_envelopes   = root / "04_Load_Envelopes";
        load_positioning = root / "05_Load_Positioning";

        env_global            = load_envelopes / "Global";
        env_global_point      = env_global / "Point_Load";
        env_global_dist       = env_global / "Distributed_Load";
        env_global_combined   = env_global / "Combined_Load";

        env_critical          = load_envelopes / "Critical_Section";
        env_critical_point    = env_critical / "Point_Load";
        env_critical_dist     = env_critical / "Distributed_Load";
        env_critical_combined = env_critical / "Combined_Load";

        pos_global            = load_positioning / "Global";
        pos_global_point      = pos_global / "Point_Load";
        pos_global_dist       = pos_global / "Distributed_Load";
        pos_global_combined   = pos_global / "Combined_Load";

        pos_critical          = load_positioning / "Critical_Section";
        pos_critical_point    = pos_critical / "Point_Load";
        pos_critical_dist     = pos_critical / "Distributed_Load";
        pos_critical_combined = pos_critical / "Combined_Load";
    }

    void createAll() const {
        for (const auto& d : {
            input, influence_lines, critical_values,
            env_global_point, env_global_dist, env_global_combined,
            env_critical_point, env_critical_dist, env_critical_combined,
            pos_global_point, pos_global_dist, pos_global_combined,
            pos_critical_point, pos_critical_dist, pos_critical_combined
        })
            std::filesystem::create_directories(d);
    }
};

#endif // __PROJECT_PATHS__
