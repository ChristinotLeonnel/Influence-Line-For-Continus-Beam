#pragma once
/**
 * plot_config.hpp
 * Configuration visuelle des tracés (équivalent de plot_config.json + plot_utils.py).
 *
 * Améliorations vs Python :
 *   - Struct typée → pas d'accès par clé string à l'exécution.
 *   - deep_merge garanti à la compilation (pas de dict dynamique).
 *   - Rechargement optionnel (hot-reload) sans redémarrage.
 */

#include <filesystem>
#include <fstream>
#include <string>

#include "data_paths.hpp"   // inclut Utils.h → nlohmann + getConfigPath() + getEnvSafe()

namespace fs   = std::filesystem;
using     json = nlohmann::json;

namespace influence_line {
namespace plots {

// ─────────────────────────────────────────────────────────────────────────────
//  Sous-structure Style
// ─────────────────────────────────────────────────────────────────────────────
struct PlotStyle {
    std::string line_color        = "royalblue";
    std::string grid_color        = "#E0E0E0";
    std::string minor_grid_color  = "#F5F5F5";
    std::string noeud_color       = "#FF4444";
    double      noeud_size        = 100.0;
    double      line_width        = 2.5;
    std::string line_style        = "-";
    std::string background_color  = "#FFFFFF";
    std::string edge_color        = "#2C3E50";
    std::string shadow_color      = "gray";
    double      shadow_alpha      = 0.3;
    std::string title             = "";
    std::string xlabel            = "Length des travées";
    std::string ylabel            = "Valeur";
    double      axis_fontsize     = 12.0;
    std::string font_family       = "Times New Roman";
    std::string legend_position   = "best";
    std::string marker_style      = "o";
    double      legend_fontsize   = 10.0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Configuration principale
// ─────────────────────────────────────────────────────────────────────────────
struct PlotConfig {
    bool      grid                    = true;
    bool      travee                  = true;
    bool      noeud                   = true;
    double    vitesse                 = 10.0;
    double    vitesse_bridge          = 0.005;
    bool      legend                  = true;
    bool      axe_y_inverser          = false;
    bool      default_matplotlib_color = true;
    bool      default_matplotlib_style = true;
    PlotStyle style;

    /// FPS dérivé de vitesse (min 1, max 120)
    int fps() const {
        int v = static_cast<int>(vitesse);
        return (v >= 1 && v <= 120) ? v : 20;
    }

    /// Intervalle animation en ms
    int interval_ms() const {
        return 1000 / fps();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Sérialisation nlohmann (MACRO_FREE — explicite)
// ─────────────────────────────────────────────────────────────────────────────
inline void from_json(const json& j, PlotStyle& s) {
    auto get = [&](const char* k, auto& v) {
        if (j.contains(k)) j.at(k).get_to(v);
    };
    get("line_color",       s.line_color);
    get("grid_color",       s.grid_color);
    get("minor_grid_color", s.minor_grid_color);
    get("noeud_color",      s.noeud_color);
    get("noeud_size",       s.noeud_size);
    get("line_width",       s.line_width);
    get("line_style",       s.line_style);
    get("background_color", s.background_color);
    get("edge_color",       s.edge_color);
    get("shadow_color",     s.shadow_color);
    get("shadow_alpha",     s.shadow_alpha);
    get("title",            s.title);
    get("xlabel",           s.xlabel);
    get("ylabel",           s.ylabel);
    get("axis_fontsize",    s.axis_fontsize);
    get("font_family",      s.font_family);
    get("legend_position",  s.legend_position);
    get("marker_style",     s.marker_style);
    get("legend_fontsize",  s.legend_fontsize);
}

inline void from_json(const json& j, PlotConfig& c) {
    auto get = [&](const char* k, auto& v) {
        if (j.contains(k)) j.at(k).get_to(v);
    };
    get("grid",                     c.grid);
    get("travee",                   c.travee);
    get("noeud",                    c.noeud);
    get("vitesse",                  c.vitesse);
    get("vitesse_bridge",           c.vitesse_bridge);
    get("legend",                   c.legend);
    get("axe_y_inverser",           c.axe_y_inverser);
    get("default_matplotlib_color", c.default_matplotlib_color);
    get("default_matplotlib_style", c.default_matplotlib_style);
    if (j.contains("style"))        from_json(j.at("style"), c.style);
}

inline void to_json(json& j, const PlotStyle& s) {
    j = json{
        {"line_color",       s.line_color},
        {"grid_color",       s.grid_color},
        {"minor_grid_color", s.minor_grid_color},
        {"noeud_color",      s.noeud_color},
        {"noeud_size",       s.noeud_size},
        {"line_width",       s.line_width},
        {"line_style",       s.line_style},
        {"background_color", s.background_color},
        {"edge_color",       s.edge_color},
        {"shadow_color",     s.shadow_color},
        {"shadow_alpha",     s.shadow_alpha},
        {"title",            s.title},
        {"xlabel",           s.xlabel},
        {"ylabel",           s.ylabel},
        {"axis_fontsize",    s.axis_fontsize},
        {"font_family",      s.font_family},
        {"legend_position",  s.legend_position},
        {"marker_style",     s.marker_style},
        {"legend_fontsize",  s.legend_fontsize},
    };
}

inline void to_json(json& j, const PlotConfig& c) {
    j = json{
        {"grid",                     c.grid},
        {"travee",                   c.travee},
        {"noeud",                    c.noeud},
        {"vitesse",                  c.vitesse},
        {"vitesse_bridge",           c.vitesse_bridge},
        {"legend",                   c.legend},
        {"axe_y_inverser",           c.axe_y_inverser},
        {"default_matplotlib_color", c.default_matplotlib_color},
        {"default_matplotlib_style", c.default_matplotlib_style},
        {"style",                    json(c.style)},
    };
    to_json(j["style"], c.style);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Chargement / sauvegarde
// ─────────────────────────────────────────────────────────────────────────────

inline fs::path config_path(const fs::path& base_dir = {}) {
    const fs::path root = base_dir.empty() ? io::influence_line_dir() : base_dir;
    return root / "plot_config.json";
}

inline PlotConfig load_plot_config(const fs::path& base_dir = {}) {
    PlotConfig cfg;   // valeurs par défaut
    const fs::path path = config_path(base_dir);

    if (fs::exists(path)) {
        try {
            std::ifstream file_stream(path);
            json j;
            file_stream >> j;
            from_json(j, cfg);
        } catch (...) {
            // Fichier corrompu : on conserve les defaults
        }
    }

    // Réécriture pour mettre à jour les nouvelles clés
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    json j;
    to_json(j, cfg);
    out << j.dump(2);

    return cfg;
}

inline void save_plot_config(const PlotConfig& cfg, const fs::path& base_dir = {}) {
    const fs::path path = config_path(base_dir);
    fs::create_directories(path.parent_path());
    std::ofstream out(path);
    json j;
    to_json(j, cfg);
    out << j.dump(2);
}

} // namespace plots
} // namespace influence_line
