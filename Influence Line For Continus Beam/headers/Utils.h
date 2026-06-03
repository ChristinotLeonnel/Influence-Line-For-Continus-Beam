/**
 * @file Utils.h
 * @brief Utilitaires généraux pour l'analyse structurelle de poutres continues.
 *
 * Fonctionnalités :
 *  - Structures de données (load, Position1D/2D/3D, SpanStats, ZeroCrossing, ...)
 *  - Recherche de valeurs maximales absolues, positives, négatives (1D/2D/3D)
 *  - Extrema par travée + statistiques (min, max, mean, rms, std_dev)
 *  - Passages par zéro des lignes d'influence
 *  - Calcul de courbure κ = M/(EI) et énergie élastique U(α)
 *  - Réactions d'appui (lignes d'influence)
 *  - Intégration numérique (trapèzes de Kahan)
 *  - Export JSON pour tous les résultats
 *  - Parsing du fichier de configuration
 *  - Génération du fichier de configuration par défaut
 *
 * @note Unités : mètres (m), kiloNewtons (kN), Pascals (Pa), m⁴
 */

#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

using json = nlohmann::json;

// =============================================================================
//  STRUCTURES DE DONNÉES (existantes)
// =============================================================================

/**
 * @brief Représente une charge appliquée sur la structure.
 *
 * Une charge est définie par une liste d'intensités (kN ou kN/m) et une liste
 * de longueurs associées (m), ainsi qu'un nom identifiant le cas de charge.
 * Les vecteurs Intensity et Length sont parallèles : Intensity[i] s'applique
 * sur la longueur Length[i].
 */
struct load {
    std::vector<double> Intensity; ///< Intensités de la charge (kN ou kN/m)
    std::vector<double> Length;    ///< Longueurs des segments de charge (m)
    std::string name;              ///< Nom/identifiant du cas de charge
};

/**
 * @brief Position d'un extremum dans une matrice 2D.
 *
 * Utilisé pour retourner la localisation exacte d'une valeur remarquable
 * (max absolu, max positif, min négatif) dans un tableau à deux dimensions
 * [travée][section] ou [travée][alpha].
 */
struct Position2D {
    size_t i;   ///< Indice de la première dimension (ex. travée)
    size_t j;   ///< Indice de la seconde dimension (ex. section ou alpha)
    double val; ///< Valeur à cette position
};

/**
 * @brief Position d'un extremum dans un tenseur 3D.
 *
 * Utilisé pour retourner la localisation exacte d'une valeur remarquable
 * dans un tableau [travée][section][alpha].
 */
struct Position3D {
    size_t i;   ///< Indice de la travée (span)
    size_t j;   ///< Indice de la section
    size_t k;   ///< Indice de la position de charge (alpha)
    double val; ///< Valeur à cette position
};

/**
 * @brief Position d'un extremum dans un vecteur 1D.
 *
 * Utilisé pour les résultats scalaires indexés par une seule dimension
 * (ex. : énergie élastique U[alpha] ou réaction d'appui ponctuelle).
 */
struct Position1D {
    size_t max_position; ///< Indice de la valeur maximale dans le vecteur
    double value;        ///< Valeur maximale
};

/**
 * @brief Résultat de la combinaison de cas de charges donnant un extremum.
 *
 * Regroupe la position (indice et abscisse) de l'extremum combiné,
 * la valeur résultante, et le détail des contributions de chaque cas de charge
 * dans la map Addition.
 */
struct CombineLoadPosition {
    size_t max_position; ///< Indice alpha de l'extremum dans le vecteur global
    double position;     ///< Abscisse réelle de la charge (m)
    double value;        ///< Valeur combinée à cet extremum
    /// Contributions par cas de charge : Addition[nom_cas][nom_parametre] = valeur
    std::map<std::string, std::map<std::string, double>> Addition;
};

/**
 * @brief Résultat d'analyse pour la livraison d'un cas de charge critique.
 *
 * Regroupe, pour une combinaison de charges donnée, la valeur maximale atteinte,
 * sa localisation dans la structure (travée, section, abscisse), et le détail
 * de la charge appliquée.
 */
struct load_delivery {
    /// Détail de la charge par cas : load[nom_cas][nom_parametre] = valeur
    std::map<std::string, std::map<std::string, double>> load;
    size_t span;           ///< Indice de la travée concernée
    size_t section;        ///< Indice de la section concernée dans la travée
    double maximum_value;  ///< Valeur maximale de la réponse (ex. moment en kN·m)
    double position = 0.0; ///< Abscisse de la charge critique (m)
};

// =============================================================================
//  NOUVELLES STRUCTURES
// =============================================================================

/**
 * @brief Statistiques descriptives pour une travée d'un tenseur 3D.
 *        sec_* / alpha_* indiquent la position des extrema dans la travée.
 */
struct SpanStats {
    double min_val = 0.0;   ///< Valeur minimale (la plus négative)
    double max_val = 0.0;   ///< Valeur maximale (la plus positive)
    double mean = 0.0;   ///< Moyenne arithmétique
    double rms = 0.0;   ///< Valeur efficace (root mean square)
    double std_dev = 0.0;   ///< Écart-type (échantillon N-1)
    size_t sec_min = 0;     ///< Section de la valeur min
    size_t alpha_min = 0;     ///< Alpha de la valeur min
    size_t sec_max = 0;     ///< Section de la valeur max
    size_t alpha_max = 0;     ///< Alpha de la valeur max
    size_t count = 0;     ///< Nombre total d'éléments
};

