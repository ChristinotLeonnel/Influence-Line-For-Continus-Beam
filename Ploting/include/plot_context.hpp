#pragma once
/**
 * plot_context.hpp
 * Contexte géométrique partagé par tous les modules de tracé.
 *
 * Équivalent C++ de plot_context.py — mais :
 *   - Données stockées en Eigen::VectorXd / std::vector<Eigen::VectorXd>
 *     pour les opérations vectorisées (calcul de min/max, normalisation).
 *   - Construction paresseuse (lazy) via singleton thread-safe.
 *   - Zéro copie : les vecteurs sont construits une seule fois.
 */

#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "json_loader.hpp"  // inclut data_paths.hpp → Utils.h

namespace influence_line {
namespace plots {

// ─────────────────────────────────────────────────────────────────────────────
//  Types alias
// ─────────────────────────────────────────────────────────────────────────────
using Vec1D = std::vector<double>;                    // une série x ou y
using Vec2D = std::vector<Vec1D>;                     // [span][alpha]
using Vec3D = std::vector<Vec2D>;                     // [span][section][alpha]

// ─────────────────────────────────────────────────────────────────────────────
//  PlotContext
// ─────────────────────────────────────────────────────────────────────────────
struct PlotContext {
    Vec1D              x_normal;    ///< abscisses globales normalisées
    Vec3D              x_forces;    ///< abscisses effort tranchant [span][sec][pt]
    Vec1D              nodes;       ///< longueurs cumulées des appuis
    std::vector<std::string> distances; ///< labels formatés des appuis

    // Valeurs pré-calculées (évite des recomputes dans chaque appel de tracé)
    double x_min{0.0};
    double x_max{0.0};

    bool empty() const { return x_normal.empty(); }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Chargement
// ─────────────────────────────────────────────────────────────────────────────
inline PlotContext load_plot_context(const std::filesystem::path& base_dir = {})
{
    using namespace influence_line::io;

    PlotContext ctx;

    // x_normal : abscisse.json → Vec1D
    ctx.x_normal = open_json_as<Vec1D>("abscissa.json",       "02_Influence_Lines", base_dir);

    // x_forces : shear_abscissa.json → Vec3D
    ctx.x_forces = open_json_as<Vec3D>("shear_abscissa.json", "02_Influence_Lines", base_dir);

    // nodes
    ctx.nodes    = open_json_as<Vec1D>("node_lengths.json",   "02_Influence_Lines", base_dir);

    // labels formatés
    ctx.distances.reserve(ctx.nodes.size());
    for (double n : ctx.nodes) {
        std::ostringstream oss;
        oss << std::fixed;
        oss.precision(5);
        oss << n;
        ctx.distances.push_back(oss.str());
    }

    // pré-calculs — utilise MaxValueInVector de Utils.h
    if (!ctx.x_normal.empty()) {
        // min : valeur de plus grande amplitude négative ou 0
        ctx.x_min = *std::min_element(ctx.x_normal.begin(), ctx.x_normal.end());
        ctx.x_max = *std::max_element(ctx.x_normal.begin(), ctx.x_normal.end());
    }

    return ctx;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Singleton thread-safe  (remplace get_context() Python)
// ─────────────────────────────────────────────────────────────────────────────
inline const PlotContext& get_context(const std::filesystem::path& base_dir = {})
{
    static std::once_flag  flag;
    static PlotContext     ctx;
    std::call_once(flag, [&]{ ctx = load_plot_context(base_dir); });
    return ctx;
}

} // namespace plots
} // namespace influence_line
