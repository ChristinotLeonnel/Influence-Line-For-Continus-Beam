#pragma once
/**
 * data_paths.hpp
 * Résolution du répertoire racine des données.
 *
 * Délègue entièrement à Utils.h :
 *   - getEnvSafe()    → lecture portable des variables d'environnement
 *   - getConfigPath() → chemin racine avec priorité :
 *       1. MATRIX_ONE_INFLUENCE_LINE_DIR (var. env.)
 *       2. path.json (clé "configPath") dans le dossier de l'exe
 *       3. ~/Documents/Matrix One/... (défaut Utils.h)
 */

// ── STL avant tout include Python ────────────────────────────────────────────
#include <filesystem>
#include <fstream>
#include <string>

#include "utils/Utils.h"   // getEnvSafe(), getConfigPath()

// ── Neutralise les macros Python.h si déjà inclus ────────────────────────────
#ifdef f
#  undef f
#endif
#ifdef s
#  undef s
#endif
#ifdef i
#  undef i
#endif
#ifdef l
#  undef l
#endif
#ifdef u
#  undef u
#endif
#ifdef b
#  undef b
#endif
#ifdef O
#  undef O
#endif
#ifdef abs
#  undef abs
#endif
#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif
// ─────────────────────────────────────────────────────────────────────────────

namespace fs = std::filesystem;

namespace influence_line {
namespace io {

/**
 * Retourne le répertoire racine "Influence Line".
 *
 * Ordre de priorité :
 *   1. Variable d'environnement  MATRIX_ONE_INFLUENCE_LINE_DIR
 *   2. path.json dans le dossier courant (clé "configPath")
 *   3. Chemin par défaut depuis getConfigPath() de Utils.h
 *
 * @param path_json  Chemin vers path.json (défaut : "path.json" = dossier exe)
 */
inline fs::path influence_line_dir(const fs::path& path_json = "path.json")
{
    // 2 — path.json dans le dossier de l'exe
    if (fs::exists(path_json)) {
        std::ifstream file_stream(path_json);
        std::string content(
            (std::istreambuf_iterator<char>(file_stream)),
             std::istreambuf_iterator<char>());

        const std::string key = "\"configPath\"";
        auto pos = content.find(key);
        if (pos != std::string::npos) {
            auto q1 = content.find('"', pos + key.size());
            auto q2 = content.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos)
                return fs::path(content.substr(q1 + 1, q2 - q1 - 1));
        }
    }

    // 3 — chemin par défaut depuis Utils.h
    return getConfigPath();
}

} // namespace io
} // namespace influence_line
