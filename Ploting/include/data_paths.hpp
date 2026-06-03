#pragma once
/**
 * data_paths.hpp
 * Resolution du repertoire racine "Influence Line".
 *
 * Ordre de priorite (du plus fort au plus faible) :
 *   1. Variable d'environnement  MATRIX_ONE_INFLUENCE_LINE_DIR
 *      (injectee par Ploting::run depuis path.json -> sert a propager
 *       le chemin de configuration aux modules sans le repasser partout)
 *   2. path.json (cle "configPath") dans le dossier de l'executable
 *   3. Chemin par defaut depuis getConfigPath() de Utils.h
 *      (~/Documents/Matrix One/Influence Line/)
 *
 * Utilitaires :
 *   - influence_line_dir(path_json)
 *       Resout la racine selon les 3 priorites ci-dessus.
 *   - resolve_base_dir(base_dir)
 *       Si base_dir est non vide, le retourne tel quel ;
 *       sinon delegue a influence_line_dir(). A utiliser systematiquement
 *       a la place de "base_dir.empty() ? influence_line_dir() : base_dir".
 */

#include <filesystem>
#include <fstream>
#include <string>

#include "Utils.h"   // getEnvSafe(), getConfigPath()

namespace fs = std::filesystem;

namespace influence_line {
namespace io {

// ─────────────────────────────────────────────────────────────────────────────
//  influence_line_dir
// ─────────────────────────────────────────────────────────────────────────────
inline fs::path influence_line_dir(const fs::path& path_json = "path.json")
{
    // 1 — Variable d'environnement (priorite la plus haute)
    const std::string env = getEnvSafe("MATRIX_ONE_INFLUENCE_LINE_DIR");
    if (!env.empty())
        return fs::path(env);

    // 2 — path.json dans le dossier de l'exe
    if (fs::exists(path_json) && fs::is_regular_file(path_json)) {
        std::ifstream file_stream(path_json);
        std::string content(
            (std::istreambuf_iterator<char>(file_stream)),
             std::istreambuf_iterator<char>());

        const std::string key = "\"configPath\"";
        const auto pos = content.find(key);
        if (pos != std::string::npos) {
            const auto q1 = content.find('"', pos + key.size());
            const auto q2 = (q1 != std::string::npos)
                          ? content.find('"', q1 + 1)
                          : std::string::npos;
            if (q1 != std::string::npos && q2 != std::string::npos)
                return fs::path(content.substr(q1 + 1, q2 - q1 - 1));
        }
    }

    // 3 — chemin par defaut depuis Utils.h
    return getConfigPath();
}

// ─────────────────────────────────────────────────────────────────────────────
//  resolve_base_dir
//  Helper pour eviter le bug recurrent "influence_line_dir(base_dir)" qui
//  passait par erreur un repertoire de donnees la ou un chemin path.json
//  etait attendu.
// ─────────────────────────────────────────────────────────────────────────────
inline fs::path resolve_base_dir(const fs::path& base_dir)
{
    return base_dir.empty() ? influence_line_dir() : base_dir;
}

} // namespace io
} // namespace influence_line
