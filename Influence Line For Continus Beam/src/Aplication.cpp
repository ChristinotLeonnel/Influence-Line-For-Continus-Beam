#include "Output.h"
#include "Input.h"
#include "UpdatePositions.h"

#include <chrono>
#include <iostream>

int main() {

    // ── Chargement de la configuration ────────────────────────────────────────
    Configuration config;
    config.loadFromFile();

    // ── Analyse structurelle ──────────────────────────────────────────────────
    auto start = std::chrono::high_resolution_clock::now();

    Output LI(config.YoungModule, config.Inertie, config.spans, config.steps);

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Analysis Time :: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
              << " ms\n";

    // ── Mise à jour des positions de charges ──────────────────────────────────
    //  Lit  : 04_Load_Envelopes/[Global|Critical_Section]/<type>/<grandeur>.json
    //  Écrit: 05_Load_Positioning/[Global|Critical_Section]/<type>/<grandeur>.txt
    try {
        UpdatePositions updater;   // chemin = getConfigPath() automatiquement
        updater.run();
    }
    catch (const std::exception& e) {
        std::cerr << "UpdatePositions error: " << e.what() << "\n";
    }

    return 0;
}
