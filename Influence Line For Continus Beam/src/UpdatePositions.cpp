#include "UpdatePositions.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

// =============================================================================
const std::vector<std::string> UpdatePositions::CURVE_NAMES = {
    "bending_moment",
    "rotation",
    "shear_force",
    "deflection"
};

// Noms affichés dans la console (correspondance avec CURVE_NAMES)
static const std::vector<std::string> CURVE_DISPLAY = {
    "Bending Moment",
    "Rotation",
    "Shear Force",
    "Deflection"
};

// =============================================================================
//  Constructeur
// =============================================================================

UpdatePositions::UpdatePositions(std::filesystem::path root)
    : P_(root.empty() ? std::filesystem::path{} : std::move(root))
{
    const auto inputPath = P_.root / "structural model input.txt";
    std::ifstream f(inputPath);
    if (!f.is_open())
        throw std::runtime_error(
            "UpdatePositions: impossible d'ouvrir " + inputPath.string());

    std::string line;
    while (std::getline(f, line))
        lines_.push_back(line + "\n");
    f.close();

    // Templates pré-filtrés (construits une seule fois)
    for (const auto& l : lines_)
        if (l.find("/Distributed/") == std::string::npos || isComment(l))
            point_texte_.push_back(l);

    for (const auto& l : lines_)
        if (l.find("/Point/") == std::string::npos || isComment(l))
            distributed_texte_.push_back(l);
}

// =============================================================================
//  Helpers de parsing
// =============================================================================

bool UpdatePositions::isComment(const std::string& line)
{
    const auto p = line.find_first_not_of(" \t\r\n");
    return p != std::string::npos && line[p] == '#';
}

std::string UpdatePositions::extractLoadName(const std::string& line)
{
    const auto sep = line.rfind("::");
    if (sep == std::string::npos) return {};
    std::string after = line.substr(sep + 2);
    const auto s = after.find_first_not_of(" \t\r\n");
    if (s == std::string::npos) return {};
    after = after.substr(s);
    const auto e = after.find_last_not_of(" \t\r\n");
    if (e != std::string::npos) after = after.substr(0, e + 1);
    std::istringstream iss(after);
    std::string w1, w2;
    if (!(iss >> w1)) return {};
    if (!(iss >> w2)) return w1;
    return w1 + " " + w2;
}

std::vector<double> UpdatePositions::extractPositions(const std::string& line,
                                                       const std::string& marker)
{
    const auto mPos = line.find(marker);
    if (mPos == std::string::npos) return {};
    std::string after = line.substr(mPos + marker.size());
    const auto ccPos = after.find("::");
    if (ccPos != std::string::npos) after = after.substr(0, ccPos);
    std::istringstream iss(after);
    std::vector<double> positions;
    double v;
    while (iss >> v) positions.push_back(v);
    return positions;
}

std::string UpdatePositions::rebuildLine(const std::string& line,
                                          const std::string& markerBare,
                                          const std::vector<double>& positions)
{
    const std::string marker = "/" + markerBare + "/";
    const auto mPos  = line.find(marker);
    const auto ccPos = line.find("::");
    if (mPos == std::string::npos || ccPos == std::string::npos) return line;

    const std::string before = line.substr(0, mPos);
    const std::string after  = "::" + line.substr(ccPos + 2);

    std::ostringstream oss;
    for (size_t i = 0; i < positions.size(); ++i) {
        if (i) oss << ' ';
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.15g", positions[i]);
        oss << buf;
    }
    return before + marker + " " + oss.str() + " " + after;
}

// =============================================================================
//  openJson
//  Lit depuis 04_Load_Envelopes/[Global|Critical_Section]/<loadType>/<curve>.json
// =============================================================================

json UpdatePositions::openJson(const std::string& loadType,
                                const std::string& curve,
                                bool critical) const
{
    // Résolution via ProjectPaths
    std::filesystem::path p;
    if (loadType == "Point_Load")
        p = (critical ? P_.env_critical_point   : P_.env_global_point)   / (curve + ".json");
    else if (loadType == "Distributed_Load")
        p = (critical ? P_.env_critical_dist    : P_.env_global_dist)    / (curve + ".json");
    else if (loadType == "Combined_Load")
        p = (critical ? P_.env_critical_combined : P_.env_global_combined) / (curve + ".json");
    else
        throw std::invalid_argument("openJson: type inconnu: " + loadType);

    std::ifstream f(p);
    if (!f.is_open())
        throw std::runtime_error("openJson: impossible d'ouvrir " + p.string());
    json j;
    f >> j;
    return j;
}

// =============================================================================
//  writeTxt
//  Écrit dans 05_Load_Positioning/[Global|Critical_Section]/<loadType>/<curve>.txt
// =============================================================================

