#pragma once
/**
 * plot_results.hpp
 * Static PNG plots — all visual parameters from PlotConfig / plot_config.json.
 */

#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include "render_common.hpp"
#include "json_loader.hpp"

namespace fs = std::filesystem;

namespace influence_line {
namespace plots {

// =============================================================================
//  Curve type
// =============================================================================
enum class CurveType {
    BendingMoment, ShearForce, Deflection,
    Rotation, SupportMoment, SupportReactions,
};

inline CurveType parse_curve_type(const std::string& name) {
    if (name == "bending_moment.json")    return CurveType::BendingMoment;
    if (name == "shear_force.json")       return CurveType::ShearForce;
    if (name == "deflection.json")        return CurveType::Deflection;
    if (name == "rotation.json")          return CurveType::Rotation;
    if (name == "support_moment.json")    return CurveType::SupportMoment;
    if (name == "support_reactions.json") return CurveType::SupportReactions;
    throw std::invalid_argument("Unknown curve type: " + name);
}

inline std::string curve_label(CurveType t) {
    switch (t) {
    case CurveType::BendingMoment:    return "Bending Moment";
    case CurveType::ShearForce:       return "Shear Force";
    case CurveType::Deflection:       return "Deflection";
    case CurveType::Rotation:         return "Rotation";
    case CurveType::SupportMoment:    return "Support Moment";
    case CurveType::SupportReactions: return "Support Reactions";
    }
    return "Unknown";
}

inline std::string curve_filename(CurveType t) {
    switch (t) {
    case CurveType::BendingMoment:    return "bending_moment.json";
    case CurveType::ShearForce:       return "shear_force.json";
    case CurveType::Deflection:       return "deflection.json";
    case CurveType::Rotation:         return "rotation.json";
    case CurveType::SupportMoment:    return "support_moment.json";
    case CurveType::SupportReactions: return "support_reactions.json";
    }
    return "bending_moment.json";
}

// =============================================================================
//  Plot options
// =============================================================================
struct PlotOptions {
    std::optional<int> span;
    std::optional<int> section;
    bool show_maximum  = false;
    bool is_show       = false;
    bool is_save       = false;
    fs::path save_dir;
};

// =============================================================================
//  Main entry point
// =============================================================================
inline void plot_analysis_results(
    const std::string& filename,
    PlotOptions        opts     = {},
    const fs::path&    base_dir = {})
{
    const PlotConfig&  cfg = load_plot_config(base_dir);
    const PlotContext& ctx = get_context(base_dir);
    const CurveType    ct  = parse_curve_type(filename);
    const std::string  name = curve_label(ct);

    std::vector<Vec1D>       xs, ys;
    std::vector<std::string> labels;

    if (opts.show_maximum) {
        const std::string stem = curve_filename(ct).substr(
            0, curve_filename(ct).rfind('.'));
        const auto& mj = io::open_json(stem + ".json", "03_Critical_Values");
        const int si = mj.value("span",0), sc = mj.value("section",0);
        if (ct == CurveType::ShearForce) {
            auto d = io::open_json_as<Vec3D>("shear_force.json","02_Influence_Lines");
            xs.push_back(ctx.x_forces[si][sc]); ys.push_back(d[si][sc]);
        } else {
            auto d = io::open_json_as<Vec3D>(curve_filename(ct),"02_Influence_Lines");
            xs.push_back(ctx.x_normal); ys.push_back(d[si][sc]);
        }
        labels.push_back("Span "+std::to_string(si+1)+" Sec "+std::to_string(sc));
    }
    else if (ct == CurveType::ShearForce) {
        auto d = io::open_json_as<Vec3D>("shear_force.json","02_Influence_Lines");
        for (int ii=0;ii<(int)d.size();++ii)
            for (int jj=0;jj<(int)d[ii].size();++jj) {
                xs.push_back(ctx.x_forces[ii][jj]); ys.push_back(d[ii][jj]);
                labels.push_back(jj==0?"T"+std::to_string(ii+1):"_");
            }
    }
    else {
        auto d = io::open_json_as<Vec3D>(curve_filename(ct),"02_Influence_Lines");
        for (int ii=0;ii<(int)d.size();++ii)
            for (int jj=0;jj<(int)d[ii].size();++jj) {
                xs.push_back(ctx.x_normal); ys.push_back(d[ii][jj]);
                labels.push_back(jj==0?"Span "+std::to_string(ii+1):"_");
            }
    }

    if (xs.empty()) return;

    // Ranges
    auto [xmin,xmax] = render::x_range(xs[0]);
    std::vector<std::reference_wrapper<const Vec1D>> all_y;
    for (const auto& v:ys) all_y.push_back(std::cref(v));
    auto [ymin,ymax] = render::y_range(all_y);

    // Render
    render::MultiFrameSpec spec{xs, ys, labels, ctx, cfg, name,
                                 xmin, xmax, ymin, ymax};
    cv::Mat frame = render::make_multi_frame(spec);

    // Annotate the peak value on Maximum plots
    if (opts.show_maximum && !xs.empty() && !ys.empty()) {
        render::Canvas cv_(cfg.figure);
        render::draw_peak_annotation(frame, cv_, cfg,
                                      xs[0], ys[0],
                                      xmin, xmax, ymin, ymax);
    }

    // Save
    if (opts.is_save) {
        const fs::path dir = opts.save_dir.empty()
            ? (io::influence_line_dir() / "06_Plots")
            : opts.save_dir;
        fs::create_directories(dir);
        const fs::path out = dir / (name + ".png");
        if (!cv::imwrite(out.string(), frame))
            throw std::runtime_error("imwrite failed: " + out.string());
    }

    if (opts.is_show) {
        cv::imshow(name, frame);
        cv::waitKey(0);
        cv::destroyWindow(name);
    }
}

} // namespace plots
} // namespace influence_line
