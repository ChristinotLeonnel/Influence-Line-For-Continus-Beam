#include "LoadEnvelope.h"
#include "utils/BeamUtils.h"

#include <sstream>
#include <iostream>
#include <map>

LoadEnvelope::LoadEnvelope(
    std::vector<std::vector<std::vector<double>>> influenceCurves,
    std::vector<double>                           globalAbscissae,
    std::vector<std::vector<double>>              spanNodes)
    : influenceCurves_(std::move(influenceCurves))
    , globalAbscissae_(std::move(globalAbscissae))
    , spanNodes_(std::move(spanNodes))
{
    loadFromFile();

    // Supprime les travées de longueur nulle
    std::vector<double> activeSpans = spanLengths;
    std::vector<size_t> zeroIdx;
    for (size_t i = 0; i < spanLengths.size(); ++i)
        if (spanLengths[i] == 0) zeroIdx.push_back(i);
    removeByIndices(activeSpans, zeroIdx);

    // ── Charge ponctuelle ─────────────────────────────────────────────────
    {
        std::vector<std::map<std::string, std::map<std::string, double>>> bestPerSpan;
        std::vector<double> maxPerSpan;

        for (size_t span = 0; span < activeSpans.size(); ++span) {
            std::vector<double> maxPerSection;
            std::vector<std::map<std::string, std::map<std::string, double>>> perSection;

            for (size_t sec = 0; sec < spanNodes_[span].size(); ++sec) {
                double total = 0.0;
                std::map<std::string, std::map<std::string, double>> detail;

                for (auto& lc : pointLoads) {
                    Position1D lo = pluralPointLoad(lc.intensities, lc.positions, span, sec);
                    total += lo.value;
                    detail[lc.name] = {
                        { "alpha",    static_cast<double>(lo.maxIndex) },
                        { "value",    lo.value },
                        { "Position", globalAbscissae_[lo.maxIndex] + lc.positions[0] }
                    };
                }
                maxPerSection.push_back(total);
                perSection.push_back(detail);
            }

            double spanMax = maxAbsInVector(maxPerSection);
            maxPerSpan.push_back(spanMax);
            pointLoadResult.section = indexOf(maxPerSection, spanMax);
            bestPerSpan.push_back(perSection[pointLoadResult.section]);
        }

        double globalMax = maxAbsInVector(maxPerSpan);
        pointLoadResult.maxValue = globalMax;
        pointLoadResult.span = indexOf(maxPerSpan, globalMax);
        pointLoadResult.load = bestPerSpan[pointLoadResult.span];
    }

    // ── Charge répartie ───────────────────────────────────────────────────
    {
        std::vector<std::map<std::string, std::map<std::string, double>>> bestPerSpan;
        std::vector<double> maxPerSpan;

        for (size_t span = 0; span < activeSpans.size(); ++span) {
            std::vector<double> maxPerSection;
            std::vector<std::map<std::string, std::map<std::string, double>>> perSection;

            for (size_t sec = 0; sec < spanNodes_[span].size(); ++sec) {
                double total = 0.0;
                std::map<std::string, std::map<std::string, double>> detail;

                for (auto& lc : distributedLoads) {
                    Position1D lo = pluralRectangularLoad(lc.intensities, lc.positions, span, sec);
                    total += lo.value;
                    detail[lc.name] = {
                        { "alpha",    static_cast<double>(lo.maxIndex) },
                        { "value",    lo.value },
                        { "Position", globalAbscissae_[lo.maxIndex] + lc.positions[0] }
                    };
                }
                maxPerSection.push_back(total);
                perSection.push_back(detail);
            }

            double spanMax = maxAbsInVector(maxPerSection);
            maxPerSpan.push_back(spanMax);
            distributedLoadResult.section = indexOf(maxPerSection, spanMax);
            bestPerSpan.push_back(perSection[distributedLoadResult.section]);
        }

        double globalMax = maxAbsInVector(maxPerSpan);
        distributedLoadResult.maxValue = globalMax;
        distributedLoadResult.span = indexOf(maxPerSpan, globalMax);
        distributedLoadResult.load = bestPerSpan[distributedLoadResult.span];
    }

    // ── Charge combinée ───────────────────────────────────────────────────
    {
        std::vector<std::map<std::string, std::map<std::string, double>>> bestPerSpan;
        std::vector<double> maxPerSpan;
        std::vector<double> posPerSpan;
        std::vector<size_t> secPerSpan;

        for (size_t span = 0; span < activeSpans.size(); ++span) {
            std::vector<double> maxPerSection;
            std::vector<double> posPerSection;
            std::vector<std::map<std::string, std::map<std::string, double>>> perSection;

            for (size_t sec = 0; sec < spanNodes_[span].size(); ++sec) {
                CombinedLoadResult lo = combinedLoad(span, sec);
                maxPerSection.push_back(lo.value);
                posPerSection.push_back(lo.position);
                perSection.push_back(lo.breakdown);
            }

            double spanMax = maxAbsInVector(maxPerSection);
            size_t critSec = indexOf(maxPerSection, spanMax);
            maxPerSpan.push_back(spanMax);
            posPerSpan.push_back(posPerSection[critSec]);
            secPerSpan.push_back(critSec);
            bestPerSpan.push_back(perSection[critSec]);
        }

        double globalMax = maxAbsInVector(maxPerSpan);
        combinedLoadResult.maxValue = globalMax;
        combinedLoadResult.span = indexOf(maxPerSpan, globalMax);
        combinedLoadResult.section = secPerSpan[combinedLoadResult.span];
        combinedLoadResult.position = posPerSpan[combinedLoadResult.span];
        combinedLoadResult.load = bestPerSpan[combinedLoadResult.span];
    }
}