/**
 * @brief Passage par zéro d'une ligne d'influence.
 *
 * Indique que pour la section j, la réponse change de signe quand la charge
 * passe de X[alpha_before] à X[alpha_after]. x_approx est la position
 * interpolée linéairement de la charge au zéro exact.
 */
struct ZeroCrossing {
    size_t section;       ///< Indice de section dans la travée
    size_t alpha_before;  ///< Dernier alpha avant le zéro
    size_t alpha_after;   ///< Premier alpha après le zéro
    double x_approx;      ///< Position interpolée de la charge (m)
};


// =============================================================================
//  FONCTIONS UTILITAIRES — Opérations de base (existantes)
// =============================================================================

/**
 * @brief Retourne l'élément de valeur absolue maximale dans un vecteur.
 *
 * Parcourt tous les éléments et retourne celui dont la valeur absolue est
 * la plus grande, en conservant son signe original.
 *
 * @tparam T  Type arithmétique (double, float, int…)
 * @param vec Vecteur d'entrée
 * @return    Élément de plus grande valeur absolue (signé), ou 0 si vide
 */
template <typename T>
static constexpr T MaxValueInVector(const std::vector<T>& vec) {
    if (vec.empty()) return 0;
    T maxVal = vec[0];
    for (const auto& val : vec)
        if (std::abs(val) > std::abs(maxVal))
            maxVal = val;
    return maxVal;
}

/**
 * @brief Supprime les éléments d'un vecteur aux indices spécifiés.
 *
 * Construit un nouveau vecteur en omettant les positions listées dans
 * @p indices, puis l'échange avec @p v. Les indices hors bornes sont ignorés.
 * Complexité : O(n) en temps et en mémoire.
 *
 * @tparam T       Type des éléments du vecteur
 * @tparam U       Type entier des indices (size_t, int…)
 * @param v        Vecteur modifié en place
 * @param indices  Liste des indices à supprimer (sans ordre requis)
 */
template <typename T, typename U>
void removeByIndices(std::vector<T>& v, const std::vector<U>& indices) {
    std::vector<bool> toRemove(v.size(), false);
    for (U i : indices)
        if (i < static_cast<U>(v.size()))
            toRemove[i] = true;
    std::vector<T> result;
    result.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i)
        if (!toRemove[i])
            result.push_back(v[i]);
    v.swap(result);
}

/**
 * @brief Intègre numériquement y(x) par la méthode des trapèzes de Kahan.
 *
 * Calcule ∫ y dx ≈ Σ (y[i] + y[i+1]) · (x[i+1] − x[i]) / 2
 * en utilisant la compensation d'erreur de Kahan pour limiter l'accumulation
 * d'erreurs d'arrondi flottant sur de longues séries.
 *
 * @tparam T  Type flottant (double recommandé)
 * @param x   Abscisses (strictement croissantes)
 * @param y   Valeurs correspondantes (même taille que x)
 * @return    Aire signée sous la courbe
 *
 * @pre  x.size() == y.size() et x.size() >= 2
 */
template <typename T>
static T trapeze(const std::vector<T>& x, const std::vector<T>& y) {
    T aire = 0, err = 0;
    for (size_t i = 0; i < x.size() - 1; ++i) {
        T h = x[i + 1] - x[i];
        T a = (y[i] + y[i + 1]) * h / 2;
        T t = aire + a;
        err += (aire - t) + a;
        aire = t;
    }
    return aire + err;
}

// --- Recherche max absolu (existantes) ----------------------------------------

/**
 * @brief Recherche la valeur de plus grande valeur absolue dans un tenseur 3D.
 *
 * Parcourt l'ensemble des éléments data[i][j][k] et retourne la position
 * et la valeur signée de celui dont |valeur| est maximale.
 * En cas d'égalité, la dernière occurrence trouvée est retournée.
 *
 * @param data  Tenseur [span][section][alpha]
 * @return      Position3D {i, j, k, valeur signée}
 */
static Position3D findMaxAbsoluteValue3D(
    const std::vector<std::vector<std::vector<double>>>& data)
{
    double maxAbs = std::numeric_limits<double>::lowest();
    Position3D pos{};
    for (size_t i = 0; i < data.size(); ++i)
        for (size_t j = 0; j < data[i].size(); ++j)
            for (size_t k = 0; k < data[i][j].size(); ++k) {
                double absVal = std::abs(data[i][j][k]);
                if (absVal >= maxAbs) { maxAbs = absVal; pos = { i, j, k, data[i][j][k] }; }
            }
    return pos;
}

/**
 * @brief Recherche la valeur de plus grande valeur absolue dans une matrice 2D.
 *
 * Parcourt l'ensemble des éléments data[i][j] et retourne la position
 * et la valeur signée de celui dont |valeur| est maximale.
 * En cas d'égalité, la dernière occurrence trouvée est retournée.
 *
 * @param data  Matrice [span][alpha] ou [span][section]
 * @return      Position2D {i, j, valeur signée}
 */
