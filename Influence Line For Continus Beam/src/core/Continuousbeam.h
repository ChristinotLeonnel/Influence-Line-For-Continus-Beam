#pragma once
#ifndef __CONTINUOUS_BEAM__
#define __CONTINUOUS_BEAM__

// =============================================================================
//  ContinuousBeam — Analyse hyperstatique d'une poutre continue multi-travées
// =============================================================================
//
//  OPTIMISATIONS v2 (dimensions des matrices inchangées) :
//
//  [OPT-1]  total_nodes_        : nombre total de nœuds pré-calculé en O(S)
//  [OPT-2]  leftMomentsCache_   : moments gauche calculés une seule fois
//           rightMomentsCache_  : moments droite calculés une seule fois
//  [OPT-3]  shearForceAbscissa_ : abscisses SF pré-calculées en O(S×N)
//
//  Dimensions des matrices de résultat (identiques à l'original) :
//    BM, Rot, Def  →  [S][N_span_s][N_total]
//    SF            →  [S][N_span_s][N_total + discontinuités]
// =============================================================================

#include <vector>

class ContinuousBeam
{
public:
    ContinuousBeam(std::vector<double>& E, std::vector<double>& I,
        std::vector<double>& L, double& steps);

    // ── Propriétés de la poutre ────────────────────────────────────────────
    std::vector<double>& spanLengths;      // L_spans

    std::vector<double> flexCoeff_a;       // a_spans
    std::vector<double> flexCoeff_b;       // b_spans
    std::vector<double> flexCoeff_c;       // c_spans

    std::vector<double> transferCoeff;         // phy
    std::vector<double> transferCoeff_prime;   // phy_prime

    std::vector<std::vector<double>> supportMoments;  // SupportMoment

    // ── Positions des nœuds par travée ────────────────────────────────────
    std::vector<std::vector<double>> spanNodePositions;  // SpanNodePositions

    // [OPT-1] Nombre total de nœuds — accès O(1) pour les reserve()
    size_t total_nodes_ = 0;

    // ── API publique ───────────────────────────────────────────────────────
    std::vector<std::vector<std::vector<double>>> BendingMoments();
    std::vector<std::vector<std::vector<double>>> Rotation();
    std::vector<std::vector<std::vector<double>>> ShearForce(bool getAllAbscissae = false);
    std::vector<std::vector<std::vector<double>>> Deflection();

    std::vector<double> globalXCoordinates(const std::vector<std::vector<double>>& perSpanPoints);

    size_t numberOfSpans;  // number_of_spans

    double steps;

    std::vector<double>& elasticModuli;   // E_spans
    std::vector<double>& inertiaMoments;  // I_spans

private:
    // ── Données isostatiques statiques ────────────────────────────────────
    std::vector<std::vector<std::vector<double>>> staticBendingMoment;       // BendingMomentStatic
    std::vector<std::vector<std::vector<double>>> staticRotation;            // RotationStatic
    std::vector<std::vector<std::vector<double>>> staticShearForceAbscissa;  // ShearForceAbscissaStatic
    std::vector<std::vector<std::vector<double>>> staticShearForce;          // ShearForceStatic
    std::vector<std::vector<std::vector<double>>> staticDeflection;          // DeflectionStatic

    // [OPT-3] Abscisses SF pré-calculées
    std::vector<std::vector<std::vector<double>>> shearForceAbscissa_;       // ShearForceAbscissa_
    std::vector<std::vector<std::vector<double>>> buildShearForceAbscissa();

    std::vector<std::vector<double>> omegaSecond;  // Omega_Second_Spans
    std::vector<std::vector<double>> omegaPrime;   // Omega_Prime_Spans

    // [OPT-2] Caches moments — calculés une seule fois dans le constructeur
    std::vector<std::vector<double>> leftMomentsCache_;   // leftMomentsCache_
    std::vector<std::vector<double>> rightMomentsCache_;  // rightMomentsCache_

    std::vector<std::vector<double>> computeLeftLoadedSupportMoments();   // leftLoadedSpanSupportMoments
    std::vector<std::vector<double>> computeRightLoadedSupportMoments();  // rightLoadedSpanSupportMoments

    // [OPT-2] Versions avec cache injecté
    std::vector<std::vector<double>> leftSupportMomentsForSpan(
        int spanIndex,
        const std::vector<std::vector<double>>& leftCache);   // LeftSupportMoments

    std::vector<std::vector<double>> rightSupportMomentsForSpan(
        int spanIndex,
        const std::vector<std::vector<double>>& rightCache);  // RightSupportMoments

    std::vector<std::vector<std::vector<double>>> spanLeftMoments;   // SpanLeftSupportMoments
    std::vector<std::vector<std::vector<double>>> spanRightMoments;  // SpanRightSupportMoments
    std::vector<std::vector<std::vector<double>>> spanBoundaryMoments; // SpanLeftRight
};

#endif // __CONTINUOUS_BEAM__