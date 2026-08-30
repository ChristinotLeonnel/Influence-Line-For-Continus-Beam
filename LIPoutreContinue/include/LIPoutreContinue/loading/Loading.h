#pragma once

#include "LIPoutreContinue/loading/Input.h"
#include "LIPoutreContinue/Utils.h"

#include <string>
#include <vector>

class Loading : public Configuration
{
	using Configuration::Configuration;

public:
	// Constructeur « sans fichier » : les portées et les charges sont fournies
	// directement par l'appelant — aucun accès disque, aucune dépendance à
	// path.json ni à structural model input.txt.
	Loading(std::vector<std::vector<std::vector<double>>> CURVES, std::vector<double> POSITION,
		std::vector<std::vector<double>> SpanNodePositions,
		const std::vector<double>& spans,
		const std::vector<load>& pointLoads,
		const std::vector<load>& distribLoads
	);

	double OnePointLoad(double intensity, size_t span, size_t section, size_t alpha);
	Position1D PluralPointLoad(const std::vector<double>& intensity, const std::vector<double>& length, size_t span, size_t section);

	double OneRectangularLoad(double intensity, size_t span, size_t section, size_t begin, size_t end); 
	Position1D PluralRectangularLoad(const std::vector<double>& intensity, const std::vector<double>& length, size_t span, size_t section);

	CombineLoadPosition CombinedLoad(size_t span, size_t section);

	// Résultat de charge critique (point / réparti / combiné) pour une travée
	// donnée : la section critique est recherchée sur toute la travée, puis
	// le détail de charge (alpha, valeur, position) est reconstruit à cette
	// section. Utilisé par Output::exportLoadEnvelopes() — tout le calcul
	// vit ici, Output.cpp se contente d'écrire le résultat.
	struct CriticalSectionResult {
		load_delivery point;
		load_delivery rect;
		load_delivery combined;
	};
	CriticalSectionResult computeCriticalSection(size_t span);

	load_delivery Rectangular_load;  
	load_delivery Point_load;
	load_delivery Combined_load;

private:
	size_t MetersToPosition(double PositionMeters);
	std::vector<std::vector<double>> SpanNodePositions;
	std::vector<std::vector<std::vector<double>>> CURVES;
	std::vector<double> POSITION; 

};