// =============================================================================
//  metersToIndex
// =============================================================================
size_t LoadEnvelope::metersToIndex(double positionMeters)
{
    if (positionMeters < globalAbscissae_.front() ||
        positionMeters > globalAbscissae_.back())
        return globalAbscissae_.size();   // sentinelle "hors borne"

    size_t idx = 0;
    double minDist = std::abs(globalAbscissae_[0] - positionMeters);

    for (size_t i = 1; i < globalAbscissae_.size(); ++i) {
        double dist = std::abs(globalAbscissae_[i] - positionMeters);
        if (dist < minDist) { minDist = dist; idx = i; }
        if (globalAbscissae_[i] > positionMeters + minDist) break;
    }
    return idx;
}

// =============================================================================
//  onePointLoad
// =============================================================================
double LoadEnvelope::onePointLoad(double intensity, size_t span,
    size_t section, size_t alpha)
{
    if (alpha >= influenceCurves_[span][section].size()) {
        std::cout << "Warning: alpha out of range for span " << span
            << " section " << section << ". Returning 0.\n";
        return 0.0;
    }
    return influenceCurves_[span][section][alpha] * intensity;
}

// =============================================================================
//  pluralPointLoad
// =============================================================================
Position1D LoadEnvelope::pluralPointLoad(const std::vector<double>& intensities,
    const std::vector<double>& offsets,
    size_t span, size_t section)
{
    std::vector<double> values;
    values.reserve(globalAbscissae_.size());

    for (const auto& origin : globalAbscissae_) {
        double total = 0.0;
        for (size_t k = 0; k < intensities.size(); ++k) {
            size_t alpha = metersToIndex(offsets[k] + origin);
            if (alpha < globalAbscissae_.size())
                total += onePointLoad(intensities[k], span, section, alpha);
        }
        values.push_back(total);
    }

    double maxVal = maxAbsInVector(values);
    return Position1D{ indexOf(values, maxVal), maxVal };
}

// =============================================================================
//  oneRectangularLoad
// =============================================================================
double LoadEnvelope::oneRectangularLoad(double intensity, size_t span,
    size_t section, size_t begin, size_t end)
{
    std::vector<double> x, y;
    x.reserve(end - begin);
    y.reserve(end - begin);
    for (size_t i = begin; i < end; ++i) {
        x.push_back(globalAbscissae_[i]);
        y.push_back(influenceCurves_[span][section][i]);
    }
    return trapeze(x, y) * intensity;
}

