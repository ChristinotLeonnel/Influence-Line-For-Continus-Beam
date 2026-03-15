#pragma once
/**
 * render_common.hpp
 * Shared OpenCV rendering engine used by both plot_results.hpp
 * and animate_results.hpp. All visual parameters come from PlotConfig.
 */

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <cmath>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "plot_config.hpp"
#include "plot_context.hpp"

namespace influence_line {
namespace render {

// =============================================================================
//  Color helpers
// =============================================================================
inline cv::Scalar hex_to_bgr(const std::string& hex, uchar default_b=200,
                               uchar default_g=80, uchar default_r=30)
{
    if (hex.size() < 7 || hex[0] != '#')
        return {(double)default_b, (double)default_g, (double)default_r};
    auto h = [&](int p){ return std::stoi(hex.substr(p,2), nullptr, 16); };
    return cv::Scalar(h(5), h(3), h(1));  // BGR order
}

// Parse "#RRGGBBAA" — alpha returned as 0-255
inline cv::Scalar hex_to_bgra(const std::string& hex) {
    if (hex.size() >= 9 && hex[0] == '#') {
        auto h = [&](int p){ return std::stoi(hex.substr(p,2), nullptr, 16); };
        return cv::Scalar(h(5), h(3), h(1), h(7));
    }
    return hex_to_bgr(hex);
}

// =============================================================================
//  Smart number formatter — scientific notation for very small/large values
// =============================================================================
/**
 * Format a double for display:
 *  - |val| >= 0.01 and < 1e6  → fixed notation  e.g. "1.234"
 *  - otherwise                → scientific       e.g. "1.23e-05"
 * Trailing zeros are stripped. Exponent shown as "e+N" / "e-N".
 */
inline std::string smart_format(double val, int sig_digits = 4) {
    if (val == 0.0) return "0";

    const double abs_val = std::abs(val);
    std::ostringstream oss;

    if (abs_val >= 0.01 && abs_val < 1e6) {
        // Fixed — show enough decimals for sig_digits significant figures
        int decimals = sig_digits - 1 - (int)std::floor(std::log10(abs_val));
        decimals = std::max(0, std::min(decimals, 6));
        oss << std::fixed << std::setprecision(decimals) << val;
    } else {
        // Scientific notation: format then clean up exponent
        oss << std::scientific << std::setprecision(sig_digits - 1) << val;
        std::string s = oss.str();
        // Normalise exponent: e+007 → e+7, e-005 → e-5
        auto epos = s.find('e');
        if (epos != std::string::npos) {
            char sign = s[epos+1];
            std::string exp_str = s.substr(epos+2);
            // Strip leading zeros from exponent
            std::size_t nz = exp_str.find_first_not_of('0');
            exp_str = (nz == std::string::npos) ? "0" : exp_str.substr(nz);
            s = s.substr(0, epos) + "e" + sign + exp_str;
        }
        // Strip trailing zeros before 'e'
        auto ep = s.find('e');
        auto dot = s.find('.');
        if (dot != std::string::npos && ep != std::string::npos) {
            std::string mantissa = s.substr(0, ep);
            while (mantissa.size() > 1 && mantissa.back() == '0') mantissa.pop_back();
            if (mantissa.back() == '.') mantissa.pop_back();
            s = mantissa + s.substr(ep);
        }
        return s;
    }

    // Strip trailing zeros for fixed notation too
    std::string s = oss.str();
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s;
}


// =============================================================================
//  Canvas
// =============================================================================
struct Canvas {
    int W, H;      // total frame size
    int x0, x1;   // plot area X bounds
    int y0, y1;   // plot area Y bounds

    explicit Canvas(const plots::FigureSize& fig,
                    int ml=90, int mr=40, int mt=65, int mb=75)
        : W(fig.width), H(fig.height)
        , x0(ml), x1(fig.width - mr)
        , y0(mt),  y1(fig.height - mb)
    {}

