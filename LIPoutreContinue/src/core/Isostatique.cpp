#include "LIPoutreContinue/core/Isostatique.h"

// FIX: steps par valeur (plus par référence)
Isostatique::Isostatique(double E, double I, double L, double steps)
    : E(E), I(I), L(L), steps(steps)
{
    // FIX: calcul correct du nombre de noeuds pour reserve()
    size_t estimatedNodes = static_cast<size_t>(L / steps) + 2;
    nodePositions.reserve(estimatedNodes);

    // Generate node positions
    double pas = 0;
    while (pas < L) {
        nodePositions.push_back(pas);
        pas += steps;
    }
    nodePositions.push_back(L);

    // Pre-calculate coefficients
    const double EI = E * I;
    const double EI_L = EI * L;

    a = L / (3 * EI);
    c = a;
    b = L / (6 * EI);

    Omega_Second.reserve(nodePositions.size());
    Omega_Prime.reserve(nodePositions.size());

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
