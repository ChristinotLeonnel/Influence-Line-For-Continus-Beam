#pragma once
// =============================================================================
//  Loading — position la plus défavorable des charges mobiles sur des lignes
//  d'influence, et enveloppes de sollicitation.
// =============================================================================
//
//  Modèle (version 2.0, voir docs/audit/03_numerique.md, défauts N2–N9) :
//
//  * Une ligne d'influence f(p) (effet à une section pour une charge unité à
//    l'abscisse globale p) est l'interpolant LINÉAIRE PAR MORCEAUX de ses
//    valeurs nodales, nul hors de la poutre. Aux discontinuités (effort
//    tranchant au droit de la section) on manipule les limites à gauche f(p⁻)
//    et à droite f(p⁺).
//
//  * Charge ponctuelle / convoi (intensités P_i, `length` = [d0, s1, s2, …]) :
//    essieu i à l'abscisse p + d_i, d_i = d0 + s1 + … + s_i (une SEULE
//    convention pour tous les calculs). Effet g(p) = Σ P_i f(p + d_i), linéaire
//    par morceaux : ses extrema sont atteints en p = t_j − d_i (t_j nœuds),
//    évalués à gauche et à droite → recherche EXACTE sur toutes les positions,
//    y compris l'entrée par la gauche, sans arrondi des essieux aux nœuds.
//
//  * Charge répartie (q_k, `length` = [c0, l1, …, ln]) : tronçon k sur
//    [p + c_k, p + c_{k+1}]. Effet G(p) = Σ q_k [F(p+c_{k+1}) − F(p+c_k)],
//    F = primitive de f : intégration exacte sur les bornes réelles, tronçons
//    partiellement hors poutre compris. Extrema aux points de rupture ou là où
//    G'(p) (linéaire par morceaux) s'annule.
//
//  * Charges indépendantes : en chaque section, enveloppe
//    E⁺ = Σ_k max_p S_k(p) et E⁻ = Σ_k min_p S_k(p) (chaque charge peut aussi
//    être absente, effet 0). Les maxima signés ne sont plus additionnés.
//    La « charge combinée » est la superposition des charges ponctuelles ET
//    réparties, chacune placée indépendamment.
//
//  Limite connue : entre deux nœuds, la ligne d'influence réelle est cubique ;
//  l'interpolation linéaire induit une erreur O(h²) (mesurée : ordre 2,00).
//
//  `scale` multiplie toutes les ordonnées : Output l'utilise pour convertir
//  les lignes de flèche/rotation (m/N, rad/N) en effets de charges en kN.
// =============================================================================

#include "LIPoutreContinue/loading/Input.h"
#include "LIPoutreContinue/loading/MovingLoadSearch.h"
#include "LIPoutreContinue/Utils.h"

#include <string>
#include <vector>

class Loading : public Configuration
{
	using Configuration::Configuration;

public:
	Loading(std::vector<std::vector<std::vector<double>>> CURVES, std::vector<double> POSITION,
		std::vector<std::vector<double>> SpanNodePositions,
		const std::vector<double>& spans,
		const std::vector<load>& pointLoads,
		const std::vector<load>& distribLoads,
		double scale = 1.0
	);

	// Variante SANS COPIE du tenseur : `CURVES` est seulement référencé et
	// doit survivre à l'objet Loading (cas d'Output, qui détient BM/SF/…).
	// Évite une copie N² par grandeur (≈ 39 Mo par tenseur pour N = 2205).
	struct BorrowCurves {};
	Loading(BorrowCurves, const std::vector<std::vector<std::vector<double>>>& CURVES,
		const std::vector<double>& POSITION,
		const std::vector<std::vector<double>>& SpanNodePositions,
		const std::vector<double>& spans,
		const std::vector<load>& pointLoads,
		const std::vector<load>& distribLoads,
		double scale = 1.0
	);

	// Un pointeur interne désigne le tenseur (possédé ou emprunté) : copier
	// l'objet le rendrait incohérent.
	Loading(const Loading&) = delete;
	Loading& operator=(const Loading&) = delete;

	// ── Briques élémentaires (API 1.x conservée) ─────────────────────────────
	// Effet d'une charge ponctuelle posée au nœud d'indice `alpha`.
	double OnePointLoad(double intensity, size_t span, size_t section, size_t alpha);
	// Position la plus défavorable d'un convoi à une section (extrême gouvernant).
	Position1D PluralPointLoad(const std::vector<double>& intensity, const std::vector<double>& length, size_t span, size_t section);
	// Effet d'une charge répartie entre les nœuds d'indices begin et end (inclus).
	double OneRectangularLoad(double intensity, size_t span, size_t section, size_t begin, size_t end);
	// Position la plus défavorable d'une charge répartie à une section.
	Position1D PluralRectangularLoad(const std::vector<double>& intensity, const std::vector<double>& length, size_t span, size_t section);
	// Superposition de toutes les charges (indépendantes) à une section.
	CombineLoadPosition CombinedLoad(size_t span, size_t section);

	struct CriticalSectionResult {
		load_delivery point;
		load_delivery rect;
		load_delivery combined;
	};
	// Recherche restreinte aux sections d'une travée.
	CriticalSectionResult computeCriticalSection(size_t span);
	// Résultat à une section donnée.
	CriticalSectionResult computeAtSection(size_t span, size_t section);

	// ── Enveloppes globales (toute la poutre), calculées au constructeur ────
	load_delivery Rectangular_load;
	load_delivery Point_load;
	load_delivery Combined_load;

	// ── Enveloppes par section : [travée][section] ──────────────────────────
	struct SectionEnvelope {
		std::vector<std::vector<double>> max;   // E⁺ (≥ 0)
		std::vector<std::vector<double>> min;   // E⁻ (≤ 0)
	};
	SectionEnvelope PointEnvelope, RectangularEnvelope, CombinedEnvelope;

	// Extrême d'une charge à une section (voir MovingLoadSearch.h).
	using Extreme = lipc::Extreme;

private:
	lipc::InfluenceLine makeLine(size_t span, size_t section) const;

	size_t nearestNode(double x) const;
	size_t globalIndex(size_t span, size_t section) const;
	std::map<std::string, double> detail(const Extreme& e, bool positive) const;

	void computeAll();
	enum class Kind { Point, Rect, Combined };
	load_delivery collect(Kind kind, size_t spanBegin, size_t spanEnd,
	                      long onlySection = -1) const;

	std::vector<std::vector<double>> SpanNodePositions;
	void init(const std::vector<double>& spans_in, const std::vector<load>& pointLoads,
	          const std::vector<load>& distribLoads);
	std::vector<std::vector<std::vector<double>>> ownedCurves_;
	const std::vector<std::vector<std::vector<double>>>* curves_ = nullptr;
	std::vector<double> POSITION;
	double scale_ = 1.0;
	std::vector<size_t> spanOffset_;   // indice global du 1er nœud de chaque travée

	// Extrêmes par charge : [travée][section][charge]
	std::vector<std::vector<std::vector<Extreme>>> pointExt_, rectExt_;
	std::vector<std::string> pointNames_, rectNames_;
};
