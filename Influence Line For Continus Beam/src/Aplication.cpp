#include "Output.h"
#include "Input.h"
#include "UpdatePositions.h"
#include "Ploting.h"          // <- Ploting.lib

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

// =============================================================================
//  Helpers d'affichage (memes que Ploting.cpp pour avoir un journal coherent)
// =============================================================================
static void banner(const std::string& title) {
    std::cout << "\n=====================================================\n";
    std::cout << "  " << title << '\n';
    std::cout << "=====================================================\n";
}
static void phase(const std::string& title) {
    std::cout << "\n----- " << title;
    int written = static_cast<int>(title.size()) + 6;
    for (int i = written; i < 53; ++i) std::cout << '-';
    std::cout << '\n';
}

// =============================================================================
//  main
// =============================================================================
int main()
{
    banner("INFLUENCE LINE - Continuous Beam Analysis");

    // ── Chargement de la configuration ────────────────────────────────────────
    Configuration config;
    try {
        config.loadFromFile();
    }
    catch (const std::exception& ex) {
        std::cerr << "  [ ERR ]  Configuration :: " << ex.what() << '\n';
        return 1;
    }

    // ── Lecture de path.json (meme dossier que l'exe) ─────────────────────────
    std::string configPath;
    {
        std::ifstream fichier("path.json");
        if (!fichier.is_open()) {
            std::cerr << "  [ ERR ]  path.json :: cannot open\n";
            return 1;
        }
        try {
            json data;
            fichier >> data;
            configPath = data.at("configPath").get<std::string>();
        }
        catch (const std::exception& ex) {
            std::cerr << "  [ ERR ]  path.json :: " << ex.what() << '\n';
            return 1;
        }
    }

    // ── Phase A : analyse structurelle ────────────────────────────────────────
    phase("Phase A : Structural Analysis");
    const auto start = std::chrono::steady_clock::now();
    try {
        Output LI(config.YoungModule, config.Inertie, config.spans,
                  config.steps, configPath);
    }
    catch (const std::exception& ex) {
        std::cerr << "  [ ERR ]  Analysis :: " << ex.what() << '\n';
        return 1;
    }
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
    std::cout << "  [ OK  ]  Analysis time : " << elapsed_ms << " ms\n";

    // ── Phase B : mise a jour des positions de charges ────────────────────────
    phase("Phase B : Update Load Positions");
    try {
        UpdatePositions updater(configPath);
        updater.run();
    }
    catch (const std::exception& ex) {
        std::cerr << "  [ ERR ]  UpdatePositions :: " << ex.what() << '\n';
    }

    // ── Phase C : generation des plots et animations (Ploting.lib) ───────────
    try {
        plotting::run(configPath);
    }
    catch (const std::exception& ex) {
        std::cerr << "  [ ERR ]  Plotting :: " << ex.what() << '\n';
    }

    banner("DONE");
    return 0;
}
