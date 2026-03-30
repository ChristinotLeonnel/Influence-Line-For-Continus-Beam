#include "Hyperstatique.h"
#include "Isostatique.h"
#include "utils/Utils.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include <future>
#include <iostream>
#include <thread>

// =============================================================================
//  Hyperstatique::Hyperstatique
// =============================================================================
//
//  OPTIMISATIONS appliquées (dimensions des matrices inchangées) :
//
//  [OPT-1]  total_nodes_ calculé une seule fois → remplace le recalcul O(S)
//           fait à chaque section dans BendingMoments / Rotation / Deflection.
//
//  [OPT-2]  leftMomentsCache_ / rightMomentsCache_ calculés UNE SEULE FOIS
//           et injectés dans LeftSupportMoments / RightSupportMoments.
//           Avant : chacune des S appels recalculait tout → O(S²×N).
//           Après : calcul unique → O(S×N).
//
//  [OPT-3]  ShearForceAbscissa_ construit en O(S×N) dans le constructeur.
//           Avant : ForShearForce(tr) recalculait toutes les travées pour
//           chaque tr → O(S²×N).
//
//  Dimensions des matrices (BM, SF, Def, Rot) : IDENTIQUES à l'original.
//    BM, Rot, Def  →  [S][N_span_s][N_total]
//    SF            →  [S][N_span_s][N_total + discontinuités]
// =============================================================================

Hyperstatique::Hyperstatique(std::vector<double>& E,
                             std::vector<double>& I,
                             std::vector<double>& L,
                             double& steps)
    : steps(steps), L_spans(L), E_spans(E), I_spans(I)
{
    number_of_spans = L.size();

    for (size_t i = 0; i < number_of_spans; ++i)
    {
        Isostatique span(E[i], I[i], L[i], steps);

        a_spans.push_back(span.a);
        b_spans.push_back(span.b);
        c_spans.push_back(span.c);

        if (L[i] != 0) {
            SpanNodePositions.push_back(span.nodePositions);
            Omega_Second_Spans.push_back(span.Omega_Second);
            Omega_Prime_Spans.push_back(span.Omega_Prime);
            BendingMomentStatic.push_back(span.BendingMoment());
            RotationStatic.push_back(span.Rotation());
            ShearForceAbscissaStatic.push_back(span.ShearForceAbscissa());
            ShearForceStatic.push_back(span.ShearForce());
            DeflectionStatic.push_back(span.Deflection());
        }
    }

    std::vector<int> LZeroIdx;
    for (size_t i = 0; i < number_of_spans; ++i)
        if (L[i] == 0)
            LZeroIdx.push_back(static_cast<int>(i));

    double phy_premier = 0;
    for (size_t i = 0; i < number_of_spans; ++i) {
        if (i != 0)
            phy_premier = b_spans[i] / (a_spans[i] + c_spans[i - 1] - b_spans[i - 1] * phy_premier);
        phy.push_back(phy_premier);
    }

    double phy_prime_premier = 0;
    for (size_t i = number_of_spans; i > 0; --i) {
        if (i != number_of_spans)
            phy_prime_premier = b_spans[i - 1] / (c_spans[i - 1] + a_spans[i] - b_spans[i] * phy_prime_premier);
        phy_prime.push_back(phy_prime_premier);
    }
    std::reverse(phy_prime.begin(), phy_prime.end());

    removeByIndices(phy,       LZeroIdx);
    removeByIndices(phy_prime, LZeroIdx);
    removeByIndices(L,         LZeroIdx);
    removeByIndices(E,         LZeroIdx);
    removeByIndices(I,         LZeroIdx);
    removeByIndices(a_spans,   LZeroIdx);
    removeByIndices(b_spans,   LZeroIdx);
    removeByIndices(c_spans,   LZeroIdx);

    number_of_spans = L.size();

    // [OPT-1] Nombre total de nœuds — calculé UNE SEULE FOIS
    total_nodes_ = 0;
    for (const auto& sp : SpanNodePositions)
        total_nodes_ += sp.size();

    // [OPT-2] Cache moments — calculés UNE SEULE FOIS
    leftMomentsCache_  = leftLoadedSpanSupportMoments();
    rightMomentsCache_ = rightLoadedSpanSupportMoments();

    for (int SpanIndex = 0; SpanIndex < static_cast<int>(number_of_spans); ++SpanIndex)
        SpanLeftSupportMoments.push_back(LeftSupportMoments(SpanIndex, leftMomentsCache_));

    for (int SpanIndex = 0; SpanIndex < static_cast<int>(number_of_spans); ++SpanIndex)
        SpanRightSupportMoments.push_back(RightSupportMoments(SpanIndex, rightMomentsCache_));

    for (size_t i = 0; i < number_of_spans; ++i) {
        std::vector<std::vector<double>> mini_g_D;
        mini_g_D.reserve(SpanLeftSupportMoments[i].size() + SpanRightSupportMoments[i].size());
        mini_g_D.insert(mini_g_D.end(), SpanLeftSupportMoments[i].begin(), SpanLeftSupportMoments[i].end());
        mini_g_D.insert(mini_g_D.end(), SpanRightSupportMoments[i].begin(), SpanRightSupportMoments[i].end());
        SpanLeftRight.push_back(std::move(mini_g_D));
    }

    for (size_t j = 0; j <= number_of_spans; ++j) {
        std::vector<std::vector<double>> appuit_courbe;
        appuit_courbe.reserve(number_of_spans);
        for (size_t i = 0; i < number_of_spans; ++i)
            appuit_courbe.push_back(SpanLeftRight[i][j]);
        SupportMoment.push_back(flatten(std::move(appuit_courbe)));
    }

    // [OPT-3] Abscisses SF — une seule passe O(S×N)
    ShearForceAbscissa_ = buildShearForceAbscissa();
}

