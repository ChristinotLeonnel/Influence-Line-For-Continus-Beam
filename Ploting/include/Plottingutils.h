/**
 * @file Utils.h
 * @brief Utilitaires généraux pour l'analyse structurelle de poutres continues.
 *
 * Fonctionnalités :
 *  - Structures de données (load, Position1D/2D/3D, ...)
 *  - Recherche de valeurs maximales (1D/2D/3D)
 *  - Intégration numérique (trapèzes de Kahan)
 *  - Export JSON pour tous les résultats
 *  - Parsing du fichier de configuration
 *  - Génération du fichier de configuration par défaut
 *
 * @note Unités : mètres (m), kiloNewtons (kN), Pascals (Pa), m⁴
 */

#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>
#include <cmath>
#include <stdexcept>

using json = nlohmann::json;

// =============================================================================
//  STRUCTURES DE DONNÉES
// =============================================================================

struct load {
    std::vector<double> Intensity;
    std::vector<double> Length;
    std::string name;
};

struct Position2D {
    size_t i;
    size_t j;
    double val;
};

struct Position3D {
    size_t i;
    size_t j;
    size_t k;
    double val;
};

struct Position1D {
    size_t max_position;
    double value;
};

struct CombineLoadPosition {
    size_t max_position;
    double position;
    double value;
    std::map<std::string, std::map<std::string, double>> Addition;
};

struct load_delivery {
    std::map<std::string, std::map<std::string, double>> load;
    size_t span;
    size_t section;
    double maximum_value;
    double position = 0.0;
};


// =============================================================================
//  FONCTIONS UTILITAIRES
// =============================================================================

template <typename T>
static constexpr T MaxValueInVector(const std::vector<T>& vec) {
    if (vec.empty()) return 0;
    T maxVal = vec[0];
    for (const auto& val : vec)
        if (std::abs(val) > std::abs(maxVal))
            maxVal = val;
    return maxVal;
}

template <typename T, typename U>
void removeByIndices(std::vector<T>& v, const std::vector<U>& indices) {
    std::vector<bool> toRemove(v.size(), false);
    for (U i : indices)
        if (i < static_cast<U>(v.size()))
            toRemove[i] = true;
    std::vector<T> result;
    result.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i)
        if (!toRemove[i])
            result.push_back(v[i]);
    v.swap(result);
}

template <typename T>
static T trapeze(const std::vector<T>& x, const std::vector<T>& y) {
    T aire = 0, err = 0;
    for (size_t i = 0; i < x.size() - 1; ++i) {
        T h = x[i + 1] - x[i];
        T a = (y[i] + y[i + 1]) * h / 2;
        T t = aire + a;
        err += (aire - t) + a;
        aire = t;
    }
    return aire + err;
}

static Position3D findMaxAbsoluteValue3D(
    const std::vector<std::vector<std::vector<double>>>& data)
{
    double maxAbs = std::numeric_limits<double>::lowest();
    Position3D pos{};
    for (size_t i = 0; i < data.size(); ++i)
        for (size_t j = 0; j < data[i].size(); ++j)
            for (size_t k = 0; k < data[i][j].size(); ++k) {
                double absVal = std::abs(data[i][j][k]);
                if (absVal >= maxAbs) {
                    maxAbs = absVal;
                    pos = { i, j, k, data[i][j][k] };
                }
            }
    return pos;
}

static Position2D findMaxAbsoluteValue2D(
    const std::vector<std::vector<double>>& data)
{
    double maxAbs = std::numeric_limits<double>::lowest();
    Position2D pos{};
    for (size_t i = 0; i < data.size(); ++i)
        for (size_t j = 0; j < data[i].size(); ++j) {
            double absVal = std::abs(data[i][j]);
            if (absVal >= maxAbs) {
                maxAbs = absVal;
                pos = { i, j, data[i][j] };
            }
        }
    return pos;
}

static double prod_list(const std::vector<double>& liste, int debut, int fin) {
    double r = 1.0;
    for (int a = debut; a <= fin; ++a) r *= liste[a];
    return r;
}

static std::vector<double> flatten(const std::vector<std::vector<double>>& m) {
    size_t n = 0;
    for (const auto& v : m) n += v.size();
    std::vector<double> out;
    out.reserve(n);
    for (const auto& v : m) out.insert(out.end(), v.begin(), v.end());
    return out;
}

template <typename T>
size_t Indice_of(std::vector<T>& vec, T& value) {
    size_t c = 0;
    for (auto& i : vec) {
        if (i == value) return c;
        ++c;
    }
    throw std::invalid_argument("This value is not in the vector");
}


// =============================================================================
//  FONCTIONS D'EXPORT JSON
// =============================================================================

/**
 * Sérialise une donnée quelconque et l'écrit dans un fichier JSON indenté.
 */
static void delivery(auto& data, std::filesystem::path path, std::string FileName) {
    json j = data;
    std::filesystem::create_directories(path);
    std::ofstream file(path / FileName);
    file << j.dump(4);
}

/**
 * Écrit les informations d'un maximum 3D dans un fichier JSON.
 *   { "span", "section", "alpha", "value" }
 */
