#include "LoadPositioner.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

// =============================================================================
const std::vector<std::string> LoadPositioner::CURVE_NAMES = {
    "bending_moment",
    "rotation",
    "shear_force",
    "deflection"
};

static const std::vector<std::string> CURVE_DISPLAY = {
    "Bending Moment",
    "Rotation",
    "Shear Force",
    "Deflection"
};

// =============================================================================
//  Constructeur
// =============================================================================

LoadPositioner::LoadPositioner(std::filesystem::path root)
    : P_(root.empty() ? std::filesystem::path{} : std::move(root))
{
    const auto inputPath = P_.root / "structural model input.txt";
    std::ifstream f(inputPath);
    if (!f.is_open())
        throw std::runtime_error(
            "LoadPositioner: impossible d'ouvrir " + inputPath.string());

    std::string line;
    while (std::getline(f, line))
        allLines_.push_back(line + "\n");
    f.close();

    // Templates pré-filtrés (construits une seule fois)
    for (const auto& l : allLines_)
        if (l.find("/Distributed/") == std::string::npos || isComment(l))
            pointOnlyLines_.push_back(l);

    for (const auto& l : allLines_)
        if (l.find("/Point/") == std::string::npos || isComment(l))
            distOnlyLines_.push_back(l);
}

// =============================================================================
//  Helpers de parsing
// =============================================================================

bool LoadPositioner::isComment(const std::string& line)
{
    const auto p = line.find_first_not_of(" \t\r\n");
    return p != std::string::npos && line[p] == '#';
}

std::string LoadPositioner::extractLoadName(const std::string& line)
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

std::vector<double> LoadPositioner::extractOffsets(const std::string& line,
    const std::string& marker)
{
    const auto mPos = line.find(marker);
    if (mPos == std::string::npos) return {};
    std::string after = line.substr(mPos + marker.size());
    const auto ccPos = after.find("::");
    if (ccPos != std::string::npos) after = after.substr(0, ccPos);
    std::istringstream iss(after);
    std::vector<double> offsets;
    double v;
    while (iss >> v) offsets.push_back(v);
    return offsets;
}

std::string LoadPositioner::rebuildLine(const std::string& line,
    const std::string& markerBare,
    const std::vector<double>& offsets)
{
    const std::string marker = "/" + markerBare + "/";
    const auto mPos = line.find(marker);
    const auto ccPos = line.find("::");
    if (mPos == std::string::npos || ccPos == std::string::npos) return line;

    const std::string before = line.substr(0, mPos);
    const std::string after = "::" + line.substr(ccPos + 2);

    std::ostringstream oss;
    for (size_t i = 0; i < offsets.size(); ++i) {
        if (i) oss << ' ';
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.15g", offsets[i]);
        oss << buf;
    }
    return before + marker + " " + oss.str() + " " + after;
}

// =============================================================================
//  readEnvelopeJson
// =============================================================================

json LoadPositioner::readEnvelopeJson(const std::string& loadType,
    const std::string& curve,
    bool critical) const
{
    std::filesystem::path p;
    if (loadType == "Point_Load")
        p = (critical ? P_.env_critical_point : P_.env_global_point) / (curve + ".json");
    else if (loadType == "Distributed_Load")
        p = (critical ? P_.env_critical_dist : P_.env_global_dist) / (curve + ".json");
    else if (loadType == "Combined_Load")
        p = (critical ? P_.env_critical_combined : P_.env_global_combined) / (curve + ".json");
    else
        throw std::invalid_argument("readEnvelopeJson: type inconnu: " + loadType);

    std::ifstream f(p);
    if (!f.is_open())
        throw std::runtime_error("readEnvelopeJson: impossible d'ouvrir " + p.string());
    json j;
    f >> j;
    return j;
}

// =============================================================================
//  writePositioningFile
// =============================================================================

void LoadPositioner::writePositioningFile(const std::string& loadType,
    const std::string& curve,
    const std::vector<std::string>& lines,
    bool critical) const
{
    std::filesystem::path p;
    if (loadType == "Point_Load")
        p = (critical ? P_.pos_critical_point : P_.pos_global_point) / (curve + ".txt");
    else if (loadType == "Distributed_Load")
        p = (critical ? P_.pos_critical_dist : P_.pos_global_dist) / (curve + ".txt");
    else if (loadType == "Combined_Load")
        p = (critical ? P_.pos_critical_combined : P_.pos_global_combined) / (curve + ".txt");
    else
        throw std::invalid_argument("writePositioningFile: type inconnu: " + loadType);

    std::ofstream f(p);
    if (!f.is_open())
        throw std::runtime_error("writePositioningFile: impossible d'ouvrir " + p.string());
    for (const auto& line : lines) f << line;
}

