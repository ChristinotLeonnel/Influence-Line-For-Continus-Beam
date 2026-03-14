#pragma once
#ifndef __OUTPUT__
#define __OUTPUT__

#include "Hyperstatique.h"
#include "ProjectPaths.h"
#include "Utils.h"

#include <vector>
#include <filesystem>

class Output : public Hyperstatique
{
    using Hyperstatique::Hyperstatique;

public:
    Output(std::vector<double>& E,
           std::vector<double>& I,
           std::vector<double>& L,
           double& steps,
           std::filesystem::path root = "");

    Position3D BendingMomentMaxPositions {0, 0, 0, 0.0};
    Position3D DeflectionMaxPositions    {0, 0, 0, 0.0};
    Position3D RotationMaxPositions      {0, 0, 0, 0.0};
    Position3D ShearForceMaxPositions    {0, 0, 0, 0.0};
    Position2D SupportMomentMaxPositions {0, 0, 0.0};
};

#endif // __OUTPUT__
