#include "LIPoutreContinue/Output.h"
#include "LIPoutreContinue/loading/Input.h"
#include "LIPoutreContinue/loading/Loading.h"

#include <future>
#include <iostream>
#include <stdexcept>

// =============================================================================
//  Output::Output — n'initialise que la géométrie et les chemins.
//  Aucun calcul, aucune écriture disque.
// =============================================================================
Output::Output(std::vector<double>& E,
               std::vector<double>& I,
               std::vector<double>& L,
               double& steps,
               std::filesystem::path root)
    : Hyperstatique(E, I, L, steps)
    , Paths(std::move(root))
{
}

// =============================================================================
//  compute() — Phase 1 : calcul structurel EN MÉMOIRE uniquement.
//  Ni fichier créé, ni dossier créé. Appel explicite requis.
// =============================================================================
void Output::compute(bool force)
{
    if (computed_ && !force) return;

    auto futureBM  = std::async(std::launch::async, [&] { return BendingMoments(); });
    auto futureSF  = std::async(std::launch::async, [&] { return ShearForce();     });
    auto futureDef = std::async(std::launch::async, [&] { return Deflection();     });
    auto futureRot = std::async(std::launch::async, [&] { return Rotation();       });

    BM  = futureBM .get();
    SF  = futureSF .get();
    Def = futureDef.get();
    Rot = futureRot.get();

    X                      = pointsXCoordinates(SpanNodePositions);
    ShearForceAllAbscissa  = ShearForce(true);

    NodeLengths.clear();
    NodeLengths.reserve(L_spans.size() + 1);
    double cumul = 0.0;
    for (const double span : L_spans) { NodeLengths.push_back(cumul); cumul += span; }
    NodeLengths.push_back(cumul);

    // Maxima — calcul pur en mémoire, ne touche pas le disque.
    auto futMaxBM  = std::async(std::launch::async, [&] { return findMaxAbsoluteValue3D(BM);  });
    auto futMaxSF  = std::async(std::launch::async, [&] { return findMaxAbsoluteValue3D(SF);  });
    auto futMaxDef = std::async(std::launch::async, [&] { return findMaxAbsoluteValue3D(Def); });
    auto futMaxRot = std::async(std::launch::async, [&] { return findMaxAbsoluteValue3D(Rot); });

    BendingMomentMaxPositions = futMaxBM.get();
    ShearForceMaxPositions    = futMaxSF.get();
    DeflectionMaxPositions    = futMaxDef.get();
    RotationMaxPositions      = futMaxRot.get();
    SupportMomentMaxPositions = findMaxAbsoluteValue2D(SupportMoment);

    computed_ = true;
}

// =============================================================================
//  exportCriticalValues() — écrit 03_Critical_Values/  (appel explicite)
// =============================================================================
void Output::exportCriticalValues()
{
    compute(); // no-op si déjà calculé
    Paths.createAll();

    auto writeMax = [&](const auto& data, const std::string& file, const auto& pos) {
        json j;
        maximum_delivery(pos, j, Paths.critical_values, file);
        (void)data;
    };

    auto f1 = std::async(std::launch::async, [&] { writeMax(BM,  "bending_moment.json", BendingMomentMaxPositions); });
    auto f2 = std::async(std::launch::async, [&] { writeMax(SF,  "shear_force.json",    ShearForceMaxPositions);    });
    auto f3 = std::async(std::launch::async, [&] { writeMax(Def, "deflection.json",     DeflectionMaxPositions);    });
    auto f4 = std::async(std::launch::async, [&] { writeMax(Rot, "rotation.json",       RotationMaxPositions);      });
    auto f5 = std::async(std::launch::async, [&] {
        json j;
        j["span"]  = SupportMomentMaxPositions.i;
        j["alpha"] = SupportMomentMaxPositions.j;
        j["value"] = SupportMomentMaxPositions.val;
        delivery(j, Paths.critical_values, "support_moment.json");
    });
    f1.wait(); f2.wait(); f3.wait(); f4.wait(); f5.wait();
}

