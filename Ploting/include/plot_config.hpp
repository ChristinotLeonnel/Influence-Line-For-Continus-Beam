#pragma once
/**
 * plot_config.hpp
 * Full visual configuration for plots and animations.
 * All settings are read from / written to plot_config.json automatically.
 *
 * Edit plot_config.json to customize — changes apply on next run,
 * no recompilation needed.
 */

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "data_paths.hpp"

namespace fs  = std::filesystem;
using     json = nlohmann::json;

namespace influence_line {
namespace plots {

// =============================================================================
//  Figure size
// =============================================================================
struct FigureSize {
    int width  = 1280;   // pixels
    int height =  720;   // pixels
};

// =============================================================================
//  Curve style  (one entry per curve index — cycles if fewer than curves)
// =============================================================================
struct CurveStyle {
    std::string color     = "#1E5FA8";  // hex or named color
    double      thickness = 2.0;        // line thickness in pixels
    bool        filled    = false;      // fill area under curve
    double      fill_alpha = 0.15;      // fill opacity 0–1
};

// =============================================================================
//  Legend
// =============================================================================
struct LegendConfig {
    bool        show        = true;
    std::string position    = "top-right";  // top-right / top-left / bottom-right / bottom-left / none
    double      font_scale  = 0.40;         // OpenCV font scale
    int         thickness   = 1;            // text thickness
    std::string bg_color    = "#FFFFFFCC";  // background (hex, last 2 = alpha 00–FF)
    int         padding     = 8;            // pixels around text
    int         line_length = 24;           // color sample line length in pixels
};

// =============================================================================
//  Labels & title
// =============================================================================
struct LabelsConfig {
    std::string title_prefix  = "";      // prepended to curve name, e.g. "Influence Line — "
    double      title_scale   = 0.65;    // OpenCV font scale for title
    int         title_thickness = 1;
    double      axis_scale    = 0.38;    // tick label font scale
    int         axis_thickness = 1;
    double      ylabel_scale  = 0.40;
    std::string xlabel        = "Support distance (m)";
    std::string ylabel_suffix = "";      // appended to curve name for Y axis
};

// =============================================================================
//  Grid & background
// =============================================================================
struct GridConfig {
    bool        show            = true;
    std::string major_color     = "#D8D8D8";  // hex
    std::string minor_color     = "#EFEFEF";
    bool        show_minor      = false;
    int         major_thickness = 1;
    std::string background      = "#FFFFFF";  // plot background
    std::string axis_color      = "#222222";  // axes and ticks
    std::string zero_color      = "#888888";  // zero line
    std::string max_line_color  = "#DD2222";  // max/min reference lines
};

// =============================================================================
//  Node markers
// =============================================================================
struct NodeConfig {
    bool        show        = true;
    std::string color       = "#CC2222";
    int         radius      = 5;          // pixels
    bool        show_labels = true;
    double      label_scale = 0.33;
};

// =============================================================================
//  Span reference line (the horizontal beam line)
// =============================================================================
struct SpanLineConfig {
    bool        show      = true;
    std::string color     = "#AAAAAA";
    int         thickness = 1;
};

// =============================================================================
//  Animation
// =============================================================================
struct AnimConfig {
    int  fps        = 20;    // frames per second (1–120)
    bool show_cursor = true; // vertical cursor line at current point
    bool show_point  = true; // dot at current point
};

// =============================================================================
//  Main config — everything in one place
// =============================================================================
struct PlotConfig {
    FigureSize   figure;
    GridConfig   grid;
    NodeConfig   nodes;
    SpanLineConfig span_line;
    LegendConfig legend;
    LabelsConfig labels;
    AnimConfig   animation;

    // Curve styles — up to 8, cycles for more curves
    std::vector<CurveStyle> curves = {
        {"#1E5FA8", 1, false, 0.15},
        {"#C84B20", 1, false, 0.15},
        {"#1E9E50", 1, false, 0.15},
        {"#8B44A8", 1, false, 0.15},
        {"#C8A020", 1, false, 0.15},
        {"#20A0A0", 1, false, 0.15},
        {"#A84444", 1, false, 0.15},
        {"#4444A8", 1, false, 0.15},
    };

