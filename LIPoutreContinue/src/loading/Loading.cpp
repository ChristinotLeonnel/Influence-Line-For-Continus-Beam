#include "LIPoutreContinue/loading/Loading.h"
#include "LIPoutreContinue/Utils.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

// Voir Loading.h pour le modèle mathématique et docs/audit/03_numerique.md
// pour les défauts corrigés (N2–N7, N9).

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Renomme les doublons ("Camion", "Camion #2", …) pour que chaque charge ait
// sa propre entrée dans les détails de résultat.
std::vector<std::string> uniqueNames(const std::vector<load>& loads, std::vector<std::string> taken = {})
{
    std::vector<std::string> out;
    for (const auto& l : loads) {
        std::string name = l.name.empty() ? std::string("charge") : l.name;
        std::string candidate = name;
        for (int k = 2; std::find(taken.begin(), taken.end(), candidate) != taken.end(); ++k)
            candidate = name + " #" + std::to_string(k);
        taken.push_back(candidate);
        out.push_back(candidate);
    }
    return out;
}

} // namespace

// =============================================================================
//  Construction
// =============================================================================
Loading::Loading(std::vector<std::vector<std::vector<double>>> CURVES_in,
                 std::vector<double> POSITION_in,
                 std::vector<std::vector<double>> SpanNodePositions_in,
                 const std::vector<double>& spans_in,
                 const std::vector<load>& pointLoads,
                 const std::vector<load>& distribLoads,
                 double scale)
    : Configuration(),
      SpanNodePositions(std::move(SpanNodePositions_in)),
      POSITION(std::move(POSITION_in)),
      scale_(scale),
      ownedCurves_(std::move(CURVES_in))
{
    curves_ = &ownedCurves_;
    init(spans_in, pointLoads, distribLoads);
}

Loading::Loading(BorrowCurves,
                 const std::vector<std::vector<std::vector<double>>>& CURVES_ref,
                 const std::vector<double>& POSITION_in,
                 const std::vector<std::vector<double>>& SpanNodePositions_in,
                 const std::vector<double>& spans_in,
                 const std::vector<load>& pointLoads,
                 const std::vector<load>& distribLoads,
                 double scale)
    : Configuration(),
      SpanNodePositions(SpanNodePositions_in),
      POSITION(POSITION_in),
      scale_(scale),
      curves_(&CURVES_ref)
{
    init(spans_in, pointLoads, distribLoads);
}

void Loading::init(const std::vector<double>& spans_in, const std::vector<load>& pointLoads,
                   const std::vector<load>& distribLoads)
{
    const auto& CURVES = *curves_;
    spans = spans_in;
    Point_LOAD = pointLoads;
    Rectangulare_LOAD = distribLoads;

    if (POSITION.size() < 2)
        throw std::invalid_argument("Loading: 'position' doit contenir au moins 2 abscisses.");
    if (!std::is_sorted(POSITION.begin(), POSITION.end()))
        throw std::invalid_argument("Loading: 'position' doit être croissante.");
    if (CURVES.size() != SpanNodePositions.size())
        throw std::invalid_argument("Loading: 'curves' et 'span_node_positions' doivent avoir le même nombre de travées.");

    size_t offset = 0;
    for (size_t s = 0; s < SpanNodePositions.size(); ++s) {
        if (CURVES[s].size() != SpanNodePositions[s].size())
            throw std::invalid_argument("Loading: travée " + std::to_string(s)
                + " — nombre de sections de 'curves' différent de 'span_node_positions'.");
        spanOffset_.push_back(offset);
        offset += SpanNodePositions[s].size();
    }
    if (offset != POSITION.size())
        throw std::invalid_argument("Loading: 'position' doit compter autant d'abscisses que de nœuds au total ("
            + std::to_string(offset) + "), reçu " + std::to_string(POSITION.size()) + ".");

    pointNames_ = uniqueNames(Point_LOAD);
    rectNames_  = uniqueNames(Rectangulare_LOAD, pointNames_);

    computeAll();
}

size_t Loading::globalIndex(size_t span, size_t section) const
{
    if (span >= SpanNodePositions.size() || section >= SpanNodePositions[span].size())
        throw std::out_of_range("Loading: indice de travée/section hors limites (travée "
            + std::to_string(span) + ", section " + std::to_string(section) + ").");
    return spanOffset_[span] + section;
}

size_t Loading::nearestNode(double x) const
{
    if (!std::isfinite(x)) return 0;
    const auto it = std::lower_bound(POSITION.begin(), POSITION.end(), x);
    if (it == POSITION.begin()) return 0;
    if (it == POSITION.end()) return POSITION.size() - 1;
    const size_t j = static_cast<size_t>(it - POSITION.begin());
    return (x - POSITION[j - 1] <= POSITION[j] - x) ? j - 1 : j;
}