// =============================================================================
//  exportInfluenceLines() — écrit 01_Input/ + 02_Influence_Lines/  (explicite)
// =============================================================================
void Output::exportInfluenceLines()
{
    compute();
    Paths.createAll();

    auto fModel = std::async(std::launch::async, [&] {
        json model;
        model["spans"]         = L_spans;
        model["young_modulus"] = E_spans;
        model["inertia"]       = I_spans;
        model["step"]          = steps;
        model["node_lengths"]  = NodeLengths;
        model["n_spans"]       = L_spans.size();
        model["n_total_nodes"] = X.size();
        delivery(model, Paths.input, "structural_model.json");
    });

    auto fBM  = std::async(std::launch::async, [&] { delivery(BM,                     Paths.influence_lines, "bending_moment.json"); });
    auto fSF  = std::async(std::launch::async, [&] { delivery(SF,                     Paths.influence_lines, "shear_force.json");    });
    auto fDef = std::async(std::launch::async, [&] { delivery(Def,                    Paths.influence_lines, "deflection.json");     });
    auto fRot = std::async(std::launch::async, [&] { delivery(Rot,                    Paths.influence_lines, "rotation.json");       });
    auto fSM  = std::async(std::launch::async, [&] { delivery(SupportMoment,          Paths.influence_lines, "support_moment.json"); });
    auto fX   = std::async(std::launch::async, [&] { delivery(X,                      Paths.influence_lines, "abscissa.json");       });
    auto fXT  = std::async(std::launch::async, [&] { delivery(ShearForceAllAbscissa,  Paths.influence_lines, "shear_abscissa.json"); });
    auto fNL  = std::async(std::launch::async, [&] { delivery(NodeLengths,            Paths.influence_lines, "node_lengths.json");   });

    fModel.wait();
    fBM.wait(); fSF.wait(); fDef.wait(); fRot.wait();
    fSM.wait(); fX.wait();  fXT.wait();  fNL.wait();
}

// =============================================================================
//  setLoads() — fournit les charges directement, aucun accès disque.
//  Obligatoire avant exportLoadEnvelopes()/exportAll().
// =============================================================================
void Output::setLoads(const std::vector<load>& pointLoads,
                       const std::vector<load>& distribLoads)
{
    userPointLoads_   = pointLoads;
    userDistribLoads_ = distribLoads;
    hasUserLoads_      = true;
}

// =============================================================================
//  computeLoadEnvelopes() — Phase « Loading » : calcul EN MÉMOIRE uniquement.
//  Tout le calcul vit dans Loading (Loading.h/.cpp) ; ici on se contente
//  d'appeler Loading puis de RÉCOLTER ses résultats dans les membres publics
//  de Output (BendingMomentGeneralLoadEnvelope, BendingMomentCriticalLoadEnvelope,
//  ...). Ni fichier ni dossier créé. Nécessite que setLoads() ait été
//  appelé au préalable.
// =============================================================================
void Output::computeLoadEnvelopes(bool force)
{
    if (!hasUserLoads_)
        throw std::runtime_error(
            "Output::computeLoadEnvelopes: aucune charge fournie — appelez "
            "setLoads(point_loads, distrib_loads) avant (une des deux "
            "listes peut être vide, mais setLoads() doit être appelée). "
            "Exemple (Python) : out.set_loads(point_loads=[...], "
            "distrib_loads=[...]) puis out.compute_load_envelopes().");

    if (loadEnvelopesComputed_ && !force) return;

    compute(); // no-op si déjà calculé — BM/SF/Def/Rot doivent exister

    // ── Enveloppes globales : Loading calcule, on récolte le résultat ──────
    auto envelopeFor = [&](const auto& data) -> LoadEnvelope {
        Loading loading(data, X, SpanNodePositions, L_spans, userPointLoads_, userDistribLoads_);
        return LoadEnvelope{ loading.Point_load, loading.Rectangular_load, loading.Combined_load };
    };

    auto fBM  = std::async(std::launch::async, [&] { return envelopeFor(BM);  });
    auto fSF  = std::async(std::launch::async, [&] { return envelopeFor(SF);  });
    auto fDef = std::async(std::launch::async, [&] { return envelopeFor(Def); });
    auto fRot = std::async(std::launch::async, [&] { return envelopeFor(Rot); });

    BendingMomentGeneralLoadEnvelope = fBM.get();
    ShearForceGeneralLoadEnvelope    = fSF.get();
    DeflectionGeneralLoadEnvelope    = fDef.get();
    RotationGeneralLoadEnvelope      = fRot.get();

    // ── Enveloppes à la section critique : Loading::computeCriticalSection()
    //    fait tout le calcul, on récolte le résultat. ─────────────────────
    auto criticalFor = [&](const auto& data, const Position3D& pos) -> LoadEnvelope {
        Loading loading(data, X, SpanNodePositions, L_spans, userPointLoads_, userDistribLoads_);
        auto result = loading.computeCriticalSection(pos.i);
        return LoadEnvelope{ result.point, result.rect, result.combined };
    };

    auto fCritBM  = std::async(std::launch::async, [&] { return criticalFor(BM,  BendingMomentMaxPositions); });
    auto fCritSF  = std::async(std::launch::async, [&] { return criticalFor(SF,  ShearForceMaxPositions);    });
    auto fCritDef = std::async(std::launch::async, [&] { return criticalFor(Def, DeflectionMaxPositions);    });
    auto fCritRot = std::async(std::launch::async, [&] { return criticalFor(Rot, RotationMaxPositions);      });

    BendingMomentCriticalLoadEnvelope = fCritBM.get();
    ShearForceCriticalLoadEnvelope    = fCritSF.get();
    DeflectionCriticalLoadEnvelope    = fCritDef.get();
    RotationCriticalLoadEnvelope      = fCritRot.get();

    loadEnvelopesComputed_ = true;
}

