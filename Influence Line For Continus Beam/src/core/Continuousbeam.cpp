#include "ContinuousBeam.h"
#include "IsostaticBeam.h"
#include "BeamFormulas.h"
#include "utils/Beamutils.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>
#include <future>
#include <iostream>
#include <thread>

// =============================================================================
//  ContinuousBeam::ContinuousBeam
// =============================================================================
//
//  OPTIMISATIONS appliquées (dimensions des matrices inchangées) :
//
//  [OPT-1]  total_nodes_ calculé une seule fois → remplace le recalcul O(S)
//           fait à chaque section dans BendingMoments / Rotation / Deflection.
//
//  [OPT-2]  leftMomentsCache_ / rightMomentsCache_ calculés UNE SEULE FOIS
//           et injectés dans leftSupportMomentsForSpan / rightSupportMomentsForSpan.
//           Avant : chacune des S appels recalculait tout → O(S²×N).
//           Après : calcul unique → O(S×N).
//
//  [OPT-3]  shearForceAbscissa_ construit en O(S×N) dans le constructeur.
//           Avant : recalcul de toutes les travées pour chaque tr → O(S²×N).
//
//  Dimensions des matrices (BM, SF, Def, Rot) : IDENTIQUES à l'original.
//    BM, Rot, Def  →  [S][N_span_s][N_total]
//    SF            →  [S][N_span_s][N_total + discontinuités]
// =============================================================================

ContinuousBeam::ContinuousBeam(std::vector<double>& E,
    std::vector<double>& I,
    std::vector<double>& L,
    double& steps)
    : steps(steps), spanLengths(L), elasticModuli(E), inertiaMoments(I)
{
    numberOfSpans = L.size();

    for (size_t i = 0; i < numberOfSpans; ++i)
    {
        IsostaticBeam span(E[i], I[i], L[i], steps);

        flexCoeff_a.push_back(span.a);
        flexCoeff_b.push_back(span.b);
        flexCoeff_c.push_back(span.c);

        if (L[i] != 0) {
            spanNodePositions.push_back(span.nodePositions);
            omegaSecond.push_back(span.Omega_Second);
            omegaPrime.push_back(span.Omega_Prime);
            staticBendingMoment.push_back(span.BendingMoment());
            staticRotation.push_back(span.Rotation());
            staticShearForceAbscissa.push_back(span.ShearForceAbscissa());
            staticShearForce.push_back(span.ShearForce());
            staticDeflection.push_back(span.Deflection());
        }
    }

    std::vector<int> zeroLengthIdx;
    for (size_t i = 0; i < numberOfSpans; ++i)
        if (L[i] == 0)
            zeroLengthIdx.push_back(static_cast<int>(i));

    // Calcul des coefficients de transfert (phy)
    double phi = 0;
    for (size_t i = 0; i < numberOfSpans; ++i) {
        if (i != 0)
            phi = flexCoeff_b[i] / (flexCoeff_a[i] + flexCoeff_c[i - 1] - flexCoeff_b[i - 1] * phi);
        transferCoeff.push_back(phi);
    }

    // Calcul des coefficients de transfert inverses (phy_prime)
    double phi_prime = 0;
    for (size_t i = numberOfSpans; i > 0; --i) {
        if (i != numberOfSpans)
            phi_prime = flexCoeff_b[i - 1] / (flexCoeff_c[i - 1] + flexCoeff_a[i] - flexCoeff_b[i] * phi_prime);
        transferCoeff_prime.push_back(phi_prime);
    }
    std::reverse(transferCoeff_prime.begin(), transferCoeff_prime.end());

    removeByIndices(transferCoeff, zeroLengthIdx);
    removeByIndices(transferCoeff_prime, zeroLengthIdx);
    removeByIndices(L, zeroLengthIdx);
    removeByIndices(E, zeroLengthIdx);
    removeByIndices(I, zeroLengthIdx);
    removeByIndices(flexCoeff_a, zeroLengthIdx);
    removeByIndices(flexCoeff_b, zeroLengthIdx);
    removeByIndices(flexCoeff_c, zeroLengthIdx);

    numberOfSpans = L.size();

    // [OPT-1] Nombre total de nœuds — calculé UNE SEULE FOIS
    total_nodes_ = 0;
    for (const auto& sp : spanNodePositions)
        total_nodes_ += sp.size();

    // [OPT-2] Caches moments — calculés UNE SEULE FOIS
    leftMomentsCache_ = computeLeftLoadedSupportMoments();
    rightMomentsCache_ = computeRightLoadedSupportMoments();

    for (int s = 0; s < static_cast<int>(numberOfSpans); ++s)
        spanLeftMoments.push_back(leftSupportMomentsForSpan(s, leftMomentsCache_));

    for (int s = 0; s < static_cast<int>(numberOfSpans); ++s)
        spanRightMoments.push_back(rightSupportMomentsForSpan(s, rightMomentsCache_));

    for (size_t i = 0; i < numberOfSpans; ++i) {
        std::vector<std::vector<double>> boundary;
        boundary.reserve(spanLeftMoments[i].size() + spanRightMoments[i].size());
        boundary.insert(boundary.end(), spanLeftMoments[i].begin(), spanLeftMoments[i].end());
        boundary.insert(boundary.end(), spanRightMoments[i].begin(), spanRightMoments[i].end());
        spanBoundaryMoments.push_back(std::move(boundary));
    }

    for (size_t j = 0; j <= numberOfSpans; ++j) {
        std::vector<std::vector<double>> supportCurve;
        supportCurve.reserve(numberOfSpans);
        for (size_t i = 0; i < numberOfSpans; ++i)
            supportCurve.push_back(spanBoundaryMoments[i][j]);
        supportMoments.push_back(flatten(std::move(supportCurve)));
    }

    // [OPT-3] Abscisses SF — une seule passe O(S×N)
    shearForceAbscissa_ = buildShearForceAbscissa();
}