// Ligne de la section (span, section). Les lignes d'effort tranchant ont une
// valeur de plus que la grille (saut unité au droit de la section) : on
// duplique alors l'abscisse de la section (défaut N7 — avant, les ordonnées à
// droite de la section étaient décalées d'un nœud).
lipc::InfluenceLine Loading::makeLine(size_t span, size_t section) const
{
    const size_t g = globalIndex(span, section);
    const auto& row = (*curves_)[span][section];
    std::vector<double> t = POSITION;
    if (row.size() == POSITION.size() + 1)
        t.insert(t.begin() + static_cast<std::ptrdiff_t>(g) + 1, POSITION[g]);
    else if (row.size() != POSITION.size())
        throw std::invalid_argument("Loading: ligne d'influence de taille " + std::to_string(row.size())
            + " incompatible avec " + std::to_string(POSITION.size()) + " abscisses.");
    std::vector<double> y(row.size());
    for (size_t i = 0; i < row.size(); ++i) y[i] = row[i] * scale_;
    return lipc::InfluenceLine(std::move(t), std::move(y));
}

// =============================================================================
//  Calcul complet (constructeur)
// =============================================================================
void Loading::computeAll()
{
    const size_t S = SpanNodePositions.size();
    pointExt_.assign(S, {});
    rectExt_.assign(S, {});
    for (auto* env : {&PointEnvelope, &RectangularEnvelope, &CombinedEnvelope}) {
        env->max.assign(S, {});
        env->min.assign(S, {});
    }

    for (size_t s = 0; s < S; ++s) {
        const size_t nSec = SpanNodePositions[s].size();
        pointExt_[s].resize(nSec);
        rectExt_[s].resize(nSec);
        for (auto* env : {&PointEnvelope, &RectangularEnvelope, &CombinedEnvelope}) {
            env->max[s].assign(nSec, 0.0);
            env->min[s].assign(nSec, 0.0);
        }
        for (size_t sec = 0; sec < nSec; ++sec) {
            const lipc::InfluenceLine line = makeLine(s, sec);
            double pmax = 0, pmin = 0, rmax = 0, rmin = 0;
            for (const auto& l : Point_LOAD) {
                pointExt_[s][sec].push_back(lipc::pointLoadExtreme(line, l));
                pmax += pointExt_[s][sec].back().max;
                pmin += pointExt_[s][sec].back().min;
            }
            for (const auto& l : Rectangulare_LOAD) {
                rectExt_[s][sec].push_back(lipc::distributedLoadExtreme(line, l));
                rmax += rectExt_[s][sec].back().max;
                rmin += rectExt_[s][sec].back().min;
            }
            PointEnvelope.max[s][sec] = pmax;        PointEnvelope.min[s][sec] = pmin;
            RectangularEnvelope.max[s][sec] = rmax;  RectangularEnvelope.min[s][sec] = rmin;
            CombinedEnvelope.max[s][sec] = pmax + rmax;
            CombinedEnvelope.min[s][sec] = pmin + rmin;
        }
    }

    Point_load       = collect(Kind::Point,    0, S);
    Rectangular_load = collect(Kind::Rect,     0, S);
    Combined_load    = collect(Kind::Combined, 0, S);
}

std::map<std::string, double> Loading::detail(const Extreme& e, bool positive) const
{
    const bool has = positive ? e.hasMax : e.hasMin;
    const double origin = has ? (positive ? e.maxOrigin : e.minOrigin) : kNaN;
    return {
        {"value", positive ? e.max : e.min},
        {"Position", origin},
        {"alpha", static_cast<double>(nearestNode(origin))},
    };
}

load_delivery Loading::collect(Kind kind, size_t spanBegin, size_t spanEnd, long onlySection) const
{
    const SectionEnvelope& env = (kind == Kind::Point) ? PointEnvelope
                               : (kind == Kind::Rect)  ? RectangularEnvelope
                                                       : CombinedEnvelope;
    load_delivery d;
    bool init = false;
    for (size_t s = spanBegin; s < spanEnd; ++s) {
        for (size_t sec = 0; sec < SpanNodePositions[s].size(); ++sec) {
            if (onlySection >= 0 && sec != static_cast<size_t>(onlySection)) continue;
            const double emax = env.max[s][sec], emin = env.min[s][sec];
            if (!init || emax > d.max_value) { d.max_value = emax; d.max_span = s; d.max_section = sec; }
            if (!init || emin < d.min_value) { d.min_value = emin; d.min_span = s; d.min_section = sec; }
            init = true;
        }
    }
    if (!init) return d;

    auto details = [&](size_t s, size_t sec, bool positive, double& firstOrigin) {
        std::map<std::string, std::map<std::string, double>> m;
        firstOrigin = kNaN;
        auto add = [&](const std::vector<Extreme>& ext, const std::vector<std::string>& names) {
            for (size_t k = 0; k < ext.size(); ++k) {
                m[names[k]] = detail(ext[k], positive);
                if (std::isnan(firstOrigin)) firstOrigin = m[names[k]]["Position"];
            }
        };
        if (kind != Kind::Rect)  add(pointExt_[s][sec], pointNames_);
        if (kind != Kind::Point) add(rectExt_[s][sec], rectNames_);
        return m;
    };

    double maxOrigin = kNaN, minOrigin = kNaN;
    d.load_max = details(d.max_span, d.max_section, true, maxOrigin);
    d.load_min = details(d.min_span, d.min_section, false, minOrigin);
    d.max_section_x = POSITION[globalIndex(d.max_span, d.max_section)];
    d.min_section_x = POSITION[globalIndex(d.min_span, d.min_section)];

    const bool positiveGoverns = std::abs(d.max_value) >= std::abs(d.min_value);
    d.maximum_value = positiveGoverns ? d.max_value : d.min_value;
    d.span          = positiveGoverns ? d.max_span : d.min_span;
    d.section       = positiveGoverns ? d.max_section : d.min_section;
    d.section_x     = positiveGoverns ? d.max_section_x : d.min_section_x;
    d.load          = positiveGoverns ? d.load_max : d.load_min;
    d.position      = positiveGoverns ? maxOrigin : minOrigin;
    return d;
}

