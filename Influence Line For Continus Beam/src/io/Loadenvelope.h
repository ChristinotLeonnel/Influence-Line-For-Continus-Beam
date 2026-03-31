#pragma once

// =============================================================================
//  LoadEnvelope — Calcul des enveloppes de charges mobiles
// =============================================================================
//
//  Pour un jeu de courbes d'influence (BM, SF, Def ou Rot) et un ensemble de
//  cas de chargement, détermine la position optimale de chaque convoi et la
//  valeur d'effet maximale résultante.
//
//  Trois résultats exposés après construction :
//    distributedLoadResult  — charge répartie seule
//    pointLoadResult        — charge ponctuelle seule
//    combinedLoadResult     — charge combinée (ponctuelle + répartie)
// =============================================================================

#include "io/StructuralConfig.h"
#include "utils/BeamUtils.h"

#include <string>
#include <vector>

class LoadEnvelope : public StructuralConfig      // ex Loading : public Configuration
{
    using StructuralConfig::StructuralConfig;

public:
    LoadEnvelope(
        std::vector<std::vector<std::vector<double>>> influenceCurves,  // ex CURVES
        std::vector<double>                           globalAbscissae,  // ex POSITION
        std::vector<std::vector<double>>              spanNodes         // ex SpanNodePositions
    );

    // ── Calcul unitaire ───────────────────────────────────────────────────
    double    onePointLoad(double intensity, size_t span, size_t section, size_t alpha);
    Position1D pluralPointLoad(const std::vector<double>& intensities,
        const std::vector<double>& offsets,
        size_t span, size_t section);

    double    oneRectangularLoad(double intensity, size_t span, size_t section,
        size_t begin, size_t end);
    Position1D pluralRectangularLoad(const std::vector<double>& intensities,
        const std::vector<double>& offsets,
        size_t span, size_t section);

    CombinedLoadResult combinedLoad(size_t span, size_t section);

    // ── Résultats ─────────────────────────────────────────────────────────
    LoadEnvelopeResult distributedLoadResult;   // ex Rectangular_load
    LoadEnvelopeResult pointLoadResult;         // ex Point_load
    LoadEnvelopeResult combinedLoadResult;      // ex Combined_load

private:
    /** Convertit une position en mètres en indice dans globalAbscissae_. */
    size_t metersToIndex(double positionMeters);   // ex MetersToPosition

    std::vector<std::vector<double>>              spanNodes_;          // ex SpanNodePositions
    std::vector<std::vector<std::vector<double>>> influenceCurves_;    // ex CURVES
    std::vector<double>                           globalAbscissae_;    // ex POSITION
};