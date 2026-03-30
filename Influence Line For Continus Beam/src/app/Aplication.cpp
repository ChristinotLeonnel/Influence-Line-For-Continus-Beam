#include "io/Output.h"
#include "io/Input.h"
#include "io/UpdatePositions.h"
#include "Ploting.h"          // ← Ploting.lib

#include <chrono>
#include <iostream>
#include <fstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

int main() {
    // ── Chargement de la configuration ────────────────────────────────────────
    Configuration config;
    config.loadFromFile();

    // ── Lecture de path.json (même dossier que l'exe) ─────────────────────────
    std::string configPath;
    {
        std::ifstream fichier("path.json");
        if (!fichier.is_open()) {
            std::cerr << "Erreur : impossible d'ouvrir path.json\n";
            return 1;
        }
        try {
            json data;
            fichier >> data;
            configPath = data["configPath"].get<std::string>();
        }
        catch (const json::parse_error& ex) {
            std::cerr << "Erreur de parsing path.json : " << ex.what() << "\n";
            return 1;
        }
    }

    // ── Analyse structurelle ──────────────────────────────────────────────────
    auto start = std::chrono::high_resolution_clock::now();

    Output LI(config.YoungModule, config.Inertie, config.spans, config.steps, configPath);

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Analysis Time :: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        << " ms\n";

    // ── Mise à jour des positions de charges ──────────────────────────────────
    start = std::chrono::high_resolution_clock::now();

    try {
        UpdatePositions updater(configPath);
        updater.run();
    }
    catch (const std::exception& ex) {
        std::cerr << "UpdatePositions error: " << ex.what() << "\n";
    }

    end = std::chrono::high_resolution_clock::now();
    std::cout << "UpdatePosition Time :: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        << " ms\n";

    // ── Génération des plots et animations (Ploting.lib) ─────────────────────
    start = std::chrono::high_resolution_clock::now();

    try {
        plotting::run(configPath);
    }
    catch (const std::exception& ex) {
        std::cerr << "Plotting error: " << ex.what() << "\n";
    }

    end = std::chrono::high_resolution_clock::now();
    std::cout << "Plots and Animations Time :: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        << " ms\n";
    return 0;
}