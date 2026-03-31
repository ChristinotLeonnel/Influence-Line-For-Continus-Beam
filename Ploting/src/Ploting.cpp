#include "Plotting.h"

/**
 * Ploting.cpp
 * Visual results generation pipeline (plots + animations).
 *
 * Compiled as Ploting.lib — called from Aplication.cpp after structural
 * analysis, with configPath read from path.json.
 */

#include <filesystem>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <opencv2/core/utils/logger.hpp>  // cv::utils::logging

#include "animate_results.hpp"
#include "plot_results.hpp"
#include "envelope_plots.hpp"
#include "thread_pool.hpp"


namespace fs = std::filesystem;
using namespace influence_line;

// =============================================================================
//  Internal helpers (local translation unit — not exposed in .h)
// =============================================================================

namespace {

// Output directories — initialized from configPath on each run() call
struct OutputDirs {
    fs::path plot;
    fs::path gif;
    fs::path mp4;
    fs::path curvature_gif;
    fs::path curvature_mp4;

    explicit OutputDirs(const fs::path& base)
        : plot         (base / "05_Output" / "Plots")
        , gif          (base / "05_Output" / "Animation" / "Results" / "GIF")
        , mp4          (base / "05_Output" / "Animation" / "Results" / "MP4")
        , curvature_gif(base / "05_Output" / "Animation" / "Curvature" / "GIF")
        , curvature_mp4(base / "05_Output" / "Animation" / "Curvature" / "MP4")
    {}

    void create_all() const {
        for (const auto& dir : {
            plot / "Maximum", plot / "All",
            plot / "Envelopes" / "Point_Load",
            plot / "Envelopes" / "Distributed_Load",
            plot / "Envelopes" / "Combined_Load",
            gif, mp4, curvature_gif, curvature_mp4
        }) fs::create_directories(dir);
    }
};

// ── Thread-safe logger ────────────────────────────────────────────────────────
std::mutex g_log_mutex;

void log(const std::string& status, const std::string& name,
         const std::string& err = "")
{
    std::lock_guard<std::mutex> lk(g_log_mutex);
    if (err.empty())
        std::cout << "    " << status << "   " << name << "\n";
    else
        std::cout << "    ERR  " << name << " : " << err << "\n";
}

// ── Workers ───────────────────────────────────────────────────────────────────

std::pair<std::string,std::string>
worker_animation(const std::string& filename, const OutputDirs& dirs)
{
    const std::string name = fs::path(filename).stem().string();
    try {
        animations::AnimationOptions opts;
        opts.is_save      = true;
        opts.is_show      = false;
        opts.save_dir_gif = dirs.gif;
        opts.save_dir_mp4 = dirs.mp4;
        animations::build_and_save_animation(filename, opts);
        return {name, ""};
    } catch (const std::exception& ex) {
        return {name, ex.what()};
    }
}

std::pair<std::string,std::string>
worker_plot(const std::string& filename, const OutputDirs& dirs)
{
    const std::string name = fs::path(filename).stem().string();
    try {
        plots::PlotOptions opts_max;
        opts_max.show_maximum = true;
        opts_max.is_save      = true;
        opts_max.is_show      = false;
        opts_max.save_dir     = dirs.plot / "Maximum";
        plots::plot_analysis_results(filename, opts_max);

        plots::PlotOptions opts_all;
        opts_all.is_save  = true;
        opts_all.is_show  = false;
        opts_all.save_dir = dirs.plot / "All";
        plots::plot_analysis_results(filename, opts_all);

        return {name, ""};
    } catch (const std::exception& ex) {
        return {name, ex.what()};
    }
}

std::pair<std::string,std::string>
worker_curvature(const std::string& filename, const OutputDirs& dirs)
{
    const std::string name = fs::path(filename).stem().string();
    try {
        const auto& max_j = io::open_json(
            fs::path(filename).stem().string() + ".json", "03_Critical_Values");
        const int span    = max_j.value("span",    0);
        const int section = max_j.value("section", 0);

        animations::CurvatureAnimOptions opts;
        opts.span         = span;
        opts.section      = section;
        opts.save_dir_gif = dirs.curvature_gif;
        opts.save_dir_mp4 = dirs.curvature_mp4;
        opts.is_save      = true;
        animations::animate_curvature(filename, opts);

        return {name, ""};
    } catch (const std::exception& ex) {
        return {name, ex.what()};
    }
}

// ── Generic parallel executor ─────────────────────────────────────────────
template<typename Worker>
void run_parallel(const std::string&              phase_name,
                  Worker&&                        worker,
                  const std::vector<std::string>& curves)
{
    std::cout << "=== " << phase_name << " ===\n";
    ThreadPool pool(curves.size());
    std::vector<std::future<std::pair<std::string,std::string>>> futures;
    futures.reserve(curves.size());
    for (const auto& c : curves)
        futures.push_back(pool.submit(worker, c));
    for (auto& fut : futures) {
        auto [name, err] = fut.get();
        log(err.empty() ? "OK  " : "ERR", name, err);
    }
    std::cout << phase_name << " completed.\n\n";
}

} // anonymous namespace

// =============================================================================
//  Public API
// =============================================================================

namespace plotting {

void run(const std::string& configPath)
{
    // Suppress OpenCV INFO messages (plugin load attempts, backend discovery)
    // Only warnings and errors will be shown.
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    // Injects configPath as an internal environment variable
    // → influence_line_dir() reads it as priority 1 in data_paths.hpp
#ifdef _WIN32
    _putenv_s("MATRIX_ONE_INFLUENCE_LINE_DIR", configPath.c_str());
#else
    setenv("MATRIX_ONE_INFLUENCE_LINE_DIR", configPath.c_str(), 1);
#endif

    // Clears the JSON cache (new session with new configPath)
    io::JsonCache::instance().clear();

    const fs::path base_path = fs::path(configPath);  // intermediate variable
    const OutputDirs dirs{ base_path };                // braces → never ambiguous
    dirs.create_all();

    // No Python init needed — matplot++ + OpenCV are pure C++

    const std::vector<std::string> curves = {
        "shear_force.json",
        "bending_moment.json",
        "deflection.json",
        "rotation.json",
    };

    run_parallel("Phase 1 : Structural Animations",
        [&](const std::string& f){ return worker_animation(f, dirs); }, curves);

    // Phase 2 runs sequentially — matplot++ uses gnuplot which is not
    // thread-safe when multiple workers call fig->save() simultaneously.
    std::cout << "=== Phase 2 : Static Plots ===\n";
    for (const auto& curve : curves) {
        auto [name, err] = worker_plot(curve, dirs);
        log(err.empty() ? "OK  " : "ERR", name, err);
    }
    std::cout << "Phase 2 : Static Plots completed.\n\n";

    run_parallel("Phase 3 : Curvature Animations",
        [&](const std::string& f){ return worker_curvature(f, dirs); }, curves);

    // Phase 4 — Global envelope plots (influence line + load position marker)
    // Runs after UpdatePositions so 04_Load_Envelopes/ is fully populated.
    const std::vector<std::string> curve_names = {
        "Shear Force", "Bending Moment", "Deflection", "Rotation"
    };
    envelopes::run_envelope_plots(base_path, dirs.plot, curves, curve_names);
}

} // namespace plotting
