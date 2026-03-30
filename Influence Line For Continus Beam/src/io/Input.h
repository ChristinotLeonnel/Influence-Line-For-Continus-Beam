#pragma once

#include "utils/Utils.h"

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

    void loadFromFile();
};

