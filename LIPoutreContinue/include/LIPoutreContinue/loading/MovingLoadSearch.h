#pragma once
// =============================================================================
//  MovingLoadSearch — position la plus défavorable d'UNE charge mobile sur
//  UNE ligne d'influence quelconque (section, réaction, …).
//
//  Module de calcul pur : aucune dépendance à JSON, au disque ou à Python.
//  Utilisé par Loading (lignes de M, V, w, θ par section) et par Output
//  (lignes de réactions d'appui).
//
//  Modèle mathématique : voir Loading.h et docs/audit/03_numerique.md.
// =============================================================================

#include "LIPoutreContinue/Utils.h"   // struct load

#include <vector>

namespace lipc {

// Ligne d'influence : interpolant linéaire par morceaux des ordonnées `y` aux
// abscisses globales `t` (croissantes ; une abscisse répétée = discontinuité),
// nul hors de [t.front(), t.back()].
class InfluenceLine {
public:
    InfluenceLine(std::vector<double> t, std::vector<double> y);

    double left(double q) const;      // f(q⁻)
    double right(double q) const;     // f(q⁺)
    double integral(double q) const;  // ∫_{-∞}^{q} f

    const std::vector<double>& abscissae() const { return t_; }

private:
    std::vector<double> t_, y_, C_;
};

// Extrêmes d'une charge : max ≥ 0 et min ≤ 0 (charge absente = 0).
struct Extreme {
    double max = 0.0, maxOrigin = 0.0;
    double min = 0.0, minOrigin = 0.0;
    bool hasMax = false, hasMin = false;   // false : extrême = charge absente
};

// Convoi : essieu i à origine + d_i, d = cumul de `length` (d0 = length[0]).
Extreme pointLoadExtreme(const InfluenceLine& line, const load& l);

// Charge répartie : tronçon k sur [origine + c_k, origine + c_{k+1}].
Extreme distributedLoadExtreme(const InfluenceLine& line, const load& l);

// Abscisses relatives cumulées (count valeurs). Lève std::invalid_argument.
std::vector<double> cumulativeOffsets(const std::vector<double>& length, size_t count, const char* what);

} // namespace lipc
