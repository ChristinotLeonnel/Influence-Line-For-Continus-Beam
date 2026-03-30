#pragma once
#ifndef __HYP__
#define __HYP__

// =============================================================================
//  Hyperstatique — Analyse hyperstatique d'une poutre continue
// =============================================================================
//
//  MODIFICATIONS v2 (dimensions des matrices inchangées) :
//
//  + total_nodes_        : nombre total de nœuds pré-calculé [OPT-1]
//  + leftMomentsCache_   : cache moments gauche calculé une seule fois [OPT-2]
//  + rightMomentsCache_  : cache moments droite calculé une seule fois [OPT-2]
//  + ShearForceAbscissa_ : abscisses SF pré-calculées en O(S×N)        [OPT-3]
//  + buildShearForceAbscissa() : construit ShearForceAbscissa_ [OPT-3]
//
//  Signatures de LeftSupportMoments / RightSupportMoments modifiées pour
//  accepter le cache en paramètre (évite O(S²×N) → O(S×N)).
// =============================================================================

#include <vector>

class Hyperstatique
{
public:
    Hyperstatique(std::vector<double>& E, std::vector<double>& I,
                  std::vector<double>& L, double& steps);

    // ── Propriétés de la poutre ────────────────────────────────────────────
    std::vector<double>& L_spans;

    std::vector<double> a_spans;
    std::vector<double> b_spans;
    std::vector<double> c_spans;

    std::vector<double> phy;
    std::vector<double> phy_prime;

    std::vector<std::vector<double>> SupportMoment;

    // ── Positions des nœuds par travée ────────────────────────────────────
    std::vector<std::vector<double>> SpanNodePositions;

    // [OPT-1] Nombre total de nœuds — accès O(1) pour les reserve()
    size_t total_nodes_ = 0;

    // ── API publique (signatures identiques à l'original) ─────────────────
    std::vector<std::vector<std::vector<double>>> BendingMoments();
    std::vector<std::vector<std::vector<double>>> Rotation();
    std::vector<std::vector<std::vector<double>>> ShearForce(bool get_all_abscisse = false);
    std::vector<std::vector<std::vector<double>>> Deflection();

    std::vector<double> pointsXCoordinates(const std::vector<std::vector<double>>& liste);

    size_t number_of_spans;

    double steps;

    std::vector<double>& E_spans;
    std::vector<double>& I_spans;

private:
    // ── Données statiques isostatiques ────────────────────────────────────
    std::vector<std::vector<std::vector<double>>> BendingMomentStatic;
    std::vector<std::vector<std::vector<double>>> RotationStatic;
    std::vector<std::vector<std::vector<double>>> ShearForceAbscissaStatic;
    std::vector<std::vector<std::vector<double>>> ShearForceStatic;
    std::vector<std::vector<std::vector<double>>> DeflectionStatic;

    // [OPT-3] Abscisses SF pré-calculées en O(S×N)
    std::vector<std::vector<std::vector<double>>> ShearForceAbscissa_;
    std::vector<std::vector<std::vector<double>>> buildShearForceAbscissa();

    std::vector<std::vector<double>> Omega_Second_Spans;
    std::vector<std::vector<double>> Omega_Prime_Spans;

    // [OPT-2] Caches moments — calculés une seule fois dans le constructeur
    std::vector<std::vector<double>> leftMomentsCache_;
    std::vector<std::vector<double>> rightMomentsCache_;

    // Calcul des moments (appelé une seule fois pour remplir les caches)
    std::vector<std::vector<double>> leftLoadedSpanSupportMoments();
    std::vector<std::vector<double>> rightLoadedSpanSupportMoments();

    // [OPT-2] Versions avec cache injecté en paramètre
    std::vector<std::vector<double>> LeftSupportMoments(
        int SpanIndex,
        const std::vector<std::vector<double>>& momentGaucheCache);

    std::vector<std::vector<double>> RightSupportMoments(
        int SpanIndex,
        const std::vector<std::vector<double>>& momentDroiteCache);

    std::vector<std::vector<std::vector<double>>> SpanLeftSupportMoments;
    std::vector<std::vector<std::vector<double>>> SpanRightSupportMoments;
    std::vector<std::vector<std::vector<double>>> SpanLeftRight;
};

#endif // __HYP__
