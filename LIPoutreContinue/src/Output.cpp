#include "LIPoutreContinue/Output.h"
#include "LIPoutreContinue/loading/Input.h"
#include "LIPoutreContinue/loading/Loading.h"

#include "LIPoutreContinue/loading/MovingLoadSearch.h"

#include <algorithm>
#include <cmath>
#include <future>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <utility>

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

    computeSupportReactions();
    SupportReactionMaxPositions = findMaxAbsoluteValue2D(SupportReaction);

    computed_ = true;
    loadEnvelopesComputed_ = false;   // la géométrie a pu être recalculée
}

// =============================================================================
//  analyze() — calcul complet en un appel : lignes d'influence, puis
//  enveloppes si des charges ont été fournies par setLoads().
// =============================================================================
void Output::analyze()
{
    compute();
    if (hasUserLoads_) computeLoadEnvelopes();
}

// =============================================================================
//  Réactions d'appui — lignes d'influence exactes déduites des moments sur
//  appuis. Charge unité (vers le bas) à l'abscisse locale a de la travée k,
//  moments d'extrémité M_k, M_{k+1} (positifs = fibre inférieure tendue) :
//      V(0⁺) = (1 − a/L_k) + (M_{k+1} − M_k)/L_k
//  d'où, pour l'appui i (réaction positive vers le haut) :
//      R_i = [a/L_{i−1}]_{charge en i−1} + (M_{i−1} − M_i)/L_{i−1}
//          + [1 − a/L_i]_{charge en i}   + (M_{i+1} − M_i)/L_i
//  Contrôles (tests) : ΣR_i = 1 et ΣR_i·x_i = x_charge pour toute position.
// =============================================================================
void Output::computeSupportReactions()
{
    const size_t S = L_spans.size();
    const size_t N = X.size();
    SupportReaction.assign(S + 1, std::vector<double>(N, 0.0));

    size_t k = 0;   // indice global de la charge
    for (size_t s = 0; s < S; ++s) {
        for (double a : SpanNodePositions[s]) {
            for (size_t i = 0; i <= S; ++i) {
                double r = 0.0;
                if (i >= 1) {                  // travée de gauche : i − 1
                    const double Ll = L_spans[i - 1];
                    r += (SupportMoment[i - 1][k] - SupportMoment[i][k]) / Ll;
                    if (s == i - 1) r += a / Ll;
                }
                if (i < S) {                   // travée de droite : i
                    const double Lr = L_spans[i];
                    r += (SupportMoment[i + 1][k] - SupportMoment[i][k]) / Lr;
                    if (s == i) r += 1.0 - a / Lr;
                }
                SupportReaction[i][k] = r;
            }
            ++k;
        }
    }
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
    auto fR   = std::async(std::launch::async, [&] { delivery(SupportReaction,        Paths.influence_lines, "support_reaction.json"); });

    fModel.wait();
    fBM.wait(); fSF.wait(); fDef.wait(); fRot.wait();
    fSM.wait(); fX.wait();  fXT.wait();  fNL.wait();  fR.wait();
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
    loadEnvelopesComputed_ = false;   // nouvelles charges : enveloppes à recalculer
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
    // Unités (défaut N1) : les lignes d'influence sont calculées pour une
    // charge unité de 1 N avec E en Pa → flèche en m/N, rotation en rad/N.
    // Les charges utilisateur sont en kN : facteur 1e3 pour ces deux
    // grandeurs. M (m) et V (sans dimension) ne dépendent pas de E : facteur 1.
    constexpr double kN_to_N = 1e3;

    // Une seule instance de Loading par grandeur : enveloppe générale ET
    // enveloppe à la section critique (= section du maximum de la ligne
    // d'influence, comme documenté — auparavant, toute la travée).
    struct Harvest { LoadEnvelope general, critical; SectionEnvelopes curves; };
    auto envelopesFor = [&](const auto& data, double scale, const Position3D& pos) {
        Loading loading(Loading::BorrowCurves{}, data, X, SpanNodePositions, L_spans,
                        userPointLoads_, userDistribLoads_, scale);
        auto crit = loading.computeAtSection(pos.i, pos.j);
        return Harvest{
            LoadEnvelope{ loading.Point_load, loading.Rectangular_load, loading.Combined_load },
            LoadEnvelope{ crit.point, crit.rect, crit.combined },
            SectionEnvelopes{ loading.PointEnvelope, loading.RectangularEnvelope, loading.CombinedEnvelope } };
    };

    auto fBM  = std::async(std::launch::async, [&] { return envelopesFor(BM,  1.0,     BendingMomentMaxPositions); });
    auto fSF  = std::async(std::launch::async, [&] { return envelopesFor(SF,  1.0,     ShearForceMaxPositions);    });
    auto fDef = std::async(std::launch::async, [&] { return envelopesFor(Def, kN_to_N, DeflectionMaxPositions);    });
    auto fRot = std::async(std::launch::async, [&] { return envelopesFor(Rot, kN_to_N, RotationMaxPositions);      });
    auto fR   = std::async(std::launch::async, [&] { return computeReactionEnvelopes(); });

    auto unpack = [](Harvest h, LoadEnvelope& g, LoadEnvelope& c, SectionEnvelopes& e) {
        g = std::move(h.general); c = std::move(h.critical); e = std::move(h.curves);
    };
    unpack(fBM.get(),  BendingMomentGeneralLoadEnvelope, BendingMomentCriticalLoadEnvelope, BendingMomentSectionEnvelopes);
    unpack(fSF.get(),  ShearForceGeneralLoadEnvelope,    ShearForceCriticalLoadEnvelope,    ShearForceSectionEnvelopes);
    unpack(fDef.get(), DeflectionGeneralLoadEnvelope,    DeflectionCriticalLoadEnvelope,    DeflectionSectionEnvelopes);
    unpack(fRot.get(), RotationGeneralLoadEnvelope,      RotationCriticalLoadEnvelope,      RotationSectionEnvelopes);
    SupportReactionLoadEnvelopes = fR.get();

    loadEnvelopesComputed_ = true;
}

