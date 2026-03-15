#include "Output.h"
#include "Input.h"
#include "UpdatePositions.h"

#include <chrono>
#include <iostream>

int main() {

    // ── Chargement de la configuration ────────────────────────────────────────
    Configuration config;
    config.loadFromFile();

    std::ifstream fichier("path.json");

    if (!fichier.is_open()) {
        std::cerr << "Erreur : impossible d'ouvrir le fichier !" << std::endl;
    }

    // Parser le JSON
    json data;
    try {
        fichier >> data;
    }
    catch (const json::parse_error& e) {
        std::cerr << "Erreur de parsing : " << e.what() << std::endl;
    }

    auto configPath = data["configPath"].get<std::string>();

    // ── Analyse structurelle ──────────────────────────────────────────────────
    auto start = std::chrono::high_resolution_clock::now();

    Output LI(config.YoungModule, config.Inertie, config.spans, config.steps, configPath);

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Analysis Time :: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms\n";

    // ── Mise à jour des positions de charges ──────────────────────────────────
    //  Lit  : 04_Load_Envelopes/[Global|Critical_Section]/<type>/<grandeur>.json
    //  Écrit: 05_Load_Positioning/[Global|Critical_Section]/<type>/<grandeur>.txt
    try {
        UpdatePositions updater(configPath);   // chemin = getConfigPath() automatiquement
        updater.run();
    }
    catch (const std::exception& e) {
        std::cerr << "UpdatePositions error: " << e.what() << "\n";
    }

    return 0;
}
