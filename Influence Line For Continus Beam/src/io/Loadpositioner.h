#pragma once
#ifndef __LOAD_POSITIONER__
#define __LOAD_POSITIONER__

// =============================================================================
//  LoadPositioner — Génération des fichiers de positionnement de charges
// =============================================================================
//
//  Lit les enveloppes JSON depuis 04_Load_Envelopes/ et écrit les fichiers
//  de positionnement dans 05_Load_Positioning/.
//
//  SOURCES JSON  (04_Load_Envelopes/) :
//    Global/Point_Load/<curve>.json
//    Global/Distributed_Load/<curve>.json
//    Global/Combined_Load/<curve>.json
//    Critical_Section/ (idem)
//
//  SORTIES TXT   (05_Load_Positioning/) :
//    Global/Point_Load/<curve>.txt
//    Global/Distributed_Load/<curve>.txt
//    Global/Combined_Load/<curve>.txt
//    Critical_Section/ (idem)
// =============================================================================

#include "ProjectPaths.h"
#include <string>
#include <vector>

class LoadPositioner                    // ex UpdatePositions
{
public:
    explicit LoadPositioner(std::filesystem::path root = {});

    /** Point d'entrée : traite Global puis Critical_Section. */
    void run();

private:
    ProjectPaths P_;

    std::vector<std::string> allLines_;          // ex lines_
    std::vector<std::string> pointOnlyLines_;    // ex point_texte_       (sans /Distributed/)
    std::vector<std::string> distOnlyLines_;     // ex distributed_texte_ (sans /Point/)

    static const std::vector<std::string> CURVE_NAMES;

    void processAll(bool critical);
    void processPoint(const std::string& curve, bool critical);
    void processDistributed(const std::string& curve, bool critical);
    void processCombined(const std::string& curve, bool critical);

    json readEnvelopeJson(const std::string& loadType,
        const std::string& curve,
        bool critical) const;     // ex openJson

    void writePositioningFile(const std::string& loadType,
        const std::string& curve,
        const std::vector<std::string>& lines,
        bool critical) const;  // ex writeTxt

    static bool                isComment(const std::string& line);
    static std::string         extractLoadName(const std::string& line);
    static std::vector<double> extractOffsets(const std::string& line,  // ex extractPositions
        const std::string& marker);
    static std::string         rebuildLine(const std::string& line,
        const std::string& markerBare,
        const std::vector<double>& offsets);
};

#endif // __LOAD_POSITIONER__