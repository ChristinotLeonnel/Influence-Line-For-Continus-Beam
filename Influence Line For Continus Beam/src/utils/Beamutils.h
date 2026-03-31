/**
 * @file BeamUtils.h
 * @brief Utilitaires généraux pour l'analyse structurelle de poutres continues.
 *
 * Fonctionnalités :
 *  - Structures de données (LoadCase, Position1D/2D/3D, ...)
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
#include <cstring>
#include <stdexcept>

using json = nlohmann::json;

// =============================================================================
//  STRUCTURES DE DONNÉES
// =============================================================================

/** Représente un cas de chargement (charges ponctuelles ou réparties). */
struct LoadCase {
    std::vector<double> intensities;   // ex Intensity
    std::vector<double> positions;     // ex Length  (offsets successifs)
    std::string         name;
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
    size_t maxIndex;   // ex max_position
    double value;
};

struct CombinedLoadResult {              // ex CombineLoadPosition
    size_t maxIndex;                     // ex max_position
    double position;
    double value;
    std::map<std::string, std::map<std::string, double>> breakdown;  // ex Addition
};

struct LoadEnvelopeResult {              // ex load_delivery
    std::map<std::string, std::map<std::string, double>> load;
    size_t  span;
    size_t  section;
    double  maxValue;                    // ex maximum_value
    double  position = 0.0;
};


// =============================================================================
//  FONCTIONS UTILITAIRES
// =============================================================================

template <typename T>
inline T maxAbsInVector(const std::vector<T>& vec) {    // ex MaxValueInVector
    if (vec.empty()) return T{};
    T maxVal = vec[0];
    for (const auto& val : vec)
        if (std::abs(val) > std::abs(maxVal))
            maxVal = val;
    return maxVal;
}

template <typename T, typename U>
inline void removeByIndices(std::vector<T>& v, const std::vector<U>& indices) {
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
inline T trapeze(const std::vector<T>& x, const std::vector<T>& y) {
    if (x.size() < 2) return T{};
    T aire = T{}, err = T{};
    for (size_t i = 0; i < x.size() - 1; ++i) {
        T h = x[i + 1] - x[i];
        T a = (y[i] + y[i + 1]) * h / 2;
        T t = aire + a;
        err += (aire - t) + a;
        aire = t;
    }
    return aire + err;
}

inline Position3D findMaxAbsoluteValue3D(
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

inline Position2D findMaxAbsoluteValue2D(
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

inline double prod_list(const std::vector<double>& list, int from, int to) {
    double r = 1.0;
    for (int a = from; a <= to; ++a) r *= list[a];
    return r;
}

inline std::vector<double> flatten(const std::vector<std::vector<double>>& m) {
    size_t n = 0;
    for (const auto& v : m) n += v.size();
    std::vector<double> out;
    out.reserve(n);
    for (const auto& v : m) out.insert(out.end(), v.begin(), v.end());
    return out;
}

template <typename T>
inline size_t indexOf(const std::vector<T>& vec, const T& value) {    // ex Indice_of
    for (size_t c = 0; c < vec.size(); ++c)
        if (vec[c] == value) return c;
    throw std::invalid_argument("indexOf: value not found in vector");
}


// =============================================================================
//  FONCTIONS D'EXPORT JSON
// =============================================================================

template <typename T>
inline void writeJsonFile(const T& data,
    const std::filesystem::path& dir,
    const std::string& fileName)     // ex delivery
{
    json j = data;
    std::filesystem::create_directories(dir);
    std::ofstream file(dir / fileName);
    if (!file.is_open())
        throw std::runtime_error("writeJsonFile: cannot open " + (dir / fileName).string());
    file << j.dump(4);
}

/** Écrit { span, section, alpha, value } dans un fichier JSON. */
template <typename T>
inline void writeCriticalValueJson(const T& data, json& file,
    const std::filesystem::path& dir,
    const std::string& name)   // ex maximum_delivery
{
    file["span"] = data.i;
    file["section"] = data.j;
    file["alpha"] = data.k;
    file["value"] = data.val;
    writeJsonFile(file, dir, name);
}

/** Écrit { maximum, span, section, position, load } dans un fichier JSON. */
template <typename T>
inline void writeEnvelopeJson(const T& data, json& file,
    const std::filesystem::path& dir,
    const std::string& name)       // ex loading_delivery
{
    file["maximum"] = data.maxValue;
    file["span"] = data.span;
    file["section"] = data.section;
    file["position"] = data.position;
    file["load"] = data.load;
    writeJsonFile(file, dir, name);
}


// =============================================================================
//  CONFIGURATION ET CHEMINS
// =============================================================================

inline bool fileExists(const std::filesystem::path& path) {   // ex checkFileExists
    return std::filesystem::exists(path);
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
//  PARSING DU FICHIER DE CONFIGURATION
// =============================================================================

inline std::vector<double> parseDoubleVector(const std::string& line) {   // ex parseVector
    std::vector<double> result;
    size_t pos = line.find(':');
    if (pos != std::string::npos) {
        std::string values = line.substr(pos + 1);
        values.erase(0, values.find_first_not_of(" \t"));
        std::istringstream iss(values);
        double value;
        while (iss >> value) result.push_back(value);
    }
    return result;
}

inline std::string parseScalarValue(const std::string& line) {            // ex getValue
    size_t pos = line.find(':');
    if (pos != std::string::npos) {
        std::string value = line.substr(pos + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        const size_t last = value.find_last_not_of(" \t");
        if (last != std::string::npos) value.erase(last + 1);
        return value;
    }
    return {};
}

inline LoadCase parseLoadLine(const std::string& line,                    // ex LoadParser
    const char* marker1,
    const char* marker2)
{
    LoadCase lc;
    const size_t work = line.find(marker1);
    const size_t name = line.find(marker2);
    if (work == std::string::npos || name == std::string::npos) return lc;

    const size_t len1 = std::strlen(marker1);
    const size_t len2 = std::strlen(marker2);

    std::string s1 = line.substr(0, work);
    std::string s2 = line.substr(work + len1, name - (work + len1));
    size_t s3Start = name + len2;
    size_t s3End = line.find_last_not_of(" \t\r\n");
    std::string s3 = (s3End != std::string::npos && s3End >= s3Start)
        ? line.substr(s3Start, s3End - s3Start + 1)
        : std::string{};

    s1.erase(0, s1.find_first_not_of(" \t"));
    s2.erase(0, s2.find_first_not_of(" \t"));
    s3.erase(0, s3.find_first_not_of(" \t"));

    std::istringstream iss(s1);
    double value;
    while (iss >> value) lc.intensities.push_back(value);
    iss = std::istringstream(s2);
    while (iss >> value) lc.positions.push_back(value);
    lc.name = s3;
    return lc;
}

inline void writeDefaultConfigFile(const std::string& filename,           // ex write_structural_model_Structuralconfig
    const std::filesystem::path& configDir)
{
    std::filesystem::create_directories(configDir);
    const std::filesystem::path fullPath = configDir / filename;
    std::ofstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "Erreur: Impossible d'ouvrir " << fullPath << '\n';
        return;
    }
    const std::vector<std::string> CONFIG = {
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
    for (const auto& t : CONFIG) file << t << '\n';
    file.close();
    std::cout << "Fichier créé: " << fullPath << '\n';
}