// =============================================================================
//  exportLoadEnvelopes() — écrit 04_Load_Envelopes/ (Global + Critical_Section)
//  (appel explicite ; les charges sont toujours celles fournies via setLoads())
//
//  Aucun calcul ici : computeLoadEnvelopes() a déjà tout récolté dans les
//  membres publics de Output. On se contente d'écrire ce qui est déjà en
//  mémoire.
// =============================================================================
void Output::exportLoadEnvelopes()
{
    computeLoadEnvelopes(); // no-op si déjà calculé
    Paths.createAll();

    auto writeEnvelope = [&](const LoadEnvelope& env, const std::string& file,
                              const std::filesystem::path& dirPoint,
                              const std::filesystem::path& dirRect,
                              const std::filesystem::path& dirCombined) {
        json j;
        loading_delivery(env.rectangularLoad, j, dirRect,     file);
        loading_delivery(env.pointLoad,       j, dirPoint,    file);
        loading_delivery(env.combinedLoad,    j, dirCombined, file);
    };

    // ── Phase 3a — Enveloppes globales ──────────────────────────────────────
    auto fGloBM  = std::async(std::launch::async, [&] { writeEnvelope(BendingMomentGeneralLoadEnvelope, "bending_moment.json", Paths.env_global_point, Paths.env_global_dist, Paths.env_global_combined); });
    auto fGloSF  = std::async(std::launch::async, [&] { writeEnvelope(ShearForceGeneralLoadEnvelope,    "shear_force.json",    Paths.env_global_point, Paths.env_global_dist, Paths.env_global_combined); });
    auto fGloDef = std::async(std::launch::async, [&] { writeEnvelope(DeflectionGeneralLoadEnvelope,    "deflection.json",     Paths.env_global_point, Paths.env_global_dist, Paths.env_global_combined); });
    auto fGloRot = std::async(std::launch::async, [&] { writeEnvelope(RotationGeneralLoadEnvelope,      "rotation.json",       Paths.env_global_point, Paths.env_global_dist, Paths.env_global_combined); });
    fGloBM.wait(); fGloSF.wait(); fGloDef.wait(); fGloRot.wait();

    // ── Phase 3b — Section critique ─────────────────────────────────────────
    auto fCritBM  = std::async(std::launch::async, [&] { writeEnvelope(BendingMomentCriticalLoadEnvelope, "bending_moment.json", Paths.env_critical_point, Paths.env_critical_dist, Paths.env_critical_combined); });
    auto fCritSF  = std::async(std::launch::async, [&] { writeEnvelope(ShearForceCriticalLoadEnvelope,    "shear_force.json",    Paths.env_critical_point, Paths.env_critical_dist, Paths.env_critical_combined); });
    auto fCritDef = std::async(std::launch::async, [&] { writeEnvelope(DeflectionCriticalLoadEnvelope,    "deflection.json",     Paths.env_critical_point, Paths.env_critical_dist, Paths.env_critical_combined); });
    auto fCritRot = std::async(std::launch::async, [&] { writeEnvelope(RotationCriticalLoadEnvelope,      "rotation.json",       Paths.env_critical_point, Paths.env_critical_dist, Paths.env_critical_combined); });
    fCritBM.wait(); fCritSF.wait(); fCritDef.wait(); fCritRot.wait();
}

// =============================================================================
//  exportAll() — la seule méthode qui reproduit l'ancien comportement
//  "tout écrire d'un coup" — mais ELLE DOIT ÊTRE APPELÉE EXPLICITEMENT.
//  Nécessite que setLoads() ait été appelé au préalable (voir exportLoadEnvelopes).
// =============================================================================
void Output::exportAll()
{
    exportInfluenceLines();
    exportLoadEnvelopes();
    exportCriticalValues();
    std::cout << "Output generated successfully in: " << Paths.root << "\n";
}