static Position2D findMaxAbsoluteValue2D(
    const std::vector<std::vector<double>>& data)
{
    double maxAbs = std::numeric_limits<double>::lowest();
    Position2D pos{};
    for (size_t i = 0; i < data.size(); ++i)
        for (size_t j = 0; j < data[i].size(); ++j) {
            double absVal = std::abs(data[i][j]);
            if (absVal >= maxAbs) { maxAbs = absVal; pos = { i, j, data[i][j] }; }
        }
    return pos;
}


// =============================================================================
//  NOUVELLES FONCTIONS — Extrema signés (max positif / min négatif)
// =============================================================================

/// Retourne la plus grande valeur positive du tenseur 3D.
static Position3D findMaxPositiveValue3D(
    const std::vector<std::vector<std::vector<double>>>& data)
{
    double best = std::numeric_limits<double>::lowest();
    Position3D pos{ 0, 0, 0, best };
    for (size_t i = 0; i < data.size(); ++i)
        for (size_t j = 0; j < data[i].size(); ++j)
            for (size_t k = 0; k < data[i][j].size(); ++k)
                if (data[i][j][k] > best) { best = data[i][j][k]; pos = { i, j, k, best }; }
    return pos;
}

/// Retourne la valeur la plus négative du tenseur 3D.
static Position3D findMinValue3D(
    const std::vector<std::vector<std::vector<double>>>& data)
{
    double best = std::numeric_limits<double>::max();
    Position3D pos{ 0, 0, 0, best };
    for (size_t i = 0; i < data.size(); ++i)
        for (size_t j = 0; j < data[i].size(); ++j)
            for (size_t k = 0; k < data[i][j].size(); ++k)
                if (data[i][j][k] < best) { best = data[i][j][k]; pos = { i, j, k, best }; }
    return pos;
}

/// Retourne la plus grande valeur positive de la matrice 2D.
static Position2D findMaxPositiveValue2D(
    const std::vector<std::vector<double>>& data)
{
    double best = std::numeric_limits<double>::lowest();
    Position2D pos{ 0, 0, best };
    for (size_t i = 0; i < data.size(); ++i)
        for (size_t j = 0; j < data[i].size(); ++j)
            if (data[i][j] > best) { best = data[i][j]; pos = { i, j, best }; }
    return pos;
}

/// Retourne la valeur la plus négative de la matrice 2D.
static Position2D findMinValue2D(
    const std::vector<std::vector<double>>& data)
{
    double best = std::numeric_limits<double>::max();
    Position2D pos{ 0, 0, best };
    for (size_t i = 0; i < data.size(); ++i)
        for (size_t j = 0; j < data[i].size(); ++j)
            if (data[i][j] < best) { best = data[i][j]; pos = { i, j, best }; }
    return pos;
}


// =============================================================================
//  NOUVELLES FONCTIONS — Statistiques par travée
// =============================================================================

/**
 * @brief Calcule min, max, mean, rms, std_dev pour chaque travée d'un tenseur 3D.
 * @param data  Tenseur [span][section][alpha]
 * @return      Vecteur de SpanStats, un élément par travée.
 */
static std::vector<SpanStats> computeSpanStats3D(
    const std::vector<std::vector<std::vector<double>>>& data)
{
    std::vector<SpanStats> result;
    result.reserve(data.size());

    for (const auto& span : data) {
        SpanStats s;
        s.min_val = std::numeric_limits<double>::max();
        s.max_val = std::numeric_limits<double>::lowest();
        double sum = 0.0, sum_sq = 0.0;

        for (size_t j = 0; j < span.size(); ++j)
            for (size_t k = 0; k < span[j].size(); ++k) {
                const double v = span[j][k];
                sum += v;  sum_sq += v * v;  ++s.count;
                if (v < s.min_val) { s.min_val = v; s.sec_min = j; s.alpha_min = k; }
                if (v > s.max_val) { s.max_val = v; s.sec_max = j; s.alpha_max = k; }
            }

        if (s.count > 0) {
            s.mean = sum / static_cast<double>(s.count);
            s.rms = std::sqrt(sum_sq / static_cast<double>(s.count));
            double var = 0.0;
            for (const auto& sec : span)
                for (double v : sec) var += (v - s.mean) * (v - s.mean);
            s.std_dev = (s.count > 1)
                ? std::sqrt(var / static_cast<double>(s.count - 1))
                : 0.0;
        }
        result.push_back(s);
    }
    return result;
}


// =============================================================================
//  NOUVELLES FONCTIONS — Passages par zéro
// =============================================================================

/**
 * @brief Détecte les changements de signe des lignes d'influence.
 *
 * Pour chaque travée (span) et chaque section j, retourne les paires
 * (alpha_before, alpha_after) encadrant un passage par zéro, avec
 * interpolation linéaire de la position exacte de la charge (x_approx).
 *
 * @param data  Tenseur [span][section][alpha]
 * @param X     Coordonnées globales X[alpha] (m)
 * @return      crossings[span] = liste de ZeroCrossing
 */
