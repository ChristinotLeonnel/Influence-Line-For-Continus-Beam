#pragma once
#ifndef __BEAM_FORMULAS__
#define __BEAM_FORMULAS__

// =============================================================================
//  BeamFormulas — Formules élémentaires hyperstatiques
// =============================================================================
//
//  Ces fonctions constexpr calculent la contribution d'un moment de rive (m, n)
//  aux diagrammes structuraux d'une travée de longueur l.
//
//  Paramètres communs :
//    m  — moment à l'appui gauche de la travée chargée
//    n  — moment à l'appui droit  de la travée chargée
//    x  — abscisse de la section courante dans la travée
//    l  — longueur de la travée
//    E  — module d'élasticité
//    I  — moment d'inertie
// =============================================================================

// ── Moment fléchissant hyperstatique ─────────────────────────────────────────
//  M(x) = m·(1 - x/l) + n·(x/l)
static constexpr double BeamFormula_BendingMoment(double m, double n,
    double x, double l)
{
    return m * (1.0 - x / l) + n * x / l;
}

// ── Rotation hyperstatique ───────────────────────────────────────────────────
//  θ(x) = -m·(2l²-6lx+3x²)/(6EIl) - n·(l²-3x²)/(6EIl)
static constexpr double BeamFormula_Rotation(double m, double n,
    double x, double l,
    double E, double I)
{
    return -m * (2.0 * (l * l) - 6.0 * l * x + 3.0 * (x * x)) / (6.0 * E * I * l)
        - n * ((l * l) - 3.0 * (x * x)) / (6.0 * E * I * l);
}

// ── Effort tranchant hyperstatique ───────────────────────────────────────────
//  T = (-m + n) / l
static constexpr double BeamFormula_ShearForce(double m, double n, double l)
{
    return (-m + n) / l;
}

// ── Flèche hyperstatique ─────────────────────────────────────────────────────
//  f(x) = -m·x(l-x)(2l-x)/(6EIl) - n·x(l-x)(l+x)/(6EIl)
static constexpr double BeamFormula_Deflection(double m, double n,
    double x, double l,
    double E, double I)
{
    return -m * x * (l - x) * (2.0 * l - x) / (6.0 * E * I * l)
        - n * x * (l - x) * (l + x) / (6.0 * E * I * l);
}

#endif // __BEAM_FORMULAS__