    // Legacy compat fields (still read from JSON if present)
    bool   axe_y_inverser = false;
    double vitesse        = 20.0;  // maps to animation.fps
    bool   noeud          = true;  // maps to nodes.show
    bool   travee         = true;  // maps to span_line.show

    int fps() const {
        int v = animation.fps;
        return (v >= 1 && v <= 120) ? v : 20;
    }

    // Get curve style by index (cycles)
    const CurveStyle& curve(int idx) const {
        if (curves.empty()) {
            static const CurveStyle fallback;
            return fallback;
        }
        return curves[idx % (int)curves.size()];
    }
};

// =============================================================================
//  JSON serialization
// =============================================================================
inline void from_json(const json& j, FigureSize& s) {
    auto g = [&](const char* k, auto& v){ if(j.contains(k)) j.at(k).get_to(v); };
    g("width", s.width); g("height", s.height);
}
inline void to_json(json& j, const FigureSize& s) {
    j = {{"width", s.width}, {"height", s.height}};
}

inline void from_json(const json& j, CurveStyle& s) {
    auto g = [&](const char* k, auto& v){ if(j.contains(k)) j.at(k).get_to(v); };
    g("color", s.color); g("thickness", s.thickness);
    g("filled", s.filled); g("fill_alpha", s.fill_alpha);
}
inline void to_json(json& j, const CurveStyle& s) {
    j = {{"color",s.color},{"thickness",s.thickness},
         {"filled",s.filled},{"fill_alpha",s.fill_alpha}};
}

inline void from_json(const json& j, LegendConfig& s) {
    auto g = [&](const char* k, auto& v){ if(j.contains(k)) j.at(k).get_to(v); };
    g("show",s.show); g("position",s.position); g("font_scale",s.font_scale);
    g("thickness",s.thickness); g("bg_color",s.bg_color);
    g("padding",s.padding); g("line_length",s.line_length);
}
inline void to_json(json& j, const LegendConfig& s) {
    j = {{"show",s.show},{"position",s.position},{"font_scale",s.font_scale},
         {"thickness",s.thickness},{"bg_color",s.bg_color},
         {"padding",s.padding},{"line_length",s.line_length}};
}

inline void from_json(const json& j, LabelsConfig& s) {
    auto g = [&](const char* k, auto& v){ if(j.contains(k)) j.at(k).get_to(v); };
    g("title_prefix",s.title_prefix); g("title_scale",s.title_scale);
    g("title_thickness",s.title_thickness); g("axis_scale",s.axis_scale);
    g("axis_thickness",s.axis_thickness); g("ylabel_scale",s.ylabel_scale);
    g("xlabel",s.xlabel); g("ylabel_suffix",s.ylabel_suffix);
}
inline void to_json(json& j, const LabelsConfig& s) {
    j = {{"title_prefix",s.title_prefix},{"title_scale",s.title_scale},
         {"title_thickness",s.title_thickness},{"axis_scale",s.axis_scale},
         {"axis_thickness",s.axis_thickness},{"ylabel_scale",s.ylabel_scale},
         {"xlabel",s.xlabel},{"ylabel_suffix",s.ylabel_suffix}};
}

inline void from_json(const json& j, GridConfig& s) {
    auto g = [&](const char* k, auto& v){ if(j.contains(k)) j.at(k).get_to(v); };
    g("show",s.show); g("major_color",s.major_color); g("minor_color",s.minor_color);
    g("show_minor",s.show_minor); g("major_thickness",s.major_thickness);
    g("background",s.background); g("axis_color",s.axis_color);
    g("zero_color",s.zero_color); g("max_line_color",s.max_line_color);
}
inline void to_json(json& j, const GridConfig& s) {
    j = {{"show",s.show},{"major_color",s.major_color},{"minor_color",s.minor_color},
         {"show_minor",s.show_minor},{"major_thickness",s.major_thickness},
         {"background",s.background},{"axis_color",s.axis_color},
         {"zero_color",s.zero_color},{"max_line_color",s.max_line_color}};
}

inline void from_json(const json& j, NodeConfig& s) {
    auto g = [&](const char* k, auto& v){ if(j.contains(k)) j.at(k).get_to(v); };
    g("show",s.show); g("color",s.color); g("radius",s.radius);
    g("show_labels",s.show_labels); g("label_scale",s.label_scale);
}
inline void to_json(json& j, const NodeConfig& s) {
    j = {{"show",s.show},{"color",s.color},{"radius",s.radius},
         {"show_labels",s.show_labels},{"label_scale",s.label_scale}};
}

inline void from_json(const json& j, SpanLineConfig& s) {
    auto g = [&](const char* k, auto& v){ if(j.contains(k)) j.at(k).get_to(v); };
    g("show",s.show); g("color",s.color); g("thickness",s.thickness);
}
inline void to_json(json& j, const SpanLineConfig& s) {
    j = {{"show",s.show},{"color",s.color},{"thickness",s.thickness}};
}

inline void from_json(const json& j, AnimConfig& s) {
    auto g = [&](const char* k, auto& v){ if(j.contains(k)) j.at(k).get_to(v); };
    g("fps",s.fps); g("show_cursor",s.show_cursor); g("show_point",s.show_point);
}
inline void to_json(json& j, const AnimConfig& s) {
    j = {{"fps",s.fps},{"show_cursor",s.show_cursor},{"show_point",s.show_point}};
}

inline void from_json(const json& j, PlotConfig& c) {
    auto g = [&](const char* k, auto& v){ if(j.contains(k)) j.at(k).get_to(v); };
    if(j.contains("figure"))    from_json(j.at("figure"),    c.figure);
    if(j.contains("grid"))      from_json(j.at("grid"),      c.grid);
    if(j.contains("nodes"))     from_json(j.at("nodes"),     c.nodes);
    if(j.contains("span_line")) from_json(j.at("span_line"), c.span_line);
    if(j.contains("legend"))    from_json(j.at("legend"),    c.legend);
    if(j.contains("labels"))    from_json(j.at("labels"),    c.labels);
    if(j.contains("animation")) from_json(j.at("animation"), c.animation);
    if(j.contains("curves"))    j.at("curves").get_to(c.curves);
    // Legacy compat
    g("axe_y_inverser", c.axe_y_inverser);
    if(j.contains("vitesse"))   { j.at("vitesse").get_to(c.vitesse); c.animation.fps = (int)c.vitesse; }
    if(j.contains("noeud"))     { j.at("noeud").get_to(c.noeud);     c.nodes.show = c.noeud; }
    if(j.contains("travee"))    { j.at("travee").get_to(c.travee);   c.span_line.show = c.travee; }
}

inline void to_json(json& j, const PlotConfig& c) {
    j = {
        {"figure",    json(c.figure)},
        {"grid",      json(c.grid)},
        {"nodes",     json(c.nodes)},
        {"span_line", json(c.span_line)},
        {"legend",    json(c.legend)},
        {"labels",    json(c.labels)},
        {"animation", json(c.animation)},
        {"curves",    c.curves},
        {"axe_y_inverser", c.axe_y_inverser},
    };
    to_json(j["figure"],    c.figure);
    to_json(j["grid"],      c.grid);
    to_json(j["nodes"],     c.nodes);
    to_json(j["span_line"], c.span_line);
    to_json(j["legend"],    c.legend);
    to_json(j["labels"],    c.labels);
    to_json(j["animation"], c.animation);
}

// =============================================================================
//  Load / save
// =============================================================================
inline fs::path config_path(const fs::path& base_dir = {}) {
    return (base_dir.empty() ? io::influence_line_dir() : base_dir)
           / "plot_config.json";
}

inline PlotConfig load_plot_config(const fs::path& base_dir = {}) {
    PlotConfig cfg;
    const fs::path path = config_path(base_dir);
    if (fs::exists(path)) {
        try {
            std::ifstream f(path); json j; f >> j; from_json(j, cfg);
        } catch (...) {}
    }
    // Always rewrite to add any new keys with defaults
    fs::create_directories(path.parent_path());
    std::ofstream out(path); json j; to_json(j, cfg); out << j.dump(2);
    return cfg;
}

inline void save_plot_config(const PlotConfig& cfg, const fs::path& base_dir = {}) {
    const fs::path path = config_path(base_dir);
    fs::create_directories(path.parent_path());
    std::ofstream out(path); json j; to_json(j, cfg); out << j.dump(2);
}

} // namespace plots
} // namespace influence_line