static std::vector<std::vector<ZeroCrossing>> findZeroCrossings3D(
    const std::vector<std::vector<std::vector<double>>>& data,
    const std::vector<double>& X)
{
    std::vector<std::vector<ZeroCrossing>> result;
    result.reserve(data.size());

    for (const auto& span : data) {
        std::vector<ZeroCrossing> crossings;
        for (size_t j = 0; j < span.size(); ++j)
            for (size_t k = 1; k < span[j].size() && k < X.size(); ++k) {
                const double y0 = span[j][k - 1], y1 = span[j][k];
                if (y0 * y1 < 0.0) {
                    const double x_zero = X[k - 1] + (X[k] - X[k - 1]) * (-y0) / (y1 - y0);
                    crossings.push_back({ j, k - 1, k, x_zero });
                }
            }
        result.push_back(crossings);
    }
    return result;
}


// =============================================================================
//  NOUVELLES FONCTIONS — Courbure κ = M/(EI)
// =============================================================================

/**
 * @brief Calcule les lignes d'influence de la courbure κ = M / (E·I).
 *
 * La courbure est la dérivée seconde du déplacement et est directement
 * proportionnelle au moment fléchissant. Elle caractérise la déformation
 * locale de la fibre neutre.
 *
 * @param BM       Tenseur [span][section][alpha]  (kN·m)
 * @param E_spans  Modules de Young par travée      (Pa)
 * @param I_spans  Moments d'inertie par travée     (m⁴)
 * @return         Tenseur courbure [span][section][alpha]  (rad/m par kN)
 */
static std::vector<std::vector<std::vector<double>>> computeCurvature3D(
    const std::vector<std::vector<std::vector<double>>>& BM,
    const std::vector<double>& E_spans,
    const std::vector<double>& I_spans)
{
    std::vector<std::vector<std::vector<double>>> Curv(BM.size());
    for (size_t k = 0; k < BM.size(); ++k) {
        const double EI_k = (k < E_spans.size() && k < I_spans.size())
            ? E_spans[k] * I_spans[k] : 1.0;
        Curv[k].resize(BM[k].size());
        for (size_t j = 0; j < BM[k].size(); ++j) {
            Curv[k][j].resize(BM[k][j].size());
            for (size_t alpha = 0; alpha < BM[k][j].size(); ++alpha)
                Curv[k][j][alpha] = (EI_k != 0.0) ? BM[k][j][alpha] / EI_k : 0.0;
        }
    }
    return Curv;
}


// =============================================================================
//  NOUVELLES FONCTIONS — Énergie élastique U(α) = Σ∫M²/(2EI)dx
// =============================================================================

/**
 * @brief Calcule l'énergie de déformation élastique pour chaque position de charge.
 *
 * U(α) = Σ_{k=0}^{n_spans-1}  (1 / (2·EI_k)) · ∫₀^{L_k} M_k(x, α)² dx
 *
 * L'intégrale est évaluée par la méthode des trapèzes sur les nœuds de la travée.
 * U(α) est le scalaire d'énergie total stocké dans la poutre pour une charge
 * unité placée à X[α]. Son maximum identifie la position la plus critique en
 * termes de déformation globale.
 *
 * @param BM                Tenseur [span][section][alpha]
 * @param E_spans           Modules de Young par travée
 * @param I_spans           Moments d'inertie par travée
 * @param SpanNodePositions Positions locales des nœuds [span][node] (m)
 * @param n_alpha           Nombre de positions de charge
 * @return                  Vecteur U[alpha]  (kN·m)
 */
static std::vector<double> computeElasticEnergy(
    const std::vector<std::vector<std::vector<double>>>& BM,
    const std::vector<double>& E_spans,
    const std::vector<double>& I_spans,
    const std::vector<std::vector<double>>& SpanNodePositions,
    size_t n_alpha)
{
    std::vector<double> energy(n_alpha, 0.0);
    for (size_t alpha = 0; alpha < n_alpha; ++alpha) {
        double U = 0.0;
        for (size_t k = 0; k < BM.size(); ++k) {
            if (k >= E_spans.size() || k >= I_spans.size()) continue;
            const double EI_k = E_spans[k] * I_spans[k];
            if (EI_k == 0.0) continue;

            std::vector<double> m2;
            m2.reserve(BM[k].size());
            for (size_t j = 0; j < BM[k].size(); ++j) {
                const double m = (alpha < BM[k][j].size()) ? BM[k][j][alpha] : 0.0;
                m2.push_back(m * m);
            }
            if (k < SpanNodePositions.size() && !SpanNodePositions[k].empty())
                U += trapeze(SpanNodePositions[k], m2) / (2.0 * EI_k);
        }
        energy[alpha] = U;
    }
    return energy;
}


// =============================================================================
//  NOUVELLES FONCTIONS — Réactions d'appui (lignes d'influence)
// =============================================================================

/**
 * @brief Calcule les lignes d'influence des réactions d'appui R_j(α).
 *
 * Pour une poutre continue à n travées (n+1 appuis), la réaction à l'appui j
 * due à une charge unité en X[α] est obtenue par équilibre :
 *
 *   Pour chaque travée k (entre appuis k et k+1, longueur L_k) :
 *     Cisaillement dû aux moments : V_gauche = (M_k(α) − M_{k+1}(α)) / L_k
 *     Cisaillement isostatique (si charge dans la travée) :
 *       V_gauche += (L_k − local_x) / L_k
 *       V_droite += local_x / L_k
 *     R[k][α]   += V_gauche
 *     R[k+1][α] += V_droite
 *
 * Convention : SupportMoment[k][α] = moment de continuité à la jonction droite
 * de la travée k (moment à l'appui intérieur k). Les appuis extrêmes ont M=0.
 *
 * @param SupportMoment   [span][alpha] — moments aux appuis
 * @param L_spans         Longueurs des travées (m)
 * @param NodeLengths     Abscisses cumulées des appuis (m)
 * @param X               Coordonnées globales X[alpha] (m)
 * @return                R[support][alpha]  (kN par kN appliqué)
 */
