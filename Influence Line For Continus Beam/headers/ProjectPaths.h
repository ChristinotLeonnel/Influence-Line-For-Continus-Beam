#pragma once
#ifndef __PROJECT_PATHS__
#define __PROJECT_PATHS__

// =============================================================================
//  ProjectPaths.h - Registre centralise de tous les chemins du projet
// =============================================================================
//
//  Arborescence complete :
//
//   <root>/
//    |-- 01_Input/
//    |   |-- structural model input.txt   (texte de configuration)
//    |   |-- structural_model.json        (resume du modele — spans, E, I, step)
//    |   |-- beam_geometry.json           (n_spans, EI, node_lengths, total_nodes) [NEW]
//    |   |-- flexibility_coefficients.json (a, b, c par travée)                   [NEW]
//    |   |-- stiffness_distribution.json  (phy, phy_prime par travée)             [NEW]
//    |   `-- span_node_positions.json     (positions locales des noeuds/travée)   [NEW]
//    |
//    |-- 02_Influence_Lines/              (tenseurs JSON [span][section][alpha])
//    |   |-- bending_moment.json
//    |   |-- shear_force.json
//    |   |-- deflection.json
//    |   |-- rotation.json
//    |   |-- support_moment.json
//    |   |-- curvature.json               [NEW] κ = M/(EI)
//    |   |-- abscissa.json
//    |   |-- shear_abscissa.json
//    |   `-- node_lengths.json
//    |
//    |-- 03_Critical_Values/              (valeurs extremes)
//    |   |-- bending_moment.json          { span, section, alpha, value }  (|max|)
//    |   |-- shear_force.json
//    |   |-- deflection.json
//    |   |-- rotation.json
//    |   |-- support_moment.json
//    |   |-- curvature.json               [NEW]
//    |   |-- bending_moment_positive.json [NEW]  max positif
//    |   |-- bending_moment_negative.json [NEW]  min négatif
//    |   |-- shear_force_positive.json    [NEW]
//    |   |-- shear_force_negative.json    [NEW]
//    |   |-- deflection_positive.json     [NEW]
//    |   |-- deflection_negative.json     [NEW]
//    |   |-- rotation_positive.json       [NEW]
//    |   |-- rotation_negative.json       [NEW]
//    |   |-- curvature_positive.json      [NEW]
//    |   |-- curvature_negative.json      [NEW]
//    |   |-- support_moment_positive.json [NEW]
//    |   |-- support_moment_negative.json [NEW]
//    |   `-- Per_Span/                    [NEW] extrema par travée
//    |       |-- bending_moment.json      [{span, max_abs, max_pos, max_neg}]
//    |       |-- shear_force.json
//    |       |-- deflection.json
//    |       |-- rotation.json
//    |       `-- curvature.json
//    |
//    |-- 04_Load_Envelopes/               (enveloppes de chargement)
//    |   |-- Global/
//    |   |   |-- Point_Load/              { maximum, span, section, load }
//    |   |   |-- Distributed_Load/
//    |   |   `-- Combined_Load/
//    |   `-- Critical_Section/
//    |       |-- Point_Load/
//    |       |-- Distributed_Load/
//    |       `-- Combined_Load/
//    |
//    |-- 05_Load_Positioning/             (txt mis a jour par UpdatePositions)
//    |   |-- Global/
//    |   |   |-- Point_Load/
//    |   |   |-- Distributed_Load/
//    |   |   `-- Combined_Load/
//    |   `-- Critical_Section/
//    |       |-- Point_Load/
//    |       |-- Distributed_Load/
//    |       `-- Combined_Load/
//    |
//    |-- 06_Plots/                        (PNG statiques produits par Ploting)
//    |   |-- All/
//    |   |-- Maximum/
//    |   `-- Envelopes/
//    |       |-- Point_Load/
//    |       |-- Distributed_Load/
//    |       `-- Combined_Load/
//    |
//    |-- 07_Animations/                   (GIF + MP4 produits par Ploting)
//    |   |-- Results/
//    |   |   |-- GIF/
//    |   |   `-- MP4/
//    |   `-- Curvature/
//    |       |-- GIF/
//    |       `-- MP4/
//    |
//    |-- 08_Statistics/                   [NEW] statistiques par travée
//    |   |-- bending_moment.json          [{span, min, max, mean, rms, std_dev, ...}]
//    |   |-- shear_force.json
//    |   |-- deflection.json
//    |   |-- rotation.json
//    |   `-- curvature.json
//    |
//    |-- 09_Zero_Crossings/               [NEW] inversions de signe des LI
//    |   |-- bending_moment.json          [{span, n_crossings, crossings:[...]}]
//    |   |-- shear_force.json
//    |   |-- rotation.json
//    |   `-- curvature.json
//    |
//    `-- 10_Support_Reactions/            [NEW] réactions d'appui
//        |-- influence_lines.json         [support][alpha] = R_j(α)
//        `-- critical_values.json         [{support, x_support, max_value, alpha, x_load}]
//
// =============================================================================

#include "Utils.h"
#include <filesystem>

struct ProjectPaths
{
    std::filesystem::path root;

    // ── Niveau 1 ─────────────────────────────────────────────────────────────
    std::filesystem::path input;
    std::filesystem::path influence_lines;
    std::filesystem::path critical_values;
    std::filesystem::path load_envelopes;
    std::filesystem::path load_positioning;
    std::filesystem::path plots;
    std::filesystem::path animations;