// =============================================================================
//  API élémentaire (1.x)
// =============================================================================
double Loading::OnePointLoad(double intensity, size_t span, size_t section, size_t alpha)
{
    const size_t g = globalIndex(span, section);
    if (alpha >= POSITION.size())
        throw std::out_of_range("Loading::OnePointLoad: alpha=" + std::to_string(alpha)
            + " hors de la grille (" + std::to_string(POSITION.size()) + " nœuds).");
    const auto& row = (*curves_)[span][section];
    // Lignes d'effort tranchant : au-delà de la section, décalage d'un indice.
    const size_t r = (row.size() == POSITION.size() + 1 && alpha > g) ? alpha + 1 : alpha;
    return row[r] * scale_ * intensity;
}

static Position1D toPosition1D(const Loading::Extreme& e, size_t node_of_max, size_t node_of_min)
{
    const bool positive = std::abs(e.max) >= std::abs(e.min);
    const bool has = positive ? e.hasMax : e.hasMin;
    Position1D p;
    p.value = positive ? e.max : e.min;
    p.position = has ? (positive ? e.maxOrigin : e.minOrigin) : kNaN;
    p.max_position = positive ? node_of_max : node_of_min;
    return p;
}

Position1D Loading::PluralPointLoad(const std::vector<double>& intensity, const std::vector<double>& length,
                                    size_t span, size_t section)
{
    load l{intensity, length, ""};
    const Extreme e = lipc::pointLoadExtreme(makeLine(span, section), l);
    return toPosition1D(e, nearestNode(e.hasMax ? e.maxOrigin : kNaN), nearestNode(e.hasMin ? e.minOrigin : kNaN));
}

double Loading::OneRectangularLoad(double intensity, size_t span, size_t section, size_t begin, size_t end)
{
    if (begin > end || end >= POSITION.size())
        throw std::out_of_range("Loading::OneRectangularLoad: il faut begin <= end < "
            + std::to_string(POSITION.size()) + ".");
    const lipc::InfluenceLine line = makeLine(span, section);
    // Intégrale exacte de l'interpolant entre les nœuds begin et end INCLUS
    // (défaut N6 : l'ancienne boucle `i < end` omettait le dernier intervalle).
    return intensity * (line.integral(POSITION[end]) - line.integral(POSITION[begin]));
}

Position1D Loading::PluralRectangularLoad(const std::vector<double>& intensity, const std::vector<double>& length,
                                          size_t span, size_t section)
{
    load l{intensity, length, ""};
    const Extreme e = lipc::distributedLoadExtreme(makeLine(span, section), l);
    return toPosition1D(e, nearestNode(e.hasMax ? e.maxOrigin : kNaN), nearestNode(e.hasMin ? e.minOrigin : kNaN));
}

CombineLoadPosition Loading::CombinedLoad(size_t span, size_t section)
{
    const load_delivery d = collect(Kind::Combined, span, span + 1, static_cast<long>(section));
    CombineLoadPosition c;
    c.value = d.maximum_value;
    c.position = d.position;
    c.max_position = nearestNode(d.position);
    c.Addition = d.load;
    return c;
}

Loading::CriticalSectionResult Loading::computeCriticalSection(size_t span)
{
    if (span >= SpanNodePositions.size())
        throw std::out_of_range("Loading::computeCriticalSection: travée " + std::to_string(span) + " inexistante.");
    return {collect(Kind::Point, span, span + 1), collect(Kind::Rect, span, span + 1),
            collect(Kind::Combined, span, span + 1)};
}

Loading::CriticalSectionResult Loading::computeAtSection(size_t span, size_t section)
{
    globalIndex(span, section); // contrôle des bornes
    const long sec = static_cast<long>(section);
    return {collect(Kind::Point, span, span + 1, sec), collect(Kind::Rect, span, span + 1, sec),
            collect(Kind::Combined, span, span + 1, sec)};
}
