#include "LIPoutreContinue/loading/Input.h"

#include <vector>
#include <stdexcept>

void Configuration::loadFromData(
    const std::vector<double>& spans_in,
    double steps_in,
    const std::vector<double>& youngModule_in,
    const std::vector<double>& inertie_in,
    const std::vector<load>& pointLoads_in,
    const std::vector<load>& distribLoads_in)
{
    spans           = spans_in;
    steps           = steps_in;
    YoungModule     = youngModule_in;
    Inertie         = inertie_in;
    Point_LOAD      = pointLoads_in;
    Rectangulare_LOAD = distribLoads_in;

    if (spans.empty()) {
        throw std::invalid_argument(
            "Configuration::loadFromData: 'spans' est vide — au moins une "
            "travée est requise. Exemple attendu : spans=[10.0, 10.0] "
            "(deux travées de 10 m).");
    }
}
