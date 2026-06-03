#include "Output.h"
#include "Input.h"
#include "Loading.h"

#include <future>
#include <iostream>

// =============================================================================
//  Output::Output  —  Pipeline d'export v6  (100% JSON)
// =============================================================================
//
//  Tous les résultats sont en JSON — aucune dépendance externe.
//
//  Phase 1  │ Calcul BM, SF, Def, Rot, Curvature                  [async ×5]
//  Phase 2a │ Valeurs critiques → 03_Critical_Values/              [async ×5]
//  Phase 2b │ Lignes d'influence → 02_Influence_Lines/             [async ×9]
//            │   bending_moment.json  shear_force.json
//            │   deflection.json      rotation.json
//            │   support_moment.json  curvature.json               [NEW]
//            │   abscissa.json        shear_abscissa.json
//            │   node_lengths.json
//  Phase 3a │ Enveloppes globales → 04_Load_Envelopes/Global/      [async ×4]
//  Phase 3b │ Section critique   → 04_Load_Envelopes/Critical_Section/ [async ×4]
//  Phase 4  │ Paramètres structuraux → 01_Input/                   [async ×4] [NEW]
//            │   beam_geometry.json           flexibility_coefficients.json
//            │   stiffness_distribution.json  span_node_positions.json
//  Phase 5  │ Valeurs critiques signées → 03_Critical_Values/      [async ×5] [NEW]
//            │   {bm,sf,def,rot,curv}_positive.json  *_negative.json
//            │   support_moment_positive.json         support_moment_negative.json
//  Phase 6  │ Extrema par travée → 03_Critical_Values/Per_Span/    [async ×5] [NEW]
//            │   bending_moment.json  shear_force.json  deflection.json
//            │   rotation.json        curvature.json
//  Phase 7  │ Statistiques → 08_Statistics/                        [async ×5] [NEW]
//            │   bending_moment.json  shear_force.json  deflection.json
//            │   rotation.json        curvature.json
//  Phase 8  │ Passages par zéro → 09_Zero_Crossings/               [async ×4] [NEW]
//            │   bending_moment.json  shear_force.json
//            │   rotation.json        curvature.json
//  Phase 9  │ Réactions d'appui → 10_Support_Reactions/             [async ×1] [NEW]
//            │   influence_lines.json  critical_values.json
//            │   statistics.json
//  Phase 10 │ Énergie élastique → 01_Input/                         [async ×1] [NEW]
//            │   elastic_energy.json   [{alpha, x_load, energy}]
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

    // Courbure κ = M / (EI)  [NEW]
    const auto Curv = computeCurvature3D(BM, E_spans, I_spans);

    const auto X   = pointsXCoordinates(SpanNodePositions);
    const auto X_T = ShearForce(true);

    // Abscisses cumulées des appuis
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

    auto fMaxBM  = std::async(std::launch::async, [&] { writeMax(BM,   "bending_moment.json", BendingMomentMaxPositions); });
    auto fMaxSF  = std::async(std::launch::async, [&] { writeMax(SF,   "shear_force.json",    ShearForceMaxPositions);    });
    auto fMaxDef = std::async(std::launch::async, [&] { writeMax(Def,  "deflection.json",     DeflectionMaxPositions);    });
    auto fMaxRot = std::async(std::launch::async, [&] { writeMax(Rot,  "rotation.json",       RotationMaxPositions);      });
    auto fMaxSM  = std::async(std::launch::async, [&] {
        SupportMomentMaxPositions = findMaxAbsoluteValue2D(SupportMoment);
        json j;
        j["span"]  = SupportMomentMaxPositions.i;
        j["alpha"] = SupportMomentMaxPositions.j;
        j["value"] = SupportMomentMaxPositions.val;
        delivery(j, P.critical_values, "support_moment.json");
    });
    // Courbure — valeur absolue max [NEW]
    auto fMaxCurv = std::async(std::launch::async, [&] {
        json j;
        auto pos = findMaxAbsoluteValue3D(Curv);
        j["span"] = pos.i; j["section"] = pos.j; j["alpha"] = pos.k; j["value"] = pos.val;
        delivery(j, P.critical_values, "curvature.json");
    });

    fMaxBM.wait(); fMaxSF.wait(); fMaxDef.wait(); fMaxRot.wait();
    fMaxSM.wait(); fMaxCurv.wait();

    // =========================================================================
    //  Phase 2b — Lignes d'influence → 02_Influence_Lines/
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

    auto fBM   = std::async(std::launch::async, [&] { delivery(BM,           P.influence_lines, "bending_moment.json");  });
    auto fSF   = std::async(std::launch::async, [&] { delivery(SF,           P.influence_lines, "shear_force.json");     });
    auto fDef  = std::async(std::launch::async, [&] { delivery(Def,          P.influence_lines, "deflection.json");      });
    auto fRot  = std::async(std::launch::async, [&] { delivery(Rot,          P.influence_lines, "rotation.json");        });
    auto fSM   = std::async(std::launch::async, [&] { delivery(SupportMoment,P.influence_lines, "support_moment.json");  });
    auto fCurv = std::async(std::launch::async, [&] { delivery(Curv,         P.influence_lines, "curvature.json");       });  // [NEW]
    auto fX    = std::async(std::launch::async, [&] { delivery(X,            P.influence_lines, "abscissa.json");        });
    auto fXT   = std::async(std::launch::async, [&] { delivery(X_T,          P.influence_lines, "shear_abscissa.json");  });
    auto fNL   = std::async(std::launch::async, [&] { delivery(NodeLengths,  P.influence_lines, "node_lengths.json");    });

    fModel.wait();
    fBM.wait(); fSF.wait(); fDef.wait(); fRot.wait();
    fSM.wait(); fCurv.wait(); fX.wait(); fXT.wait(); fNL.wait();

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

    // =========================================================================
    //  Phase 4 — Paramètres structuraux → 01_Input/   [NEW]
    // =========================================================================

    // Géométrie complète de la poutre
    auto fBeamGeom = std::async(std::launch::async, [&] {
        json j;
        j["n_spans"]      = number_of_spans;
        j["total_nodes"]  = total_nodes_;
        j["steps"]        = steps;
        j["L_spans"]      = L_spans;
        j["E_spans"]      = E_spans;
        j["I_spans"]      = I_spans;
        // Rigidité EI par travée
        std::vector<double> EI_spans;
        EI_spans.reserve(E_spans.size());
        for (size_t k = 0; k < E_spans.size() && k < I_spans.size(); ++k)
            EI_spans.push_back(E_spans[k] * I_spans[k]);
        j["EI_spans"]        = EI_spans;
        j["node_lengths"]    = NodeLengths;
        j["total_length"]    = NodeLengths.back();
        delivery(j, P.input, "beam_geometry.json");
    });

    // Coefficients de souplesse de Clapeyron (a, b, c)
    // a[k] = L_k / (3EI_k),  b[k] = L_k / (6EI_k),  c[k] = a[k]
    auto fFlex = std::async(std::launch::async, [&] {
        json j;
        j["a_spans"] = a_spans;   // souplesse droite travée k
        j["b_spans"] = b_spans;   // souplesse croisée travée k
        j["c_spans"] = c_spans;   // souplesse gauche travée k  (= a pour E,I constants)
        j["description"] = {
            {"a", "L/(3EI) — coefficient diagonal gauche"},
            {"b", "L/(6EI) — coefficient extra-diagonal"},
            {"c", "L/(3EI) — coefficient diagonal droit"}
        };
        delivery(j, P.input, "flexibility_coefficients.json");
    });

    // Distribution de rigidité (phi, phi') — coefficients trois moments
    auto fStiff = std::async(std::launch::async, [&] {
        json j;
        j["phy"]       = phy;
        j["phy_prime"] = phy_prime;
        j["description"] = {
            {"phy",       "Coefficient de distribution de rigidité gauche"},
            {"phy_prime", "Coefficient de distribution de rigidité droit"}
        };
        delivery(j, P.input, "stiffness_distribution.json");
    });

    // Positions locales des noeuds par travée
    auto fSpanNodes = std::async(std::launch::async, [&] {
        delivery(SpanNodePositions, P.input, "span_node_positions.json");
    });

    fBeamGeom.wait(); fFlex.wait(); fStiff.wait(); fSpanNodes.wait();

    // =========================================================================
    //  Phase 5 — Valeurs critiques signées → 03_Critical_Values/   [NEW]
    //   *_positive.json  { span, section, alpha, value }  ← max positif
    //   *_negative.json  { span, section, alpha, value }  ← min négatif
    // =========================================================================

    auto fSignBM   = std::async(std::launch::async, [&] {
        signed_delivery(BM,   P.critical_values, "bending_moment");
    });
    auto fSignSF   = std::async(std::launch::async, [&] {
        signed_delivery(SF,   P.critical_values, "shear_force");
    });
    auto fSignDef  = std::async(std::launch::async, [&] {
        signed_delivery(Def,  P.critical_values, "deflection");
    });
    auto fSignRot  = std::async(std::launch::async, [&] {
        signed_delivery(Rot,  P.critical_values, "rotation");
    });
    auto fSignCurv = std::async(std::launch::async, [&] {
        signed_delivery(Curv, P.critical_values, "curvature");
    });
    // Moment d'appui signé (matrice 2D)
    auto fSignSM   = std::async(std::launch::async, [&] {
        signed_delivery_2D(SupportMoment, P.critical_values, "support_moment");
    });

    fSignBM.wait(); fSignSF.wait(); fSignDef.wait();
    fSignRot.wait(); fSignCurv.wait(); fSignSM.wait();

    // =========================================================================
    //  Phase 6 — Extrema par travée → 03_Critical_Values/Per_Span/  [NEW]
    //   [{span, max_absolute:{value,section,alpha},
    //           max_positive:{value,section,alpha},
    //           max_negative:{value,section,alpha}}]
    // =========================================================================

    auto fPsBM   = std::async(std::launch::async, [&] {
        perspan_extrema_delivery(BM,   P.crit_per_span, "bending_moment.json");
    });
    auto fPsSF   = std::async(std::launch::async, [&] {
        perspan_extrema_delivery(SF,   P.crit_per_span, "shear_force.json");
    });
    auto fPsDef  = std::async(std::launch::async, [&] {
        perspan_extrema_delivery(Def,  P.crit_per_span, "deflection.json");
    });
    auto fPsRot  = std::async(std::launch::async, [&] {
        perspan_extrema_delivery(Rot,  P.crit_per_span, "rotation.json");
    });
    auto fPsCurv = std::async(std::launch::async, [&] {
        perspan_extrema_delivery(Curv, P.crit_per_span, "curvature.json");
    });

    fPsBM.wait(); fPsSF.wait(); fPsDef.wait(); fPsRot.wait(); fPsCurv.wait();

    // =========================================================================
    //  Phase 7 — Statistiques → 08_Statistics/                      [NEW]
    //   [{span, min, min_section, min_alpha, max, max_section, max_alpha,
    //     mean, rms, std_dev, count}]
    // =========================================================================

    auto fStatBM   = std::async(std::launch::async, [&] {
        spanstats_delivery(computeSpanStats3D(BM),   P.statistics, "bending_moment.json");
    });
    auto fStatSF   = std::async(std::launch::async, [&] {
        spanstats_delivery(computeSpanStats3D(SF),   P.statistics, "shear_force.json");
    });
    auto fStatDef  = std::async(std::launch::async, [&] {
        spanstats_delivery(computeSpanStats3D(Def),  P.statistics, "deflection.json");
    });
    auto fStatRot  = std::async(std::launch::async, [&] {
        spanstats_delivery(computeSpanStats3D(Rot),  P.statistics, "rotation.json");
    });
    auto fStatCurv = std::async(std::launch::async, [&] {
        spanstats_delivery(computeSpanStats3D(Curv), P.statistics, "curvature.json");
    });

    fStatBM.wait(); fStatSF.wait(); fStatDef.wait(); fStatRot.wait(); fStatCurv.wait();

    // =========================================================================
    //  Phase 8 — Passages par zéro → 09_Zero_Crossings/             [NEW]
    //   [{span, n_crossings, crossings:[{section, alpha_before,
    //                                    alpha_after, x_approx},...]}]
    //   x_approx = position de la charge (m) au passage par zéro (interpolé)
    // =========================================================================

    auto fZCBM   = std::async(std::launch::async, [&] {
        zerocrossings_delivery(findZeroCrossings3D(BM,   X), P.zero_crossings, "bending_moment.json");
    });
    auto fZCSF   = std::async(std::launch::async, [&] {
        zerocrossings_delivery(findZeroCrossings3D(SF,   X), P.zero_crossings, "shear_force.json");
    });
    auto fZCRot  = std::async(std::launch::async, [&] {
        zerocrossings_delivery(findZeroCrossings3D(Rot,  X), P.zero_crossings, "rotation.json");
    });
    auto fZCCurv = std::async(std::launch::async, [&] {
        zerocrossings_delivery(findZeroCrossings3D(Curv, X), P.zero_crossings, "curvature.json");
    });

    fZCBM.wait(); fZCSF.wait(); fZCRot.wait(); fZCCurv.wait();

    // =========================================================================
    //  Phase 9 — Réactions d'appui → 10_Support_Reactions/          [NEW]
    //
    //   influence_lines.json  : matrice R[support][alpha]
    //   critical_values.json  : [{support, x_support, max_value, alpha, x_load,
    //                             max_positive, max_negative}]
    //   statistics.json       : [{support, min, max, mean, rms, std_dev}]
    // =========================================================================

    auto fReactions = std::async(std::launch::async, [&] {
        // Calcul des réactions
        const auto R = computeSupportReactions(SupportMoment, L_spans, NodeLengths, X);

        // 1) Lignes d'influence complètes
        delivery(R, P.support_reactions, "influence_lines.json");

        // 2) Valeurs critiques par appui
        json crit = json::array();
        for (size_t j = 0; j < R.size(); ++j) {
            if (R[j].empty()) continue;

            // Max absolu
            double max_abs = 0.0;
            size_t idx_abs = 0;
            // Max positif
            double max_pos = std::numeric_limits<double>::lowest();
            size_t idx_pos = 0;
            // Min négatif
            double max_neg = std::numeric_limits<double>::max();
            size_t idx_neg = 0;

            for (size_t alpha = 0; alpha < R[j].size(); ++alpha) {
                const double v = R[j][alpha];
                if (std::abs(v) > std::abs(max_abs)) { max_abs = v; idx_abs = alpha; }
                if (v > max_pos) { max_pos = v; idx_pos = alpha; }
                if (v < max_neg) { max_neg = v; idx_neg = alpha; }
            }

            const double x_sup = (j < NodeLengths.size()) ? NodeLengths[j] : 0.0;
            json sj;
            sj["support"]          = j;
            sj["x_support"]        = x_sup;
            sj["max_absolute"]     = {
                {"value", max_abs},
                {"alpha", idx_abs},
                {"x_load", (idx_abs < X.size() ? X[idx_abs] : 0.0)}
            };
            sj["max_positive"]     = {
                {"value", max_pos},
                {"alpha", idx_pos},
                {"x_load", (idx_pos < X.size() ? X[idx_pos] : 0.0)}
            };
            sj["max_negative"]     = {
                {"value", max_neg},
                {"alpha", idx_neg},
                {"x_load", (idx_neg < X.size() ? X[idx_neg] : 0.0)}
            };
            crit.push_back(sj);
        }
        delivery(crit, P.support_reactions, "critical_values.json");

        // 3) Statistiques par appui
        json stats = json::array();
        for (size_t j = 0; j < R.size(); ++j) {
            if (R[j].empty()) continue;
            double sum = 0.0, sum_sq = 0.0;
            double vmin = std::numeric_limits<double>::max();
            double vmax = std::numeric_limits<double>::lowest();
            for (double v : R[j]) {
                sum    += v;  sum_sq += v * v;
                if (v < vmin) vmin = v;
                if (v > vmax) vmax = v;
            }
            const double n    = static_cast<double>(R[j].size());
            const double mean = sum / n;
            double var = 0.0;
            for (double v : R[j]) var += (v - mean) * (v - mean);
            json sj;
            sj["support"] = j;
            sj["x_support"] = (j < NodeLengths.size()) ? NodeLengths[j] : 0.0;
            sj["min"]     = vmin;
            sj["max"]     = vmax;
            sj["mean"]    = mean;
            sj["rms"]     = std::sqrt(sum_sq / n);
            sj["std_dev"] = (R[j].size() > 1)
                            ? std::sqrt(var / static_cast<double>(R[j].size() - 1))
                            : 0.0;
            stats.push_back(sj);
        }
        delivery(stats, P.support_reactions, "statistics.json");
    });

    fReactions.wait();

    // =========================================================================
    //  Phase 10 — Énergie élastique → 01_Input/                      [NEW]
    //
    //   elastic_energy.json : [{alpha, x_load, energy_kNm},
    //                          "max":{alpha, x_load, value},
    //                          "min":{alpha, x_load, value}]
    //
    //   U(α) = Σ_k ∫₀^{L_k} M²_k(x,α) / (2·EI_k) dx
    //   Identifie la position de charge la plus critique en énergie totale.
    // =========================================================================

    auto fEnergy = std::async(std::launch::async, [&] {
        const auto energy = computeElasticEnergy(BM, E_spans, I_spans,
                                                  SpanNodePositions, X.size());

        // Tableau détaillé [alpha]
        json arr = json::array();
        for (size_t alpha = 0; alpha < energy.size(); ++alpha) {
            json e;
            e["alpha"]     = alpha;
            e["x_load"]    = (alpha < X.size() ? X[alpha] : 0.0);
            e["energy"]    = energy[alpha];
            arr.push_back(e);
        }

        // Max et min
        double e_max = std::numeric_limits<double>::lowest();
        double e_min = std::numeric_limits<double>::max();
        size_t idx_max = 0, idx_min = 0;
        for (size_t a = 0; a < energy.size(); ++a) {
            if (energy[a] > e_max) { e_max = energy[a]; idx_max = a; }
            if (energy[a] < e_min) { e_min = energy[a]; idx_min = a; }
        }

        json j;
        j["data"]    = arr;
        j["max"]     = {{"alpha", idx_max},
                        {"x_load", (idx_max < X.size() ? X[idx_max] : 0.0)},
                        {"value", e_max}};
        j["min"]     = {{"alpha", idx_min},
                        {"x_load", (idx_min < X.size() ? X[idx_min] : 0.0)},
                        {"value", e_min}};
        j["unit"]    = "kN.m (charge unitaire kN)";
        delivery(j, P.input, "elastic_energy.json");
    });

    fEnergy.wait();

    std::cout << "  [ OK  ]  Output written to: " << P.root.string() << '\n';
}