static std::vector<std::vector<double>> computeSupportReactions(
    const std::vector<std::vector<double>>& SupportMoment,
    const std::vector<double>& L_spans,
    const std::vector<double>& NodeLengths,
    const std::vector<double>& X)
{
    const size_t n_spans = L_spans.size();
    const size_t n_sup = n_spans + 1;
    const size_t n_alpha = X.size();

    std::vector<std::vector<double>> R(n_sup, std::vector<double>(n_alpha, 0.0));

    for (size_t k = 0; k < n_spans; ++k) {
        const double Lk = L_spans[k];
        const double x_left = (k < NodeLengths.size()) ? NodeLengths[k] : 0.0;
        const double x_right = (k + 1 < NodeLengths.size()) ? NodeLengths[k + 1] : x_left + Lk;

        for (size_t alpha = 0; alpha < n_alpha; ++alpha) {
            // Moments aux deux extrémités de la travée k (0 aux appuis de rive)
            const double M_L = (k < SupportMoment.size() &&
                alpha < SupportMoment[k].size())
                ? SupportMoment[k][alpha] : 0.0;
            const double M_R = (k + 1 < SupportMoment.size() &&
                alpha < SupportMoment[k + 1].size())
                ? SupportMoment[k + 1][alpha] : 0.0;

            // Cisaillement dû aux moments (correction hyperstatique)
            const double V_g_m = (Lk > 0.0) ? (M_L - M_R) / Lk : 0.0;
            const double V_d_m = -V_g_m;

            // Cisaillement isostatique (charge dans la travée)
            double V_g_p = 0.0, V_d_p = 0.0;
            const double xa = X[alpha];
            if (xa >= x_left && xa <= x_right && Lk > 0.0) {
                const double lx = xa - x_left;
                V_g_p = (Lk - lx) / Lk;
                V_d_p = lx / Lk;
            }

            R[k][alpha] += V_g_m + V_g_p;
            R[k + 1][alpha] += V_d_m + V_d_p;
        }
    }
    return R;
}


// =============================================================================
//  FONCTIONS D'EXPORT JSON — Nouvelles livraisons
// =============================================================================

/**
 * @brief Exporte max positif et min négatif d'un tenseur 3D dans deux fichiers JSON.
 *        Écrit <path>/<name_base>_positive.json  et  <path>/<name_base>_negative.json
 *        Format : { "span", "section", "alpha", "value" }
 */
static void signed_delivery(
    const std::vector<std::vector<std::vector<double>>>& data,
    const std::filesystem::path& path,
    const std::string& name_base)
{
    const auto p_max = findMaxPositiveValue3D(data);
    const auto p_min = findMinValue3D(data);

    {
        json j;
        j["span"] = p_max.i; j["section"] = p_max.j;
        j["alpha"] = p_max.k; j["value"] = p_max.val;
        std::filesystem::create_directories(path);
        std::ofstream f(path / (name_base + "_positive.json"));
        f << j.dump(4);
    }
    {
        json j;
        j["span"] = p_min.i; j["section"] = p_min.j;
        j["alpha"] = p_min.k; j["value"] = p_min.val;
        std::filesystem::create_directories(path);
        std::ofstream f(path / (name_base + "_negative.json"));
        f << j.dump(4);
    }
}

/**
 * @brief Exporte max positif et min négatif d'une matrice 2D dans deux fichiers JSON.
 *        Format : { "span", "alpha", "value" }
 */
static void signed_delivery_2D(
    const std::vector<std::vector<double>>& data,
    const std::filesystem::path& path,
    const std::string& name_base)
{
    const auto p_max = findMaxPositiveValue2D(data);
    const auto p_min = findMinValue2D(data);

    {
        json j;
        j["span"] = p_max.i; j["alpha"] = p_max.j; j["value"] = p_max.val;
        std::filesystem::create_directories(path);
        std::ofstream f(path / (name_base + "_positive.json"));
        f << j.dump(4);
    }
    {
        json j;
        j["span"] = p_min.i; j["alpha"] = p_min.j; j["value"] = p_min.val;
        std::filesystem::create_directories(path);
        std::ofstream f(path / (name_base + "_negative.json"));
        f << j.dump(4);
    }
}

/**
 * @brief Exporte les statistiques par travée.
 *        Format : [{span, min, min_section, min_alpha, max, max_section,
 *                   max_alpha, mean, rms, std_dev, count}, ...]
 */
static void spanstats_delivery(
    const std::vector<SpanStats>& stats,
    const std::filesystem::path& path,
    const std::string& filename)
{
    json arr = json::array();
    for (size_t i = 0; i < stats.size(); ++i) {
        const auto& s = stats[i];
        json j;
        j["span"] = i;
        j["min"] = s.min_val;
        j["min_section"] = s.sec_min;
        j["min_alpha"] = s.alpha_min;
        j["max"] = s.max_val;
        j["max_section"] = s.sec_max;
        j["max_alpha"] = s.alpha_max;
        j["mean"] = s.mean;
        j["rms"] = s.rms;
        j["std_dev"] = s.std_dev;
        j["count"] = s.count;
        arr.push_back(j);
    }
    std::filesystem::create_directories(path);
    std::ofstream f(path / filename);
    f << arr.dump(4);
}