// =============================================================================
//  buildShearForceAbscissa — O(S×N) au lieu de O(S²×N)
// =============================================================================
std::vector<std::vector<std::vector<double>>> ContinuousBeam::buildShearForceAbscissa()
{
    std::vector<std::vector<std::vector<double>>> result;
    result.reserve(numberOfSpans);

    auto perSpanAbscissae = spanNodePositions;

    for (size_t tr = 0; tr < numberOfSpans; ++tr) {
        std::vector<std::vector<double>> abscissae_tr;
        abscissae_tr.reserve(spanNodePositions[tr].size());

        for (size_t sec = 0; sec < spanNodePositions[tr].size(); ++sec) {
            perSpanAbscissae[tr] = staticShearForceAbscissa[tr][sec];
            abscissae_tr.push_back(globalXCoordinates(perSpanAbscissae));
            perSpanAbscissae[tr] = spanNodePositions[tr];
        }
        result.push_back(std::move(abscissae_tr));
    }
    return result;
}

// =============================================================================
//  globalXCoordinates
// =============================================================================
std::vector<double> ContinuousBeam::globalXCoordinates(
    const std::vector<std::vector<double>>& perSpanPoints)
{
    size_t total_size = 0;
    for (const auto& sp : perSpanPoints)
        total_size += sp.size();

    std::vector<double> x;
    x.reserve(total_size);

    for (size_t i = 0; i < perSpanPoints.size(); ++i) {
        double offset = 0.0;
        for (size_t j = 0; j < i; ++j)
            offset += spanLengths[j];
        for (const auto& alpha : perSpanPoints[i])
            x.push_back(alpha + offset);
    }
    return x;
}

// =============================================================================
//  Moments aux appuis
// =============================================================================

std::vector<std::vector<double>> ContinuousBeam::computeLeftLoadedSupportMoments()
{
    std::vector<std::vector<double>> M(numberOfSpans);
    for (size_t i = 0; i < numberOfSpans; ++i) {
        M[i].reserve(spanNodePositions[i].size());
        const double denom = 1.0 - transferCoeff[i] * transferCoeff_prime[i];
        for (size_t j = 0; j < spanNodePositions[i].size(); ++j) {
            double val = (transferCoeff[i] / flexCoeff_b[i]) *
                ((omegaPrime[i][j] + omegaSecond[i][j] * transferCoeff_prime[i]) / denom);
            M[i].push_back(val);
        }
    }
    return M;
}

std::vector<std::vector<double>> ContinuousBeam::computeRightLoadedSupportMoments()
{
    std::vector<std::vector<double>> M(numberOfSpans);
    for (size_t i = 0; i < numberOfSpans; ++i) {
        M[i].reserve(spanNodePositions[i].size());
        const double denom = 1.0 - transferCoeff[i] * transferCoeff_prime[i];
        for (size_t j = 0; j < spanNodePositions[i].size(); ++j) {
            double val = -(transferCoeff_prime[i] / flexCoeff_b[i]) *
                ((omegaPrime[i][j] * transferCoeff[i] + omegaSecond[i][j]) / denom);
            M[i].push_back(val);
        }
    }
    return M;
}

