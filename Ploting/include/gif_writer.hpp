#pragma once
/**
 * gif_writer.hpp
 * Thin wrapper around gif.h (Charlie Tangora, MIT license) for writing
 * animated GIFs from OpenCV cv::Mat frames.
 *
 * gif.h is a single-header, public domain GIF encoder.
 * Source: https://github.com/charlietangora/gif-h
 * Place gif.h in your include/ folder alongside this file.
 *
 * Usage:
 *   GifWriter writer;
 *   open_gif(writer, "output.gif", width, height, delay_cs);
 *   for (auto& frame : frames)
 *       write_gif_frame(writer, frame);
 *   close_gif(writer);
 */

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

// gif.h must be placed in include/ folder
// Download from: https://raw.githubusercontent.com/charlietangora/gif-h/master/gif.h
#define GIF_TEMP_MALLOC  malloc
#define GIF_TEMP_FREE    free
#define GIF_MALLOC       malloc
#define GIF_FREE         free
#include "gif.h"

namespace gif_utils {

/**
 * Open a GIF file for writing.
 * @param delay_cs  Frame delay in centiseconds (100/fps). e.g. fps=20 → delay=5
 */
inline GifWriter open_gif(const std::string& path,
                           int width, int height,
                           int delay_cs = 5)
{
    GifWriter writer{};
    if (!GifBegin(&writer, path.c_str(),
                  static_cast<uint32_t>(width),
                  static_cast<uint32_t>(height),
                  static_cast<uint32_t>(delay_cs)))
        throw std::runtime_error("gif_utils: cannot open " + path);
    return writer;
}

/**
 * Write one cv::Mat frame (BGR or BGRA) to the GIF.
 * gif.h expects RGBA — we convert from BGR.
 */
inline void write_frame(GifWriter& writer,
                         const cv::Mat& frame_bgr,
                         int delay_cs = 5)
{
    // Convert BGR → RGBA
    cv::Mat rgba;
    cv::cvtColor(frame_bgr, rgba, cv::COLOR_BGR2RGBA);

    GifWriteFrame(&writer,
                  rgba.data,
                  static_cast<uint32_t>(rgba.cols),
                  static_cast<uint32_t>(rgba.rows),
                  static_cast<uint32_t>(delay_cs));
}

/**
 * Finalize and close the GIF file.
 */
inline void close_gif(GifWriter& writer) {
    GifEnd(&writer);
}

} // namespace gif_utils
