#pragma once
/**
 * envelope_plots.hpp
 * Phase 4 — Global load envelope plots.
 *
 * For each curve (BM, SF, Deflection, Rotation) × each load type
 * (Point_Load, Distributed_Load, Combined_Load), draws:
 *
 *   1. The influence line at the critical section (span/section from envelope JSON)
 *   2. A vertical marker at the optimal load position
 *   3. An annotation box showing: load name, value, position
 *   4. The peak value annotation (reuses draw_peak_annotation from render_common)
 *
 * Output: 05_Output/Plots/Envelopes/{Point_Load,Distributed_Load,Combined_Load}/
 *         one PNG per curve × load type = 12 PNGs total
 */

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>

#include "render_common.hpp"
#include "json_loader.hpp"

namespace fs = std::filesystem;

namespace influence_line {
namespace envelopes {

using namespace plots;
using namespace render;

// =============================================================================
//  Load type metadata
// =============================================================================
struct LoadType {
    std::string dir_name;    // folder under 04_Load_Envelopes/Global/
    std::string label;       // display label
    cv::Scalar  color;       // BGR color for markers
};

inline const std::vector<LoadType>& load_types() {
    static const std::vector<LoadType> LT = {
        {"Point_Load",       "Point Load",       {30,  100, 200}},  // orange
        {"Distributed_Load", "Distributed Load", {30,  160,  30}},  // green
        {"Combined_Load",    "Combined Load",    {160,  30, 180}},  // purple
    };
    return LT;
}

// =============================================================================
//  Draw load position markers onto a frame
//
//  The envelope JSON has:
//    { "maximum": val, "span": i, "section": j, "position": pos,
//      "load": { "LoadName": { "Position": x, "value": v, ... } } }
//
//  We draw a vertical dashed line at each load's Position,
//  plus an annotation box summarising the envelope result.
// =============================================================================
inline void draw_envelope_markers(
    cv::Mat&              frame,
    const Canvas&         cv_,
    const nlohmann::json& env_json,
    const LoadType&       lt,
    double xmin, double xmax,
    double ymin, double ymax,
    double abs_max)
{
    const cv::Scalar& col    = lt.color;
    const double      ax_sc  = 0.36;
    const int         font   = cv::FONT_HERSHEY_SIMPLEX;

    // ── 1. Vertical marker at global optimum position ─────────────────────
    if (env_json.contains("position")) {
        const double pos = env_json["position"].get<double>();
        const int    px  = cv_.px(pos, xmin, xmax);
        // Dashed vertical line
        const int dash = 7, gap = 5;
        for (int y = cv_.y0; y < cv_.y1; y += dash + gap) {
            int y2 = std::min(y + dash, cv_.y1);
            cv::line(frame, {px, y}, {px, y2}, col, 2, cv::LINE_AA);
        }
        // Arrow at top
        cv::arrowedLine(frame, {px, cv_.y0 + 20}, {px, cv_.y0 + 5},
                        col, 2, cv::LINE_AA, 0, 0.3);
    }

    // ── 2. Per-load vertical ticks at each individual load position ────────
    if (env_json.contains("load") && env_json["load"].is_object()) {
        for (const auto& [name, info] : env_json["load"].items()) {
            if (!info.contains("Position")) continue;
            const double lpos = info["Position"].get<double>();
            if (lpos < xmin || lpos > xmax) continue;
            const int lpx = cv_.px(lpos, xmin, xmax);
            // Short tick at bottom of plot
            cv::line(frame, {lpx, cv_.y1}, {lpx, cv_.y1 - 12}, col, 2, cv::LINE_AA);
            // Small triangle marker
            std::vector<cv::Point> tri = {
                {lpx,     cv_.y1 - 12},
                {lpx - 5, cv_.y1 - 20},
                {lpx + 5, cv_.y1 - 20}
            };
            cv::fillPoly(frame, {tri}, col);
        }
    }

    // ── 3. Annotation box (top-left corner) ───────────────────────────────
    const double maximum = env_json.value("maximum", 0.0);
    const int    span    = env_json.value("span",    0);
    const int    section = env_json.value("section", 0);

    std::vector<std::string> lines;
    lines.push_back(lt.label);
    lines.push_back("max = " + smart_format(maximum, 5));
    lines.push_back("span " + std::to_string(span + 1)
                  + "  sec " + std::to_string(section));

    // Measure box
    int max_w = 0, total_h = 0;
    const int row_h = 17, pad = 8;
    for (const auto& ln : lines) {
        int bl = 0;
        auto ts = cv::getTextSize(ln, font, ax_sc, 1, &bl);
        max_w = std::max(max_w, ts.width);
        total_h += row_h;
    }

    const int bx = cv_.x0 + pad;
    const int by = cv_.y0 + pad;
    const int bw = max_w + pad * 2;
    const int bh = total_h + pad;

    // Box background
    cv::rectangle(frame, {bx, by}, {bx + bw, by + bh},
                  col, 1, cv::LINE_AA);
    cv::rectangle(frame, {bx + 1, by + 1}, {bx + bw - 1, by + bh - 1},
                  cv::Scalar(245, 245, 255), cv::FILLED);

    // Text rows
    for (int i = 0; i < (int)lines.size(); ++i) {
        const cv::Scalar tc = (i == 0) ? col : cv::Scalar(50, 50, 50);
        const double     sc = (i == 0) ? ax_sc * 1.05 : ax_sc;
        cv::putText(frame, lines[i],
                    {bx + pad, by + pad + (i + 1) * row_h - 3},
                    font, sc, tc, 1, cv::LINE_AA);
    }

    // ── 4. Peak annotation on the curve ───────────────────────────────────
    // (drawn last so it appears on top)
}

// =============================================================================
//  plot_envelope — one PNG per (curve, load_type)
// =============================================================================
inline void plot_envelope(
    const std::string& curve_filename,   // e.g. "shear_force.json"
    const std::string& curve_name,       // e.g. "Shear Force"
    const LoadType&    lt,
    const fs::path&    base_dir,
    const fs::path&    out_dir)
{
    const PlotConfig&  cfg = load_plot_config(base_dir);
    const PlotContext& ctx = get_context(base_dir);

    // ── Load envelope JSON ─────────────────────────────────────────────────
    const fs::path env_path = base_dir / "04_Load_Envelopes" / "Global"
                            / lt.dir_name
                            / curve_filename;
    if (!fs::exists(env_path)) return;  // load type not computed

    const nlohmann::json env_json = io::JsonCache::instance().get(env_path);

    const int  span_idx = env_json.value("span",    0);
    const int  sec_idx  = env_json.value("section", 0);

    // ── Load influence line at critical section ────────────────────────────
    Vec1D x_vals, y_vals;

    const bool is_shear = (curve_filename == "shear_force.json");
    if (is_shear) {
        auto data = io::open_json_as<Vec3D>("shear_force.json", "02_Influence_Lines");
        y_vals = data[span_idx][sec_idx];
        x_vals = ctx.x_forces[span_idx][sec_idx];
        // Normalize X to start at 0
        double xmin_raw = *std::min_element(x_vals.begin(), x_vals.end());
        for (double& v : x_vals) v -= xmin_raw;
    } else {
        auto data = io::open_json_as<Vec3D>(curve_filename, "02_Influence_Lines");
        y_vals = data[span_idx][sec_idx];
        x_vals = ctx.x_normal;
    }

    if (x_vals.empty() || y_vals.empty()) return;

    // ── Compute ranges ─────────────────────────────────────────────────────
    const double xmin_d = *std::min_element(x_vals.begin(), x_vals.end());
    const double xmax_d = *std::max_element(x_vals.begin(), x_vals.end());
    const double xm     = (xmax_d - xmin_d) * 0.03;
    const double xmin   = xmin_d - xm;
    const double xmax   = xmax_d + xm;

    double ymin_d = *std::min_element(y_vals.begin(), y_vals.end());
    double ymax_d = *std::max_element(y_vals.begin(), y_vals.end());
    if (ymin_d == ymax_d) { ymin_d -= 1.0; ymax_d += 1.0; }
    const double ym   = (ymax_d - ymin_d) * 0.15;
    const double ymin = ymin_d - ym;
    const double ymax = ymax_d + ym;

    const double abs_max = *std::max_element(
        y_vals.begin(), y_vals.end(),
        [](double a, double b){ return std::abs(a) < std::abs(b); });

    // ── Build context for shifted nodes (shear) ───────────────────────────
    PlotContext ctx_draw = ctx;
    if (is_shear) {
        double xmin_raw = *std::min_element(
            ctx.x_forces[span_idx][sec_idx].begin(),
            ctx.x_forces[span_idx][sec_idx].end());
        ctx_draw.nodes.clear();
        ctx_draw.distances.clear();
        // Use original nodes shifted
        for (double nd : ctx.nodes) {
            double shifted = nd - xmin_raw;
            if (shifted >= xmin && shifted <= xmax) {
                ctx_draw.nodes.push_back(shifted);
                std::ostringstream oss;
                oss << std::fixed; oss.precision(1); oss << nd;
                ctx_draw.distances.push_back(oss.str());
            }
        }
    }

    // ── Render base frame ─────────────────────────────────────────────────
    const std::string title = curve_name + " — " + lt.label
                            + "  (span " + std::to_string(span_idx + 1)
                            + "  sec " + std::to_string(sec_idx) + ")";

    render::MultiFrameSpec spec{
        {x_vals}, {y_vals}, {""},
        ctx_draw, cfg, title,
        xmin, xmax, ymin, ymax
    };
    cv::Mat frame = render::make_multi_frame(spec);

    // ── Draw envelope markers on top ──────────────────────────────────────
    Canvas cv_(cfg.figure);
    draw_envelope_markers(frame, cv_, env_json, lt,
                          xmin, xmax, ymin, ymax, std::abs(abs_max));

    // ── Peak annotation ───────────────────────────────────────────────────
    draw_peak_annotation(frame, cv_, cfg, x_vals, y_vals,
                         xmin, xmax, ymin, ymax);

    // ── Save ──────────────────────────────────────────────────────────────
    fs::create_directories(out_dir);
    const fs::path out = out_dir / (curve_name + ".png");
    if (!cv::imwrite(out.string(), frame))
        throw std::runtime_error("envelope_plots: imwrite failed: " + out.string());
}

// =============================================================================
//  run_envelope_plots — Phase 4 entry point
//  Called from Ploting.cpp after Phase 3
// =============================================================================
inline void run_envelope_plots(
    const fs::path&                  base_dir,
    const fs::path&                  plots_dir,
    const std::vector<std::string>&  curve_files,
    const std::vector<std::string>&  curve_names)
{
    const fs::path env_out = plots_dir / "Envelopes";

    std::cout << "=== Phase 4 : Global Envelope Plots ===\n";

    for (const auto& lt : load_types()) {
        const fs::path type_out = env_out / lt.dir_name;
        fs::create_directories(type_out);

        for (std::size_t i = 0; i < curve_files.size(); ++i) {
            const std::string& cf   = curve_files[i];
            const std::string& name = curve_names[i];
            try {
                plot_envelope(cf, name, lt, base_dir, type_out);
                std::cout << "    OK     " << name
                          << " [" << lt.label << "]\n";
            } catch (const std::exception& ex) {
                std::cout << "    ERR  " << name
                          << " [" << lt.label << "] : " << ex.what() << "\n";
            }
        }
    }

    std::cout << "Phase 4 : Global Envelope Plots completed.\n\n";
}

} // namespace envelopes
} // namespace influence_line
