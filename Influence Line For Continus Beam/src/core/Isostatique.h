#pragma once
#ifndef __ISO__
#define __ISO__

#include <vector>

class Isostatique
{
public:
    double E;                                   // Modulus of elasticity
    double I;                                   // Moment of inertia
    double L;                                   // Length of the beam
    double steps;                               // Step size (FIX: valeur, pas référence)

    std::vector<double> nodePositions;          // Positions of the nodes
    std::vector<double> Omega_Second;           // Second flexibility coefficient
    std::vector<double> Omega_Prime;            // First flexibility coefficient

    Isostatique(double E, double I, double L, double steps); // FIX: pass by value

    // Flexibility coefficients
    double a;
    double b;
    double c;

    // Shear force methods
    std::vector<double> Eq_ShearForce(double x, bool returnAbscissa);
    std::vector<std::vector<double>> ShearForce();
    std::vector<std::vector<double>> ShearForceAbscissa();

    // Bending moment methods
    std::vector<double> Eq_BendingMoment(double x);
    std::vector<std::vector<double>> BendingMoment();

    // Deflection methods
    std::vector<double> Eq_Deflection(double x);
    std::vector<std::vector<double>> Deflection();

    // Rotation methods
    std::vector<double> Eq_Rotation(double x);
    std::vector<std::vector<double>> Rotation();
};

#endif // __ISO__