// =============================================================================
//  ex01_isostatique.cpp — Ligne d'influence : poutre ISOSTATIQUE simple
// =============================================================================
//
//  Cas  : poutre bi-articulée de 10 m, section rectangulaire (béton 30 GPa).
//  Objet: calculer les lignes d'influence de M, V, w, θ pour la section
//         centrale (x = L/2) et afficher les valeurs extrêmes.
//
//  Build:
//      cmake -B build -DTSARALOHA_BUILD_EXAMPLES=ON
//      cmake --build build --config Release
//      ./build/LIPoutreContinue/examples/Release/ex01_isostatique   (Windows)
//      ./build/LIPoutreContinue/examples/ex01_isostatique           (Linux)
// =============================================================================

#include <LIPoutreContinue/StructuralAnalysis.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

// Retourne la valeur de |max| d'un vecteur (valeur absolue la plus grande).
static double absMax(const std::vector<double>& v)
{
    double m = 0.0;
    for (double x : v)
        if (std::abs(x) > std::abs(m)) m = x;
    return m;
}

int main()
{
    std::cout << std::fixed << std::setprecision(4);

    // ── 1. Propriétés physiques ──────────────────────────────────────────────
    const double E     = 30e9;                    // Module de Young béton [Pa]
    const double b     = 0.30, h = 0.60;
    const double I     = b * h * h * h / 12.0;   // Inertie rectangulaire [m⁴]
    const double L     = 10.0;                    // Longueur de la travée [m]
    const double steps = 0.25;                    // Pas de discrétisation [m]

    std::cout << "=== Exemple 1 - Poutre isostatique ===\n"
              << "  E = " << E / 1e9 << " GPa   I = " << I << " m4\n"
              << "  L = " << L << " m   steps = " << steps << " m\n\n";

    // ── 2. Instanciation et calcul ──────────────────────────────────────────
    Isostatique beam(E, I, L, steps);

    const double x_mid = L / 2.0;   // Section étudiée : mi-travée

    // Lignes d'influence pour la section centrale
    std::vector<double> LI_M  = beam.Eq_BendingMoment(x_mid);
    std::vector<double> LI_w  = beam.Eq_Deflection(x_mid);
    std::vector<double> LI_th = beam.Eq_Rotation(x_mid);

    // Effort tranchant : returnAbscissa=false -> valeurs ; true -> abscisses
    std::vector<double> LI_V   = beam.Eq_ShearForce(x_mid, false);
    std::vector<double> absc_V = beam.Eq_ShearForce(x_mid, true);

    // ── 3. Affichage des résultats ──────────────────────────────────────────
    std::cout << "Positions de la charge unité (alpha) :\n"
              << "  Nombre de points : " << beam.nodePositions.size() << "\n"
              << "  Premier : " << beam.nodePositions.front()
              << " m   Dernier : " << beam.nodePositions.back() << " m\n\n";

    std::cout << "Ligne d'influence -- Moment flechissant M (x = " << x_mid << " m) :\n"
              << "  Max |M|  = " << absMax(LI_M)  << " kN.m/kN\n"
              << "  Taille vecteur : " << LI_M.size() << "\n\n";

    std::cout << "Ligne d'influence -- Effort tranchant V (x = " << x_mid << " m) :\n"
              << "  Max |V|  = " << absMax(LI_V)  << " kN/kN\n"
              << "  Taille vecteur : " << LI_V.size()
              << "   (abscisses propres : " << absc_V.size() << ")\n\n";

    std::cout << "Ligne d'influence -- Fleche w (x = " << x_mid << " m) :\n"
              << "  Max |w|  = " << absMax(LI_w)  << " m/kN\n\n";

    std::cout << "Ligne d'influence -- Rotation theta (x = " << x_mid << " m) :\n"
              << "  Max |theta| = " << absMax(LI_th) << " rad/kN\n\n";

    // ── 4. Matrice complète — toutes sections x toutes positions ─────────────
    // (utile pour construire une enveloppe sans passer par Output)
    auto BM_all = beam.BendingMoment();   // [section][alpha]
    auto V_all  = beam.ShearForce();
    auto w_all  = beam.Deflection();

    std::cout << "Matrices completes (toutes sections x toutes positions alpha) :\n"
              << "  BM_all : " << BM_all.size() << " sections x "
              << (BM_all.empty() ? 0 : BM_all[0].size()) << " alphas\n"
              << "  V_all  : " << V_all.size()  << " sections x "
              << (V_all.empty()  ? 0 : V_all[0].size())  << " alphas\n"
              << "  w_all  : " << w_all.size()  << " sections x "
              << (w_all.empty()  ? 0 : w_all[0].size())  << " alphas\n\n";

    std::cout << "Termine.\n";
    return 0;
}