/**
 * @brief Exporte les extrema par travée (absolu, positif, négatif).
 *        Format : [{span,
 *                   max_absolute:{value, section, alpha},
 *                   max_positive:{value, section, alpha},
 *                   max_negative:{value, section, alpha}}, ...]
 */
static void perspan_extrema_delivery(
    const std::vector<std::vector<std::vector<double>>>& data,
    const std::filesystem::path& path,
    const std::string& filename)
{
    json arr = json::array();
    for (size_t i = 0; i < data.size(); ++i) {
        const auto& span = data[i];
        double max_abs = 0.0;
        double max_pos = std::numeric_limits<double>::lowest();
        double max_neg = std::numeric_limits<double>::max();
        size_t sec_abs = 0, a_abs = 0;
        size_t sec_pos = 0, a_pos = 0;
        size_t sec_neg = 0, a_neg = 0;

        for (size_t j = 0; j < span.size(); ++j)
            for (size_t k = 0; k < span[j].size(); ++k) {
                const double v = span[j][k];
                if (std::abs(v) > std::abs(max_abs)) { max_abs = v; sec_abs = j; a_abs = k; }
                if (v > max_pos) { max_pos = v; sec_pos = j; a_pos = k; }
                if (v < max_neg) { max_neg = v; sec_neg = j; a_neg = k; }
            }

        json j;
        j["span"] = i;
        j["max_absolute"] = { {"value", max_abs}, {"section", sec_abs}, {"alpha", a_abs} };
        j["max_positive"] = { {"value", max_pos}, {"section", sec_pos}, {"alpha", a_pos} };
        j["max_negative"] = { {"value", max_neg}, {"section", sec_neg}, {"alpha", a_neg} };
        arr.push_back(j);
    }
    std::filesystem::create_directories(path);
    std::ofstream f(path / filename);
    f << arr.dump(4);
}

/**
 * @brief Exporte les passages par zéro.
 *        Format : [{span, n_crossings, crossings:[{section, alpha_before,
 *                   alpha_after, x_approx}, ...]}, ...]
 */
static void zerocrossings_delivery(
    const std::vector<std::vector<ZeroCrossing>>& crossings,
    const std::filesystem::path& path,
    const std::string& filename)
{
    json arr = json::array();
    for (size_t i = 0; i < crossings.size(); ++i) {
        json span_j;
        span_j["span"] = i;
        json c_arr = json::array();
        for (const auto& c : crossings[i]) {
            json cj;
            cj["section"] = c.section;
            cj["alpha_before"] = c.alpha_before;
            cj["alpha_after"] = c.alpha_after;
            cj["x_approx"] = c.x_approx;
            c_arr.push_back(cj);
        }
        span_j["n_crossings"] = c_arr.size();
        span_j["crossings"] = c_arr;
        arr.push_back(span_j);
    }
    std::filesystem::create_directories(path);
    std::ofstream f(path / filename);
    f << arr.dump(4);
}


// =============================================================================
//  FONCTIONS D'EXPORT JSON (existantes)
// =============================================================================

/**
 * @brief Sérialise n'importe quelle donnée convertible en JSON et l'écrit dans un fichier.
 *
 * Crée le répertoire @p path si nécessaire, puis écrit la représentation JSON
 * de @p data indentée à 4 espaces dans le fichier @p path/FileName.
 *
 * @param data      Donnée à exporter (compatible nlohmann::json)
 * @param path      Répertoire de destination
 * @param FileName  Nom du fichier de sortie (ex. "result.json")
 */
static void delivery(auto& data, std::filesystem::path path, std::string FileName) {
    json j = data;
    std::filesystem::create_directories(path);
    std::ofstream file(path / FileName);
    file << j.dump(4);
}

/**
 * @brief Exporte la position et la valeur d'un extremum 3D dans un fichier JSON.
 *
 * Écrit les champs "span", "section", "alpha" et "value" extraits d'une
 * Position3D, puis délègue l'écriture à delivery().
 *
 * @param data  Position3D décrivant l'extremum (i=span, j=section, k=alpha)
 * @param file  Objet JSON dans lequel les champs sont ajoutés (peut être préchargé)
 * @param Path  Répertoire de destination
 * @param name  Nom du fichier de sortie
 */
static void maximum_delivery(auto& data, json& file,
    std::filesystem::path Path, std::string name) {
    file["span"] = data.i;
    file["section"] = data.j;
    file["alpha"] = data.k;
    file["value"] = data.val;
    delivery(file, Path, name);
}

/**
 * @brief Exporte le résultat d'un cas de charge critique dans un fichier JSON.
 *
 * Écrit les champs "maximum", "span", "section", "position" et "load"
 * extraits d'une structure load_delivery, puis délègue l'écriture à delivery().
 *
 * @param data  Résultat du cas de charge (valeur max, localisation, détail charge)
 * @param file  Objet JSON dans lequel les champs sont ajoutés (peut être préchargé)
 * @param Path  Répertoire de destination
 * @param name  Nom du fichier de sortie
 */
