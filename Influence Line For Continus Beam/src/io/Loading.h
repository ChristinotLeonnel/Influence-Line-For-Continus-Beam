#pragma once

#include "Input.h"
#include "utils/Utils.h"

#include <string>
#include <vector>

class Loading : public Configuration
{
	using Configuration::Configuration;

public:
	Loading(std::vector<std::vector<std::vector<double>>> CURVES, std::vector<double> POSITION, 
		std::vector<std::vector<double>> SpanNodePositions
	);

	double OnePointLoad(double intensity, size_t span, size_t section, size_t alpha);
	Position1D PluralPointLoad(const std::vector<double>& intensity, const std::vector<double>& length, size_t span, size_t section);

	double OneRectangularLoad(double intensity, size_t span, size_t section, size_t begin, size_t end); 
	Position1D PluralRectangularLoad(const std::vector<double>& intensity, const std::vector<double>& length, size_t span, size_t section);

	CombineLoadPosition CombinedLoad(size_t span, size_t section);

	load_delivery Rectangular_load;  
	load_delivery Point_load;
	load_delivery Combined_load;

private:
	size_t MetersToPosition(double PositionMeters);
	std::vector<std::vector<double>> SpanNodePositions;
	std::vector<std::vector<std::vector<double>>> CURVES;
	std::vector<double> POSITION; 

};