// =============================================================================
//  pluralRectangularLoad
// =============================================================================
Position1D LoadEnvelope::pluralRectangularLoad(const std::vector<double>& intensities,
    const std::vector<double>& offsets,
    size_t span, size_t section)
{
    // Positions cumulatives des bords de chaque segment
    std::vector<double> cumOffsets;
    double acc = offsets.front();
    for (size_t i = 0; i < offsets.size() - 1; ++i) {
        cumOffsets.push_back(acc);
        acc += offsets[i + 1];
    }
    cumOffsets.push_back(acc);

    std::vector<double> values;
    values.reserve(globalAbscissae_.size());

    for (const auto& origin : globalAbscissae_) {
        double total = 0.0;

        for (size_t k = 0; k < intensities.size(); ++k) {
            double xBegin = std::max(cumOffsets[k] + origin, globalAbscissae_.front());
            double xEnd = std::min(cumOffsets[k + 1] + origin, globalAbscissae_.back());
            if (xEnd <= xBegin) continue;

            size_t iBegin = metersToIndex(xBegin);
            size_t iEnd = metersToIndex(xEnd);
            if (iBegin >= globalAbscissae_.size()) iBegin = 0;
            if (iEnd >= globalAbscissae_.size()) iEnd = globalAbscissae_.size() - 1;
            if (iEnd > iBegin)
                total += oneRectangularLoad(intensities[k], span, section, iBegin, iEnd);
        }
        values.push_back(total);
    }

    double maxVal = maxAbsInVector(values);
    return Position1D{ indexOf(values, maxVal), maxVal };
}

// =============================================================================
//  combinedLoad
// =============================================================================
CombinedLoadResult LoadEnvelope::combinedLoad(size_t span, size_t section)
{
    std::vector<double> values;
    values.reserve(globalAbscissae_.size());
    std::vector<std::map<std::string, std::map<std::string, double>>> breakdowns;

    for (size_t wi = 0; wi < globalAbscissae_.size(); ++wi) {
        const double origin = globalAbscissae_[wi];
        std::map<std::string, std::map<std::string, double>> detail;
        double total = 0.0;

        // Charges ponctuelles
        for (auto& lc : pointLoads) {
            std::map<std::string, double> row;
            for (size_t k = 0; k < lc.intensities.size(); ++k) {
                size_t alpha = metersToIndex(lc.positions[k] + origin);
                double contrib = (alpha < globalAbscissae_.size())
                    ? onePointLoad(lc.intensities[k], span, section, alpha)
                    : 0.0;
                total += contrib;
                row[std::to_string(static_cast<int>(lc.intensities[k])) + " " +
                    std::to_string(k + 1)] = contrib;
            }
            row["Position"] = origin + lc.positions[0];
            detail[lc.name] = row;
        }

        // Charges réparties
        for (auto& lc : distributedLoads) {
            std::map<std::string, double> row;

            std::vector<double> cumOffsets;
            double acc = lc.positions.front();
            for (size_t i = 0; i < lc.positions.size() - 1; ++i) {
                cumOffsets.push_back(acc);
                acc += lc.positions[i + 1];
            }
            cumOffsets.push_back(acc);

            for (size_t k = 0; k < lc.intensities.size(); ++k) {
                double xBegin = std::max(cumOffsets[k] + origin, globalAbscissae_.front());
                double xEnd = std::min(cumOffsets[k + 1] + origin, globalAbscissae_.back());
                double contrib = 0.0;
                if (xEnd > xBegin) {
                    size_t iB = metersToIndex(xBegin);
                    size_t iE = metersToIndex(xEnd);
                    if (iB >= globalAbscissae_.size()) iB = 0;
                    if (iE >= globalAbscissae_.size()) iE = globalAbscissae_.size() - 1;
                    if (iE > iB)
                        contrib = oneRectangularLoad(lc.intensities[k], span, section, iB, iE);
                }
                total += contrib;
                row[std::to_string(static_cast<int>(lc.intensities[k])) + " " +
                    std::to_string(k + 1)] = contrib;
            }
            row["Position"] = origin + lc.positions[0];
            detail[lc.name] = row;
        }

        values.push_back(total);
        breakdowns.push_back(detail);
    }

    double maxVal = maxAbsInVector(values);
    size_t bestIdx = indexOf(values, maxVal);
    return CombinedLoadResult{ bestIdx, globalAbscissae_[bestIdx], maxVal, breakdowns[bestIdx] };
}