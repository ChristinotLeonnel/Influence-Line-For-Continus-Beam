#include "Input.h"
#include "Loading.h"
#include "Utils.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// =============================================================================
//  Configuration::loadFromFile
//
//  Lit (ou crée) path.json dans le dossier courant, en extrait le chemin de
//  travail (configPath), puis charge le fichier "structural model input.txt"
//  depuis ce chemin (et le crée avec une configuration par défaut si absent).
// =============================================================================
void Configuration::loadFromFile()
{
    // ── 1.  path.json  (création si absent, lecture sinon) ────────────────────
    std::filesystem::path configPath = getConfigPath();

    if (!checkFileExists("path.json")) {
        // Création du fichier path.json avec le chemin par défaut
        json j;
        j["configPath"] = configPath.string();

        std::ofstream configFile("path.json");
        if (!configFile.is_open())
            throw std::runtime_error(
                "Erreur : impossible de creer le fichier path.json");

        configFile << j.dump(4);
        configFile.close();
    }
    else {
        // Lecture de path.json
        std::ifstream fichier("path.json");
        if (!fichier.is_open())
            throw std::runtime_error(
                "Erreur : impossible d'ouvrir path.json");

        json data;
        try {
            fichier >> data;
        }
        catch (const json::parse_error& e) {
            throw std::runtime_error(
                std::string("Erreur de parsing path.json : ") + e.what());
        }

        if (!data.contains("configPath"))
            throw std::runtime_error(
                "path.json : cle 'configPath' manquante");

        configPath = data["configPath"].get<std::string>();
    }

    // ── 2.  Repertoire de travail ─────────────────────────────────────────────
    std::filesystem::create_directories(configPath);

    // ── 3.  structural model input.txt (creation si absent) ──────────────────
    const auto inputFilePath = configPath / "structural model input.txt";
    if (!checkFileExists(inputFilePath))
        write_structural_model_input("structural model input.txt", configPath);

    std::ifstream inputFile(inputFilePath);
    if (!inputFile.is_open())
        throw std::runtime_error(
            "Erreur : impossible d'ouvrir " + inputFilePath.string());

    // ── 4.  Parsing ligne par ligne ───────────────────────────────────────────
    std::string line;
    while (std::getline(inputFile, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.find("Length:") != std::string::npos) {
            spans = parseVector(line);
        }
        else if (line.find("Steps:") != std::string::npos) {
            steps = std::stod(getValue(line));
        }
        else if (line.find("Moment of Inertia:") != std::string::npos) {
            Inertie = parseVector(line);
        }
        else if (line.find("Young Modulus:") != std::string::npos) {
            YoungModule = parseVector(line);
        }
        else if (line.find("/Point/") != std::string::npos) {
            Point_LOAD.push_back(LoadParser(line, "/Point/", "::"));
        }
        else if (line.find("/Distributed/") != std::string::npos) {
            Rectangulare_LOAD.push_back(LoadParser(line, "/Distributed/", "::"));
        }
    }
    inputFile.close();

    if (spans.empty())
        throw std::invalid_argument(
            "Erreur : aucune travee dans le fichier de configuration");
}
