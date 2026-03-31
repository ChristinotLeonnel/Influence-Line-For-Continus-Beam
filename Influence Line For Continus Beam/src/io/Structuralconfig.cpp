#include <fstream>
#include <stdexcept>

#include "StructuralConfig.h"
#include "io/LoadEnvelope.h"
#include "utils/BeamUtils.h"

#include <vector>
#include <limits>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <future>
#include <iostream>

void StructuralConfig::loadFromFile()
{
    std::filesystem::path configDir = getDefaultConfigPath();

    // ── Gestion de path.json ───────────────────────────────────────────────
    if (!fileExists("path.json"))
    {
        json j;
        j["configPath"] = configDir.string();
        std::ofstream configFile("path.json");
        if (!configFile.is_open())
            throw std::runtime_error("Erreur: Impossible de créer le fichier path.json");
        configFile << j.dump(4);
        configFile.close();
    }
    else
    {
        std::ifstream file("path.json");
        if (!file.is_open()) {
            std::cerr << "Erreur : impossible d'ouvrir path.json\n";
        }
        json data;
        try { file >> data; }
        catch (const json::parse_error& e) {
            std::cerr << "Erreur de parsing path.json : " << e.what() << '\n';
        }
        configDir = data["configPath"].get<std::string>();
    }

    std::filesystem::create_directories(configDir);

    // ── Génération du fichier de config si absent ──────────────────────────
    if (!fileExists(configDir / "structural model input.txt"))
        writeDefaultConfigFile("structural model input.txt", configDir);

    // ── Lecture du fichier de configuration ───────────────────────────────
    std::ifstream inputFile(configDir / "structural model input.txt");
    if (!inputFile.is_open())
        throw std::runtime_error("Erreur: Impossible d'ouvrir structural model input.txt");

    std::string line;
    while (std::getline(inputFile, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.find("Length:") != std::string::npos)
            spanLengths = parseDoubleVector(line);
        else if (line.find("Steps:") != std::string::npos)
            stepSize = std::stod(parseScalarValue(line));
        else if (line.find("Moment of Inertia:") != std::string::npos)
            inertiaMoments = parseDoubleVector(line);
        else if (line.find("Young Modulus:") != std::string::npos)
            elasticModuli = parseDoubleVector(line);
        else if (line.find("/Point/") != std::string::npos)
            pointLoads.push_back(parseLoadLine(line, "/Point/", "::"));
        else if (line.find("/Distributed/") != std::string::npos)
            distributedLoads.push_back(parseLoadLine(line, "/Distributed/", "::"));
    }
    inputFile.close();

    if (spanLengths.empty())
        throw std::invalid_argument("Error: No spans provided in input file!");
}