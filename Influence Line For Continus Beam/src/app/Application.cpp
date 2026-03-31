#include "io/ResultsExporter.h"
#include "io/StructuralConfig.h"
#include "io/LoadPositioner.h"
#include "Plotting.h"              // ex plotting.h — faute corrigée

#include <chrono>
#include <iostream>
#include <fstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

int main()
{
    // ── Lecture de la configuration du modèle structural ──────────────────
    StructuralConfig config;        // ex Configuration config
    config.loadFromFile();

    // ── Lecture de path.json ───────────────────────────────────────────────
    std::string outputRoot;
    {
        std::ifstream file("path.json");
        if (!file.is_open()) {
            std::cerr << "Erreur : impossible d'ouvrir path.json\n";
            return 1;
        }
        try {
            json data;
            file >> data;
            outputRoot = data["configPath"].get<std::string>();
        }
        catch (const json::parse_error& ex) {
            std::cerr << "Erreur de parsing path.json : " << ex.what() << "\n";
            return 1;
        }
    }

    // ── Analyse structurelle et export ────────────────────────────────────
    auto t0 = std::chrono::high_resolution_clock::now();

    ResultsExporter exporter(           // ex Output LI(...)
        config.elasticModuli,
        config.inertiaMoments,
        config.spanLengths,
        config.stepSize,
        outputRoot);

    auto t1 = std::chrono::high_resolution_clock::now();
    std::cout << "Analysis Time :: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
        << " ms\n";

    // ── Positionnement des charges ─────────────────────────────────────────
    t0 = std::chrono::high_resolution_clock::now();

    try {
        LoadPositioner positioner(outputRoot);   // ex UpdatePositions updater(...)
        positioner.run();
    }
    catch (const std::exception& ex) {
        std::cerr << "LoadPositioner error: " << ex.what() << "\n";
    }

    t1 = std::chrono::high_resolution_clock::now();
    std::cout << "UpdatePosition Time :: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
        << " ms\n";
    
    // ── Génération des plots et animations ────────────────────────────────
    t0 = std::chrono::high_resolution_clock::now();

    try {
        plotting::run(outputRoot);
    }
    catch (const std::exception& ex) {
        std::cerr << "Plotting error: " << ex.what() << "\n";
    }

    t1 = std::chrono::high_resolution_clock::now();
    std::cout << "Plots and Animations Time :: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
        << " ms\n";
        
    return 0;
}