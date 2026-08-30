#pragma once

#include "LIPoutreContinue/Utils.h"

class Configuration {
public:
    std::vector<double> spans;
    double steps;
    std::vector<double> Inertie;
    std::vector<double> YoungModule;
    std::vector<load> Point_LOAD; 
    std::vector<load> Rectangulare_LOAD; 

    Configuration() :
        steps(1) {
    }

    // Mode « sans fichier » : toutes les données sont fournies directement
    // par l'appelant — aucun accès disque, aucune dépendance à path.json
    // ni à structural model input.txt.
    void loadFromData(
        const std::vector<double>& spans_in,
        double steps_in,
        const std::vector<double>& youngModule_in,
        const std::vector<double>& inertie_in,
        const std::vector<load>& pointLoads_in,
        const std::vector<load>& distribLoads_in
    );
};
