#pragma once
/**
 * plot_results.hpp
 * Static plots of influence lines — rendered with OpenCV (pure C++).
 *
 * Replaces matplot++ for file output entirely.
 * OpenCV cv::imwrite handles paths with spaces, is thread-safe,
 * and requires no gnuplot process.
 *
 * matplot++ is still included for interactive fig->show() only.
 */

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include "plot_config.hpp"
#include "plot_context.hpp"
#include "json_loader.hpp"

namespace fs = std::filesystem;

namespace influence_line {
    namespace plots {

        // =============================================================================
        //  Curve type enum
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
        //  OpenCV plot renderer (same engine as animate_results.hpp)
        // =============================================================================
        static constexpr int PLT_W = 1280;
        static constexpr int PLT_H = 720;
        static constexpr int PLT_ML = 90;
        static constexpr int PLT_MR = 30;
        static constexpr int PLT_MT = 60;
        static constexpr int PLT_MB = 70;
        static constexpr int PLT_X0 = PLT_ML;
        static constexpr int PLT_X1 = PLT_W - PLT_MR;
        static constexpr int PLT_Y0 = PLT_MT;
        static constexpr int PLT_Y1 = PLT_H - PLT_MB;

        namespace detail {

            inline cv::Scalar hex_to_bgr(const std::string& hex) {
                if (hex.size() < 7 || hex[0] != '#') return { 200, 80, 30 };
                auto h = [&](int p) { return std::stoi(hex.substr(p, 2), nullptr, 16); };
                return cv::Scalar(h(5), h(3), h(1));
            }

            inline int map_py(double val, double y_min, double y_max) {
                if (std::abs(y_max - y_min) < 1e-12) return (PLT_Y0 + PLT_Y1) / 2;
                double t = (val - y_min) / (y_max - y_min);
                return PLT_Y1 - static_cast<int>(t * (PLT_Y1 - PLT_Y0));
            }

            inline int map_px(double val, double x_min, double x_max) {
                if (std::abs(x_max - x_min) < 1e-12) return (PLT_X0 + PLT_X1) / 2;
                double t = (val - x_min) / (x_max - x_min);
                return PLT_X0 + static_cast<int>(t * (PLT_X1 - PLT_X0));
            }

            // Draw axes, grid, nodes, title onto a blank frame
            inline cv::Mat make_base_frame(
                const PlotContext& ctx, const PlotConfig& cfg,
                const std::string& title,
                double x_min, double x_max, double y_min, double y_max)
            {
                const cv::Scalar WHITE(255, 255, 255), BLACK(0, 0, 0), LGRAY(210, 210, 210), GRAY(150, 150, 150);
                cv::Mat frame(PLT_H, PLT_W, CV_8UC3, WHITE);

                // Grid
                if (cfg.grid) {
                    for (int g = 0; g <= 8; ++g) {
                        int py = map_py(y_min + g * (y_max - y_min) / 8.0, y_min, y_max);
                        cv::line(frame, { PLT_X0, py }, { PLT_X1, py }, LGRAY, 1, cv::LINE_AA);
                    }
                    for (double nd : ctx.nodes) {
                        int px = map_px(nd, x_min, x_max);
                        cv::line(frame, { px, PLT_Y0 }, { px, PLT_Y1 }, LGRAY, 1, cv::LINE_AA);
                    }
                }

                // Axes
                cv::line(frame, { PLT_X0, PLT_Y1 }, { PLT_X1, PLT_Y1 }, BLACK, 2, cv::LINE_AA);
                cv::line(frame, { PLT_X0, PLT_Y0 }, { PLT_X0, PLT_Y1 }, BLACK, 2, cv::LINE_AA);

                // Zero line
                int y0 = map_py(0.0, y_min, y_max);
                cv::line(frame, { PLT_X0, y0 }, { PLT_X1, y0 }, GRAY, 1, cv::LINE_AA);

                // Node ticks + labels
                const cv::Scalar nc = cfg.default_matplotlib_style
                    ? cv::Scalar(0, 0, 180) : detail::hex_to_bgr(cfg.style.noeud_color);
                for (std::size_t ni = 0; ni < ctx.nodes.size(); ++ni) {
                    int px = map_px(ctx.nodes[ni], x_min, x_max);
                    cv::line(frame, { px, PLT_Y1 }, { px, PLT_Y1 + 5 }, BLACK, 1, cv::LINE_AA);
                    int bl = 0; auto ts = cv::getTextSize(ctx.distances[ni],
                        cv::FONT_HERSHEY_SIMPLEX, 0.33, 1, &bl);
                    cv::putText(frame, ctx.distances[ni],
                        { px - ts.width / 2, PLT_Y1 + 18 },
                        cv::FONT_HERSHEY_SIMPLEX, 0.33, BLACK, 1, cv::LINE_AA);
                    if (cfg.noeud)
                        cv::circle(frame, { px, y0 }, 4, nc, -1, cv::LINE_AA);
                }

                // Y-axis labels
                for (int g = 0; g <= 4; ++g) {
                    double gy = y_min + g * (y_max - y_min) / 4.0;
                    int py = map_py(gy, y_min, y_max);
                    std::ostringstream oss; oss << std::fixed; oss.precision(1); oss << gy;
                    int bl = 0; auto ts = cv::getTextSize(oss.str(),
                        cv::FONT_HERSHEY_SIMPLEX, 0.33, 1, &bl);
                    cv::putText(frame, oss.str(), { PLT_X0 - ts.width - 6, py + 4 },
                        cv::FONT_HERSHEY_SIMPLEX, 0.33, BLACK, 1, cv::LINE_AA);
                }

                // Title
                {
                    int bl = 0; auto ts = cv::getTextSize(title,
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, &bl);
                    cv::putText(frame, title, { (PLT_W - ts.width) / 2, PLT_MT - 15 },
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, BLACK, 1, cv::LINE_AA);
                }

                return frame;
            }