// =============================================================================
//  Enveloppes des réactions d'appui : une ligne d'influence par appui, même
//  algorithme exact que pour les sections (lipc::*Extreme).
//  Champs remplis de chaque load_delivery : max/min (valeur, détail par
//  charge), maximum_value (gouvernant), span = indice d'appui, section_x =
//  abscisse de l'appui.
// =============================================================================
std::vector<Output::LoadEnvelope> Output::computeReactionEnvelopes() const
{
    auto names = [](const std::vector<load>& loads, std::vector<std::string>& taken) {
        std::vector<std::string> out;
        for (const auto& l : loads) {
            const std::string base = l.name.empty() ? std::string("charge") : l.name;
            std::string c = base;
            for (int k = 2; std::find(taken.begin(), taken.end(), c) != taken.end(); ++k)
                c = base + " #" + std::to_string(k);
            taken.push_back(c);
            out.push_back(c);
        }
        return out;
    };
    std::vector<std::string> taken;
    const auto pNames = names(userPointLoads_, taken);
    const auto dNames = names(userDistribLoads_, taken);

    auto detail = [](const lipc::Extreme& e, bool positive) {
        const bool has = positive ? e.hasMax : e.hasMin;
        return std::map<std::string, double>{
            {"value", positive ? e.max : e.min},
            {"Position", has ? (positive ? e.maxOrigin : e.minOrigin) : std::nan("")}};
    };
    auto build = [&](const std::vector<lipc::Extreme>& pe, const std::vector<lipc::Extreme>& de,
                     bool usePoint, bool useDist, size_t support, double x) {
        load_delivery d;
        d.span = d.max_span = d.min_span = support;
        d.section_x = d.max_section_x = d.min_section_x = x;
        d.position = std::nan("");
        auto add = [&](const std::vector<lipc::Extreme>& ext, const std::vector<std::string>& nm) {
            for (size_t k = 0; k < ext.size(); ++k) {
                d.max_value += ext[k].max;
                d.min_value += ext[k].min;
                d.load_max[nm[k]] = detail(ext[k], true);
                d.load_min[nm[k]] = detail(ext[k], false);
            }
        };
        if (usePoint) add(pe, pNames);
        if (useDist)  add(de, dNames);
        const bool positive = std::abs(d.max_value) >= std::abs(d.min_value);
        d.maximum_value = positive ? d.max_value : d.min_value;
        d.load = positive ? d.load_max : d.load_min;
        for (const auto& [n, f] : d.load) { d.position = f.at("Position"); break; }
        return d;
    };

    std::vector<LoadEnvelope> out;
    for (size_t i = 0; i < SupportReaction.size(); ++i) {
        const lipc::InfluenceLine line(X, SupportReaction[i]);
        std::vector<lipc::Extreme> pe, de;
        for (const auto& l : userPointLoads_)   pe.push_back(lipc::pointLoadExtreme(line, l));
        for (const auto& l : userDistribLoads_) de.push_back(lipc::distributedLoadExtreme(line, l));
        const double x = NodeLengths[i];
        out.push_back(LoadEnvelope{ build(pe, de, true, false, i, x),
                                    build(pe, de, false, true, i, x),
                                    build(pe, de, true, true, i, x) });
    }
    return out;
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

    // ── Phase 3c — Réactions d'appui (une entrée par appui) ────────────────
    json reactions = json::array();
    for (const auto& env : SupportReactionLoadEnvelopes)
        reactions.push_back({ {"support", env.combinedLoad.span}, {"x", env.combinedLoad.section_x},
                              {"point_load", toJson(env.pointLoad)},
                              {"distributed_load", toJson(env.rectangularLoad)},
                              {"combined_load", toJson(env.combinedLoad)} });
    delivery(reactions, Paths.load_envelopes, "support_reactions.json");

    // ── Phase 3d — Courbes enveloppes E⁺(x), E⁻(x) ─────────────────────────
    auto curves = [&](const SectionEnvelopes& e) {
        auto flat = [](const std::vector<std::vector<double>>& v) {
            std::vector<double> o;
            for (const auto& r : v) o.insert(o.end(), r.begin(), r.end());
            return o;
        };
        auto cat = [&](const Loading::SectionEnvelope& c) { return json{ {"max", flat(c.max)}, {"min", flat(c.min)} }; };
        return json{ {"x", X}, {"point_load", cat(e.point)}, {"distributed_load", cat(e.rect)},
                     {"combined_load", cat(e.combined)} };
    };
    json all = { {"bending_moment", curves(BendingMomentSectionEnvelopes)},
                 {"shear_force", curves(ShearForceSectionEnvelopes)},
                 {"deflection", curves(DeflectionSectionEnvelopes)},
                 {"rotation", curves(RotationSectionEnvelopes)} };
    delivery(all, Paths.load_envelopes, "envelope_curves.json");
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