// =============================================================================
//  buildShearForceAbscissa — O(S×N) au lieu de O(S²×N)
// =============================================================================
std::vector<std::vector<std::vector<double>>> Hyperstatique::buildShearForceAbscissa()
{
    std::vector<std::vector<std::vector<double>>> result;
    result.reserve(number_of_spans);

    auto JOL = SpanNodePositions;

    for (size_t tr = 0; tr < number_of_spans; ++tr) {
        std::vector<std::vector<double>> x_tr;
        x_tr.reserve(SpanNodePositions[tr].size());

        for (size_t sec = 0; sec < SpanNodePositions[tr].size(); ++sec) {
            JOL[tr] = ShearForceAbscissaStatic[tr][sec];
            x_tr.push_back(pointsXCoordinates(JOL));
            JOL[tr] = SpanNodePositions[tr];
        }
        result.push_back(std::move(x_tr));
    }
    return result;
}

// =============================================================================
//  pointsXCoordinates
// =============================================================================
std::vector<double> Hyperstatique::pointsXCoordinates(const std::vector<std::vector<double>>& liste)
{
    size_t total_size = 0;
    for (const auto& span : liste)
        total_size += span.size();

    std::vector<double> x;
    x.reserve(total_size);

    for (size_t i = 0; i < liste.size(); ++i) {
        double offset = 0.0;
        for (size_t j = 0; j < i; ++j)
            offset += L_spans[j];
        for (const auto& alpha : liste[i])
            x.push_back(alpha + offset);
    }
    return x;
}

// =============================================================================
//  Moments aux appuis
// =============================================================================

std::vector<std::vector<double>> Hyperstatique::leftLoadedSpanSupportMoments()
{
    std::vector<std::vector<double>> M_appuit(number_of_spans);
    for (size_t i = 0; i < number_of_spans; ++i) {
        M_appuit[i].reserve(SpanNodePositions[i].size());
        const double denom = 1.0 - phy[i] * phy_prime[i];
        for (size_t j = 0; j < SpanNodePositions[i].size(); ++j) {
            double gauche = (phy[i] / b_spans[i]) *
                ((Omega_Prime_Spans[i][j] + Omega_Second_Spans[i][j] * phy_prime[i]) / denom);
            M_appuit[i].push_back(gauche);
        }
    }
    return M_appuit;
}

