#include "LIPoutreContinue/loading/MovingLoadSearch.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace lipc {

InfluenceLine::InfluenceLine(std::vector<double> t, std::vector<double> y)
    : t_(std::move(t)), y_(std::move(y))
{
    if (t_.size() < 2 || t_.size() != y_.size())
        throw std::invalid_argument("InfluenceLine: il faut au moins 2 points et autant d'ordonnées que d'abscisses.");
    if (!std::is_sorted(t_.begin(), t_.end()))
        throw std::invalid_argument("InfluenceLine: abscisses non croissantes.");
    C_.assign(t_.size(), 0.0);
    for (size_t i = 1; i < t_.size(); ++i)
        C_[i] = C_[i - 1] + 0.5 * (t_[i] - t_[i - 1]) * (y_[i] + y_[i - 1]);
}

double InfluenceLine::right(double q) const
{
    if (q < t_.front() || q >= t_.back()) return 0.0;
    const size_t j = static_cast<size_t>(std::upper_bound(t_.begin(), t_.end(), q) - t_.begin()) - 1; // t_j <= q < t_{j+1}
    return y_[j] + (y_[j + 1] - y_[j]) * (q - t_[j]) / (t_[j + 1] - t_[j]);
}

double InfluenceLine::left(double q) const
{
    if (q <= t_.front() || q > t_.back()) return 0.0;
    const size_t j = static_cast<size_t>(std::lower_bound(t_.begin(), t_.end(), q) - t_.begin()); // t_{j-1} < q <= t_j
    return y_[j - 1] + (y_[j] - y_[j - 1]) * (q - t_[j - 1]) / (t_[j] - t_[j - 1]);
}

double InfluenceLine::integral(double q) const
{
    if (q <= t_.front()) return 0.0;
    if (q >= t_.back()) return C_.back();
    const size_t j = static_cast<size_t>(std::upper_bound(t_.begin(), t_.end(), q) - t_.begin()) - 1;
    const double fq = y_[j] + (y_[j + 1] - y_[j]) * (q - t_[j]) / (t_[j + 1] - t_[j]);
    return C_[j] + 0.5 * (q - t_[j]) * (y_[j] + fq);
}

std::vector<double> cumulativeOffsets(const std::vector<double>& length, size_t count, const char* what)
{
    if (count == 0) return {};
    if (length.size() < count)
        throw std::invalid_argument(std::string("charge ") + what + " — 'length' contient "
            + std::to_string(length.size()) + " valeur(s), " + std::to_string(count) + " attendue(s).");
    std::vector<double> d(count);
    d[0] = length[0];
    for (size_t i = 1; i < count; ++i) d[i] = d[i - 1] + length[i];
    for (double v : d)
        if (!std::isfinite(v)) throw std::invalid_argument("'length' contient une valeur non finie.");
    return d;
}

static void keep(Extreme& e, double v, double origin)
{
    if (v > e.max) { e.max = v; e.maxOrigin = origin; e.hasMax = true; }
    if (v < e.min) { e.min = v; e.minOrigin = origin; e.hasMin = true; }
}

Extreme pointLoadExtreme(const InfluenceLine& line, const load& l)
{
    Extreme e;
    const size_t n = l.Intensity.size();
    if (n == 0) return e;
    const auto d = cumulativeOffsets(l.Length, n, "ponctuelle");
    const auto& t = line.abscissae();

    // g(p) linéaire par morceaux → extrema en p = t_j − d_i, limites à gauche
    // et à droite. L'essieu « porteur » i est posé exactement sur t_j.
    for (double tj : t) {
        for (size_t i = 0; i < n; ++i) {
            const double p = tj - d[i];
            for (int side = 0; side < 2; ++side) {
                double v = 0.0;
                for (size_t k = 0; k < n; ++k) {
                    const double q = (k == i) ? tj : p + d[k];
                    v += l.Intensity[k] * (side == 0 ? line.left(q) : line.right(q));
                }
                keep(e, v, p);
            }
        }
    }
    return e;
}

Extreme distributedLoadExtreme(const InfluenceLine& line, const load& l)
{
    Extreme e;
    const size_t n = l.Intensity.size();
    if (n == 0) return e;
    const auto c = cumulativeOffsets(l.Length, n + 1, "répartie");

    auto G = [&](double p) {
        double v = 0.0;
        for (size_t k = 0; k < n; ++k)
            v += l.Intensity[k] * (line.integral(p + c[k + 1]) - line.integral(p + c[k]));
        return v;
    };
    auto dG = [&](double p) {   // à l'intérieur d'un intervalle (f continue)
        double v = 0.0;
        for (size_t k = 0; k < n; ++k)
            v += l.Intensity[k] * (line.right(p + c[k + 1]) - line.right(p + c[k]));
        return v;
    };

    std::vector<double> cand;
    cand.reserve(line.abscissae().size() * (n + 1));
    for (double tj : line.abscissae())
        for (double cm : c) cand.push_back(tj - cm);
    std::sort(cand.begin(), cand.end());
    cand.erase(std::unique(cand.begin(), cand.end()), cand.end());

    for (size_t i = 0; i < cand.size(); ++i) {
        keep(e, G(cand[i]), cand[i]);
        if (i + 1 == cand.size()) break;
        // G' affine sur ]p_a, p_b[ : échantillonnage intérieur + zéro.
        const double pa = cand[i], pb = cand[i + 1], w = pb - pa;
        if (!(w > 0.0)) continue;
        const double u1 = pa + 0.25 * w, u2 = pa + 0.75 * w;
        const double g1 = dG(u1), g2 = dG(u2);
        if (g1 == g2) continue;
        const double root = u1 - g1 * (u2 - u1) / (g2 - g1);
        if (root > pa && root < pb) keep(e, G(root), root);
    }
    return e;
}

} // namespace lipc
