#include "LIPoutreContinue/core/Isostatique.h"

#include <cmath>
#include <stdexcept>
#include <string>

// =============================================================================
//  Maillage déterministe (défaut N8, docs/audit/03_numerique.md).
//
//  Les nœuds sont x_i = i·h calculés par MULTIPLICATION (et non par
//  accumulation `x += h`, qui dérive de quelques ulp à chaque pas). Le dernier
//  nœud intérieur est abandonné s'il est à moins de 1e-9·h de L : sans cela,
//  L = 6, h = 0,1 produisait un nœud à 5,999999999999995 puis L, soit un
//  élément de 5e-15 m. Si h ne divise pas L, le dernier élément est plus
//  court que h (comportement inchangé).
// =============================================================================
static std::vector<double> buildSpanMesh(double L, double h)
{
    std::vector<double> x;
    if (L == 0.0) {             // travée nulle = encastrement (un seul nœud)
        x.push_back(0.0);
        return x;
    }
    const double tol = 1e-9 * h;
    const size_t n = static_cast<size_t>(std::ceil(L / h - 1e-9)); // nombre d'éléments
    x.reserve(n + 1);
    for (size_t i = 0; i < n; ++i) {
        const double xi = static_cast<double>(i) * h;
        if (xi >= L - tol) break;
        x.push_back(xi);
    }
    x.push_back(L);
    return x;
}

Isostatique::Isostatique(double E, double I, double L, double steps)
    : E(E), I(I), L(L), steps(steps)
{
    if (!(std::isfinite(steps) && steps > 0.0))
        throw std::invalid_argument("Isostatique: le pas 'steps' doit être un réel strictement positif, reçu "
                                    + std::to_string(steps) + ".");
    if (!(std::isfinite(L) && L >= 0.0))
        throw std::invalid_argument("Isostatique: la longueur L doit être un réel positif ou nul (0 = encastrement), reçu "
                                    + std::to_string(L) + ".");
    if (L > 0.0 && !(std::isfinite(E * I) && E > 0.0 && I > 0.0))
        throw std::invalid_argument("Isostatique: E et I doivent être strictement positifs pour une travée de longueur non nulle.");

    nodePositions = buildSpanMesh(L, steps);

    // Pre-calculate coefficients
    const double EI = E * I;
    const double EI_L = EI * L;

    // Travée nulle (encastrement) : souplesse nulle, quel que soit E·I
    // (évite 0/0 si l'appelant fournit E = 0 ou I = 0 pour cette travée).
    a = (L == 0.0) ? 0.0 : L / (3 * EI);
    c = a;
    b = (L == 0.0) ? 0.0 : L / (6 * EI);

    Omega_Second.reserve(nodePositions.size());
    Omega_Prime.reserve(nodePositions.size());

    if (L == 0.0) {                       // pas de ligne d'influence propre
        Omega_Second.push_back(0.0);
        Omega_Prime.push_back(0.0);
        return;
    }
    for (double pos : nodePositions) {
        Omega_Second.push_back(pos * (L - pos) * (L + pos) / (6 * EI_L));
        Omega_Prime.push_back(-pos * (L - pos) * (2 * L - pos) / (6 * EI_L));
    }
}

std::vector<double> Isostatique::Eq_ShearForce(double x, bool returnAbscissa)
{
    std::vector<double> shearForce;
    std::vector<double> coordinates;

    if (x <= L) {
        for (double pos : nodePositions) {
            if (pos < x) {
                shearForce.push_back(-pos / L);
                coordinates.push_back(pos);
            }
            else if (pos > x) {
                shearForce.push_back(1.0 - pos / L);
                coordinates.push_back(pos);
            }
            else { // pos == x : discontinuité — on insère les deux valeurs
                shearForce.push_back(-pos / L);
                coordinates.push_back(pos);
                shearForce.push_back(1.0 - pos / L);
                coordinates.push_back(pos);
            }
        }
    }
    else {
        // FIX: utiliser nodePositions.size() au lieu de steps (qui est un pas, pas un count)
        shearForce.assign(nodePositions.size(), 0.0);
        coordinates = nodePositions;
    }

    return returnAbscissa ? coordinates : shearForce;
}

