#include "LIPoutreContinue/loading/Loading.h"
#include "LIPoutreContinue/Utils.h"
#include "LIPoutreContinue/loading/Input.h"

#include <iostream>
#include <map>
#include <sstream>

Loading::Loading(std::vector<std::vector<std::vector<double>>> CURVES,
                 std::vector<double> POSITION,
                 std::vector<std::vector<double>> SpanNodePositions,
                 const std::vector<double> &spans_in,
                 const std::vector<load> &pointLoads,
                 const std::vector<load> &distribLoads)
    :

      Configuration(), SpanNodePositions(SpanNodePositions), CURVES(CURVES),
      POSITION(POSITION) {
  spans = spans_in;
  Point_LOAD = pointLoads;
  Rectangulare_LOAD = distribLoads;

  std::vector<double> CopieSpan = spans;
  std::vector<size_t> LZeroIdx;
  for (size_t i = 0; i < spans.size(); ++i)
    if (spans[i] == 0)
      LZeroIdx.push_back(i);

  removeByIndices(CopieSpan, LZeroIdx);

  std::vector<std::map<std::string, std::map<std::string, double>>> first;
  std::vector<double> ValuesPerSpan;
  double val = 0;

  for (size_t span = 0; span < CopieSpan.size(); span++) {

    std::vector<double> ValuesPerSection;
    std::vector<std::map<std::string, std::map<std::string, double>>> Second;

    for (size_t section = 0; section < SpanNodePositions[span].size();
         section++) {

      val = 0;
      std::map<std::string, std::map<std::string, double>> TT;

      for (auto &k : Point_LOAD) {
        std::map<std::string, double> RR;
        Position1D lo = PluralPointLoad(k.Intensity, k.Length, span, section);
        val += lo.value;
        RR["alpha"] = static_cast<double>(lo.max_position);
        RR["value"] = lo.value;
        // Position physique de la 1ère charge = first_wall + length[0]
        // POSITION[max_position] = first_wall optimal → +length[0] donne la
        // vraie position
        RR["Position"] = POSITION[lo.max_position];
        TT[k.name] = RR;
      }
      ValuesPerSection.push_back(val);
      Second.push_back(TT);
    }
    auto maximum_section = MaxValueInVector(ValuesPerSection);
    ValuesPerSpan.push_back(maximum_section);
    Point_load.section = Indice_of(ValuesPerSection, maximum_section);
    first.push_back(Second[Point_load.section]);
  }
  double maximum_span = MaxValueInVector(ValuesPerSpan);

  Point_load.maximum_value = maximum_span;
  Point_load.span = Indice_of(ValuesPerSpan, maximum_span);
  Point_load.load = first[Point_load.span];

  ValuesPerSpan = {};
  first = {};
  for (size_t span = 0; span < CopieSpan.size(); span++) {

    std::vector<double> ValuesPerSection;
    std::vector<std::map<std::string, std::map<std::string, double>>> Second;

    for (size_t section = 0; section < SpanNodePositions[span].size();
         section++) {

      val = 0;
      std::map<std::string, std::map<std::string, double>> TT;

      for (auto &k : Rectangulare_LOAD) {
        std::map<std::string, double> RR;
        Position1D lo =
            PluralRectangularLoad(k.Intensity, k.Length, span, section);
        val += lo.value;
        RR["alpha"] = static_cast<double>(lo.max_position);
        RR["value"] = lo.value;
        // Position physique du début du 1er segment = first_wall + length[0]
        RR["Position"] = POSITION[lo.max_position];
        TT[k.name] = RR;
      }
      ValuesPerSection.push_back(val);
      Second.push_back(TT);
    }
    auto maximum_section = MaxValueInVector(ValuesPerSection);
    ValuesPerSpan.push_back(maximum_section);
    Rectangular_load.section = Indice_of(ValuesPerSection, maximum_section);
    first.push_back(Second[Rectangular_load.section]);
  }
  maximum_span = MaxValueInVector(ValuesPerSpan);

  Rectangular_load.maximum_value = maximum_span;
  Rectangular_load.span = Indice_of(ValuesPerSpan, maximum_span);
  Rectangular_load.load = first[Rectangular_load.span];

  first = {};
  ValuesPerSpan = {};
  std::vector<double>
      PositionsPerSpan;                // position optimale du convoi par travée
  std::vector<size_t> SectionsPerSpan; // section critique par travée

  for (size_t span = 0; span < CopieSpan.size(); span++) {

    std::vector<double> ValuesPerSection;
    std::vector<double> PositionsPerSection;
    std::vector<std::map<std::string, std::map<std::string, double>>> Second;

    for (size_t section = 0; section < SpanNodePositions[span].size();
         section++) {
      CombineLoadPosition LO = CombinedLoad(span, section);
      ValuesPerSection.push_back(LO.value);
      PositionsPerSection.push_back(
          LO.position); // POSITION[IJ] pour cette section
      Second.push_back(LO.Addition);
    }
    auto maximum_section = MaxValueInVector(ValuesPerSection);
    size_t critical_sec = Indice_of(ValuesPerSection, maximum_section);

    ValuesPerSpan.push_back(maximum_section);
    PositionsPerSpan.push_back(
        PositionsPerSection[critical_sec]); // position exacte pour cette travée
    SectionsPerSpan.push_back(critical_sec);
    first.push_back(Second[critical_sec]);
  }
  maximum_span = MaxValueInVector(ValuesPerSpan);

  Combined_load.maximum_value = maximum_span;
  Combined_load.span = Indice_of(ValuesPerSpan, maximum_span);
  Combined_load.section = SectionsPerSpan[Combined_load.span];
  Combined_load.position =
      PositionsPerSpan[Combined_load.span]; // position exacte de la travée
                                            // critique
  Combined_load.load = first[Combined_load.span];
}

