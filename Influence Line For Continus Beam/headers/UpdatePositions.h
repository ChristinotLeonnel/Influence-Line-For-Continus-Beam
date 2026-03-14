#pragma once
#ifndef __UPDATE_POSITIONS__
#define __UPDATE_POSITIONS__

// =============================================================================
//  UpdatePositions.h
//  Conversion C++ de update_positions_from_bm.py — chemins via ProjectPaths
// =============================================================================
//
//  SOURCES JSON  (lues depuis 04_Load_Envelopes/) :
//    Global/Point_Load/<curve>.json
//    Global/Distributed_Load/<curve>.json
//    Global/Combined_Load/<curve>.json
//    Critical_Section/Point_Load/<curve>.json
//    Critical_Section/Distributed_Load/<curve>.json
//    Critical_Section/Combined_Load/<curve>.json
//
//  SORTIES TXT  (écrites dans 05_Load_Positioning/) :
//    Global/Point_Load/<curve>.txt
//    Global/Distributed_Load/<curve>.txt
//    Global/Combined_Load/<curve>.txt
//    Critical_Section/Point_Load/<curve>.txt
//    Critical_Section/Distributed_Load/<curve>.txt
//    Critical_Section/Combined_Load/<curve>.txt
//
//  Les chemins sont résolus par ProjectPaths — aucun chemin en dur ici.
// =============================================================================

#include "ProjectPaths.h"
#include <string>
#include <vector>

class UpdatePositions
{
public:
    // root : chemin racine du projet (défaut → getConfigPath())
    explicit UpdatePositions(std::filesystem::path root = {});

    // Point d'entrée : process_all(false) + process_all(true)
    void run();

private:
    ProjectPaths P_;

    std::vector<std::string> lines_;              // readlines() du fichier config
    std::vector<std::string> point_texte_;        // lines_ sans /Distributed/
    std::vector<std::string> distributed_texte_;  // lines_ sans /Point/

    static const std::vector<std::string> CURVE_NAMES;

    void processAll       (bool critical);
    void processPoint      (const std::string& curve, bool critical);
    void processDistributed(const std::string& curve, bool critical);
    void processCombined   (const std::string& curve, bool critical);

    // Lit 04_Load_Envelopes/[Global|Critical_Section]/<loadType>/<curve>.json
    json openJson(const std::string& loadType,
                  const std::string& curve,
                  bool critical) const;

    // Écrit 05_Load_Positioning/[Global|Critical_Section]/<loadType>/<curve>.txt
    void writeTxt(const std::string& loadType,
                  const std::string& curve,
                  const std::vector<std::string>& data,
                  bool critical) const;

    // Helpers de parsing (voir UpdatePositions.cpp pour les équivalents Python)
    static bool isComment         (const std::string& line);
    static std::string extractLoadName  (const std::string& line);
    static std::vector<double> extractPositions(const std::string& line,
                                                const std::string& marker);
    static std::string rebuildLine(const std::string& line,
                                   const std::string& markerBare,
                                   const std::vector<double>& positions);
};

#endif // __UPDATE_POSITIONS__