            // Draw one curve (x,y) onto frame
            inline void draw_curve(cv::Mat& frame,
                const Vec1D& x, const Vec1D& y,
                const std::string& label,
                const PlotConfig& cfg,
                double x_min, double x_max, double y_min, double y_max,
                int color_idx = 0)
            {
                if (x.size() < 2) return;
                // Cycle through a few distinct colors for multi-curve plots
                static const cv::Scalar COLORS[] = {
                    {200,80,30}, {30,130,200}, {30,180,30},
                    {180,30,180}, {180,130,0}, {0,160,160}
                };
                const cv::Scalar col = cfg.default_matplotlib_color
                    ? COLORS[color_idx % 6]
                    : detail::hex_to_bgr(cfg.style.line_color);
                const int thickness = cfg.default_matplotlib_color
                    ? 2 : static_cast<int>(std::max(1.0, cfg.style.line_width));

                std::vector<cv::Point> pts;
                pts.reserve(x.size());
                for (std::size_t i = 0; i < x.size(); ++i)
                    pts.push_back({ map_px(x[i], x_min, x_max),
                                   map_py(y[i], y_min, y_max) });
                const cv::Point* p = pts.data();
                int n = (int)pts.size();
                cv::polylines(frame, &p, &n, 1, false, col, thickness, cv::LINE_AA);

                // Label at end of curve
                if (!label.empty() && label[0] != '_') {
                    cv::putText(frame, label, { pts.back().x + 4, pts.back().y - 4 },
                        cv::FONT_HERSHEY_SIMPLEX, 0.35,
                        cv::Scalar(col[0] * 0.7, col[1] * 0.7, col[2] * 0.7), 1, cv::LINE_AA);
                }
            }

            // Compute Y range across multiple Vec1D series
            inline std::pair<double, double> y_range(
                const std::vector<std::reference_wrapper<const Vec1D>>& series)
            {
                double ymin = std::numeric_limits<double>::max();
                double ymax = -std::numeric_limits<double>::max();
                for (const Vec1D& v : series) {
                    for (double val : v) {
                        ymin = std::min(ymin, val);
                        ymax = std::max(ymax, val);
                    }
                }
                if (ymin == ymax) { ymin -= 1.0; ymax += 1.0; }
                double margin = (ymax - ymin) * 0.12;
                return { ymin - margin, ymax + margin };
            }

            inline std::pair<double, double> x_range(const Vec1D& x) {
                if (x.empty()) return { 0.0, 1.0 };
                double xmin = *std::min_element(x.begin(), x.end());
                double xmax = *std::max_element(x.begin(), x.end());
                if (xmin == xmax) { xmin -= 1.0; xmax += 1.0; }
                double m = (xmax - xmin) * 0.03;
                return { xmin - m, xmax + m };
            }

        } // namespace detail

        // =============================================================================
        //  Plot options
        // =============================================================================
        struct PlotOptions {
            std::optional<int> span;
            std::optional<int> section;
            bool show_maximum = false;
            bool show_legend = true;
            bool exclude_boundaries = true;
            bool is_show = false;
            bool is_save = false;
            fs::path save_dir;
        };

