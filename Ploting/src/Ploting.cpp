#include "Ploting.h"

/**
 * Ploting.cpp
 * Pipeline de generation des plots et animations.
 *
 * Compile en Ploting.lib - appele depuis Aplication.cpp apres l'analyse
 * structurelle, avec configPath lu depuis path.json.
 *
 * Arborescence produite (relative a configPath) :
 *   06_Plots/
 *     ├── All/                     (tracage de toutes les travees)
 *     ├── Maximum/                 (sections critiques)
 *     └── Envelopes/
 *         ├── Point_Load/
 *         ├── Distributed_Load/
 *         └── Combined_Load/
 *   07_Animations/
 *     ├── Results/
 *     │   ├── GIF/
 *     │   └── MP4/
 *     └── Curvature/
 *         ├── GIF/
 *         └── MP4/
 */

#include <filesystem>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <opencv2/core/utils/logger.hpp>

#include "animate_results.hpp"
#include "plot_results.hpp"
#include "envelope_plots.hpp"
#include "thread_pool.hpp"


namespace fs = std::filesystem;
using namespace influence_line;

// =============================================================================
//  Helpers internes (TU-locaux)
// =============================================================================
namespace {

// Repertoires de sortie - initialises depuis configPath a chaque run()
struct OutputDirs {
    fs::path plot;
    fs::path anim_gif;
    fs::path anim_mp4;
    fs::path curvature_gif;
    fs::path curvature_mp4;

    explicit OutputDirs(const fs::path& base)
        : plot         (base / "06_Plots")
        , anim_gif     (base / "07_Animations" / "Results"   / "GIF")
        , anim_mp4     (base / "07_Animations" / "Results"   / "MP4")
        , curvature_gif(base / "07_Animations" / "Curvature" / "GIF")
        , curvature_mp4(base / "07_Animations" / "Curvature" / "MP4")
    {}

    void create_all() const {
        for (const auto& dir : {
            plot / "Maximum", plot / "All",
            plot / "Envelopes" / "Point_Load",
            plot / "Envelopes" / "Distributed_Load",
            plot / "Envelopes" / "Combined_Load",
            anim_gif, anim_mp4, curvature_gif, curvature_mp4
        })
            fs::create_directories(dir);
    }
};

// ── Logger thread-safe avec format unifie ────────────────────────────────────
std::mutex g_log_mutex;

void log_ok(const std::string& name) {
    std::lock_guard<std::mutex> lk(g_log_mutex);
    std::cout << "  [ OK  ]  " << name << '\n';
}
void log_err(const std::string& name, const std::string& reason) {
    std::lock_guard<std::mutex> lk(g_log_mutex);
    std::cout << "  [ ERR ]  " << name << " :: " << reason << '\n';
}

void phase_header(const std::string& title) {
    std::cout << "\n----- " << title;
    // pad la ligne a 53 caracteres avec des tirets
    int written = static_cast<int>(title.size()) + 6;
    for (int i = written; i < 53; ++i) std::cout << '-';
    std::cout << '\n';
}
void phase_footer(const std::string& title) {
    (void)title;
    std::cout << "----- done ------------------------------------------\n";
}

// ── Workers ──────────────────────────────────────────────────────────────────
std::pair<std::string, std::string>
worker_animation(const std::string& filename, const OutputDirs& dirs)
{
    const std::string name = fs::path(filename).stem().string();
    try {
        animations::AnimationOptions opts;
        opts.is_save      = true;
        opts.is_show      = false;
        opts.save_dir_gif = dirs.anim_gif;
        opts.save_dir_mp4 = dirs.anim_mp4;
        animations::build_and_save_animation(filename, opts);
        return {name, ""};
    } catch (const std::exception& ex) {
        return {name, ex.what()};
    }
}

std::pair<std::string, std::string>
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

std::pair<std::string, std::string>
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

// ── Executor parallele generique ─────────────────────────────────────────────
template <typename Worker>
void run_parallel(const std::string&              phase_name,
                  Worker&&                        worker,
                  const std::vector<std::string>& curves)
{
    phase_header(phase_name);
    ThreadPool pool(curves.size());
    std::vector<std::future<std::pair<std::string, std::string>>> futures;
    futures.reserve(curves.size());
    for (const auto& c : curves)
        futures.push_back(pool.submit(worker, c));
    for (auto& fut : futures) {
        auto [name, err] = fut.get();
        if (err.empty()) log_ok(name);
        else             log_err(name, err);
    }
    phase_footer(phase_name);
}

} // anonymous namespace

// =============================================================================
//  API publique
// =============================================================================
namespace plotting {

void run(const std::string& configPath)
{
    // Suppression des messages OpenCV de niveau INFO
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_WARNING);

    // Injecte configPath dans la variable d'environnement interne.
    // influence_line_dir() la lira en priorite 1 - voir data_paths.hpp.
#ifdef _WIN32
    _putenv_s("MATRIX_ONE_INFLUENCE_LINE_DIR", configPath.c_str());
#else
    setenv("MATRIX_ONE_INFLUENCE_LINE_DIR", configPath.c_str(), 1);
#endif

    // Vide le cache JSON (nouvelle session, nouveau configPath)
    io::JsonCache::instance().clear();

    const fs::path  base_path = fs::path(configPath);
    const OutputDirs dirs{ base_path };
    dirs.create_all();

    // En-tete general
    std::cout << "\n=====================================================\n";
    std::cout << "  PLOTTING PIPELINE\n";
    std::cout << "  root: " << base_path.string() << '\n';
    std::cout << "=====================================================\n";

    const std::vector<std::string> curves = {
        "shear_force.json",
        "bending_moment.json",
        "deflection.json",
        "rotation.json",
    };

    run_parallel("Phase 1 : Structural Animations",
        [&](const std::string& f) { return worker_animation(f, dirs); }, curves);

    // Phase 2 sequentielle - matplot++/gnuplot etait non thread-safe ; OpenCV
    // l'est, mais on garde la sequentialite pour limiter la pression memoire
    // pendant l'ecriture des PNG.
    phase_header("Phase 2 : Static Plots");
    for (const auto& curve : curves) {
        auto [name, err] = worker_plot(curve, dirs);
        if (err.empty()) log_ok(name);
        else             log_err(name, err);
    }
    phase_footer("Phase 2");

    run_parallel("Phase 3 : Curvature Animations",
        [&](const std::string& f) { return worker_curvature(f, dirs); }, curves);

    // Phase 4 - enveloppes globales (ligne d'influence + marqueur de position)
    const std::vector<std::string> curve_names = {
        "Shear Force", "Bending Moment", "Deflection", "Rotation"
    };
    envelopes::run_envelope_plots(base_path, dirs.plot, curves, curve_names);

    std::cout << "\n=====================================================\n";
    std::cout << "  PLOTTING PIPELINE - completed\n";
    std::cout << "=====================================================\n\n";
}

} // namespace plotting