// =============================================================================
//  MetersToPosition — Fix bug 1 & 2
//
//  Retourne l'indice dans POSITION le plus proche de PositionMeters
//  (arrondi au point de discrétisation le plus proche, pas seulement UP).
//  Retourne POSITION.size() (hors borne) si la valeur est strictement en
//  dehors de [POSITION.front(), POSITION.back()] — valeur sentinelle
//  distincte de 0 pour lever l'ambiguïté avec la position x=0.
// =============================================================================
size_t Loading::MetersToPosition(double PositionMeters) {
  // Hors borne → sentinelle SIZE (jamais confondue avec indice 0)
  if (PositionMeters < POSITION.front() || PositionMeters > POSITION.back())
    return POSITION.size(); // sentinelle "invalide"

  // Recherche du point de grille le plus proche (arrondi au plus près)
  size_t idx = 0;
  double minDist = std::abs(POSITION[0] - PositionMeters);

  for (size_t i = 1; i < POSITION.size(); ++i) {
    double dist = std::abs(POSITION[i] - PositionMeters);
    if (dist < minDist) {
      minDist = dist;
      idx = i;
    }
    // Optimisation : dès que la distance repart en hausse on peut sortir
    // (POSITION est trié en ordre croissant)
    if (POSITION[i] > PositionMeters + minDist)
      break;
  }
  return idx;
}

double Loading::OnePointLoad(double intensity, size_t span, size_t section,
                             size_t alpha) {
  if (alpha >= CURVES[span][section].size()) {
    std::cout << "Warning: Alpha value exceeds the maximum defined in CURVES "
                 "for span "
              << span << " and section " << section << ". Returning 0."
              << std::endl;
    return 0;
  }
  return CURVES[span][section][alpha] * intensity;
}

Position1D Loading::PluralPointLoad(const std::vector<double> &intensity,
                                    const std::vector<double> &length,
                                    size_t span, size_t section) {
  std::vector<double> VALUES;
  double val = 0;
  size_t compteur = 0;

  // Calcul des positions successives relatives au début du convoi
  // LengthSuccessive[k] = distance depuis le bord gauche du convoi jusqu'au
  // début du segment k LengthSuccessive[k+1] = distance jusqu'à la fin du
  // segment k
  std::vector<double> LengthSuccessive;
  double acc = length.front(); // offset initial (length[0])
  for (size_t i = 0; i < length.size() - 1; ++i) {
    LengthSuccessive.push_back(acc);
    acc += length[i + 1];
  }
  LengthSuccessive.push_back(acc);
  // Exemple length={0,3,5,2} → LengthSuccessive={0,3,8,10}

  for (auto &first_wall : POSITION) {
    compteur = 0;
    val = 0;
    for (auto &I : intensity) {
      size_t alpha = MetersToPosition(LengthSuccessive[compteur] + first_wall);
      std::cout << alpha << " ==> " << LengthSuccessive[compteur] + first_wall
                << std::endl;
      // Ignorer les charges hors de la poutre (sentinelle)
      if (alpha < POSITION.size())
        val += OnePointLoad(I, span, section, alpha);
      compteur += 1;
    }
    VALUES.push_back(val);
  }
  val = MaxValueInVector(VALUES);

  Position1D L{Indice_of(VALUES, val), val};
  return L;
}

double Loading::OneRectangularLoad(double intensity, size_t span,
                                   size_t section, size_t begin, size_t end) {
  std::vector<double> x, y;
  for (size_t i = begin; i < end; i++) {
    x.push_back(POSITION[i]);
    y.push_back(CURVES[span][section][i]);
  }
  return trapeze(x, y) * intensity;
}

