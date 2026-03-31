#pragma once
#ifndef __SPAN_RESULT__
#define __SPAN_RESULT__

// =============================================================================
//  SpanResult — Résultats structuraux d'une travée
// =============================================================================
//
//  Unité de calcul de l'architecture Lazy-Streaming.
//  Calculée travée par travée, écrite en JSON, puis détruite immédiatement.
//
//  RAM max simultanée : O(S × N²) au lieu de O(S² × N²)
// =============================================================================

#include <vector>
#include <cstddef>

struct SpanResult
{
    size_t spanIndex = 0;  // indice de la travée dans la poutre
    size_t N = 0;  // nombre de nœuds de cette travée
    size_t N_total = 0;  // nombre total de nœuds de la poutre entière

    // Chaque grandeur : [section (N lignes)][alpha (N_total colonnes)]
    // SF peut avoir N_total + discontinuités colonnes
    std::vector<std::vector<double>> bendingMoment;  // BM
    std::vector<std::vector<double>> shearForce;     // SF
    std::vector<std::vector<double>> deflection;     // Def
    std::vector<std::vector<double>> rotation;       // Rot

    // ── Tracking des maxima locaux à cette travée ──────────────────────────
    double maxBendingMoment = 0.0;
    double maxShearForce = 0.0;
    double maxDeflection = 0.0;
    double maxRotation = 0.0;

    // Section (indice j) où se trouve le maximum
    size_t maxBM_sectionIdx = 0;
    size_t maxSF_sectionIdx = 0;
    size_t maxDef_sectionIdx = 0;
    size_t maxRot_sectionIdx = 0;

    // Alpha (indice k dans le vecteur de la section) où se trouve le maximum
    size_t maxBM_alphaIdx = 0;
    size_t maxSF_alphaIdx = 0;
    size_t maxDef_alphaIdx = 0;
    size_t maxRot_alphaIdx = 0;
};

#endif // __SPAN_RESULT__