std::vector<std::vector<double>> Hyperstatique::rightLoadedSpanSupportMoments()
{
    std::vector<std::vector<double>> M_appuit(number_of_spans);
    for (size_t i = 0; i < number_of_spans; ++i) {
        M_appuit[i].reserve(SpanNodePositions[i].size());
        const double denom = 1.0 - phy[i] * phy_prime[i];
        for (size_t j = 0; j < SpanNodePositions[i].size(); ++j) {
            double droite = -(phy_prime[i] / b_spans[i]) *
                ((Omega_Prime_Spans[i][j] * phy[i] + Omega_Second_Spans[i][j]) / denom);
            M_appuit[i].push_back(droite);
        }
    }
    return M_appuit;
}

// [OPT-2] — cache injecté, pas de recalcul
std::vector<std::vector<double>> Hyperstatique::LeftSupportMoments(
    int SpanIndex,
    const std::vector<std::vector<double>>& momentGaucheCache)
{
    std::vector<std::vector<double>> gauche;
    gauche.reserve(SpanIndex + 1);

    for (int i = 0; i <= SpanIndex; ++i) {
        std::vector<double> q;
        q.reserve(momentGaucheCache[SpanIndex].size());
        for (const auto& j : momentGaucheCache[SpanIndex])
            q.push_back(std::pow(-1.0, SpanIndex - i) * prod_list(phy, i, SpanIndex - 1) * j);
        gauche.push_back(std::move(q));
    }
    return gauche;
}

std::vector<std::vector<double>> Hyperstatique::RightSupportMoments(
    int SpanIndex,
    const std::vector<std::vector<double>>& momentDroiteCache)
{
    std::vector<std::vector<double>> droite;
    droite.reserve(number_of_spans - SpanIndex);

    for (int i = SpanIndex, g = SpanIndex; i < static_cast<int>(number_of_spans); ++i, ++g) {
        double p = prod_list(phy_prime, SpanIndex + 1, g);
        std::vector<double> q;
        q.reserve(momentDroiteCache[SpanIndex].size());
        for (const auto& j : momentDroiteCache[SpanIndex])
            q.push_back(std::pow(-1.0, SpanIndex - g) * p * j);
        droite.push_back(std::move(q));
    }
    return droite;
}

// =============================================================================
//  Formules hyperstatiques
// =============================================================================

static constexpr double HypPartBendingMoment(double m, double n, double x, double l) {
    return m * (1.0 - x / l) + n * x / l;
}

static constexpr double HypPartRotation(double m, double n, double x, double l, double E, double I) {
    return -m * (2.0*(l*l) - 6.0*l*x + 3.0*(x*x)) / (6.0*E*I*l)
           -n  * ((l*l) - 3.0*(x*x))                / (6.0*E*I*l);
}

static constexpr double HypPartShearForce(double m, double n, double l) {
    return (-m + n) / l;
}

static constexpr double HypPartDeflection(double m, double n, double x, double l, double E, double I) {
    return -m * x*(l-x)*(2.0*l-x) / (6.0*E*I*l)
           -n  * x*(l-x)*(l+x)    / (6.0*E*I*l);
}

