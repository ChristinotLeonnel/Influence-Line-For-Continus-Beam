// =============================================================================
//  ex02_hyperstatique.cpp — Poutre continue HYPERSTATIQUE (3 travées)
// =============================================================================
//
//  Cas  : pont-route 3 travées (12 m + 16 m + 12 m), section rectangulaire
//         variée (travée centrale plus inerte).
//  Objet: calculer les lignes d'influence complètes, afficher les moments sur
//         appuis + moment max absolu sur la structure.
//
//  Build:
//      cmake -B build -DTSARALOHA_BUILD_EXAMPLES=ON
//      cmake --build build --config Release
//      ./build/LIPoutreContinue/examples/Release/ex02_hyperstatique  (Windows)
//      ./build/LIPoutreContinue/examples/ex02_hyperstatique          (Linux)
// =============================================================================

#include <LIPoutreContinue/StructuralAnalysis.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

int main()
{
    std::cout << std::fixed << std::setprecision(4);

    // ── 1. Geometrie et materiaux ────────────────────────────────────────────
    std::vector<double> L_spans{ 12.0, 16.0, 12.0 };   // Longueurs [m]
    const double E_val = 35e9;                           // Beton HP 35 GPa
    const double I_rive = 0.35 * 0.70 * 0.70 * 0.70 / 12.0;  // Travees de rive
    const double I_cent = 0.40 * 0.80 * 0.80 * 0.80 / 12.0;  // Travee centrale

    std::vector<double> E{ E_val, E_val, E_val };
    std::vector<double> I{ I_rive, I_cent, I_rive };
    double steps = 0.5;

    std::cout << "=== Exemple 2 - Poutre hyperstatique 3 travees ===\n"
              << "  Travees : 12 m + 16 m + 12 m\n"
              << "  E = " << E_val / 1e9 << " GPa\n"
              << "  I_rive = " << I_rive << " m4   I_cent = " << I_cent << " m4\n"
              << "  pas = " << steps << " m\n\n";

    // ── 2. Calcul des lignes d'influence ─────────────────────────────────────
    Hyperstatique beam(E, I, L_spans, steps);

    // BM[travee][section][alpha] — dimensions en mémoire
    auto BM  = beam.BendingMoments();
    auto SF  = beam.ShearForce();
    auto Def = beam.Deflection();
    auto Rot = beam.Rotation();
    auto X   = beam.pointsXCoordinates(beam.SpanNodePositions);

    std::cout << "Nombre de travees  : " << beam.number_of_spans << "\n"
              << "Nœuds X totaux     : " << X.size() << "\n"
              << "Longueur totale    : " << X.back() << " m\n\n";

    // ── 3. Moments sur appuis (SupportMoment[appui][alpha]) ──────────────────
    std::cout << "Moments sur appuis :\n";
    for (size_t k = 0; k < beam.SupportMoment.size(); ++k) {
        double mMax = 0.0;
        for (double v : beam.SupportMoment[k])
            if (std::abs(v) > std::abs(mMax)) mMax = v;
        std::cout << "  Appui " << k << "  : M_max = " << mMax << " kN.m\n";
    }
    std::cout << "\n";

    // ── 4. Maximum global du moment flechissant ───────────────────────────────
    Position3D maxBM = findMaxAbsoluteValue3D(BM);
    std::cout << "Maximum global |M| :\n"
              << "  Travee " << maxBM.i
              << "  section " << maxBM.j
              << "  alpha " << maxBM.k
              << "  -> M = " << maxBM.val << " kN.m\n\n";

    // ── 5. Maximum global de la fleche ───────────────────────────────────────
    Position3D maxDef = findMaxAbsoluteValue3D(Def);
    std::cout << "Maximum global |w| :\n"
              << "  Travee " << maxDef.i
              << "  section " << maxDef.j
              << "  alpha " << maxDef.k
              << "  -> w = " << maxDef.val << " m/kN\n\n";

    // ── 6. Affichage de la L.I. de M en mi-travee de la travee centrale ──────
    size_t span_mid = 1;                          // travee centrale (index 1)
    size_t sec_mid  = BM[span_mid].size() / 2;   // section centrale

    std::cout << "L.I. moment flechissant — mi-travee 1 (section " << sec_mid << ") :\n";
    std::cout << "  alpha [m]   M [kN.m/kN]\n";
    for (size_t k = 0; k < BM[span_mid][sec_mid].size(); k += 5) {
        std::cout << "  " << std::setw(8) << X[k]
                  << "   " << BM[span_mid][sec_mid][k] << "\n";
    }

    std::cout << "\nTermine.\n";
    return 0;
}
