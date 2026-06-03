#pragma once
/**
 * animate_results.hpp
 * MP4 + GIF animations - tous les parametres visuels viennent de PlotConfig.
 *
 * Arborescence de sortie :
 *   <root>/07_Animations/Results/{GIF,MP4}/   (animations completes)
 *   <root>/07_Animations/Curvature/{GIF,MP4}/ (animations point par point)
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/videoio.hpp>

#include "render_common.hpp"
#include "gif_writer.hpp"
#include "json_loader.hpp"

namespace fs = std::filesystem;

namespace influence_line {
namespace animations {

using namespace plots;

// =============================================================================
//  Liste des courbes
// =============================================================================
inline constexpr std::array<const char*, 4> ALL_CURVES = {
    "shear_force.json", "bending_moment.json", "deflection.json", "rotation.json"
};

// =============================================================================
//  Frames
// =============================================================================
struct CurveFrames {
    std::vector<Vec1D> frames;
    std::vector<Vec1D> x_coords;
    bool is_shear = false;
};

inline CurveFrames load_curve_frames(const std::string& filename,
                                     const PlotContext& ctx)
{
    CurveFrames r;
    r.is_shear = (filename == "shear_force.json");
    auto data  = io::open_json_as<Vec3D>(filename, "02_Influence_Lines");

    if (r.is_shear) {
        for (int sp = 0; sp < (int)data.size(); ++sp)
            for (int sc = 0; sc < (int)data[sp].size(); ++sc) {
                r.frames.push_back(data[sp][sc]);
                Vec1D xv = ctx.x_forces[sp][sc];
                if (!xv.empty()) {
                    const double mn = *std::min_element(xv.begin(), xv.end());
                    for (double& v : xv) v -= mn;
                }
                r.x_coords.push_back(std::move(xv));
            }
    } else {
        Vec1D x_norm = ctx.x_normal;
        if (!x_norm.empty()) {
            const double mn = *std::min_element(x_norm.begin(), x_norm.end());
            for (double& v : x_norm) v -= mn;
        }
        for (auto& sp : data)
            for (auto& sc : sp)
                r.frames.push_back(sc);
        r.x_coords.assign(r.frames.size(), x_norm);
    }
    return r;
}

// =============================================================================
//  Options
// =============================================================================
struct AnimationOptions {
    bool     is_save = true;
    bool     is_show = false;
    fs::path save_dir_gif;
    fs::path save_dir_mp4;
};

// =============================================================================
//  Backend video portable
// =============================================================================
//  Sous Windows, MSMF est generalement deja present. Sous Linux/macOS, on
//  utilise FFMPEG (presque toujours disponible avec libopencv-videoio-ffmpeg).
//  Si l'ouverture echoue, on retombe sur CAP_ANY pour laisser OpenCV decider.
// =============================================================================
inline cv::VideoWriter open_writer(const fs::path& mp4,
                                   int fps, int w, int h)
{
    cv::VideoWriter wr;
#ifdef _WIN32
    constexpr int preferred = cv::CAP_MSMF;
#else
    constexpr int preferred = cv::CAP_FFMPEG;
#endif
    const int fourcc = cv::VideoWriter::fourcc('H', '2', '6', '4');

    wr.open(mp4.string(), preferred, fourcc, fps, {w, h});
    if (!wr.isOpened())
        wr.open(mp4.string(), cv::CAP_ANY, fourcc, fps, {w, h});

    if (!wr.isOpened())
        throw std::runtime_error("VideoWriter: cannot open " + mp4.string());
    return wr;
}

// =============================================================================
//  build_and_save_animation - Phase 1
// =============================================================================
inline void build_and_save_animation(const std::string& filename,
                                     AnimationOptions   opts     = {},
                                     const fs::path&    base_dir = {})
{
    const PlotConfig&  cfg = load_plot_config(base_dir);
    const PlotContext& ctx = get_context(base_dir);
    const CurveFrames  cf  = load_curve_frames(filename, ctx);
    const int          fps = cfg.fps();

    // Amplitude
    double abs_max = 0.0;
    for (const auto& fd : cf.frames)
        abs_max = std::max(abs_max, std::abs(MaxValueInVector(fd)));
    abs_max *= 1.1;

    // Nom
    std::string name = filename.substr(0, filename.rfind('.'));
    for (char& c : name) if (c == '_') c = ' ';

    // Plage X
    double xmin =  std::numeric_limits<double>::max();
    double xmax = -std::numeric_limits<double>::max();
    for (const auto& xv : cf.x_coords) {
        if (xv.empty()) continue;
        xmin = std::min(xmin, *std::min_element(xv.begin(), xv.end()));
        xmax = std::max(xmax, *std::max_element(xv.begin(), xv.end()));
    }

    // Repertoires de sortie - racine resolue UNE FOIS
    const fs::path root = io::resolve_base_dir(base_dir);
    if (opts.save_dir_mp4.empty())
        opts.save_dir_mp4 = root / "07_Animations" / "Results" / "MP4";
    if (opts.save_dir_gif.empty())
        opts.save_dir_gif = root / "07_Animations" / "Results" / "GIF";
    if (opts.is_save) {
        fs::create_directories(opts.save_dir_mp4);
        fs::create_directories(opts.save_dir_gif);
    }

    const int W = cfg.figure.width, H = cfg.figure.height;
    const int delay_cs = std::max(1, 100 / fps);

    cv::VideoWriter writer;
    GifWriter gif_writer{};
    if (opts.is_save) {
        writer     = open_writer(opts.save_dir_mp4 / (name + ".mp4"), fps, W, H);
        gif_writer = gif_utils::open_gif(
            (opts.save_dir_gif / (name + ".gif")).string(), W, H, delay_cs);
    }

    // Plage Y sur l'ensemble des frames
    double ymin =  std::numeric_limits<double>::max();
    double ymax = -std::numeric_limits<double>::max();
    for (const auto& fd : cf.frames)
        for (double v : fd) { ymin = std::min(ymin, v); ymax = std::max(ymax, v); }
    const double ym = (ymax - ymin) * 0.12;
    ymin -= ym; ymax += ym;

    for (std::size_t fidx = 0; fidx < cf.frames.size(); ++fidx) {
        render::FrameSpec spec{
            cf.x_coords[fidx], cf.frames[fidx],
            cf.frames[fidx].size(),
            ctx, cfg, name,
            xmin, xmax, ymin, ymax,
            abs_max,
            false,
            (int)(fidx % cfg.curves.size()),
            {name}, {(int)(fidx % cfg.curves.size())}
        };
        cv::Mat frame = render::make_frame(spec);
        if (opts.is_save) {
            writer.write(frame);
            gif_utils::write_frame(gif_writer, frame, delay_cs);
        }
    }

    if (opts.is_save) {
        writer.release();
        gif_utils::close_gif(gif_writer);
    }
}

// =============================================================================
//  Curvature
// =============================================================================
struct CurvatureAnimOptions {
    int      span    = 0;
    int      section = 0;
    fs::path save_dir_gif;
    fs::path save_dir_mp4;
    bool     is_save = true;
};

// =============================================================================
//  animate_curvature - Phase 3
// =============================================================================
inline void animate_curvature(const std::string&   filename,
                              CurvatureAnimOptions opts     = {},
                              const fs::path&      base_dir = {})
{
    const PlotConfig&  cfg = load_plot_config(base_dir);
    const PlotContext& ctx = get_context(base_dir);

    Vec1D y_vals = io::open_json_as<Vec3D>(filename, "02_Influence_Lines")
                       [opts.span][opts.section];
    Vec1D x_vals = (filename == "shear_force.json")
        ? ctx.x_forces[opts.span][opts.section]
        : ctx.x_normal;

    if (x_vals.empty() || y_vals.empty()) return;

    const double x0 = *std::min_element(x_vals.begin(), x_vals.end());
    for (double& v : x_vals) v -= x0;

    // Contexte avec noeuds decales
    PlotContext ctx2 = ctx;
    for (double& v : ctx2.nodes) v -= x0;
    ctx2.distances.clear();
    for (double nd : ctx2.nodes) {
        std::ostringstream oss;
        oss << std::fixed; oss.precision(2); oss << nd;
        ctx2.distances.push_back(oss.str());
    }

    // Plages
    const double yr = *std::max_element(y_vals.begin(), y_vals.end())
                    - *std::min_element(y_vals.begin(), y_vals.end());
    const double ym   = (yr > 1e-12) ? yr * 0.15 : 1.0;
    const double ymin = *std::min_element(y_vals.begin(), y_vals.end()) - ym;
    const double ymax = *std::max_element(y_vals.begin(), y_vals.end()) + ym;
    auto [xmin, xmax] = render::x_range(x_vals);

    std::string name = filename.substr(0, filename.rfind('.'));
    for (char& c : name) if (c == '_') c = ' ';
    const std::string title = name + " T" + std::to_string(opts.span + 1)
                            + " S" + std::to_string(opts.section);

    // Repertoires de sortie - racine resolue UNE FOIS
    const fs::path root = io::resolve_base_dir(base_dir);
    if (opts.save_dir_mp4.empty())
        opts.save_dir_mp4 = root / "07_Animations" / "Curvature" / "MP4";
    if (opts.save_dir_gif.empty())
        opts.save_dir_gif = root / "07_Animations" / "Curvature" / "GIF";
    if (opts.is_save) {
        fs::create_directories(opts.save_dir_mp4);
        fs::create_directories(opts.save_dir_gif);
    }

    const int W = cfg.figure.width, H = cfg.figure.height;
    const int fps = cfg.fps();
    const int delay_cs = std::max(1, 100 / fps);

    cv::VideoWriter writer;
    GifWriter gif_writer{};
    if (opts.is_save) {
        writer     = open_writer(opts.save_dir_mp4 / (name + ".mp4"), fps, W, H);
        gif_writer = gif_utils::open_gif(
            (opts.save_dir_gif / (name + ".gif")).string(), W, H, delay_cs);
    }

    // Reveler un point par frame
    for (std::size_t count = 1; count <= x_vals.size(); ++count) {
        render::FrameSpec spec{
            x_vals, y_vals, count,
            ctx2, cfg, title,
            xmin, xmax, ymin, ymax,
            0.0,
            true,
            0,
            {title}, {0}
        };
        cv::Mat frame = render::make_frame(spec);
        if (opts.is_save) {
            writer.write(frame);
            gif_utils::write_frame(gif_writer, frame, delay_cs);
        }
    }

    if (opts.is_save) {
        writer.release();
        gif_utils::close_gif(gif_writer);
    }
}

} // namespace animations
} // namespace influence_line