std::vector<std::vector<double>> Isostatique::ShearForce()
{
    std::vector<std::vector<double>> shearForces;
    shearForces.reserve(nodePositions.size());

    for (double pos : nodePositions) {
        shearForces.push_back(Eq_ShearForce(pos, false));
    }
    return shearForces;
}

std::vector<std::vector<double>> Isostatique::ShearForceAbscissa()
{
    std::vector<std::vector<double>> shearForces;
    shearForces.reserve(nodePositions.size());

    for (double pos : nodePositions) {
        shearForces.push_back(Eq_ShearForce(pos, true));
    }
    return shearForces;
}

std::vector<double> Isostatique::Eq_BendingMoment(double x)
{
    std::vector<double> moment;

    if (x <= L) {
        for (double pos : nodePositions) {
            if (pos <= x) {
                moment.push_back(pos * (1.0 - x / L));
            }
            else {
                moment.push_back(x * (1.0 - pos / L));
            }
        }
    }
    else {
        // FIX: utiliser nodePositions.size() au lieu de steps
        moment.assign(nodePositions.size(), 0.0);
    }
    return moment;
}

std::vector<std::vector<double>> Isostatique::BendingMoment()
{
    std::vector<std::vector<double>> bendingMoments;
    bendingMoments.reserve(nodePositions.size());

    for (double pos : nodePositions) {
        bendingMoments.push_back(Eq_BendingMoment(pos));
    }
    return bendingMoments;
}

std::vector<double> Isostatique::Eq_Deflection(double x)
{
    std::vector<double> deflection;
    const double EI = E * I;
    const double EI_L = EI * L;

    for (double pos : nodePositions) {
        double f;
        if (pos <= x) {
            f = -(pos * (L - x) / (6.0 * EI_L)) * (x * (2.0 * L - x) - pos * pos);
        }
        else {
            f = -(x * (L - pos) / (6.0 * EI_L)) * (pos * (2.0 * L - pos) - x * x);
        }
        deflection.push_back(f);
    }
    return deflection;
}

std::vector<std::vector<double>> Isostatique::Deflection()
{
    std::vector<std::vector<double>> deflections;
    deflections.reserve(nodePositions.size());

    for (double pos : nodePositions) {
        deflections.push_back(Eq_Deflection(pos));
    }
    return deflections;
}

std::vector<double> Isostatique::Eq_Rotation(double x)
{
    std::vector<double> rotation;
    const double EI = E * I;
    const double EI_L = EI * L;

    for (double pos : nodePositions) {
        double angle;
        if (pos < x) {
            // FIX: séparation correcte des cas pos < x et pos > x
            angle = ((L - pos) * (L + pos) - 3.0 * (L - x) * (L - x)) * pos / (6.0 * EI_L);
        }
        else if (pos > x) {
            angle = -(pos * (2.0 * L - pos) - 3.0 * x * x) * (L - pos) / (6.0 * EI_L);
        }
        else {
            // FIX: cas pos == x — les deux expressions donnent la même valeur ici,
            // on utilise la formule gauche (limite par la gauche)
            angle = ((L - pos) * (L + pos) - 3.0 * (L - x) * (L - x)) * pos / (6.0 * EI_L);
        }
        rotation.push_back(angle);
    }
    return rotation;
}

std::vector<std::vector<double>> Isostatique::Rotation()
{
    std::vector<std::vector<double>> rotations;
    rotations.reserve(nodePositions.size());

    for (double pos : nodePositions) {
        rotations.push_back(Eq_Rotation(pos));
    }
    return rotations;
}
