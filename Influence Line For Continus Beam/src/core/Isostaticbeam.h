#pragma once
#ifndef __ISOSTATIC_BEAM__
#define __ISOSTATIC_BEAM__

// =============================================================================
//  IsostaticBeam — Analyse d'une travée isostatique (poutre simple appuyée)
// =============================================================================
//
//  Calcule, pour une unité de charge unitaire déplacée en x, les diagrammes :
//    - moment fléchissant   (BendingMoment)
//    - effort tranchant     (ShearForce)
//    - flèche               (Deflection)
//    - rotation             (Rotation)
//
//  Chaque méthode Eq_*  retourne le vecteur sur tous les nœuds pour un alpha.
//  Chaque méthode sans  préfixe retourne la matrice complète [nœud][alpha].
// =============================================================================

#include <vector>

class IsostaticBeam
{
public:
    double E;                          // Module d'élasticité
    double I;                          // Moment d'inertie
    double L;                          // Longueur de la travée
    double steps;                      // Pas de discrétisation (valeur)

    std::vector<double> nodePositions; // Positions des nœuds
    std::vector<double> Omega_Second;  // Coefficient de flexibilité secondaire
    std::vector<double> Omega_Prime;   // Coefficient de flexibilité primaire

    IsostaticBeam(double E, double I, double L, double steps);

    // Coefficients de flexibilité
    double a;
    double b;
    double c;

    // ── Effort tranchant ──────────────────────────────────────────────────
    std::vector<double>              Eq_ShearForce(double x, bool returnAbscissa);
    std::vector<std::vector<double>> ShearForce();
    std::vector<std::vector<double>> ShearForceAbscissa();

    // ── Moment fléchissant ────────────────────────────────────────────────
    std::vector<double>              Eq_BendingMoment(double x);
    std::vector<std::vector<double>> BendingMoment();

    // ── Flèche ────────────────────────────────────────────────────────────
    std::vector<double>              Eq_Deflection(double x);
    std::vector<std::vector<double>> Deflection();

    // ── Rotation ──────────────────────────────────────────────────────────
    std::vector<double>              Eq_Rotation(double x);
    std::vector<std::vector<double>> Rotation();
};

#endif // __ISOSTATIC_BEAM__