    int px(double val, double xmin, double xmax) const {
        if (std::abs(xmax-xmin) < 1e-12) return (x0+x1)/2;
        return x0 + (int)(((val-xmin)/(xmax-xmin)) * (x1-x0));
    }
    int py(double val, double ymin, double ymax) const {
        if (std::abs(ymax-ymin) < 1e-12) return (y0+y1)/2;
        return y1 - (int)(((val-ymin)/(ymax-ymin)) * (y1-y0));
    }
};

// =============================================================================
//  Draw background + grid
// =============================================================================
inline void draw_background(cv::Mat& frame, const Canvas& cv_,
                              const plots::PlotConfig& cfg,
                              double xmin, double xmax,
                              double ymin, double ymax,
                              const plots::PlotContext& ctx)
{
    // Background
    frame.setTo(hex_to_bgr(cfg.grid.background));

    const cv::Scalar major_col  = hex_to_bgr(cfg.grid.major_color);
    const cv::Scalar minor_col  = hex_to_bgr(cfg.grid.minor_color);
    const cv::Scalar axis_col   = hex_to_bgr(cfg.grid.axis_color);

    if (cfg.grid.show) {
        // Minor grid (5 subdivisions per major)
        if (cfg.grid.show_minor) {
            for (int g = 0; g <= 40; ++g) {
                double gy = ymin + g*(ymax-ymin)/40.0;
                int py = cv_.py(gy, ymin, ymax);
                cv::line(frame, {cv_.x0,py}, {cv_.x1,py}, minor_col, 1, cv::LINE_AA);
            }
        }
        // Major grid (8 horizontal lines)
        for (int g = 0; g <= 8; ++g) {
            double gy = ymin + g*(ymax-ymin)/8.0;
            int py = cv_.py(gy, ymin, ymax);
            cv::line(frame, {cv_.x0,py}, {cv_.x1,py},
                     major_col, cfg.grid.major_thickness, cv::LINE_AA);
        }
        // Vertical grid at node positions
        for (double nd : ctx.nodes) {
            int px = cv_.px(nd, xmin, xmax);
            cv::line(frame, {px,cv_.y0}, {px,cv_.y1},
                     major_col, cfg.grid.major_thickness, cv::LINE_AA);
        }
    }

    // Axes
    cv::line(frame, {cv_.x0,cv_.y1}, {cv_.x1,cv_.y1}, axis_col, 2, cv::LINE_AA);
    cv::line(frame, {cv_.x0,cv_.y0}, {cv_.x0,cv_.y1}, axis_col, 2, cv::LINE_AA);

    // Zero line
    int y0 = cv_.py(0.0, ymin, ymax);
    cv::line(frame, {cv_.x0,y0}, {cv_.x1,y0},
             hex_to_bgr(cfg.grid.zero_color), 1, cv::LINE_AA);

    // Span reference line (horizontal beam)
    if (cfg.span_line.show) {
        cv::line(frame, {cv_.x0,y0}, {cv_.x1,y0},
                 hex_to_bgr(cfg.span_line.color),
                 cfg.span_line.thickness, cv::LINE_AA);
    }
}

// =============================================================================
//  Draw axes labels, ticks, title
// =============================================================================
inline void draw_labels(cv::Mat& frame, const Canvas& cv_,
                         const plots::PlotConfig& cfg,
                         const std::string& title,
                         double xmin, double xmax,
                         double ymin, double ymax,
                         const plots::PlotContext& ctx)
{
    const cv::Scalar text_col = hex_to_bgr(cfg.grid.axis_color);
    const double ax_sc  = cfg.labels.axis_scale;
    const int    ax_th  = cfg.labels.axis_thickness;

    // Y-axis tick labels
    for (int g = 0; g <= 5; ++g) {
        double gy = ymin + g*(ymax-ymin)/5.0;
        int py = cv_.py(gy, ymin, ymax);
        const std::string tick_lbl = smart_format(gy, 3);
        int bl=0;
        auto ts = cv::getTextSize(tick_lbl, cv::FONT_HERSHEY_SIMPLEX, ax_sc, ax_th, &bl);
        cv::putText(frame, tick_lbl,
                    {cv_.x0 - ts.width - 6, py + ts.height/2},
                    cv::FONT_HERSHEY_SIMPLEX, ax_sc, text_col, ax_th, cv::LINE_AA);
        // Tick mark
        cv::line(frame, {cv_.x0-4, py}, {cv_.x0, py}, text_col, 1, cv::LINE_AA);
    }

    // Node tick labels (X-axis)
    if (cfg.nodes.show_labels) {
        for (std::size_t ni = 0; ni < ctx.nodes.size(); ++ni) {
            int px = cv_.px(ctx.nodes[ni], xmin, xmax);
            cv::line(frame, {px,cv_.y1}, {px,cv_.y1+5}, text_col, 1, cv::LINE_AA);
            const std::string& lbl = ctx.distances[ni];
            int bl=0;
            auto ts = cv::getTextSize(lbl, cv::FONT_HERSHEY_SIMPLEX,
                                      cfg.nodes.label_scale, 1, &bl);
            cv::putText(frame, lbl,
                        {px - ts.width/2, cv_.y1 + 18},
                        cv::FONT_HERSHEY_SIMPLEX, cfg.nodes.label_scale,
                        text_col, 1, cv::LINE_AA);
        }
    }

    // X-axis label
    if (!cfg.labels.xlabel.empty()) {
        int bl=0;
        auto ts = cv::getTextSize(cfg.labels.xlabel,
                                  cv::FONT_HERSHEY_SIMPLEX, ax_sc, ax_th, &bl);
        cv::putText(frame, cfg.labels.xlabel,
                    {(cv_.x0+cv_.x1)/2 - ts.width/2, cv_.y1 + 42},
                    cv::FONT_HERSHEY_SIMPLEX, ax_sc, text_col, ax_th, cv::LINE_AA);
    }

    // Title
    const std::string full_title = cfg.labels.title_prefix + title;
    {
        int bl=0;
        auto ts = cv::getTextSize(full_title, cv::FONT_HERSHEY_SIMPLEX,
                                  cfg.labels.title_scale,
                                  cfg.labels.title_thickness, &bl);
        cv::putText(frame, full_title,
                    {(cv_.W - ts.width)/2, cv_.y0 - 15},
                    cv::FONT_HERSHEY_SIMPLEX, cfg.labels.title_scale,
                    text_col, cfg.labels.title_thickness, cv::LINE_AA);
    }
}

// =============================================================================
//  Draw node dots on zero line
// =============================================================================
inline void draw_nodes(cv::Mat& frame, const Canvas& cv_,
                        const plots::PlotConfig& cfg,
                        double xmin, double xmax,
                        double ymin, double ymax,
                        const plots::PlotContext& ctx)
{
    if (!cfg.nodes.show) return;
    const cv::Scalar nc = hex_to_bgr(cfg.nodes.color);
    int y0 = cv_.py(0.0, ymin, ymax);
    for (double nd : ctx.nodes) {
        int px = cv_.px(nd, xmin, xmax);
        cv::circle(frame, {px, y0}, cfg.nodes.radius, nc, -1, cv::LINE_AA);
        // Outline
        cv::circle(frame, {px, y0}, cfg.nodes.radius,
                   hex_to_bgr(cfg.grid.axis_color), 1, cv::LINE_AA);
    }
}

// =============================================================================
//  Draw max/min reference lines
// =============================================================================
inline void draw_max_lines(cv::Mat& frame, const Canvas& cv_,
                             const plots::PlotConfig& cfg,
                             double abs_max,
                             double xmin, double xmax,
                             double ymin, double ymax)
{
    const cv::Scalar col = hex_to_bgr(cfg.grid.max_line_color);
    // Draw at the actual data max (abs_max already has 1.1 margin from caller)
    const double display_max = abs_max / 1.1;   // remove the 10% margin for display
    int py_max = cv_.py( display_max, ymin, ymax);
    int py_min = cv_.py(-display_max, ymin, ymax);

    // Dashed max lines (simulate dashes with short segments)
    const int dash = 8, gap = 5;
    for (int x = cv_.x0; x < cv_.x1; x += dash + gap) {
        int x2 = std::min(x + dash, cv_.x1);
        cv::line(frame, {x,py_max}, {x2,py_max}, col, 1, cv::LINE_AA);
        cv::line(frame, {x,py_min}, {x2,py_min}, col, 1, cv::LINE_AA);
    }

    // Max value label — use smart_format for scientific notation when needed
    const std::string max_str  = "max = "  + smart_format(display_max,  5);
    const std::string min_str  = "min = " + smart_format(-display_max, 5);

    // Draw label box (background rect + text) near right edge
    const double lbl_sc = cfg.labels.axis_scale * 0.92;
    int bl = 0;
    auto ts_max = cv::getTextSize(max_str, cv::FONT_HERSHEY_SIMPLEX, lbl_sc, 1, &bl);
    auto ts_min = cv::getTextSize(min_str, cv::FONT_HERSHEY_SIMPLEX, lbl_sc, 1, &bl);

    // Max label above the max line
    {
        int lx = cv_.x1 - ts_max.width - 6;
        int ly = py_max - 4;
        cv::rectangle(frame,
            {lx - 2, ly - ts_max.height - 1},
            {lx + ts_max.width + 2, ly + 2},
            cv::Scalar(255,255,255), cv::FILLED);
        cv::putText(frame, max_str, {lx, ly},
                    cv::FONT_HERSHEY_SIMPLEX, lbl_sc, col, 1, cv::LINE_AA);
    }
    // Min label below the min line
    {
        int lx = cv_.x1 - ts_min.width - 6;
        int ly = py_min + ts_min.height + 4;
        cv::rectangle(frame,
            {lx - 2, ly - ts_min.height - 1},
            {lx + ts_min.width + 2, ly + 2},
            cv::Scalar(255,255,255), cv::FILLED);
        cv::putText(frame, min_str, {lx, ly},
                    cv::FONT_HERSHEY_SIMPLEX, lbl_sc, col, 1, cv::LINE_AA);
    }
}

// =============================================================================
//  Draw one curve
// =============================================================================
inline void draw_curve(cv::Mat& frame, const Canvas& cv_,
                        const plots::Vec1D& x, const plots::Vec1D& y,
                        std::size_t count,
                        const plots::CurveStyle& style,
                        double xmin, double xmax,
                        double ymin, double ymax)
{
    if (count < 2) return;
    const cv::Scalar col = hex_to_bgr(style.color);
    const int thick = std::max(1, (int)std::round(style.thickness));

    std::vector<cv::Point> pts;
    pts.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        pts.push_back({cv_.px(x[i], xmin, xmax),
                       cv_.py(y[i], ymin, ymax)});

    // Filled area
    if (style.filled && count >= 3) {
        int y_zero = cv_.py(0.0, ymin, ymax);
        std::vector<cv::Point> poly = pts;
        poly.push_back({pts.back().x,  y_zero});
        poly.push_back({pts.front().x, y_zero});
        cv::Mat overlay = frame.clone();
        cv::fillPoly(overlay, {poly}, col);
        const double alpha = std::clamp(style.fill_alpha, 0.0, 1.0);
        cv::addWeighted(overlay, alpha, frame, 1.0 - alpha, 0, frame);
    }

    // Line
    const cv::Point* pp = pts.data();
    int n = (int)pts.size();
    cv::polylines(frame, &pp, &n, 1, false, col, thick, cv::LINE_AA);
}

// =============================================================================
//  Draw legend box
// =============================================================================
inline void draw_legend(cv::Mat& frame, const Canvas& cv_,
                         const plots::PlotConfig& cfg,
                         const std::vector<std::string>& labels,
                         const std::vector<int>& curve_indices)
{
    if (!cfg.legend.show || labels.empty()) return;

    const auto& lc  = cfg.legend;
    const int font  = cv::FONT_HERSHEY_SIMPLEX;
    const int pad   = lc.padding;
    const int ll    = lc.line_length;
    const int gap   = 6;   // between color line and text
    const int lh    = 18;  // row height

    // Measure all labels
    int max_w = 0;
    for (const auto& lbl : labels) {
        if (lbl.empty() || lbl[0] == '_') continue;
        int bl=0;
        auto ts = cv::getTextSize(lbl, font, lc.font_scale, lc.thickness, &bl);
        max_w = std::max(max_w, ts.width);
    }
    if (max_w == 0) return;

    // Count visible rows
    int rows = 0;
    for (const auto& lbl : labels)
        if (!lbl.empty() && lbl[0] != '_') ++rows;
    if (rows == 0) return;

    const int box_w = pad + ll + gap + max_w + pad;
    const int box_h = pad + rows * lh + pad;

    // Position
    int bx = cv_.x0 + pad, by = cv_.y0 + pad;
    const std::string& pos = lc.position;
    if (pos == "top-right"    || pos == "upper right")
        bx = cv_.x1 - box_w - pad;
    else if (pos == "bottom-left"  || pos == "lower left")
        { bx = cv_.x0 + pad; by = cv_.y1 - box_h - pad; }
    else if (pos == "bottom-right" || pos == "lower right")
        { bx = cv_.x1 - box_w - pad; by = cv_.y1 - box_h - pad; }

    // Background with alpha
    {
        cv::Mat roi = frame(cv::Rect(
            std::max(0,bx), std::max(0,by),
            std::min(box_w, frame.cols-bx),
            std::min(box_h, frame.rows-by)));
        roi.setTo(cv::Scalar(248,248,248));
        cv::rectangle(frame, {bx,by}, {bx+box_w,by+box_h},
                      cv::Scalar(180,180,180), 1, cv::LINE_AA);
    }

    // Rows
    int row = 0;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (labels[i].empty() || labels[i][0] == '_') continue;
        const int cy = by + pad + row * lh + lh/2;
        const int style_idx = (i < curve_indices.size()) ? curve_indices[i] : (int)i;
        // Color swatch line
        cv::line(frame,
                 {bx + pad,           cy},
                 {bx + pad + ll,      cy},
                 hex_to_bgr(cfg.curve(style_idx).color),
                 std::max(1,(int)cfg.curve(style_idx).thickness),
                 cv::LINE_AA);
        // Label
        cv::putText(frame, labels[i],
                    {bx + pad + ll + gap, cy + 5},
                    font, lc.font_scale,
                    hex_to_bgr(cfg.grid.axis_color),
                    lc.thickness, cv::LINE_AA);
        ++row;
    }
}


// =============================================================================
//  Peak annotation — dot + leader line + value box at the max/min point
// =============================================================================
inline void draw_peak_annotation(cv::Mat& frame, const Canvas& cv_,
                                  const plots::PlotConfig& cfg,
                                  const plots::Vec1D& x,
                                  const plots::Vec1D& y,
                                  double xmin, double xmax,
                                  double ymin, double ymax)
{
    if (x.empty() || y.empty()) return;

    // Find index of absolute maximum
    std::size_t imax = 0;
    double      vmax = std::abs(y[0]);
    for (std::size_t i = 1; i < y.size(); ++i) {
        if (std::abs(y[i]) > vmax) { vmax = std::abs(y[i]); imax = i; }
    }
    const double peak_x = x[imax];
    const double peak_y = y[imax];

    const int px = cv_.px(peak_x, xmin, xmax);
    const int py = cv_.py(peak_y, ymin, ymax);

    const cv::Scalar dot_col  = hex_to_bgr(cfg.grid.max_line_color);
    const cv::Scalar text_col = hex_to_bgr(cfg.grid.axis_color);

    // ── Dot at peak ──────────────────────────────────────────────────────────
    cv::circle(frame, {px, py}, 6, dot_col, -1, cv::LINE_AA);
    cv::circle(frame, {px, py}, 7, cv::Scalar(255,255,255), 1, cv::LINE_AA);

    // ── Value label ──────────────────────────────────────────────────────────
    const std::string val_str = smart_format(peak_y, 5);
    const std::string x_str  = "x = " + smart_format(peak_x, 4);

    const double lbl_sc = cfg.labels.axis_scale * 1.05;
    const int    font   = cv::FONT_HERSHEY_SIMPLEX;
    int bl = 0;
    auto ts_val = cv::getTextSize(val_str, font, lbl_sc, 1, &bl);
    auto ts_x   = cv::getTextSize(x_str,   font, lbl_sc * 0.85, 1, &bl);

    const int box_w   = std::max(ts_val.width, ts_x.width) + 14;
    const int box_h   = ts_val.height + ts_x.height + 14;
    const int leader  = 18;  // leader line length

    // Place box above or below depending on position in plot area
    bool above = (py - cv_.y0) > (cv_.y1 - py);  // more space below → label above
    int box_x = px - box_w / 2;
    int box_y = above ? py - leader - box_h : py + leader;

    // Clamp to plot area
    box_x = std::clamp(box_x, cv_.x0, cv_.x1 - box_w);
    box_y = std::clamp(box_y, cv_.y0, cv_.y1 - box_h);

    // Leader line from dot to box
    int leader_end_y = above ? box_y + box_h : box_y;
    cv::line(frame, {px, py}, {px, leader_end_y}, dot_col, 1, cv::LINE_AA);

    // Box background
    cv::rectangle(frame,
        {box_x - 1, box_y - 1},
        {box_x + box_w + 1, box_y + box_h + 1},
        dot_col, 1, cv::LINE_AA);
    cv::rectangle(frame,
        {box_x, box_y},
        {box_x + box_w, box_y + box_h},
        cv::Scalar(255, 252, 235), cv::FILLED);

    // Value (large, colored)
    cv::putText(frame, val_str,
        {box_x + 7, box_y + 7 + ts_val.height},
        font, lbl_sc, dot_col, 1, cv::LINE_AA);

    // X position (smaller, gray)
    cv::putText(frame, x_str,
        {box_x + 7, box_y + 7 + ts_val.height + 4 + ts_x.height},
        font, lbl_sc * 0.85, text_col, 1, cv::LINE_AA);
}

// =============================================================================
//  Full frame builder
// =============================================================================
struct FrameSpec {
    const plots::Vec1D&       x;
    const plots::Vec1D&       y;
    std::size_t               count;      // points to draw (0 = all)
    const plots::PlotContext& ctx;
    const plots::PlotConfig&  cfg;
    std::string               title;
    double xmin, xmax, ymin, ymax;
    double abs_max      = 0.0;  // if >0, draw max/min reference lines
    bool   show_cursor  = false;
    int    curve_idx    = 0;
    std::vector<std::string> legend_labels;
    std::vector<int>         legend_curve_ids;
};

inline cv::Mat make_frame(const FrameSpec& s)
{
    Canvas cv_(s.cfg.figure);
    const std::size_t n = (s.count == 0) ? s.x.size() : s.count;

    cv::Mat frame(cv_.H, cv_.W, CV_8UC3);
    draw_background(frame, cv_, s.cfg, s.xmin, s.xmax, s.ymin, s.ymax, s.ctx);

    if (s.abs_max > 0.0)
        draw_max_lines(frame, cv_, s.cfg, s.abs_max,
                       s.xmin, s.xmax, s.ymin, s.ymax);

    draw_nodes(frame, cv_, s.cfg, s.xmin, s.xmax, s.ymin, s.ymax, s.ctx);

    draw_curve(frame, cv_, s.x, s.y, n,
               s.cfg.curve(s.curve_idx),
               s.xmin, s.xmax, s.ymin, s.ymax);

    // Cursor dot + vertical line
    if (s.show_cursor && n >= 1 && s.cfg.animation.show_cursor) {
        int px = cv_.px(s.x[n-1], s.xmin, s.xmax);
        int py = cv_.py(s.y[n-1], s.ymin, s.ymax);
        cv::line(frame, {px, cv_.y0}, {px, cv_.y1},
                 hex_to_bgr(s.cfg.curve(s.curve_idx).color), 1, cv::LINE_AA);
        if (s.cfg.animation.show_point)
            cv::circle(frame, {px,py}, 5,
                       hex_to_bgr(s.cfg.curve(s.curve_idx).color), -1, cv::LINE_AA);
    }

    draw_labels(frame, cv_, s.cfg, s.title,
                s.xmin, s.xmax, s.ymin, s.ymax, s.ctx);

    if (!s.legend_labels.empty())
        draw_legend(frame, cv_, s.cfg, s.legend_labels, s.legend_curve_ids);

    return frame;
}

// =============================================================================
//  Multi-curve frame (static plots — all spans)
// =============================================================================
struct MultiFrameSpec {
    std::vector<plots::Vec1D> xs;
    std::vector<plots::Vec1D> ys;
    std::vector<std::string>  labels;
    const plots::PlotContext& ctx;
    const plots::PlotConfig&  cfg;
    std::string               title;
    double xmin, xmax, ymin, ymax;
};

inline cv::Mat make_multi_frame(const MultiFrameSpec& s)
{
    Canvas cv_(s.cfg.figure);
    cv::Mat frame(cv_.H, cv_.W, CV_8UC3);
    draw_background(frame, cv_, s.cfg, s.xmin, s.xmax, s.ymin, s.ymax, s.ctx);
    draw_nodes(frame, cv_, s.cfg, s.xmin, s.xmax, s.ymin, s.ymax, s.ctx);

    int cidx = 0;
    for (std::size_t i = 0; i < s.xs.size(); ++i) {
        bool skip = s.labels[i].empty() || s.labels[i][0] == '_';
        draw_curve(frame, cv_, s.xs[i], s.ys[i], s.xs[i].size(),
                   s.cfg.curve(cidx), s.xmin, s.xmax, s.ymin, s.ymax);
        if (!skip) ++cidx;
    }

    draw_labels(frame, cv_, s.cfg, s.title,
                s.xmin, s.xmax, s.ymin, s.ymax, s.ctx);

    // Build legend (skip "_" entries)
    std::vector<std::string> leg_labels;
    std::vector<int>         leg_ids;
    int ci2 = 0;
    for (std::size_t i = 0; i < s.labels.size(); ++i) {
        bool skip = s.labels[i].empty() || s.labels[i][0] == '_';
        if (!skip) { leg_labels.push_back(s.labels[i]); leg_ids.push_back(ci2++); }
        else ++ci2;
    }
    draw_legend(frame, cv_, s.cfg, leg_labels, leg_ids);

    return frame;
}

// =============================================================================
//  Y-range helper
// =============================================================================
inline std::pair<double,double> y_range(
    const std::vector<std::reference_wrapper<const plots::Vec1D>>& series,
    double margin_pct = 0.12)
{
    double ymin =  std::numeric_limits<double>::max();
    double ymax = -std::numeric_limits<double>::max();
    for (const plots::Vec1D& v : series)
        for (double val : v) { ymin=std::min(ymin,val); ymax=std::max(ymax,val); }
    if (ymin == ymax) { ymin -= 1.0; ymax += 1.0; }
    double m = (ymax-ymin) * margin_pct;
    return {ymin-m, ymax+m};
}

inline std::pair<double,double> x_range(const plots::Vec1D& x, double margin_pct=0.02) {
    if (x.empty()) return {0.0,1.0};
    double xmin = *std::min_element(x.begin(),x.end());
    double xmax = *std::max_element(x.begin(),x.end());
    if (xmin==xmax) { xmin-=1.0; xmax+=1.0; }
    double m=(xmax-xmin)*margin_pct;
    return {xmin-m, xmax+m};
}

} // namespace render
} // namespace influence_line