static void loading_delivery(auto& data, json& file,
    std::filesystem::path Path, std::string name) {
    file["maximum"] = data.maximum_value;
    file["span"] = data.span;
    file["section"] = data.section;
    file["position"] = data.position;
    file["load"] = data.load;
    delivery(file, Path, name);
}


// =============================================================================
//  FONCTIONS DE CONFIGURATION ET DE CHEMINS
// =============================================================================

/**
 * @brief Vérifie si un fichier ou un répertoire existe sur le système de fichiers.
 *
 * @param filePath  Chemin absolu ou relatif à tester
 * @return          true si le chemin existe, false sinon
 */
static bool checkFileExists(const std::filesystem::path& filePath) {
    return std::filesystem::exists(filePath);
}

/**
 * @brief Lit une variable d'environnement de manière portable (Windows / POSIX).
 *
 * Sur Windows, utilise _dupenv_s() pour éviter les avertissements de sécurité.
 * Sur les systèmes POSIX, utilise std::getenv().
 *
 * @param varName  Nom de la variable d'environnement (ex. "HOME", "USERPROFILE")
 * @return         Valeur de la variable, ou chaîne vide si non définie
 */
static std::string getEnvSafe(const char* varName) {
#ifdef _WIN32
    char* buf = nullptr;
    size_t sz = 0;
    if (_dupenv_s(&buf, &sz, varName) == 0 && buf != nullptr) {
        std::string result(buf);
        free(buf);
        return result;
    }
    return {};
#else
    const char* v = std::getenv(varName);
    return v ? std::string(v) : std::string{};
#endif
}

/**
 * @brief Retourne le chemin du répertoire de configuration de l'application.
 *
 * Construit le chemin "Documents/Matrix One/Influence Line" à partir du
 * répertoire personnel de l'utilisateur (USERPROFILE sur Windows, HOME sur POSIX).
 * Si la variable d'environnement n'est pas définie, retourne un chemin relatif
 * "Matrix One/Influence Line".
 *
 * @return  Chemin vers le répertoire de configuration
 */
static std::filesystem::path getConfigPath() {
#ifdef _WIN32
    const std::string userProfile = getEnvSafe("USERPROFILE");
    if (!userProfile.empty())
        return std::filesystem::path(userProfile) / "Documents" / "Matrix One" / "Influence Line";
#else
    const std::string home = getEnvSafe("HOME");
    if (!home.empty())
        return std::filesystem::path(home) / "Documents" / "Matrix One" / "Influence Line";
#endif
    return std::filesystem::path("Matrix One") / "Influence Line";
}


// =============================================================================
//  FONCTIONS DE PARSING DU FICHIER DE CONFIGURATION
// =============================================================================

/**
 * @brief Extrait une liste de valeurs numériques depuis une ligne de configuration.
 *
 * Recherche le premier caractère ':' dans @p line, puis parse les nombres
 * flottants qui suivent (séparés par des espaces ou tabulations).
 *
 * Exemple : "Length: 20 25 20"  →  {20.0, 25.0, 20.0}
 *
 * @param line  Ligne brute du fichier de configuration
 * @return      Vecteur des valeurs parsées (vide si ':' absent ou aucun nombre)
 */
static std::vector<double> parseVector(const std::string& line) {
    std::vector<double> vector;
    size_t pos = line.find(":");
    if (pos != std::string::npos) {
        std::string values = line.substr(pos + 1);
        values.erase(0, values.find_first_not_of(" \t"));
        std::istringstream iss(values);
        double value;
        while (iss >> value) vector.push_back(value);
    }
    return vector;
}

/**
 * @brief Extrait la valeur texte à droite du ':' dans une ligne de configuration.
 *
 * Supprime les espaces et tabulations en tête et en fin de la valeur extraite.
 *
 * Exemple : "Steps: 1"  →  "1"
 *
 * @param line  Ligne brute du fichier de configuration
 * @return      Valeur sous forme de chaîne (vide si ':' absent)
 */
