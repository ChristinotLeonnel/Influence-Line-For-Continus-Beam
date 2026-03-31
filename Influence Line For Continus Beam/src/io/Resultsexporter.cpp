#include "ResultsExporter.h"
#include "io/StructuralConfig.h"
#include "io/LoadEnvelope.h"

#include <future>

// =============================================================================
//  ResultsExporter::ResultsExporter  —  Pipeline d'export (100% JSON)
// =============================================================================
//
//  Phase 1  │ Calcul BM, SF, Def, Rot                              [async ×4]
//  Phase 2a │ Valeurs critiques → 03_Critical_Values/              [async ×5]
//  Phase 2b │ Lignes d'influence → 02_Influence_Lines/             [async ×8]
//  Phase 3a │ Enveloppes globales → 04_Load_Envelopes/Global/      [async ×4]
//  Phase 3b │ Section critique   → 04_Load_Envelopes/Critical_Section/ [async ×4]
// =============================================================================

ResultsExporter::ResultsExporter(std::vector<double>& E,
    std::vector<double>& I,
    std::vector<double>& L,
    double& steps,
    std::filesystem::path outputRoot)
    : ContinuousBeam(E, I, L, steps)
{
    ProjectPaths P(std::move(outputRoot));
    P.createAll();

    // =========================================================================
    //  Phase 1 — Calcul des grandeurs structurelles
    // =========================================================================

    auto futureBM = std::async(std::launch::async, [&] { return BendingMoments(); });
    auto futureSF = std::async(std::launch::async, [&] { return ShearForce();     });
    auto futureDef = std::async(std::launch::async, [&] { return Deflection();     });
    auto futureRot = std::async(std::launch::async, [&] { return Rotation();       });

    const auto BM = futureBM.get();
    const auto SF = futureSF.get();
    const auto Def = futureDef.get();
    const auto Rot = futureRot.get();

    const auto X = globalXCoordinates(spanNodePositions);   // ex pointsXCoordinates(SpanNodePositions)
    const auto X_T = ShearForce(true);

    std::vector<double> nodeLengths;
    nodeLengths.reserve(spanLengths.size() + 1);
    double cumul = 0.0;
    for (const double span : spanLengths) { nodeLengths.push_back(cumul); cumul += span; }
    nodeLengths.push_back(cumul);

    // =========================================================================
    //  Phase 2a — Valeurs critiques
    // =========================================================================

    auto writeCritical = [&](const auto& data, const std::string& file, auto& outPos) {
        outPos = findMaxAbsoluteValue3D(data);
        json j;
        writeCriticalValueJson(outPos, j, P.critical_values, file);
        };

    auto fMaxBM = std::async(std::launch::async, [&] { writeCritical(BM, "bending_moment.json", bendingMomentCritical); });
    auto fMaxSF = std::async(std::launch::async, [&] { writeCritical(SF, "shear_force.json", shearForceCritical);    });
    auto fMaxDef = std::async(std::launch::async, [&] { writeCritical(Def, "deflection.json", deflectionCritical);    });
    auto fMaxRot = std::async(std::launch::async, [&] { writeCritical(Rot, "rotation.json", rotationCritical);      });
    auto fMaxSM = std::async(std::launch::async, [&] {
        supportMomentCritical = findMaxAbsoluteValue2D(supportMoments);
        json j;
        j["span"] = supportMomentCritical.i;
        j["alpha"] = supportMomentCritical.j;
        j["value"] = supportMomentCritical.val;
        writeJsonFile(j, P.critical_values, "support_moment.json");
        });

    fMaxBM.wait(); fMaxSF.wait(); fMaxDef.wait(); fMaxRot.wait(); fMaxSM.wait();

    // =========================================================================
    //  Phase 2b — Lignes d'influence
    // =========================================================================

    auto fModel = std::async(std::launch::async, [&] {
        json model;
        model["spans"] = spanLengths;
        model["young_modulus"] = elasticModuli;
        model["inertia"] = inertiaMoments;
        model["step"] = steps;
        model["node_lengths"] = nodeLengths;
        model["n_spans"] = spanLengths.size();
        model["n_total_nodes"] = X.size();
        writeJsonFile(model, P.input, "structural_model.json");
        });

    auto fBM = std::async(std::launch::async, [&] { writeJsonFile(BM, P.influence_lines, "bending_moment.json"); });
    auto fSF = std::async(std::launch::async, [&] { writeJsonFile(SF, P.influence_lines, "shear_force.json");    });
    auto fDef = std::async(std::launch::async, [&] { writeJsonFile(Def, P.influence_lines, "deflection.json");     });
    auto fRot = std::async(std::launch::async, [&] { writeJsonFile(Rot, P.influence_lines, "rotation.json");       });
    auto fSM = std::async(std::launch::async, [&] { writeJsonFile(supportMoments, P.influence_lines, "support_moment.json"); });
    auto fX = std::async(std::launch::async, [&] { writeJsonFile(X, P.influence_lines, "abscissa.json");       });
    auto fXT = std::async(std::launch::async, [&] { writeJsonFile(X_T, P.influence_lines, "shear_abscissa.json"); });
    auto fNL = std::async(std::launch::async, [&] { writeJsonFile(nodeLengths, P.influence_lines, "node_lengths.json");   });

    fModel.wait();
    fBM.wait(); fSF.wait(); fDef.wait(); fRot.wait();
    fSM.wait(); fX.wait();  fXT.wait();  fNL.wait();

    // =========================================================================
    //  Phase 3a — Enveloppes globales
    // =========================================================================

    auto computeEnvelope = [&](const auto& data, const std::string& file) {
        LoadEnvelope env(data, X, spanNodePositions);
        json j;
        writeEnvelopeJson(env.distributedLoadResult, j, P.env_global_dist, file);
        writeEnvelopeJson(env.pointLoadResult, j, P.env_global_point, file);
        writeEnvelopeJson(env.combinedLoadResult, j, P.env_global_combined, file);
        };

    auto fGloBM = std::async(std::launch::async, [&] { computeEnvelope(BM, "bending_moment.json"); });
    auto fGloSF = std::async(std::launch::async, [&] { computeEnvelope(SF, "shear_force.json");    });
    auto fGloDef = std::async(std::launch::async, [&] { computeEnvelope(Def, "deflection.json");     });
    auto fGloRot = std::async(std::launch::async, [&] { computeEnvelope(Rot, "rotation.json");       });

    fGloBM.wait(); fGloSF.wait(); fGloDef.wait(); fGloRot.wait();

    // =========================================================================
    //  Phase 3b — Section critique
    // =========================================================================

    auto computeEnvelopeAt = [&](const auto& data, const Position3D& critPos,
        const std::string& file)
        {
            LoadEnvelope env(data, X, spanNodePositions);

            // Charge ponctuelle à la section critique
            LoadEnvelopeResult pointResult;
            {
                std::vector<double> vals;
                for (size_t sec = 0; sec < spanNodePositions[critPos.i].size(); ++sec) {
                    double v = 0.0;
                    for (auto& lc : env.pointLoads)
                        v += env.pluralPointLoad(lc.intensities, lc.positions, critPos.i, sec).value;
                    vals.push_back(v);
                }
                double mv = maxAbsInVector(vals);
                pointResult.span = critPos.i;
                pointResult.section = indexOf(vals, mv);
                pointResult.maxValue = mv;
                std::map<std::string, std::map<std::string, double>> detail;
                for (auto& lc : env.pointLoads) {
                    auto lo = env.pluralPointLoad(lc.intensities, lc.positions, critPos.i, pointResult.section);
                    detail[lc.name] = {
                        { "alpha",    static_cast<double>(lo.maxIndex) },
                        { "value",    lo.value },
                        { "Position", X[lo.maxIndex] + lc.positions[0] }
                    };
                }
                pointResult.load = detail;
            }

            // Charge répartie à la section critique
            LoadEnvelopeResult distResult;
            {
                std::vector<double> vals;
                for (size_t sec = 0; sec < spanNodePositions[critPos.i].size(); ++sec) {
                    double v = 0.0;
                    for (auto& lc : env.distributedLoads)
                        v += env.pluralRectangularLoad(lc.intensities, lc.positions, critPos.i, sec).value;
                    vals.push_back(v);
                }
                double mv = maxAbsInVector(vals);
                distResult.span = critPos.i;
                distResult.section = indexOf(vals, mv);
                distResult.maxValue = mv;
                std::map<std::string, std::map<std::string, double>> detail;
                for (auto& lc : env.distributedLoads) {
                    auto lo = env.pluralRectangularLoad(lc.intensities, lc.positions, critPos.i, distResult.section);
                    detail[lc.name] = {
                        { "alpha",    static_cast<double>(lo.maxIndex) },
                        { "value",    lo.value },
                        { "Position", X[lo.maxIndex] + lc.positions[0] }
                    };
                }
                distResult.load = detail;
            }

            // Charge combinée à la section critique
            LoadEnvelopeResult combinedResult;
            {
                std::vector<double> vals, positions;
                std::vector<std::map<std::string, std::map<std::string, double>>> maps;
                for (size_t sec = 0; sec < spanNodePositions[critPos.i].size(); ++sec) {
                    auto lo = env.combinedLoad(critPos.i, sec);
                    vals.push_back(lo.value);
                    positions.push_back(lo.position);
                    maps.push_back(lo.breakdown);
                }
                double mv = maxAbsInVector(vals);
                combinedResult.span = critPos.i;
                combinedResult.section = indexOf(vals, mv);
                combinedResult.maxValue = mv;
                combinedResult.position = positions[combinedResult.section];
                combinedResult.load = maps[combinedResult.section];
            }

            json j;
            writeEnvelopeJson(distResult, j, P.env_critical_dist, file);
            writeEnvelopeJson(pointResult, j, P.env_critical_point, file);
            writeEnvelopeJson(combinedResult, j, P.env_critical_combined, file);
        };

    auto fCritBM = std::async(std::launch::async, [&] { computeEnvelopeAt(BM, bendingMomentCritical, "bending_moment.json"); });
    auto fCritSF = std::async(std::launch::async, [&] { computeEnvelopeAt(SF, shearForceCritical, "shear_force.json");    });
    auto fCritDef = std::async(std::launch::async, [&] { computeEnvelopeAt(Def, deflectionCritical, "deflection.json");     });
    auto fCritRot = std::async(std::launch::async, [&] { computeEnvelopeAt(Rot, rotationCritical, "rotation.json");       });

    fCritBM.wait(); fCritSF.wait(); fCritDef.wait(); fCritRot.wait();

    std::cout << "Output generated successfully in: " << P.root << "\n";
}