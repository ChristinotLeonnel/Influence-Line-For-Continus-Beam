// =============================================================================
//  ex03_output_enveloppes.cpp — Output complet avec enveloppes de charge
// =============================================================================
//
//  Cas  : pont 3 travees (10 m + 14 m + 10 m), convois lourds.
//  Objet: Utiliser la classe Output pour :
//           1) calculer les L.I. en memoire (compute())
//           2) definir des charges reelles (setLoads())
//           3) calculer les enveloppes de charge (computeLoadEnvelopes())
//           4) exporter optionnellement tout en JSON (exportAll())
//
//  Build:
//      cmake -B build -DTSARALOHA_BUILD_EXAMPLES=ON
//      cmake --build build --config Release
//      ./build/LIPoutreContinue/examples/Release/ex03_output_enveloppes  (Windows)
//      ./build/LIPoutreContinue/examples/ex03_output_enveloppes          (Linux)
// =============================================================================

#include <LIPoutreContinue/StructuralAnalysis.h>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <vector>

int main()
{
    std::cout << std::fixed << std::setprecision(4);

    // ── 1. Geometrie et materiaux ────────────────────────────────────────────
    std::vector<double> L_spans{ 10.0, 14.0, 10.0 };
    std::vector<double> E{ 30e9, 30e9, 30e9 };
    std::vector<double> I{ 1.2e-3, 1.8e-3, 1.2e-3 };
    double steps = 0.5;

    // Repertoire de sortie JSON (vide = pas d'export sur disque)
    // Decommenter la ligne suivante pour activer l'export :
    // std::filesystem::path root = "output_ex03";
    std::filesystem::path root = "";

    std::cout << "=== Exemple 3 - Output : calcul complet + enveloppes ===\n"
              << "  Travees : 10 m + 14 m + 10 m\n"
              << "  pas = " << steps << " m\n\n";

    // ── 2. Construction et calcul des L.I. en memoire ────────────────────────
    Output out(E, I, L_spans, steps, root);
    out.compute();   // rien n'est ecrit sur disque

    std::cout << "compute() termine :\n"
              << "  isComputed()  = " << std::boolalpha << out.isComputed() << "\n"
              << "  X.size()      = " << out.X.size() << "\n"
              << "  BM dimensions : " << out.BM.size() << " travees\n";
    for (size_t s = 0; s < out.BM.size(); ++s)
        std::cout << "    Travee " << s << " : " << out.BM[s].size()
                  << " sections x " << (out.BM[s].empty() ? 0 : out.BM[s][0].size())
                  << " alphas\n";
    std::cout << "\n";

    // Maxima globaux disponibles directement apres compute()
    std::cout << "Maxima globaux :\n"
              << "  BM max : travee=" << out.BendingMomentMaxPositions.i
              <<          " sec="    << out.BendingMomentMaxPositions.j
              <<          " alpha="  << out.BendingMomentMaxPositions.k
              <<          " -> "     << out.BendingMomentMaxPositions.val << " kN.m\n"
              << "  SF max : travee=" << out.ShearForceMaxPositions.i
              <<          " sec="    << out.ShearForceMaxPositions.j
              <<          " alpha="  << out.ShearForceMaxPositions.k
              <<          " -> "     << out.ShearForceMaxPositions.val << " kN\n"
              << "  Def max: travee=" << out.DeflectionMaxPositions.i
              <<          " sec="    << out.DeflectionMaxPositions.j
              <<          " alpha="  << out.DeflectionMaxPositions.k
              <<          " -> "     << out.DeflectionMaxPositions.val << " m/kN\n\n";

    // ── 3. Definition des charges ─────────────────────────────────────────────
    //  load { std::vector<double> Intensity; std::vector<double> Length; std::string name; }
    //
    //  Charge ponctuelle — convoi 3 essieux (P=70 kN, Q=130 kN, R=130 kN)
    //    Intensity : forces par essieu [kN]
    //    Length    : distances entre essieux [m] (la derniere valeur n'est
    //                pas utilisee par le moteur, mais doit etre fournie)
    load convoi;
    convoi.Intensity = { 70.0, 130.0, 130.0 };
    convoi.Length    = { 0.0, 1.8, 1.4 };
    convoi.name      = "Convoi-Lourd";

    //  Charge repartie — UDL 12 kN/m sur 6 m depuis le debut de travee
    //    Intensity : intensites de chaque troncon [kN/m]
    //    Length    : [PositionDepart, L_troncon1, ..., L_tronconN] [m]
    load udl;
    udl.Intensity = { 12.0, 20.0 };
    udl.Length    = { 0.0, 6.0, 2.0 };
    udl.name      = "UDL";

    out.setLoads({ convoi }, { udl });

    // ── 4. Calcul des enveloppes de charge en memoire ─────────────────────────
    out.computeLoadEnvelopes();

    std::cout << "computeLoadEnvelopes() termine :\n"
              << "  isLoadEnvelopesComputed() = "
              << out.isLoadEnvelopesComputed() << "\n\n";

    // Enveloppe generale du moment flechissant
    const auto& bmEnv = out.BendingMomentGeneralLoadEnvelope;
    std::cout << "Enveloppe generale BM :\n"
              << "  Charge ponctuelle  : M_max = " << bmEnv.pointLoad.maximum_value
              <<                       " kN.m  (travee=" << bmEnv.pointLoad.span
              <<                       " sec=" << bmEnv.pointLoad.section << ")\n"
              << "  Charge repartie    : M_max = " << bmEnv.rectangularLoad.maximum_value
              <<                       " kN.m  (travee=" << bmEnv.rectangularLoad.span
              <<                       " sec=" << bmEnv.rectangularLoad.section << ")\n"
              << "  Charge combinee    : M_max = " << bmEnv.combinedLoad.maximum_value
              <<                       " kN.m  (travee=" << bmEnv.combinedLoad.span
              <<                       " sec=" << bmEnv.combinedLoad.section << ")\n\n";

    // Enveloppe critique (section la plus sollicitee)
    const auto& bmCrit = out.BendingMomentCriticalLoadEnvelope;
    std::cout << "Enveloppe critique BM :\n"
              << "  Ponctuel  : M = " << bmCrit.pointLoad.maximum_value
              <<                " kN.m  (section=" << bmCrit.pointLoad.section << ")\n"
              << "  Reparti   : M = " << bmCrit.rectangularLoad.maximum_value
              <<                " kN.m  (section=" << bmCrit.rectangularLoad.section << ")\n"
              << "  Combine   : M = " << bmCrit.combinedLoad.maximum_value
              <<                " kN.m  (section=" << bmCrit.combinedLoad.section << ")\n\n";

    // ── 5. Export JSON (optionnel, uniquement si root != "") ──────────────────
    if (!root.empty()) {
        out.exportAll();
        std::cout << "JSON exporte dans : " << std::filesystem::absolute(root) << "\n";
    } else {
        std::cout << "Export JSON desactive (root vide).\n"
                  << "  -> Decommenter la ligne 'root = \"output_ex03\"' pour activer.\n";
    }

    std::cout << "\nTermine.\n";
    return 0;
}