// =============================================================================
//  BendingMoments — dimensions [S][N_span_s][N_total] identiques à l'original
// =============================================================================
std::vector<std::vector<std::vector<double>>> Hyperstatique::BendingMoments()
{
    std::vector<std::vector<std::vector<double>>> hyp_moment_total;
    hyp_moment_total.reserve(number_of_spans);

    std::vector<std::future<std::vector<std::vector<double>>>> futures;
    futures.reserve(number_of_spans);

    for (int SpanIndex = 0; SpanIndex < static_cast<int>(number_of_spans); ++SpanIndex) {
        futures.push_back(std::async(std::launch::async, [this, SpanIndex]() {

            std::vector<std::vector<double>> Mu_travee;
            Mu_travee.reserve(SpanNodePositions[SpanIndex].size());
            int SectionCompteur = 0;

            for (const auto& section : SpanNodePositions[SpanIndex]) {
                std::vector<double> hyp_mu;
                hyp_mu.reserve(total_nodes_); // [OPT-1] : plus de boucle O(S)

                auto& Mut = BendingMomentStatic[SpanIndex][SectionCompteur++];

                for (int i = 0; i < static_cast<int>(number_of_spans); ++i) {
                    for (int j = 0; j < static_cast<int>(SpanNodePositions[i].size()); ++j) {
                        double mi = HypPartBendingMoment(
                            SpanLeftRight[i][SpanIndex][j],
                            SpanLeftRight[i][SpanIndex + 1][j],
                            section, L_spans[SpanIndex]);
                        hyp_mu.push_back((SpanIndex == i) ? Mut[j] + mi : mi);
                    }
                }
                Mu_travee.push_back(std::move(hyp_mu));
            }
            return Mu_travee;
        }));
    }

    for (auto& f : futures)
        hyp_moment_total.push_back(f.get());

    return hyp_moment_total;
}

// =============================================================================
//  Rotation — dimensions [S][N_span_s][N_total] identiques à l'original
// =============================================================================
std::vector<std::vector<std::vector<double>>> Hyperstatique::Rotation()
{
    std::vector<std::vector<std::vector<double>>> hyp_rotation_total;
    hyp_rotation_total.reserve(number_of_spans);

    std::vector<std::future<std::vector<std::vector<double>>>> futures;
    futures.reserve(number_of_spans);

    for (int SpanIndex = 0; SpanIndex < static_cast<int>(number_of_spans); ++SpanIndex) {
        futures.push_back(std::async(std::launch::async, [this, SpanIndex]() {

            std::vector<std::vector<double>> rot_travee;
            rot_travee.reserve(SpanNodePositions[SpanIndex].size());

            for (int section = 0; section < static_cast<int>(SpanNodePositions[SpanIndex].size()); ++section) {
                std::vector<double> hyp_rot;
                hyp_rot.reserve(total_nodes_); // [OPT-1]

                for (int i = 0; i < static_cast<int>(number_of_spans); ++i) {
                    for (int j = 0; j < static_cast<int>(SpanNodePositions[i].size()); ++j) {
                        double rv = HypPartRotation(
                            SpanLeftRight[i][SpanIndex][j],
                            SpanLeftRight[i][SpanIndex + 1][j],
                            SpanNodePositions[SpanIndex][section],
                            L_spans[SpanIndex], E_spans[SpanIndex], I_spans[SpanIndex]);
                        hyp_rot.push_back((SpanIndex == i)
                            ? RotationStatic[SpanIndex][section][j] + rv : rv);
                    }
                }
                rot_travee.push_back(std::move(hyp_rot));
            }
            return rot_travee;
        }));
    }

    for (auto& f : futures)
        hyp_rotation_total.push_back(f.get());

    return hyp_rotation_total;
}

// =============================================================================
//  Deflection — dimensions [S][N_span_s][N_total] identiques à l'original
// =============================================================================
std::vector<std::vector<std::vector<double>>> Hyperstatique::Deflection()
{
    std::vector<std::vector<std::vector<double>>> hyp_deflection_total;
    hyp_deflection_total.reserve(number_of_spans);

    std::vector<std::future<std::vector<std::vector<double>>>> futures;
    futures.reserve(number_of_spans);

    for (int SpanIndex = 0; SpanIndex < static_cast<int>(number_of_spans); ++SpanIndex) {
        futures.push_back(std::async(std::launch::async, [this, SpanIndex]() {

            std::vector<std::vector<double>> defl_travee;
            defl_travee.reserve(SpanNodePositions[SpanIndex].size());

            for (int section = 0; section < static_cast<int>(SpanNodePositions[SpanIndex].size()); ++section) {
                std::vector<double> hyp_defl;
                hyp_defl.reserve(total_nodes_); // [OPT-1]

                for (int i = 0; i < static_cast<int>(number_of_spans); ++i) {
                    for (int j = 0; j < static_cast<int>(SpanNodePositions[i].size()); ++j) {
                        double dv = HypPartDeflection(
                            SpanLeftRight[i][SpanIndex][j],
                            SpanLeftRight[i][SpanIndex + 1][j],
                            SpanNodePositions[SpanIndex][section],
                            L_spans[SpanIndex], E_spans[SpanIndex], I_spans[SpanIndex]);
                        hyp_defl.push_back((SpanIndex == i)
                            ? DeflectionStatic[SpanIndex][section][j] + dv : dv);
                    }
                }
                defl_travee.push_back(std::move(hyp_defl));
            }
            return defl_travee;
        }));
    }

    for (auto& f : futures)
        hyp_deflection_total.push_back(f.get());

    return hyp_deflection_total;
}

