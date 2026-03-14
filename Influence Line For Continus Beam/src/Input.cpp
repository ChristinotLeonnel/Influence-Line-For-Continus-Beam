#include <fstream>
#include <stdexcept>

#include "Input.h"  
#include "Loading.h"
#include "Utils.h"

#include <vector>
#include <limits>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <future>
#include <iostream>

void Configuration::loadFromFile() {

    std::filesystem::path configPath = getConfigPath();

    if (!checkFileExists("path.json"))
    {
        
        json j;
        j["configPath"] = configPath.string();
        std::filesystem::path configFilePath = "path.json";
        std::ofstream configFile(configFilePath);
        if (!configFile.is_open()) {
            throw std::runtime_error("Erreur: Impossible de créer le fichier path.json");
        }
        configFile << j.dump(4); // Indentation de 4 espaces pour la lisibilité
		configFile.close();
    }

    
    std::filesystem::create_directories(configPath);

    if (!checkFileExists(configPath / "structural model input.txt"))
        write_structural_model_input("structural model input.txt");

    std::ifstream inputFile(configPath / "structural model input.txt");
    if (!inputFile.is_open()) {
        throw std::runtime_error("Erreur: Impossible d'ouvrir le fichier input.txt");
    }

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

    if (spans.empty()) {
        throw std::invalid_argument("Error: No spans provided in input file!");
    }

}