        // =============================================================================
        //  Main entry point
        // =============================================================================
        inline void plot_analysis_results(
            const std::string& filename,
            PlotOptions        opts = {},
            const fs::path& base_dir = {})
        {
            const PlotConfig& cfg = load_plot_config(base_dir);
            const PlotContext& ctx = get_context(base_dir);
            const CurveType    ct = parse_curve_type(filename);
            const std::string  name = curve_label(ct);

            // Collect all (x,y) series to plot
            std::vector<Vec1D> xs, ys;
            std::vector<std::string> labels;

            if (opts.show_maximum) {
                const std::string stem = curve_filename(ct).substr(
                    0, curve_filename(ct).rfind('.'));
                const auto& mj = io::open_json(stem + ".json", "03_Critical_Values");
                const int span_idx = mj.value("span", 0);
                const int section_idx = mj.value("section", 0);

                if (ct == CurveType::ShearForce) {
                    auto data = io::open_json_as<Vec3D>("shear_force.json", "02_Influence_Lines");
                    xs.push_back(ctx.x_forces[span_idx][section_idx]);
                    ys.push_back(data[span_idx][section_idx]);
                }
                else if (ct == CurveType::SupportMoment) {
                    auto data = io::open_json_as<Vec2D>("support_moment.json", "02_Influence_Lines");
                    xs.push_back(ctx.x_normal);
                    ys.push_back(data[span_idx]);
                }
                else {
                    auto data = io::open_json_as<Vec3D>(curve_filename(ct), "02_Influence_Lines");
                    xs.push_back(ctx.x_normal);
                    ys.push_back(data[span_idx][section_idx]);
                }
                labels.push_back("Span " + std::to_string(span_idx + 1)
                    + " Sec " + std::to_string(section_idx));
            }
            else if (ct == CurveType::ShearForce) {
                auto data = io::open_json_as<Vec3D>("shear_force.json", "02_Influence_Lines");
                for (int ii = 0; ii < (int)data.size(); ++ii)
                    for (int jj = 0; jj < (int)data[ii].size(); ++jj) {
                        xs.push_back(ctx.x_forces[ii][jj]);
                        ys.push_back(data[ii][jj]);
                        labels.push_back(jj == 0 ? "T" + std::to_string(ii + 1) : "_");
                    }
            }
            else if (ct == CurveType::SupportMoment) {
                auto data = io::open_json_as<Vec2D>("support_moment.json", "02_Influence_Lines");
                for (int ii = 0; ii < (int)data.size(); ++ii) {
                    xs.push_back(ctx.x_normal);
                    ys.push_back(data[ii]);
                    labels.push_back("M_" + std::to_string(ii));
                }
            }
            else {
                if (opts.span && opts.section) {
                    auto data = io::open_json_as<Vec3D>(curve_filename(ct), "02_Influence_Lines");
                    xs.push_back(ctx.x_normal);
                    ys.push_back(data[*opts.span][*opts.section]);
                    labels.push_back("Span " + std::to_string(*opts.span + 1)
                        + " Sec " + std::to_string(*opts.section));
                }
                else {
                    auto data = io::open_json_as<Vec3D>(curve_filename(ct), "02_Influence_Lines");
                    for (int ii = 0; ii < (int)data.size(); ++ii)
                        for (int jj = 0; jj < (int)data[ii].size(); ++jj) {
                            xs.push_back(ctx.x_normal);
                            ys.push_back(data[ii][jj]);
                            labels.push_back(jj == 0 ? "Span " + std::to_string(ii + 1) : "_");
                        }
                }
            }

            if (xs.empty()) return;

            // Compute ranges
            const auto [xmin, xmax] = detail::x_range(xs[0]);
            std::vector<std::reference_wrapper<const Vec1D>> all_y;
            for (const auto& v : ys) all_y.push_back(std::cref(v));
            const auto [ymin, ymax] = detail::y_range(all_y);

            // Render frame
            cv::Mat frame = detail::make_base_frame(ctx, cfg, name,
                xmin, xmax, ymin, ymax);
            for (int idx = 0; idx < (int)xs.size(); ++idx)
                detail::draw_curve(frame, xs[idx], ys[idx], labels[idx],
                    cfg, xmin, xmax, ymin, ymax, idx);

            // Save
            if (opts.is_save) {
                const fs::path dir = opts.save_dir.empty()
                    ? (io::influence_line_dir() / "05_Output" / "Plots")
                    : opts.save_dir;
                fs::create_directories(dir);
                const fs::path out = dir / (name + ".png");
                if (!cv::imwrite(out.string(), frame))
                    throw std::runtime_error(
                        "plot_analysis_results: cv::imwrite failed for " + out.string());
            }

            if (opts.is_show) {
                // For interactive display: use OpenCV window (no gnuplot needed)
                cv::imshow(name, frame);
                cv::waitKey(0);
                cv::destroyWindow(name);
            }
        }

    } // namespace plots
} // namespace influence_line