// =============================================================================
//  processPoint
// =============================================================================

void LoadPositioner::processPoint(const std::string& curve, bool critical)
{
    auto lines = pointOnlyLines_;
    const json data = readEnvelopeJson("Point_Load", curve, critical);

    for (auto& line : lines) {
        if (line.find("/Point/") == std::string::npos || isComment(line)) continue;
        auto offsets = extractOffsets(line, "/Point/");
        if (offsets.empty()) continue;
        offsets[0] += data["load"][extractLoadName(line)]["Position"].get<double>();
        line = rebuildLine(line, "Point", offsets);
    }

    writePositioningFile("Point_Load", curve, lines, critical);
}

// =============================================================================
//  processDistributed
// =============================================================================

void LoadPositioner::processDistributed(const std::string& curve, bool critical)
{
    auto lines = distOnlyLines_;
    const json data = readEnvelopeJson("Distributed_Load", curve, critical);

    for (auto& line : lines) {
        if (line.find("/Distributed/") == std::string::npos || isComment(line)) continue;
        auto offsets = extractOffsets(line, "/Distributed/");
        if (offsets.empty()) continue;
        offsets[0] += data["load"][extractLoadName(line)]["Position"].get<double>();
        line = rebuildLine(line, "Distributed", offsets);
    }

    writePositioningFile("Distributed_Load", curve, lines, critical);
}

// =============================================================================
//  processCombined
// =============================================================================

void LoadPositioner::processCombined(const std::string& curve, bool critical)
{
    auto lines = allLines_;
    const json jsonCombined = readEnvelopeJson("Combined_Load", curve, critical);

    // Passe 1 : /Distributed/
    for (auto& line : lines) {
        if (line.find("/Distributed/") == std::string::npos || isComment(line)) continue;
        auto offsets = extractOffsets(line, "/Distributed/");
        if (offsets.empty()) continue;
        offsets[0] += jsonCombined["load"][extractLoadName(line)]["Position"].get<double>();
        line = rebuildLine(line, "Distributed", offsets);
    }

    // Passe 2 : /Point/
    for (auto& line : lines) {
        if (line.find("/Point/") == std::string::npos || isComment(line)) continue;
        auto offsets = extractOffsets(line, "/Point/");
        if (offsets.empty()) continue;
        offsets[0] += jsonCombined["load"][extractLoadName(line)]["Position"].get<double>();
        line = rebuildLine(line, "Point", offsets);
    }

    writePositioningFile("Combined_Load", curve, lines, critical);
}

// =============================================================================
//  processAll
// =============================================================================

void LoadPositioner::processAll(bool critical)
{
    const std::string scope = critical ? "Critical_Section" : "Global";
    std::cout << "\n[" << scope << "] Traitement de " << CURVE_NAMES.size() << " courbes...\n";

    for (size_t i = 0; i < CURVE_NAMES.size(); ++i) {
        const auto& curve = CURVE_NAMES[i];
        const auto& display = CURVE_DISPLAY[i];

        try {
            processPoint(curve, critical);
            std::cout << "  [OK] Point_Load        - " << display << "\n";
        }
        catch (const std::exception& e)
        {
            std::cerr << "  [ERR] Point_Load        - " << display << ": " << e.what() << "\n";
        }

        try {
            processDistributed(curve, critical);
            std::cout << "  [OK] Distributed_Load  - " << display << "\n";
        }
        catch (const std::exception& e)
        {
            std::cerr << "  [ERR] Distributed_Load  - " << display << ": " << e.what() << "\n";
        }

        try {
            processCombined(curve, critical);
            std::cout << "  [OK] Combined_Load     - " << display << "\n";
        }
        catch (const std::exception& e)
        {
            std::cerr << "  [ERR] Combined_Load     - " << display << ": " << e.what() << "\n";
        }
    }

    std::cout << "[" << scope << "] Done.\n";
}

// =============================================================================
//  run
// =============================================================================

void LoadPositioner::run()
{
    P_.createAll();
    processAll(false);   // Global
    processAll(true);    // Critical_Section
}