void UpdatePositions::writeTxt(const std::string& loadType,
                                const std::string& curve,
                                const std::vector<std::string>& data,
                                bool critical) const
{
    std::filesystem::path p;
    if (loadType == "Point_Load")
        p = (critical ? P_.pos_critical_point   : P_.pos_global_point)   / (curve + ".txt");
    else if (loadType == "Distributed_Load")
        p = (critical ? P_.pos_critical_dist    : P_.pos_global_dist)    / (curve + ".txt");
    else if (loadType == "Combined_Load")
        p = (critical ? P_.pos_critical_combined : P_.pos_global_combined) / (curve + ".txt");
    else
        throw std::invalid_argument("writeTxt: type inconnu: " + loadType);

    std::ofstream f(p);
    if (!f.is_open())
        throw std::runtime_error("writeTxt: impossible d'ouvrir " + p.string());
    for (const auto& line : data) f << line;
}

// =============================================================================
//  processPoint
// =============================================================================

void UpdatePositions::processPoint(const std::string& curve, bool critical)
{
    auto new_lines = point_texte_;
    const json data = openJson("Point_Load", curve, critical);

    for (auto& line : new_lines) {
        if (line.find("/Point/") == std::string::npos || isComment(line)) continue;
        auto positions = extractPositions(line, "/Point/");
        if (positions.empty()) continue;
        const std::string load_name = extractLoadName(line);
        positions[0] += data["load"][load_name]["Position"].get<double>();
        line = rebuildLine(line, "Point", positions);
    }

    writeTxt("Point_Load", curve, new_lines, critical);
}

// =============================================================================
//  processDistributed
// =============================================================================

void UpdatePositions::processDistributed(const std::string& curve, bool critical)
{
    auto new_lines = distributed_texte_;
    const json data = openJson("Distributed_Load", curve, critical);

    for (auto& line : new_lines) {
        if (line.find("/Distributed/") == std::string::npos || isComment(line)) continue;
        auto positions = extractPositions(line, "/Distributed/");
        if (positions.empty()) continue;
        const std::string load_name = extractLoadName(line);
        positions[0] += data["load"][load_name]["Position"].get<double>();
        line = rebuildLine(line, "Distributed", positions);
    }

    writeTxt("Distributed_Load", curve, new_lines, critical);
}

// =============================================================================
//  processCombined
// =============================================================================

void UpdatePositions::processCombined(const std::string& curve, bool critical)
{
    auto new_lines = lines_;
    const json json_combined = openJson("Combined_Load", curve, critical);

    // Passe 1 : /Distributed/
    for (auto& line : new_lines) {
        if (line.find("/Distributed/") == std::string::npos || isComment(line)) continue;
        auto positions = extractPositions(line, "/Distributed/");
        if (positions.empty()) continue;
        positions[0] += json_combined["load"][extractLoadName(line)]["Position"].get<double>();
        line = rebuildLine(line, "Distributed", positions);
    }

    // Passe 2 : /Point/ (sur new_lines déjà modifié)
    for (auto& line : new_lines) {
        if (line.find("/Point/") == std::string::npos || isComment(line)) continue;
        auto positions = extractPositions(line, "/Point/");
        if (positions.empty()) continue;
        positions[0] += json_combined["load"][extractLoadName(line)]["Position"].get<double>();
        line = rebuildLine(line, "Point", positions);
    }

    writeTxt("Combined_Load", curve, new_lines, critical);
}

// =============================================================================
//  processAll
// =============================================================================

void UpdatePositions::processAll(bool critical)
{
    const std::string scope = critical ? "Critical_Section" : "Global";
    std::cout << "\n[" << scope << "] Traitement de " << CURVE_NAMES.size() << " courbes...\n";

    for (size_t i = 0; i < CURVE_NAMES.size(); ++i) {
        const auto& curve   = CURVE_NAMES[i];
        const auto& display = CURVE_DISPLAY[i];

        try   { processPoint(curve, critical);
                std::cout << "  [OK] Point_Load        - " << display << "\n"; }
        catch (const std::exception& e)
              { std::cerr << "  [ERR] Point_Load        - " << display << " : " << e.what() << "\n"; }

        try   { processDistributed(curve, critical);
                std::cout << "  [OK] Distributed_Load  - " << display << "\n"; }
        catch (const std::exception& e)
              { std::cerr << "  [ERR] Distributed_Load  - " << display << " : " << e.what() << "\n"; }

        try   { processCombined(curve, critical);
                std::cout << "  [OK] Combined_Load     - " << display << "\n"; }
        catch (const std::exception& e)
              { std::cerr << "  [ERR] Combined_Load     - " << display << " : " << e.what() << "\n"; }
    }

    std::cout << "[" << scope << "] Done.\n";
}

// =============================================================================
//  run
// =============================================================================

void UpdatePositions::run()
{
    P_.createAll();          // crée 05_Load_Positioning/ si besoin
    processAll(false);       // Global
    processAll(true);        // Critical_Section
}
