#pragma once
/**
 * gnuplot_init.hpp
 * Ensures gnuplot.exe is on the process PATH before matplot++ uses it.
 * Include this ONCE before any matplot++ figure() call.
 */

#include <filesystem>
#include <string>

#ifdef _WIN32
#  ifndef _CRT_SECURE_NO_WARNINGS
#    define _CRT_SECURE_NO_WARNINGS
#  endif
#  include <cstdlib>
#endif

namespace gnuplot_utils {

/**
 * Prepends the gnuplot bin folder to the process PATH.
 * Called automatically — runs only once per process (thread-safe via static).
 *
 * Gnuplot default install: C:\Program Files\gnuplot\bin\gnuplot.exe
 */
inline void ensure_in_path() {
    static bool done = false;
    if (done) return;
    done = true;

#ifdef _WIN32
    const char* candidates[] = {
        "C:\\Program Files\\gnuplot\\bin",
        "C:\\Program Files (x86)\\gnuplot\\bin",
        "C:\\gnuplot\\bin",
        nullptr
    };
    for (int idx = 0; candidates[idx]; ++idx) {
        namespace fs = std::filesystem;
        if (fs::exists(fs::path(candidates[idx]) / "gnuplot.exe")) {
            // Read existing PATH
            char*  existing = nullptr;
            size_t sz       = 0;
            std::string cur_path;
            if (_dupenv_s(&existing, &sz, "PATH") == 0 && existing) {
                cur_path = existing;
                free(existing);
            }
            // Prepend gnuplot folder
            _putenv_s("PATH", (std::string(candidates[idx]) + ";" + cur_path).c_str());
            break;
        }
    }
#endif
    // On Linux/macOS gnuplot is typically already on PATH via package manager
}

} // namespace gnuplot_utils