    // ── 04_Load_Envelopes ────────────────────────────────────────────────────
    std::filesystem::path env_global;
    std::filesystem::path env_global_point;
    std::filesystem::path env_global_dist;
    std::filesystem::path env_global_combined;

    std::filesystem::path env_critical;
    std::filesystem::path env_critical_point;
    std::filesystem::path env_critical_dist;
    std::filesystem::path env_critical_combined;

    // ── 05_Load_Positioning ──────────────────────────────────────────────────
    std::filesystem::path pos_global;
    std::filesystem::path pos_global_point;
    std::filesystem::path pos_global_dist;
    std::filesystem::path pos_global_combined;

    std::filesystem::path pos_critical;
    std::filesystem::path pos_critical_point;
    std::filesystem::path pos_critical_dist;
    std::filesystem::path pos_critical_combined;

    // ── 06_Plots ─────────────────────────────────────────────────────────────
    std::filesystem::path plots_all;
    std::filesystem::path plots_maximum;
    std::filesystem::path plots_envelopes;
    std::filesystem::path plots_env_point;
    std::filesystem::path plots_env_dist;
    std::filesystem::path plots_env_combined;

    // ── 07_Animations ────────────────────────────────────────────────────────
    std::filesystem::path anim_results;
    std::filesystem::path anim_results_gif;
    std::filesystem::path anim_results_mp4;
    std::filesystem::path anim_curvature;
    std::filesystem::path anim_curvature_gif;
    std::filesystem::path anim_curvature_mp4;

    // ── 03_Critical_Values/Per_Span  [NEW] ────────────────────────────────────
    std::filesystem::path crit_per_span;

    // ── 08_Statistics  [NEW] ──────────────────────────────────────────────────
    std::filesystem::path statistics;

    // ── 09_Zero_Crossings  [NEW] ──────────────────────────────────────────────
    std::filesystem::path zero_crossings;

    // ── 10_Support_Reactions  [NEW] ───────────────────────────────────────────
    std::filesystem::path support_reactions;

    explicit ProjectPaths(std::filesystem::path r = {})
        : root(r.empty() ? getConfigPath() : std::move(r))
    {
        // Niveau 1
        input            = root / "01_Input";
        influence_lines  = root / "02_Influence_Lines";
        critical_values  = root / "03_Critical_Values";
        load_envelopes   = root / "04_Load_Envelopes";
        load_positioning = root / "05_Load_Positioning";
        plots            = root / "06_Plots";
        animations       = root / "07_Animations";

        // 04
        env_global            = load_envelopes / "Global";
        env_global_point      = env_global / "Point_Load";
        env_global_dist       = env_global / "Distributed_Load";
        env_global_combined   = env_global / "Combined_Load";

        env_critical          = load_envelopes / "Critical_Section";
        env_critical_point    = env_critical / "Point_Load";
        env_critical_dist     = env_critical / "Distributed_Load";
        env_critical_combined = env_critical / "Combined_Load";

        // 05
        pos_global            = load_positioning / "Global";
        pos_global_point      = pos_global / "Point_Load";
        pos_global_dist       = pos_global / "Distributed_Load";
        pos_global_combined   = pos_global / "Combined_Load";

        pos_critical          = load_positioning / "Critical_Section";
        pos_critical_point    = pos_critical / "Point_Load";
        pos_critical_dist     = pos_critical / "Distributed_Load";
        pos_critical_combined = pos_critical / "Combined_Load";

        // 06
        plots_all          = plots / "All";
        plots_maximum      = plots / "Maximum";
        plots_envelopes    = plots / "Envelopes";
        plots_env_point    = plots_envelopes / "Point_Load";
        plots_env_dist     = plots_envelopes / "Distributed_Load";
        plots_env_combined = plots_envelopes / "Combined_Load";

        // 07
        anim_results       = animations / "Results";
        anim_results_gif   = anim_results / "GIF";
        anim_results_mp4   = anim_results / "MP4";
        anim_curvature     = animations / "Curvature";
        anim_curvature_gif = anim_curvature / "GIF";
        anim_curvature_mp4 = anim_curvature / "MP4";

        // 03 / Per_Span  [NEW]
        crit_per_span = critical_values / "Per_Span";

        // 08  [NEW]
        statistics = root / "08_Statistics";

        // 09  [NEW]
        zero_crossings = root / "09_Zero_Crossings";

        // 10  [NEW]
        support_reactions = root / "10_Support_Reactions";
    }

    /// Cree toute l'arborescence sur disque (idempotent).
    void createAll() const {
        for (const auto& d : {
            input, influence_lines, critical_values,
            env_global_point,    env_global_dist,    env_global_combined,
            env_critical_point,  env_critical_dist,  env_critical_combined,
            pos_global_point,    pos_global_dist,    pos_global_combined,
            pos_critical_point,  pos_critical_dist,  pos_critical_combined,
            plots_all, plots_maximum,
            plots_env_point, plots_env_dist, plots_env_combined,
            anim_results_gif,   anim_results_mp4,
            anim_curvature_gif, anim_curvature_mp4,
            crit_per_span,                           // [NEW]
            statistics,                              // [NEW]
            zero_crossings,                          // [NEW]
            support_reactions                        // [NEW]
        })
            std::filesystem::create_directories(d);
    }
};

#endif // __PROJECT_PATHS__