// [OPT-2] — cache injecté, pas de recalcul
std::vector<std::vector<double>> ContinuousBeam::leftSupportMomentsForSpan(
    int spanIndex,
    const std::vector<std::vector<double>>& leftCache)
{
    std::vector<std::vector<double>> result;
    result.reserve(spanIndex + 1);

    for (int i = 0; i <= spanIndex; ++i) {
        std::vector<double> row;
        row.reserve(leftCache[spanIndex].size());
        for (const auto& val : leftCache[spanIndex])
            row.push_back(std::pow(-1.0, spanIndex - i) *
                prod_list(transferCoeff, i, spanIndex - 1) * val);
        result.push_back(std::move(row));
    }
    return result;
}

std::vector<std::vector<double>> ContinuousBeam::rightSupportMomentsForSpan(
    int spanIndex,
    const std::vector<std::vector<double>>& rightCache)
{
    std::vector<std::vector<double>> result;
    result.reserve(numberOfSpans - spanIndex);

    for (int i = spanIndex, g = spanIndex; i < static_cast<int>(numberOfSpans); ++i, ++g) {
        double p = prod_list(transferCoeff_prime, spanIndex + 1, g);
        std::vector<double> row;
        row.reserve(rightCache[spanIndex].size());
        for (const auto& val : rightCache[spanIndex])
            row.push_back(std::pow(-1.0, spanIndex - g) * p * val);
        result.push_back(std::move(row));
    }
    return result;
}

// =============================================================================
//  BendingMoments — [S][N_span_s][N_total]
// =============================================================================
std::vector<std::vector<std::vector<double>>> ContinuousBeam::BendingMoments()
{
    std::vector<std::vector<std::vector<double>>> result;
    result.reserve(numberOfSpans);

    std::vector<std::future<std::vector<std::vector<double>>>> futures;
    futures.reserve(numberOfSpans);

    for (int s = 0; s < static_cast<int>(numberOfSpans); ++s) {
        futures.push_back(std::async(std::launch::async, [this, s]() {

            std::vector<std::vector<double>> bm_span;
            bm_span.reserve(spanNodePositions[s].size());

            int sectionIdx = 0;
            for (const auto& section : spanNodePositions[s]) {
                std::vector<double> row;
                row.reserve(total_nodes_); // [OPT-1]

                auto& staticRow = staticBendingMoment[s][sectionIdx++];

                for (int i = 0; i < static_cast<int>(numberOfSpans); ++i) {
                    for (int j = 0; j < static_cast<int>(spanNodePositions[i].size()); ++j) {
                        double hyp = BeamFormula_BendingMoment(
                            spanBoundaryMoments[i][s][j],
                            spanBoundaryMoments[i][s + 1][j],
                            section, spanLengths[s]);
                        row.push_back((s == i) ? staticRow[j] + hyp : hyp);
                    }
                }
                bm_span.push_back(std::move(row));
            }
            return bm_span;
            }));
    }

    for (auto& f : futures)
        result.push_back(f.get());

    return result;
}

// =============================================================================
//  Rotation — [S][N_span_s][N_total]
// =============================================================================
std::vector<std::vector<std::vector<double>>> ContinuousBeam::Rotation()
{
    std::vector<std::vector<std::vector<double>>> result;
    result.reserve(numberOfSpans);

    std::vector<std::future<std::vector<std::vector<double>>>> futures;
    futures.reserve(numberOfSpans);

    for (int s = 0; s < static_cast<int>(numberOfSpans); ++s) {
        futures.push_back(std::async(std::launch::async, [this, s]() {

            std::vector<std::vector<double>> rot_span;
            rot_span.reserve(spanNodePositions[s].size());

            for (int sec = 0; sec < static_cast<int>(spanNodePositions[s].size()); ++sec) {
                std::vector<double> row;
                row.reserve(total_nodes_); // [OPT-1]

                for (int i = 0; i < static_cast<int>(numberOfSpans); ++i) {
                    for (int j = 0; j < static_cast<int>(spanNodePositions[i].size()); ++j) {
                        double hyp = BeamFormula_Rotation(
                            spanBoundaryMoments[i][s][j],
                            spanBoundaryMoments[i][s + 1][j],
                            spanNodePositions[s][sec],
                            spanLengths[s], elasticModuli[s], inertiaMoments[s]);
                        row.push_back((s == i)
                            ? staticRotation[s][sec][j] + hyp : hyp);
                    }
                }
                rot_span.push_back(std::move(row));
            }
            return rot_span;
            }));
    }

    for (auto& f : futures)
        result.push_back(f.get());

    return result;
}