static void maximum_delivery(auto& data, json& file,
    std::filesystem::path Path, std::string name) {
    file["span"] = data.i;
    file["section"] = data.j;
    file["alpha"] = data.k;
    file["value"] = data.val;
    delivery(file, Path, name);
}

/**
 * Écrit les informations d'une enveloppe de charge dans un fichier JSON.
 *   { "maximum", "span", "section", "position", "load" }
 */
static void loading_delivery(auto& data, json& file,
    std::filesystem::path Path, std::string name) {
    file["maximum"] = data.maximum_value;
    file["span"] = data.span;
    file["section"] = data.section;
    file["position"] = data.position;
    file["load"] = data.load;
    delivery(file, Path, name);
}


// =============================================================================
//  FONCTIONS DE CONFIGURATION ET DE CHEMINS
// =============================================================================

static bool checkFileExists(const std::filesystem::path& filePath) {
    return std::filesystem::exists(filePath);
}

inline std::filesystem::path getDefaultConfigPath() {          // ex getConfigPath
    auto getEnvSafe = [](const char* varName) -> std::string {
#ifdef _WIN32
        char* buf = nullptr;
        size_t sz = 0;
        if (_dupenv_s(&buf, &sz, varName) == 0 && buf != nullptr) {
            std::string result(buf);
            free(buf);
            return result;
        }
        return {};
#else
        const char* val = std::getenv(varName);
        return val ? std::string(val) : std::string{};
#endif
        };

#ifdef _WIN32
    std::string userProfile = getEnvSafe("USERPROFILE");
    if (!userProfile.empty())
        return std::filesystem::path(userProfile) / "Documents" / "Matrix One" / "New Folder" / "Influence Line";
#else
    std::string home = getEnvSafe("HOME");
    if (!home.empty())
        return std::filesystem::path(home) / "Documents" / "Matrix One" / "Influence Line";
#endif
    return std::filesystem::path("Matrix One") / "Influence Line";
}



// =============================================================================
//  FONCTIONS DE PARSING DU FICHIER DE CONFIGURATION
// =============================================================================

static std::vector<double> parseVector(const std::string& line) {
    std::vector<double> vector;
    size_t pos = line.find(":");
    if (pos != std::string::npos) {
        std::string values = line.substr(pos + 1);
        values.erase(0, values.find_first_not_of(" \t"));
        std::istringstream iss(values);
        double value;
        while (iss >> value) vector.push_back(value);
    }
    return vector;
}

static std::string getValue(const std::string& line) {
    size_t pos = line.find(":");
    if (pos != std::string::npos) {
        std::string value = line.substr(pos + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        return value;
    }
    return "";
}

static load LoadParser(std::string& line, const char* marker_1, const char* marker_2) {
    load l;
    const size_t work = line.find(marker_1);
    const size_t name = line.find(marker_2);
    const size_t plus1 = std::strlen(marker_1);
    const size_t plus2 = std::strlen(marker_2);

    std::string s1 = line.substr(0, work);
    std::string s2 = line.substr(work + plus1, name - (work + plus1));
    size_t s3s = name + plus2;
    size_t s3e = line.find_last_not_of(" \t\r\n");
    std::string s3 = line.substr(s3s, s3e - s3s + 1);

    s1.erase(0, s1.find_first_not_of(" \t"));
    s2.erase(0, s2.find_first_not_of(" \t"));
    s3.erase(0, s3.find_first_not_of(" \t"));

    std::istringstream iss(s1);
    double value;
    while (iss >> value) l.Intensity.push_back(value);
    iss = std::istringstream(s2);
    while (iss >> value) l.Length.push_back(value);
    l.name = s3;
    return l;
}

static void write_structural_model_input(const std::string& filename, std::filesystem::path configPath) {
    std::filesystem::create_directories(configPath);
    std::filesystem::path fullPath = configPath / filename;
    std::ofstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "Erreur: Impossible d'ouvrir " << fullPath << std::endl;
        return;
    }
    std::vector<std::string> CONFIG = {
"# =============================================================================",
"#                 STRUCTURAL ANALYSIS CONFIGURATION FILE",
"# =============================================================================",
"# UNITS: Length(m), Force(kN), Distributed Load(kN/m), E(Pa), I(m^4)",
"# =============================================================================",
"",
"Length: 20 25 20",
"Steps: 1",
"Young Modulus: 210e9 210e9 210e9",
"Moment of Inertia: 1e-6 1e-6 1e-6",
"",
"6 12 12 6 12 12 /Point/ 2.25 4.5 1.5 5 4.5 1.5 2.5 :: BC 1",
"5 10 10 5 10 10 /Point/ 2.25 4.5 1.5 5 4.5 1.5 2.5 :: BC 2",
"20 /Point/ 0 0 :: BE 1",
"20 /Point/ 4 2 :: BE 2",
"45 /Distributed/ 0 3 :: UDL 1",
"45 10 25 /Distributed/ 0 3 5 2 :: UDL 2",
""
    };
    for (auto& t : CONFIG) file << t << std::endl;
    file.close();
    std::cout << "Fichier créé: " << fullPath << std::endl;
}
