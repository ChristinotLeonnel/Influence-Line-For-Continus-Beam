#include "Output.h"
#include "Input.h"
#include "Loading.h"

#include <future>

// =============================================================================
//  Output::Output  —  Pipeline d'export v5  (100% JSON)
// =============================================================================
//
//  Tous les résultats sont en JSON — aucune dépendance externe.
//
//  Phase 1  │ Calcul BM, SF, Def, Rot                              [async ×4]
//  Phase 2a │ Valeurs critiques → 03_Critical_Values/              [async ×5]
//  Phase 2b │ Lignes d'influence → 02_Influence_Lines/             [async ×8]
//            │   bending_moment.json  shear_force.json
//            │   deflection.json      rotation.json
//            │   support_moment.json  abscissa.json
//            │   shear_abscissa.json  node_lengths.json
//  Phase 3a │ Enveloppes globales → 04_Load_Envelopes/Global/      [async ×4]
//  Phase 3b │ Section critique   → 04_Load_Envelopes/Critical_Section/ [async ×4]
// =============================================================================

Output::Output(std::vector<double>& E,
               std::vector<double>& I,
               std::vector<double>& L,
               double& steps,
               std::filesystem::path root)
    : Hyperstatique(E, I, L, steps)
{
    ProjectPaths P(std::move(root));
    P.createAll();

    // =========================================================================
    //  Phase 1 — Calcul des grandeurs structurelles
    // =========================================================================

    auto futureBM  = std::async(std::launch::async, [&] { return BendingMoments(); });
    auto futureSF  = std::async(std::launch::async, [&] { return ShearForce();     });
    auto futureDef = std::async(std::launch::async, [&] { return Deflection();     });
    auto futureRot = std::async(std::launch::async, [&] { return Rotation();       });

    const auto BM  = futureBM .get();
    const auto SF  = futureSF .get();
    const auto Def = futureDef.get();
    const auto Rot = futureRot.get();

    const auto X   = pointsXCoordinates(SpanNodePositions);
    const auto X_T = ShearForce(true);

    std::vector<double> NodeLengths;
    NodeLengths.reserve(L_spans.size() + 1);
    double cumul = 0.0;
    for (const double span : L_spans) { NodeLengths.push_back(cumul); cumul += span; }
    NodeLengths.push_back(cumul);

    // =========================================================================
    //  Phase 2a — Valeurs critiques → 03_Critical_Values/
    // =========================================================================

    auto writeMax = [&](const auto& data, const std::string& file, auto& outPos) {
        outPos = findMaxAbsoluteValue3D(data);
        json j;
        maximum_delivery(outPos, j, P.critical_values, file);
    };

    auto fMaxBM  = std::async(std::launch::async, [&] { writeMax(BM,  "bending_moment.json", BendingMomentMaxPositions); });
    auto fMaxSF  = std::async(std::launch::async, [&] { writeMax(SF,  "shear_force.json",    ShearForceMaxPositions);    });
    auto fMaxDef = std::async(std::launch::async, [&] { writeMax(Def, "deflection.json",     DeflectionMaxPositions);    });
    auto fMaxRot = std::async(std::launch::async, [&] { writeMax(Rot, "rotation.json",       RotationMaxPositions);      });
    auto fMaxSM  = std::async(std::launch::async, [&] {
        SupportMomentMaxPositions = findMaxAbsoluteValue2D(SupportMoment);
        json j;
        j["span"]  = SupportMomentMaxPositions.i;
        j["alpha"] = SupportMomentMaxPositions.j;
        j["value"] = SupportMomentMaxPositions.val;
        delivery(j, P.critical_values, "support_moment.json");
    });

    fMaxBM.wait(); fMaxSF.wait(); fMaxDef.wait(); fMaxRot.wait(); fMaxSM.wait();

    // =========================================================================
    //  Phase 2b — Lignes d'influence → 02_Influence_Lines/  (parallèle)
    // =========================================================================

    // Résumé du modèle structural
    auto fModel = std::async(std::launch::async, [&] {
        json model;
        model["spans"]         = L_spans;
        model["young_modulus"] = E_spans;
        model["inertia"]       = I_spans;
		model["step"]          = steps;
        model["node_lengths"]  = NodeLengths;
        model["n_spans"]       = L_spans.size();
        model["n_total_nodes"] = X.size();
        delivery(model, P.input, "structural_model.json");
    });

    // Tenseurs des lignes d'influence (chacun dans son propre fichier JSON)
    auto fBM   = std::async(std::launch::async, [&] { delivery(BM,           P.influence_lines, "bending_moment.json");  });
    auto fSF   = std::async(std::launch::async, [&] { delivery(SF,           P.influence_lines, "shear_force.json");     });
    auto fDef  = std::async(std::launch::async, [&] { delivery(Def,          P.influence_lines, "deflection.json");      });
    auto fRot  = std::async(std::launch::async, [&] { delivery(Rot,          P.influence_lines, "rotation.json");        });
    auto fSM   = std::async(std::launch::async, [&] { delivery(SupportMoment,P.influence_lines, "support_moment.json");  });
    auto fX    = std::async(std::launch::async, [&] { delivery(X,            P.influence_lines, "abscissa.json");        });
    auto fXT   = std::async(std::launch::async, [&] { delivery(X_T,          P.influence_lines, "shear_abscissa.json");  });
    auto fNL   = std::async(std::launch::async, [&] { delivery(NodeLengths,  P.influence_lines, "node_lengths.json");    });

    fModel.wait();
    fBM.wait(); fSF.wait(); fDef.wait(); fRot.wait();
    fSM.wait(); fX.wait();  fXT.wait();  fNL.wait();

    // =========================================================================
    //  Phase 3a — Enveloppes globales → 04_Load_Envelopes/Global/
    // =========================================================================

    auto computeLoading = [&](const auto& data, const std::string& file) {
        Loading loading(data, X, SpanNodePositions);
        json j;
        loading_delivery(loading.Rectangular_load, j, P.env_global_dist,     file);
        loading_delivery(loading.Point_load,        j, P.env_global_point,    file);
        loading_delivery(loading.Combined_load,     j, P.env_global_combined, file);
    };

    auto fGloBM  = std::async(std::launch::async, [&] { computeLoading(BM,  "bending_moment.json"); });
    auto fGloSF  = std::async(std::launch::async, [&] { computeLoading(SF,  "shear_force.json");    });
    auto fGloDef = std::async(std::launch::async, [&] { computeLoading(Def, "deflection.json");     });
    auto fGloRot = std::async(std::launch::async, [&] { computeLoading(Rot, "rotation.json");       });

    fGloBM.wait(); fGloSF.wait(); fGloDef.wait(); fGloRot.wait();

    // =========================================================================
    //  Phase 3b — Section critique → 04_Load_Envelopes/Critical_Section/
    // =========================================================================

    auto computeLoadingAt = [&](const auto& data, const Position3D& pos,
                                const std::string& file)
    {
        Loading loading(data, X, SpanNodePositions);

        // Point load
        load_delivery point_result;
        {
            std::vector<double> vals;
            for (size_t sec = 0; sec < SpanNodePositions[pos.i].size(); ++sec) {
                double v = 0.0;
                for (auto& k : loading.Point_LOAD)
                    v += loading.PluralPointLoad(k.Intensity, k.Length, pos.i, sec).value;
                vals.push_back(v);
            }
            double mv = MaxValueInVector(vals);
            point_result.span          = pos.i;
            point_result.section       = Indice_of(vals, mv);
            point_result.maximum_value = mv;
            std::map<std::string, std::map<std::string, double>> TT;
            for (auto& k : loading.Point_LOAD) {
                auto lo = loading.PluralPointLoad(k.Intensity, k.Length, pos.i, point_result.section);
                TT[k.name] = { {"alpha",(double)lo.max_position}, {"value",lo.value},
                               {"Position", X[lo.max_position] + k.Length[0]} };
            }
            point_result.load = TT;
        }

        // Distributed load
        load_delivery rect_result;
        {
            std::vector<double> vals;
            for (size_t sec = 0; sec < SpanNodePositions[pos.i].size(); ++sec) {
                double v = 0.0;
                for (auto& k : loading.Rectangulare_LOAD)
                    v += loading.PluralRectangularLoad(k.Intensity, k.Length, pos.i, sec).value;
                vals.push_back(v);
            }
            double mv = MaxValueInVector(vals);
            rect_result.span          = pos.i;
            rect_result.section       = Indice_of(vals, mv);
            rect_result.maximum_value = mv;
            std::map<std::string, std::map<std::string, double>> TT;
            for (auto& k : loading.Rectangulare_LOAD) {
                auto lo = loading.PluralRectangularLoad(k.Intensity, k.Length, pos.i, rect_result.section);
                TT[k.name] = { {"alpha",(double)lo.max_position}, {"value",lo.value},
                               {"Position", X[lo.max_position] + k.Length[0]} };
            }
            rect_result.load = TT;
        }

        // Combined load
        load_delivery combined_result;
        {
            std::vector<double> vals, positions;
            std::vector<std::map<std::string, std::map<std::string, double>>> maps;
            for (size_t sec = 0; sec < SpanNodePositions[pos.i].size(); ++sec) {
                auto lo = loading.CombinedLoad(pos.i, sec);
                vals.push_back(lo.value);
                positions.push_back(lo.position);
                maps.push_back(lo.Addition);
            }
            double mv = MaxValueInVector(vals);
            combined_result.span          = pos.i;
            combined_result.section       = Indice_of(vals, mv);
            combined_result.maximum_value = mv;
            combined_result.position      = positions[combined_result.section];
            combined_result.load          = maps[combined_result.section];
        }

        json j;
        loading_delivery(rect_result,     j, P.env_critical_dist,     file);
        loading_delivery(point_result,    j, P.env_critical_point,    file);
        loading_delivery(combined_result, j, P.env_critical_combined, file);
    };

    auto fCritBM  = std::async(std::launch::async, [&] { computeLoadingAt(BM,  BendingMomentMaxPositions, "bending_moment.json"); });
    auto fCritSF  = std::async(std::launch::async, [&] { computeLoadingAt(SF,  ShearForceMaxPositions,    "shear_force.json");    });
    auto fCritDef = std::async(std::launch::async, [&] { computeLoadingAt(Def, DeflectionMaxPositions,    "deflection.json");     });
    auto fCritRot = std::async(std::launch::async, [&] { computeLoadingAt(Rot, RotationMaxPositions,      "rotation.json");       });

    fCritBM.wait(); fCritSF.wait(); fCritDef.wait(); fCritRot.wait();

    std::cout << "Output generated successfully in: " << P.root << "\n";
}
