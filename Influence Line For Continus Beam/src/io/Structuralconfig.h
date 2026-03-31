#pragma once

// =============================================================================
//  StructuralConfig — Lecture et stockage de la configuration du modèle
// =============================================================================
//
//  Lit le fichier "structural model input.txt" et expose :
//    - les longueurs de travées, le pas de discrétisation
//    - les modules d'élasticité et moments d'inertie
//    - les cas de chargement ponctuel et réparti
// =============================================================================

#include "utils/BeamUtils.h"

class StructuralConfig {                    // ex Configuration
public:
    std::vector<double> spanLengths;        // ex spans
    double              stepSize;           // ex steps
    std::vector<double> inertiaMoments;     // ex Inertie
    std::vector<double> elasticModuli;      // ex YoungModule
    std::vector<LoadCase> pointLoads;       // ex Point_LOAD
    std::vector<LoadCase> distributedLoads; // ex Rectangulare_LOAD

    StructuralConfig() : stepSize(1.0) {}

    void loadFromFile();
};