// =============================================================================
//  PluralRectangularLoad — Fix bug 3 & 4
//
//  Convention des positions (identique à PluralPointLoad) :
//    length[0]          = offset de départ du convoi (distance avant le 1er
//    segment) length[1..N]       = longueurs successives des segments
//
//  first_wall balaye toutes les positions X de la grille.
//  Pour chaque position de balayage, l'offset du convoi est :
//    segment_k_debut = first_wall + LengthSuccessive[k]
//    segment_k_fin   = first_wall + LengthSuccessive[k+1]
//
//  Correction :
//   - MetersToPosition retourne maintenant SIZE comme sentinelle (hors borne),
//     on clip proprement begin/end dans [0, POSITION.size()-1].
//   - On intègre uniquement la portion du segment qui se trouve sur la poutre.
//   - La position retournée (max_position) est l'indice de first_wall optimal,
//     soit POSITION[max_position] = x de départ du convoi → position exacte.
// =============================================================================
Position1D Loading::PluralRectangularLoad(const std::vector<double> &intensity,
                                          const std::vector<double> &length,
                                          size_t span, size_t section) {
  // Calcul des positions successives relatives au début du convoi
  // LengthSuccessive[k] = distance depuis le bord gauche du convoi jusqu'au
  // début du segment k LengthSuccessive[k+1] = distance jusqu'à la fin du
  // segment k
  std::vector<double> LengthSuccessive;
  double acc = length.front(); // offset initial (length[0])
  for (size_t i = 0; i < length.size() - 1; ++i) {
    LengthSuccessive.push_back(acc);
    acc += length[i + 1];
  }
  LengthSuccessive.push_back(acc);
  // Exemple length={0,3,5,2} → LengthSuccessive={0,3,8,10}

  std::vector<double> VALUES;
  VALUES.reserve(POSITION.size());

  for (const auto &first_wall : POSITION) {

    double val = 0.0;

    for (size_t k = 0; k < intensity.size(); ++k) {

      double x_begin_m = LengthSuccessive[k] + first_wall;
      double x_end_m = LengthSuccessive[k + 1] + first_wall;

      // Clipping : on ignore la partie hors de la poutre
      x_begin_m = std::max(x_begin_m, POSITION.front());
      x_end_m = std::min(x_end_m, POSITION.back());

      if (x_end_m <= x_begin_m)
        continue; // segment entièrement hors de la poutre

      size_t begin = MetersToPosition(x_begin_m);
      size_t end = MetersToPosition(x_end_m);

      // Sécurité : MetersToPosition ne retourne plus SIZE ici (x clippé),
      // mais on garde le guard pour robustesse
      if (begin >= POSITION.size())
        begin = 0;
      if (end >= POSITION.size())
        end = POSITION.size() - 1;

      if (end > begin)
        val += OneRectangularLoad(intensity[k], span, section, begin, end);
    }
    VALUES.push_back(val);
  }

  double maxVal = MaxValueInVector(VALUES);
  // max_position = indice dans POSITION du décalage first_wall optimal
  // → POSITION[max_position] est la position exacte de départ du convoi
  return Position1D{Indice_of(VALUES, maxVal), maxVal};
}

// =============================================================================
//  computeCriticalSection — calcule, pour une travée donnée, la section
//  critique et le détail de charge (point / réparti / combiné).
//
//  Anciennement dupliqué dans Output::exportLoadEnvelopes() (lambda
//  computeLoadingAt) : tout le calcul de chargement vit maintenant ici,
//  Output.cpp se contente d'appeler cette méthode et d'écrire le résultat.
// =============================================================================
Loading::CriticalSectionResult Loading::computeCriticalSection(size_t span) {
  CriticalSectionResult result;

  // ── Charge ponctuelle ────────────────────────────────────────────────────
  {
    std::vector<double> vals;
    for (size_t sec = 0; sec < SpanNodePositions[span].size(); ++sec) {
      double v = 0.0;
      for (auto &k : Point_LOAD)
        v += PluralPointLoad(k.Intensity, k.Length, span, sec).value;
      vals.push_back(v);
    }
    double mv = MaxValueInVector(vals);
    result.point.span = span;
    result.point.section = Indice_of(vals, mv);
    result.point.maximum_value = mv;
    std::map<std::string, std::map<std::string, double>> TT;
    for (auto &k : Point_LOAD) {
      auto lo =
          PluralPointLoad(k.Intensity, k.Length, span, result.point.section);
      TT[k.name] = {{"alpha", (double)lo.max_position},
                    {"value", lo.value},
                    {"Position", POSITION[lo.max_position]}};
    }
    result.point.load = TT;
  }

  // ── Charge répartie ──────────────────────────────────────────────────────
  {
    std::vector<double> vals;
    for (size_t sec = 0; sec < SpanNodePositions[span].size(); ++sec) {
      double v = 0.0;
      for (auto &k : Rectangulare_LOAD)
        v += PluralRectangularLoad(k.Intensity, k.Length, span, sec).value;
      vals.push_back(v);
    }
    double mv = MaxValueInVector(vals);
    result.rect.span = span;
    result.rect.section = Indice_of(vals, mv);
    result.rect.maximum_value = mv;
    std::map<std::string, std::map<std::string, double>> TT;
    for (auto &k : Rectangulare_LOAD) {
      auto lo = PluralRectangularLoad(k.Intensity, k.Length, span,
                                      result.rect.section);
      TT[k.name] = {{"alpha", (double)lo.max_position},
                    {"value", lo.value},
                    {"Position", POSITION[lo.max_position]}};
    }
    result.rect.load = TT;
  }

  // ── Charge combinée ──────────────────────────────────────────────────────
  {
    std::vector<double> vals, positions;
    std::vector<std::map<std::string, std::map<std::string, double>>> maps;
    for (size_t sec = 0; sec < SpanNodePositions[span].size(); ++sec) {
      auto lo = CombinedLoad(span, sec);
      vals.push_back(lo.value);
      positions.push_back(lo.position);
      maps.push_back(lo.Addition);
    }
    double mv = MaxValueInVector(vals);
    result.combined.span = span;
    result.combined.section = Indice_of(vals, mv);
    result.combined.maximum_value = mv;
    result.combined.position = positions[result.combined.section];
    result.combined.load = maps[result.combined.section];
  }

  return result;
}