static std::string getValue(const std::string& line) {
    size_t pos = line.find(":");
    if (pos != std::string::npos) {
        std::string value = line.substr(pos + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        return value;
    }
    return "";
}

/**
 * @brief Analyse une ligne de chargement du fichier de configuration.
 *
 * La ligne est découpée en trois segments délimités par deux marqueurs :
 *   - s1 (avant marker_1)  → intensités de charge (kN ou kN/m)
 *   - s2 (entre marker_1 et marker_2) → longueurs des segments (m)
 *   - s3 (après marker_2)  → nom du cas de charge
 *
 * Exemple :
 *   "45 10 25 /Distributed/ 0 3 5 2 :: UDL 2"
 *   avec marker_1="/Distributed/" et marker_2="::"
 *   → Intensity={45,10,25}, Length={0,3,5,2}, name="UDL 2"
 *
 * @param line      Ligne brute à analyser (modifiable pour nettoyage)
 * @param marker_1  Délimiteur séparant intensités et longueurs (ex. "/Point/")
 * @param marker_2  Délimiteur séparant longueurs et nom (ex. "::")
 * @return          Structure load remplie
 */
static load LoadParser(std::string& line, const char* marker_1, const char* marker_2) {
    load l;
    const size_t work = line.find(marker_1);
    const size_t name = line.find(marker_2);
    const size_t plus1 = std::strlen(marker_1);
    const size_t plus2 = std::strlen(marker_2);

    std::string s1 = line.substr(0, work);
    std::string s2 = line.substr(work + plus1, name - (work + plus1));
    size_t s3s = name + plus2;
    size_t s3e = line.find_last_not_of(" \t\r\n");
    std::string s3 = line.substr(s3s, s3e - s3s + 1);

    s1.erase(0, s1.find_first_not_of(" \t"));
    s2.erase(0, s2.find_first_not_of(" \t"));
    s3.erase(0, s3.find_first_not_of(" \t"));

    std::istringstream iss(s1);
    double value;
    while (iss >> value) l.Intensity.push_back(value);
    iss = std::istringstream(s2);
    while (iss >> value) l.Length.push_back(value);
    l.name = s3;
    return l;
}

/**
 * @brief Génère un fichier de configuration structurale avec des valeurs par défaut.
 *
 * Crée le répertoire @p configPath si nécessaire, puis écrit un fichier texte
 * illustrant la syntaxe complète : longueurs de travées, pas de discrétisation,
 * module de Young, moment d'inertie, et plusieurs exemples de cas de charge
 * (charges ponctuelles BC/BE et charges réparties UDL).
 *
 * Affiche un message de confirmation sur std::cout ou une erreur sur std::cerr.
 *
 * @param filename    Nom du fichier à créer (ex. "config.txt")
 * @param configPath  Répertoire de destination (créé automatiquement si absent)
 */
static void write_structural_model_input(const std::string& filename,
    std::filesystem::path configPath)
{
    std::filesystem::create_directories(configPath);
    std::filesystem::path fullPath = configPath / filename;
    std::ofstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "Erreur: Impossible d'ouvrir " << fullPath << std::endl;
        return;
    }
    std::vector<std::string> CONFIG = {
"# =============================================================================",
"#                 STRUCTURAL ANALYSIS CONFIGURATION FILE",
"# =============================================================================",
"# UNITS: Length(m), Force(kN), Distributed Load(kN/m), E(Pa), I(m^4)",
"# =============================================================================",
"",
"Length: 20 25 20",
"Steps: 1",
"Young Modulus: 210e9 210e9 210e9",
"Moment of Inertia: 1e-6 1e-6 1e-6",
"",
"6 12 12 6 12 12 /Point/ 2.25 4.5 1.5 5 4.5 1.5 2.5 :: BC 1",
"5 10 10 5 10 10 /Point/ 2.25 4.5 1.5 5 4.5 1.5 2.5 :: BC 2",
"20 /Point/ 0 0 :: BE 1",
"20 /Point/ 4 2 :: BE 2",
"45 /Distributed/ 0 3 :: UDL 1",
"45 10 25 /Distributed/ 0 3 5 2 :: UDL 2",
""
    };
    for (auto& t : CONFIG) file << t << std::endl;
    file.close();
    std::cout << "Fichier créé: " << fullPath << std::endl;
}

/**
 * @brief Calcule le produit d'une sous-liste de valeurs.
 *
 * Retourne liste[debut] × liste[debut+1] × … × liste[fin].
 * Utile pour le calcul de termes polynomiaux ou de facteurs cumulatifs
 * dans les formules de la méthode des trois moments.
 *
 * @param liste  Vecteur de valeurs (m, EI, etc.)
 * @param debut  Indice de début (inclusif)
 * @param fin    Indice de fin (inclusif)
 * @return       Produit des éléments entre debut et fin, ou 1.0 si debut > fin
 */
static double prod_list(const std::vector<double>& liste, int debut, int fin) {
    double r = 1.0;
    for (int a = debut; a <= fin; ++a) r *= liste[a];
    return r;
}

/**
 * @brief Aplatit une matrice 2D en un vecteur 1D (concaténation des lignes).
 *
 * Concatène toutes les lignes de @p m dans un vecteur continu.
 * L'ordre des éléments est préservé : m[0][0], m[0][1], …, m[1][0], …
 *
 * @param m  Matrice d'entrée (lignes de tailles potentiellement différentes)
 * @return   Vecteur 1D contenant tous les éléments dans l'ordre de parcours
 */
static std::vector<double> flatten(const std::vector<std::vector<double>>& m) {
    size_t n = 0;
    for (const auto& v : m) n += v.size();
    std::vector<double> out;
    out.reserve(n);
    for (const auto& v : m) out.insert(out.end(), v.begin(), v.end());
    return out;
}

/**
 * @brief Retourne l'indice de la première occurrence d'une valeur dans un vecteur.
 *
 * Parcourt @p vec séquentiellement et retourne la position de la première
 * occurrence égale à @p value (comparaison par ==).
 *
 * @tparam T     Type des éléments (doit supporter ==)
 * @param vec    Vecteur à parcourir
 * @param value  Valeur à rechercher
 * @return       Indice de la première occurrence
 * @throws std::invalid_argument si @p value n'est pas dans @p vec
 */
template <typename T>
size_t Indice_of(std::vector<T>& vec, T& value) {
    size_t c = 0;
    for (auto& i : vec) {
        if (i == value) return c;
        ++c;
    }
    throw std::invalid_argument("This value is not in the vector");
}