// =============================================================================
//  ShearForce — dimensions identiques à l'original
//  [OPT-3] get_all_abscisse=true : retourne le cache pré-calculé
// =============================================================================
std::vector<std::vector<std::vector<double>>> Hyperstatique::ShearForce(bool get_all_abscisse)
{
    if (get_all_abscisse)
        return ShearForceAbscissa_; // [OPT-3]

    std::vector<std::vector<std::vector<double>>> hyp_effort_tranchant_total;
    hyp_effort_tranchant_total.reserve(number_of_spans);

    for (int SpanIndex = 0; SpanIndex < static_cast<int>(number_of_spans); ++SpanIndex) {
        std::vector<std::vector<double>> T_travee;
        T_travee.reserve(SpanNodePositions[SpanIndex].size());

        for (size_t sectionIdx = 0; sectionIdx < SpanNodePositions[SpanIndex].size(); ++sectionIdx) {
            const auto& section = SpanNodePositions[SpanIndex][sectionIdx];
            std::vector<double> hyp_T;

            size_t estimated_size = 0;
            for (int i = 0; i < static_cast<int>(number_of_spans); ++i)
                estimated_size += (i == SpanIndex)
                    ? SpanNodePositions[i].size() * 2
                    : SpanNodePositions[i].size();
            hyp_T.reserve(estimated_size);

            for (int i = 0; i < static_cast<int>(number_of_spans); ++i) {
                if (SpanIndex == i) {
                    size_t compteur = 0;
                    for (int j = 0; j < static_cast<int>(SpanNodePositions[i].size()); ++j) {
                        double mi = HypPartShearForce(
                            SpanLeftRight[i][SpanIndex][j],
                            SpanLeftRight[i][SpanIndex + 1][j],
                            L_spans[SpanIndex]);

                        if (sectionIdx < ShearForceStatic[SpanIndex].size() &&
                            compteur   < ShearForceStatic[SpanIndex][sectionIdx].size())
                        {
                            if (j < static_cast<int>(SpanNodePositions[SpanIndex].size()) &&
                                section == SpanNodePositions[SpanIndex][j])
                            {
                                hyp_T.push_back(ShearForceStatic[SpanIndex][sectionIdx][compteur] + mi);
                                ++compteur;
                                if (compteur < ShearForceStatic[SpanIndex][sectionIdx].size()) {
                                    hyp_T.push_back(ShearForceStatic[SpanIndex][sectionIdx][compteur] + mi);
                                    ++compteur;
                                }
                            } else {
                                hyp_T.push_back(ShearForceStatic[SpanIndex][sectionIdx][compteur] + mi);
                                ++compteur;
                            }
                        }
                    }
                } else {
                    for (int j = 0; j < static_cast<int>(SpanNodePositions[i].size()); ++j) {
                        hyp_T.push_back(HypPartShearForce(
                            SpanLeftRight[i][SpanIndex][j],
                            SpanLeftRight[i][SpanIndex + 1][j],
                            L_spans[SpanIndex]));
                    }
                }
            }
            T_travee.push_back(std::move(hyp_T));
        }
        hyp_effort_tranchant_total.push_back(std::move(T_travee));
    }

    return hyp_effort_tranchant_total;
}
