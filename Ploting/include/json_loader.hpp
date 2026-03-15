#pragma once
/**
 * json_loader.hpp
 * Chargement des fichiers JSON de données structurelles.
 *
 * Dépendance : nlohmann/json (header-only, inclus dans third_party/).
 *
 * Nouvelle architecture vs Python :
 *   - Cache en mémoire : chaque fichier n'est lu qu'une seule fois par session.
 *   - Thread-safe     : mutex par entrée de cache (fine-grained locking).
 *   - Typage fort     : templates pour extraire directement le bon type C++.
 */

#include <filesystem>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

 // ── Supprime le warning C26495 (m_data uninitialized) de l'analyseur MSVC ────
 // nlohmann/json est une lib tierce — ces warnings sont des faux positifs.
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 26495)  // Variable non initialisée (type.6)
#  pragma warning(disable: 26451)  // Arithmetic overflow (es.78)
#endif
#include "data_paths.hpp"   // inclut déjà Utils.h → nlohmann/json.hpp
#ifdef _MSC_VER
#  pragma warning(pop)
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace influence_line {
    namespace io {

        // ─────────────────────────────────────────────────────────────────────────────
        //  Cache thread-safe
        // ─────────────────────────────────────────────────────────────────────────────
        class JsonCache {
        public:
            static JsonCache& instance() {
                static JsonCache inst;
                return inst;
            }

            /// Retourne le json brut (chargé depuis disque si absent du cache).
            const json& get(const fs::path& path) {
                const std::string key = path.string();
                {
                    std::shared_lock<std::shared_mutex> lock(mutex_);
                    auto it = cache_.find(key);
                    if (it != cache_.end()) return it->second;
                }
                // Chargement effectif — verrou exclusif
                std::unique_lock<std::shared_mutex> lock(mutex_);
                auto it = cache_.find(key);          // double-check après promotion
                if (it != cache_.end()) return it->second;

                std::ifstream file_stream(path);
                if (!file_stream.is_open())
                    throw std::runtime_error("JsonCache: impossible d'ouvrir " + key);

                auto& entry = cache_[key];
                file_stream >> entry;
                return entry;
            }

            void clear() {
                std::unique_lock<std::shared_mutex> lock(mutex_);
                cache_.clear();
            }

        private:
            std::unordered_map<std::string, json> cache_;
            mutable std::shared_mutex             mutex_;

            JsonCache() = default;
            ~JsonCache() = default;
            JsonCache(const JsonCache&) = delete;
            JsonCache& operator=(const JsonCache&) = delete;
        };

        // ─────────────────────────────────────────────────────────────────────────────
        //  API publique
        // ─────────────────────────────────────────────────────────────────────────────

        /**
         * Charge (ou récupère depuis le cache) un fichier JSON.
         *
         * @param filename  Nom du fichier, ex : "abscissa.json"
         * @param folder    Sous-dossier dans influence_line_dir(), ex : "02_Influence_Lines"
         * @param base_dir  Répertoire racine (détecté automatiquement si vide)
         */
        inline const json& open_json(
            const std::string& filename,
            const std::string& folder = "02_Influence_Lines",
            const fs::path& base_dir = {})
        {
            const fs::path root = base_dir.empty() ? influence_line_dir() : base_dir;
            const fs::path path = root / folder / filename;
            return JsonCache::instance().get(path);
        }

        /**
         * Surcharge typée — décode directement en type C++ via nlohmann.
         *
         * Exemple :
         *   auto nodes = open_json<std::vector<double>>("node_lengths.json");
         */
        template<typename T>
        T open_json_as(
            const std::string& filename,
            const std::string& folder = "02_Influence_Lines",
            const fs::path& base_dir = {})
        {
            return open_json(filename, folder, base_dir).get<T>();
        }

    } // namespace io
} // namespace influence_line