// =============================================================================
//  CombinedLoad — Fix bug 3 (position UDL) + cohérence avec les corrections
//  ci-dessus
// =============================================================================
CombineLoadPosition Loading::CombinedLoad(size_t span, size_t section) {
  std::vector<double> VALUES;
  VALUES.reserve(POSITION.size());
  std::vector<std::map<std::string, std::map<std::string, double>>> first;

  for (size_t wi = 0; wi < POSITION.size(); ++wi) {

    const double first_wall = POSITION[wi];
    std::map<std::string, std::map<std::string, double>> second;
    double val = 0.0;

    // ── Charges ponctuelles ───────────────────────────────────────────────
    for (auto &j : Point_LOAD) {

      std::map<std::string, double> third;
      size_t compteur = 0;

      for (auto &I : j.Intensity) {
        size_t alpha = MetersToPosition(j.Length[compteur] + first_wall);
        double contribution = (alpha < POSITION.size())
                                  ? OnePointLoad(I, span, section, alpha)
                                  : 0.0;
        val += contribution;
        third[std::to_string(static_cast<int>(I)) + " " +
              std::to_string(compteur + 1)] = contribution;
        ++compteur;
      }
      // Position physique de la 1ère charge de ce groupe
      third["Position"] = first_wall + j.Length[0];
      second[j.name] = third;
    }

    // ── Charges réparties ─────────────────────────────────────────────────
    for (auto &jk : Rectangulare_LOAD) {

      std::map<std::string, double> third;

      // Positions successives relatives au convoi
      std::vector<double> LengthSuccessive;
      double y = jk.Length.front();
      for (size_t i = 0; i < jk.Length.size() - 1; ++i) {
        LengthSuccessive.push_back(y);
        y += jk.Length[i + 1];
      }
      LengthSuccessive.push_back(y);

      for (size_t k = 0; k < jk.Intensity.size(); ++k) {

        double x_begin_m = LengthSuccessive[k] + first_wall;
        double x_end_m = LengthSuccessive[k + 1] + first_wall;

        // Clipping sur la poutre
        x_begin_m = std::max(x_begin_m, POSITION.front());
        x_end_m = std::min(x_end_m, POSITION.back());

        double contribution = 0.0;
        if (x_end_m > x_begin_m) {
          size_t begin = MetersToPosition(x_begin_m);
          size_t end = MetersToPosition(x_end_m);
          if (begin >= POSITION.size())
            begin = 0;
          if (end >= POSITION.size())
            end = POSITION.size() - 1;
          if (end > begin)
            contribution =
                OneRectangularLoad(jk.Intensity[k], span, section, begin, end);
        }

        val += contribution;
        // Fix bug 3 : on stocke contribution ET la position de début du segment
        // pour permettre au post-traitement de reconstruire l'emplacement exact
        std::string key = std::to_string(static_cast<int>(jk.Intensity[k])) +
                          " " + std::to_string(k + 1);
        third[key] = contribution;
      }
      // Position exacte de départ du convoi UDL pour ce balayage
      // Position physique du début du 1er segment de ce groupe
      third["Position"] = first_wall + jk.Length[0];
      second[jk.name] = third;
    }

    VALUES.push_back(val);
    first.push_back(second);
  }

  double ff = MaxValueInVector(VALUES);
  size_t IJ = Indice_of(VALUES, ff);

  return CombineLoadPosition{IJ, POSITION[IJ], ff, first[IJ]};
}
