#pragma once
/**
 * animate_results.hpp
 * MP4 animations of structural curves — pure OpenCV rendering.
 *
 * ARCHITECTURE (v3 — no gnuplot, no temp files):
 *   OpenCV drawing → frame rendered directly in memory as cv::Mat
 *   OpenCV VideoWriter → H.264 MP4 via MSMF (Windows built-in)
 *
 *   Why no matplot++ for animation:
 *     matplot++ uses gnuplot via popen() which is not thread-safe.
 *     Worker threads cannot share the gnuplot pipe — fig->save() produces
 *     no output when called from multiple threads simultaneously.
 *     Pure OpenCV drawing is thread-safe, faster, and needs no temp files.
 *
 * Phase 2 static plots still use matplot++ (single thread, works fine).
 */

#include <algorithm>
#include <array>
#include <cstdlib>   // malloc/free for gif.h
#include <cmath>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

// gif_writer.hpp must come before plot_config to ensure GifWriter type is defined
#include "gif_writer.hpp"
#include "plot_config.hpp"
#include "plot_context.hpp"
#include "json_loader.hpp"

namespace fs = std::filesystem;

namespace influence_line {
namespace animations {

// =============================================================================
//  Constants
// =============================================================================
inline constexpr std::array<const char*, 4> ALL_CURVES = {
    "shear_force.json",
    "bending_moment.json",
    "deflection.json",
    "rotation.json",
};

static constexpr int FRAME_W  = 1280;
static constexpr int FRAME_H  = 720;
static constexpr int MARGIN_L = 90;   // left  margin (y-axis labels)
static constexpr int MARGIN_R = 30;   // right margin
static constexpr int MARGIN_T = 60;   // top   margin (title)
static constexpr int MARGIN_B = 70;   // bottom margin (x-axis labels)

// Plot area bounds
static constexpr int PX0 = MARGIN_L;
static constexpr int PX1 = FRAME_W - MARGIN_R;
static constexpr int PY0 = MARGIN_T;
static constexpr int PY1 = FRAME_H - MARGIN_B;

// =============================================================================
//  OpenCV color helpers
// =============================================================================
namespace detail {

inline cv::Scalar hex_to_bgr(const std::string& hex) {
    if (hex.size() < 7 || hex[0] != '#') return {200, 80, 30};
    auto h = [&](int p){ return std::stoi(hex.substr(p,2), nullptr, 16); };
    return cv::Scalar(h(5), h(3), h(1));
}

// Map data value → pixel Y
inline int py(double val, double y_min, double y_max) {
    if (std::abs(y_max - y_min) < 1e-12) return (PY0 + PY1) / 2;
    double t = (val - y_min) / (y_max - y_min);
    return PY1 - static_cast<int>(t * (PY1 - PY0));
}

// Map data value → pixel X
inline int px(double val, double x_min, double x_max) {
    if (std::abs(x_max - x_min) < 1e-12) return (PX0 + PX1) / 2;
    double t = (val - x_min) / (x_max - x_min);
    return PX0 + static_cast<int>(t * (PX1 - PX0));
}

// Draw a complete frame: axes + grid + nodes + curve up to index `count`
inline cv::Mat draw_frame(
    const plots::Vec1D&       x_vals,
    const plots::Vec1D&       y_vals,
    std::size_t               count,      // how many points to draw (0 = empty)
    const plots::PlotContext& ctx,
    const plots::PlotConfig&  cfg,
    const std::string&        title,
    double                    x_min, double x_max,
    double                    y_min, double y_max)
{
    const cv::Scalar WHITE(255,255,255);
    const cv::Scalar BLACK(0,0,0);
    const cv::Scalar LGRAY(210,210,210);
    const cv::Scalar GRAY (150,150,150);
    const cv::Scalar RED  (0,0,220);

    cv::Mat frame(FRAME_H, FRAME_W, CV_8UC3, WHITE);

    // ── Grid ─────────────────────────────────────────────────────────────────
    if (cfg.grid) {
        for (int g = 0; g <= 8; ++g) {
            double gy = y_min + g * (y_max - y_min) / 8.0;
            int pyg = py(gy, y_min, y_max);
            cv::line(frame, {PX0, pyg}, {PX1, pyg}, LGRAY, 1, cv::LINE_AA);
        }
        for (double nd : ctx.nodes) {
            int pxn = px(nd, x_min, x_max);
            cv::line(frame, {pxn, PY0}, {pxn, PY1}, LGRAY, 1, cv::LINE_AA);
        }
    }

    // ── Axes ──────────────────────────────────────────────────────────────────
    cv::line(frame, {PX0, PY1}, {PX1, PY1}, BLACK, 2, cv::LINE_AA);
    cv::line(frame, {PX0, PY0}, {PX0, PY1}, BLACK, 2, cv::LINE_AA);

    // ── Zero line ─────────────────────────────────────────────────────────────
    int y0 = py(0.0, y_min, y_max);
    cv::line(frame, {PX0, y0}, {PX1, y0}, GRAY, 1, cv::LINE_AA);

    // ── Max/min reference lines ────────────────────────────────────────────────
    double true_max = y_max / 1.1;
    int py_max = py( true_max, y_min, y_max);
    int py_min = py(-true_max, y_min, y_max);
    cv::line(frame, {PX0, py_max}, {PX1, py_max}, RED, 1, cv::LINE_AA);
    cv::line(frame, {PX0, py_min}, {PX1, py_min}, RED, 1, cv::LINE_AA);
    // Max label
    std::ostringstream mss; mss << std::fixed; mss.precision(2);
    mss << "Max: " << true_max;
    cv::putText(frame, mss.str(), {PX1 + 4, py_max + 4},
                cv::FONT_HERSHEY_SIMPLEX, 0.38, RED, 1, cv::LINE_AA);

    // ── Node ticks + labels ───────────────────────────────────────────────────
    const cv::Scalar NODE_COLOR = cfg.default_matplotlib_style
        ? cv::Scalar(0,0,180)
        : detail::hex_to_bgr(cfg.style.noeud_color);

    for (std::size_t ni = 0; ni < ctx.nodes.size(); ++ni) {
        int pxn = px(ctx.nodes[ni], x_min, x_max);
        // Tick mark
        cv::line(frame, {pxn, PY1}, {pxn, PY1 + 5}, BLACK, 1, cv::LINE_AA);
        // Label
        const std::string& lbl = ctx.distances[ni];
        int baseline = 0;
        cv::Size ts = cv::getTextSize(lbl, cv::FONT_HERSHEY_SIMPLEX,
                                      0.33, 1, &baseline);
        cv::putText(frame, lbl,
                    {pxn - ts.width/2, PY1 + 18},
                    cv::FONT_HERSHEY_SIMPLEX, 0.33, BLACK, 1, cv::LINE_AA);
        // Node dot on zero line
        if (cfg.noeud)
            cv::circle(frame, {pxn, y0}, 4, NODE_COLOR, -1, cv::LINE_AA);
    }

    // ── Y-axis tick labels ────────────────────────────────────────────────────
    for (int g = 0; g <= 4; ++g) {
        double gy = y_min + g * (y_max - y_min) / 4.0;
        int pyg = py(gy, y_min, y_max);
        std::ostringstream oss; oss << std::fixed; oss.precision(1); oss << gy;
        int baseline = 0;
        cv::Size ts = cv::getTextSize(oss.str(), cv::FONT_HERSHEY_SIMPLEX,
                                      0.33, 1, &baseline);
        cv::putText(frame, oss.str(),
                    {PX0 - ts.width - 6, pyg + 4},
                    cv::FONT_HERSHEY_SIMPLEX, 0.33, BLACK, 1, cv::LINE_AA);
    }

    // ── Title ─────────────────────────────────────────────────────────────────
    {
        int baseline = 0;
        cv::Size ts = cv::getTextSize(title, cv::FONT_HERSHEY_SIMPLEX,
                                      0.6, 1, &baseline);
        cv::putText(frame, title,
                    {(FRAME_W - ts.width)/2, MARGIN_T - 15},
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, BLACK, 1, cv::LINE_AA);
    }

    // ── Curve ─────────────────────────────────────────────────────────────────
    if (count >= 2) {
        const cv::Scalar line_color = cfg.default_matplotlib_color
            ? cv::Scalar(200, 80, 30)
            : detail::hex_to_bgr(cfg.style.line_color);
        const int thickness = cfg.default_matplotlib_color
            ? 2
            : static_cast<int>(std::max(1.0, cfg.style.line_width));

        std::vector<cv::Point> pts;
        pts.reserve(count);
        for (std::size_t idx = 0; idx < count; ++idx)
            pts.push_back({px(x_vals[idx], x_min, x_max),
                           py(y_vals[idx], y_min, y_max)});

        const cv::Point* ppts = pts.data();
        int npts = static_cast<int>(pts.size());
        cv::polylines(frame, &ppts, &npts, 1, false,
                      line_color, thickness, cv::LINE_AA);

        // Current point indicator
        cv::circle(frame, pts.back(), 5, line_color, -1, cv::LINE_AA);

        // Vertical cursor line
        cv::line(frame, {pts.back().x, PY0}, {pts.back().x, PY1},
                 line_color, 1, cv::LINE_AA);
    }

    return frame;
}

} // namespace detail

// =============================================================================
//  Frame data loader
// =============================================================================
struct CurveFrames {
    std::vector<plots::Vec1D> frames;
    std::vector<plots::Vec1D> x_coords;
    bool is_shear = false;
};

inline CurveFrames load_curve_frames(const std::string&        filename,
                                      const plots::PlotContext& ctx)
{
    CurveFrames result;
    result.is_shear = (filename == "shear_force.json");
    auto data = io::open_json_as<plots::Vec3D>(filename, "02_Influence_Lines");

    if (result.is_shear) {
        for (int sp = 0; sp < (int)data.size(); ++sp)
            for (int sc = 0; sc < (int)data[sp].size(); ++sc) {
                result.frames.push_back(data[sp][sc]);
                plots::Vec1D xv = ctx.x_forces[sp][sc];
                double mn = *std::min_element(xv.begin(), xv.end());
                for (double& v : xv) v -= mn;
                result.x_coords.push_back(std::move(xv));
            }
    } else {
        plots::Vec1D x_norm = ctx.x_normal;
        double mn = *std::min_element(x_norm.begin(), x_norm.end());
        for (double& v : x_norm) v -= mn;
        for (auto& sp : data)
            for (auto& sec : sp)
                result.frames.push_back(sec);
        result.x_coords.assign(result.frames.size(), x_norm);
    }
    return result;
}

// =============================================================================
//  Animation options
// =============================================================================
struct AnimationOptions {
    bool     is_save    = true;
    bool     is_show    = false;
    fs::path save_dir_gif;
    fs::path save_dir_mp4;
};

// =============================================================================
//  build_and_save_animation — Phase 1
// =============================================================================
inline void build_and_save_animation(const std::string& filename,
                                      AnimationOptions   opts     = {},
                                      const fs::path&    base_dir = {})
{
    using namespace plots;
    const PlotConfig&  cfg = load_plot_config(base_dir);
    const PlotContext& ctx = get_context(base_dir);
    const CurveFrames  cf  = load_curve_frames(filename, ctx);
    const int          fps = cfg.fps();

    // Global amplitude
    double abs_max = 0.0;
    for (const auto& fd : cf.frames)
        abs_max = std::max(abs_max, std::abs(MaxValueInVector(fd)));
    abs_max *= 1.1;

    // Display name
    std::string name = filename.substr(0, filename.rfind('.'));
    for (char& c : name) if (c == '_') c = ' ';

    // X range across all frames
    double x_min =  std::numeric_limits<double>::max();
    double x_max = -std::numeric_limits<double>::max();
    for (const auto& xv : cf.x_coords) {
        if (xv.empty()) continue;
        x_min = std::min(x_min, *std::min_element(xv.begin(), xv.end()));
        x_max = std::max(x_max, *std::max_element(xv.begin(), xv.end()));
    }

    // Output directories
    if (opts.save_dir_mp4.empty())
        opts.save_dir_mp4 = io::influence_line_dir(base_dir)
                          / "05_Output" / "Animation" / "Results" / "MP4";
    if (opts.save_dir_gif.empty())
        opts.save_dir_gif = io::influence_line_dir(base_dir)
                          / "05_Output" / "Animation" / "Results" / "GIF";
    if (opts.is_save) {
        fs::create_directories(opts.save_dir_mp4);
        fs::create_directories(opts.save_dir_gif);
    }

    const fs::path mp4 = opts.save_dir_mp4 / (name + ".mp4");
    const fs::path gif = opts.save_dir_gif / (name + ".gif");

    // MP4 writer
    cv::VideoWriter writer;
    if (opts.is_save) {
        writer.open(mp4.string(), cv::CAP_MSMF,
                    cv::VideoWriter::fourcc('H','2','6','4'),
                    fps, {FRAME_W, FRAME_H});
        if (!writer.isOpened())
            throw std::runtime_error("VideoWriter: cannot open " + mp4.string());
    }

    // GIF writer — delay in centiseconds = 100/fps
    const int delay_cs = std::max(1, 100 / fps);
    GifWriter gif_writer{};
    if (opts.is_save)
        gif_writer = gif_utils::open_gif(gif.string(), FRAME_W, FRAME_H, delay_cs);

    for (std::size_t fidx = 0; fidx < cf.frames.size(); ++fidx) {
        cv::Mat frame = detail::draw_frame(
            cf.x_coords[fidx], cf.frames[fidx],
            cf.frames[fidx].size(),
            ctx, cfg, name,
            x_min, x_max, -abs_max, abs_max);
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
//  Curvature animation options
// =============================================================================
struct CurvatureAnimOptions {
    int      span    = 0;
    int      section = 0;
    fs::path save_dir_gif;
    fs::path save_dir_mp4;
    bool     is_save = true;
};

// =============================================================================
//  animate_curvature — Phase 3
// =============================================================================
inline void animate_curvature(const std::string&      filename,
                               CurvatureAnimOptions    opts     = {},
                               const fs::path&         base_dir = {})
{
    using namespace plots;
    const PlotConfig&  cfg = load_plot_config(base_dir);
    const PlotContext& ctx = get_context(base_dir);

    Vec1D y_vals = io::open_json_as<Vec3D>(filename, "02_Influence_Lines")
                       [opts.span][opts.section];
    Vec1D x_vals = (filename == "shear_force.json")
        ? ctx.x_forces[opts.span][opts.section]
        : ctx.x_normal;

    // Normalize X to start at 0
    const double x0 = *std::min_element(x_vals.begin(), x_vals.end());
    for (double& v : x_vals) v -= x0;

    // Shift nodes to match
    PlotContext ctx_shifted = ctx;
    ctx_shifted.nodes = ctx.nodes;
    for (double& v : ctx_shifted.nodes) v -= x0;
    ctx_shifted.distances.clear();
    for (double nd : ctx_shifted.nodes) {
        std::ostringstream oss; oss << std::fixed; oss.precision(2); oss << nd;
        ctx_shifted.distances.push_back(oss.str());
    }

    // Y range with margin
    const double y_raw_min = *std::min_element(y_vals.begin(), y_vals.end());
    const double y_raw_max = *std::max_element(y_vals.begin(), y_vals.end());
    const double y_span    = y_raw_max - y_raw_min;
    const double y_margin  = (y_span > 1e-12) ? y_span * 0.15 : 1.0;
    const double y_min     = y_raw_min - y_margin;
    const double y_max     = y_raw_max + y_margin;
    const double x_min     = *std::min_element(x_vals.begin(), x_vals.end());
    const double x_max     = *std::max_element(x_vals.begin(), x_vals.end());

    // Title
    std::string name = filename.substr(0, filename.rfind('.'));
    for (char& c : name) if (c == '_') c = ' ';
    const std::string title = name + " T" + std::to_string(opts.span + 1)
                            + " S" + std::to_string(opts.section);

    // Output
    if (opts.save_dir_mp4.empty())
        opts.save_dir_mp4 = io::influence_line_dir(base_dir)
                          / "05_Output" / "Animation" / "Curvature" / "MP4";
    if (opts.save_dir_gif.empty())
        opts.save_dir_gif = io::influence_line_dir(base_dir)
                          / "05_Output" / "Animation" / "Curvature" / "GIF";
    if (opts.is_save) {
        fs::create_directories(opts.save_dir_mp4);
        fs::create_directories(opts.save_dir_gif);
    }

    const fs::path mp4 = opts.save_dir_mp4 / (name + ".mp4");
    const fs::path gif = opts.save_dir_gif / (name + ".gif");
    const int delay_cs = std::max(1, 100 / cfg.fps());

    cv::VideoWriter writer;
    if (opts.is_save) {
        writer.open(mp4.string(), cv::CAP_MSMF,
                    cv::VideoWriter::fourcc('H','2','6','4'),
                    cfg.fps(), {FRAME_W, FRAME_H});
        if (!writer.isOpened())
            throw std::runtime_error("VideoWriter: cannot open " + mp4.string());
    }

    GifWriter gif_writer{};
    if (opts.is_save)
        gif_writer = gif_utils::open_gif(gif.string(), FRAME_W, FRAME_H, delay_cs);

    // Animate: reveal one more point per frame
    for (std::size_t count = 1; count <= x_vals.size(); ++count) {
        cv::Mat frame = detail::draw_frame(
            x_vals, y_vals, count,
            ctx_shifted, cfg, title,
            x_min, x_max, y_min, y_max);
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
