#pragma once
#ifndef __SPAN_RESULT__
#define __SPAN_RESULT__

// =============================================================================
//  SpanResult — Résultats structurels d'une seule travée
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
    size_t span_index = 0;   // indice de la travée dans la poutre
    size_t N          = 0;   // nombre de nœuds de cette travée
    size_t N_total    = 0;   // nombre total de nœuds de la poutre entière

    // Chaque grandeur : [section (N lignes)][alpha (N_total colonnes)]
    // SF peut avoir N_total + discontinuités colonnes
    std::vector<std::vector<double>> BM;   // moment fléchissant
    std::vector<std::vector<double>> SF;   // effort tranchant
    std::vector<std::vector<double>> Def;  // flèche
    std::vector<std::vector<double>> Rot;  // rotation

    // ── Tracking des maxima locaux à cette travée ──────────────────────────
    // Valeur absolue maximale trouvée dans chaque grandeur
    double max_BM  = 0.0;
    double max_SF  = 0.0;
    double max_Def = 0.0;
    double max_Rot = 0.0;

    // Section (indice j) où se trouve le max
    size_t sec_BM  = 0;
    size_t sec_SF  = 0;
    size_t sec_Def = 0;
    size_t sec_Rot = 0;

    // Alpha (indice k dans le vecteur de la section) où se trouve le max
    size_t alpha_BM  = 0;
    size_t alpha_SF  = 0;
    size_t alpha_Def = 0;
    size_t alpha_Rot = 0;
};

#endif // __SPAN_RESULT__