// =============================================================================
//  Deflection — [S][N_span_s][N_total]
// =============================================================================
std::vector<std::vector<std::vector<double>>> ContinuousBeam::Deflection()
{
    std::vector<std::vector<std::vector<double>>> result;
    result.reserve(numberOfSpans);

    std::vector<std::future<std::vector<std::vector<double>>>> futures;
    futures.reserve(numberOfSpans);

    for (int s = 0; s < static_cast<int>(numberOfSpans); ++s) {
        futures.push_back(std::async(std::launch::async, [this, s]() {

            std::vector<std::vector<double>> defl_span;
            defl_span.reserve(spanNodePositions[s].size());

            for (int sec = 0; sec < static_cast<int>(spanNodePositions[s].size()); ++sec) {
                std::vector<double> row;
                row.reserve(total_nodes_); // [OPT-1]

                for (int i = 0; i < static_cast<int>(numberOfSpans); ++i) {
                    for (int j = 0; j < static_cast<int>(spanNodePositions[i].size()); ++j) {
                        double hyp = BeamFormula_Deflection(
                            spanBoundaryMoments[i][s][j],
                            spanBoundaryMoments[i][s + 1][j],
                            spanNodePositions[s][sec],
                            spanLengths[s], elasticModuli[s], inertiaMoments[s]);
                        row.push_back((s == i)
                            ? staticDeflection[s][sec][j] + hyp : hyp);
                    }
                }
                defl_span.push_back(std::move(row));
            }
            return defl_span;
            }));
    }

    for (auto& f : futures)
        result.push_back(f.get());

    return result;
}

// =============================================================================
//  ShearForce — [S][N_span_s][N_total + discontinuités]
//  [OPT-3] getAllAbscissae=true : retourne le cache pré-calculé directement
// =============================================================================
std::vector<std::vector<std::vector<double>>> ContinuousBeam::ShearForce(bool getAllAbscissae)
{
    if (getAllAbscissae)
        return shearForceAbscissa_; // [OPT-3]

    std::vector<std::vector<std::vector<double>>> result;
    result.reserve(numberOfSpans);

    for (int s = 0; s < static_cast<int>(numberOfSpans); ++s) {
        std::vector<std::vector<double>> T_span;
        T_span.reserve(spanNodePositions[s].size());

        for (size_t secIdx = 0; secIdx < spanNodePositions[s].size(); ++secIdx) {
            const auto& section = spanNodePositions[s][secIdx];
            std::vector<double> row;

            // Estimation de la taille avec discontinuités
            size_t estimated = 0;
            for (int i = 0; i < static_cast<int>(numberOfSpans); ++i)
                estimated += (i == s)
                ? spanNodePositions[i].size() * 2
                : spanNodePositions[i].size();
            row.reserve(estimated);

            for (int i = 0; i < static_cast<int>(numberOfSpans); ++i) {
                if (s == i) {
                    size_t compteur = 0;
                    for (int j = 0; j < static_cast<int>(spanNodePositions[i].size()); ++j) {
                        double hyp = BeamFormula_ShearForce(
                            spanBoundaryMoments[i][s][j],
                            spanBoundaryMoments[i][s + 1][j],
                            spanLengths[s]);

                        if (secIdx < staticShearForce[s].size() &&
                            compteur < staticShearForce[s][secIdx].size())
                        {
                            if (j < static_cast<int>(spanNodePositions[s].size()) &&
                                section == spanNodePositions[s][j])
                            {
                                row.push_back(staticShearForce[s][secIdx][compteur] + hyp);
                                ++compteur;
                                if (compteur < staticShearForce[s][secIdx].size()) {
                                    row.push_back(staticShearForce[s][secIdx][compteur] + hyp);
                                    ++compteur;
                                }
                            }
                            else {
                                row.push_back(staticShearForce[s][secIdx][compteur] + hyp);
                                ++compteur;
                            }
                        }
                    }
                }
                else {
                    for (int j = 0; j < static_cast<int>(spanNodePositions[i].size()); ++j) {
                        row.push_back(BeamFormula_ShearForce(
                            spanBoundaryMoments[i][s][j],
                            spanBoundaryMoments[i][s + 1][j],
                            spanLengths[s]));
                    }
                }
            }
            T_span.push_back(std::move(row));
        }
        result.push_back(std::move(T_span));
    }

    return result;
}