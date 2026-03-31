#pragma once
#ifndef __RESULTS_EXPORTER__
#define __RESULTS_EXPORTER__

// =============================================================================
//  ResultsExporter — Pipeline complet d'export des résultats structuraux
// =============================================================================
//
//  Hérite de ContinuousBeam, calcule les 4 grandeurs (BM, SF, Def, Rot),
//  puis écrit tous les fichiers JSON dans l'arborescence ProjectPaths.
// =============================================================================

#include "core/ContinuousBeam.h"
#include "io/ProjectPaths.h"
#include "utils/BeamUtils.h"

#include <vector>
#include <filesystem>

class ResultsExporter : public ContinuousBeam    // ex Output : public Hyperstatique
{
    using ContinuousBeam::ContinuousBeam;

public:
    ResultsExporter(std::vector<double>& E,
        std::vector<double>& I,
        std::vector<double>& L,
        double& steps,
        std::filesystem::path outputRoot = "");

    Position3D bendingMomentCritical{ 0, 0, 0, 0.0 };   // ex BendingMomentMaxPositions
    Position3D deflectionCritical{ 0, 0, 0, 0.0 };   // ex DeflectionMaxPositions
    Position3D rotationCritical{ 0, 0, 0, 0.0 };   // ex RotationMaxPositions
    Position3D shearForceCritical{ 0, 0, 0, 0.0 };   // ex ShearForceMaxPositions
    Position2D supportMomentCritical{ 0, 0, 0.0 };      // ex SupportMomentMaxPositions
};

#endif // __RESULTS_EXPORTER__