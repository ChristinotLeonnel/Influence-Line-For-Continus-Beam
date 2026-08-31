#pragma once
// =============================================================================
//  DictConverter.h — Conversion centralisée struct C++ -> dict Python
// =============================================================================
//
//  RÈGLE DU PROJET : toute structure de données C++ renvoyée côté Python
//  doit arriver sous forme de `dict` — indexable, sérialisable en JSON
//  (json.dumps), sans jamais obliger l'appelant à connaître un type opaque
//  de l'extension compilée ni à appeler explicitement une méthode de
//  conversion.
//
//  Toutes les conversions struct -> dict de la librairie sont regroupées
//  ICI, en un seul endroit, sous forme de méthodes statiques surchargées
//  (une par struct). N'importe quelle classe/fonction de bindings.cpp (ou
//  d'un futur fichier de bindings) y accède simplement via :
//
//      #include "DictConverter.h"
//      ...
//      py::dict d = DictConverter::toDict(maStruct);
//
//  Avantages de centraliser ainsi plutôt que d'avoir une fonction libre par
//  struct éparpillée dans bindings.cpp :
//    - Un seul endroit à modifier si un champ change/s'ajoute.
//    - La surcharge (même nom `toDict` pour tous les types) donne une API
//      uniforme, quel que soit le struct converti.
//    - Réutilisable tel quel dans n'importe quelle classe pybind11 exposée
//      (Output, Hyperstatique, Loading, Configuration, ProjectPaths, ...).
// =============================================================================

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include "LIPoutreContinue/Output.h"
#include "LIPoutreContinue/Utils.h"
#include "LIPoutreContinue/io/ProjectPaths.h"
#include "LIPoutreContinue/loading/Input.h"
#include "LIPoutreContinue/loading/Loading.h"

namespace py = pybind11;

class DictConverter {
public:
  // -- load ------------------------------------------------------------------
  static py::dict toDict(const load &l) {
    py::dict out;
    out["intensity"] = l.Intensity;
    out["length"] = l.Length;
    out["name"] = l.name;
    return out;
  }

  // -- Position1D / Position2D / Position3D -----------------------------------
  static py::dict toDict(const Position1D &p) {
    py::dict out;
    out["max_position"] = p.max_position;
    out["value"] = p.value;
    return out;
  }

  static py::dict toDict(const Position2D &p) {
    py::dict out;
    out["i"] = p.i;
    out["j"] = p.j;
    out["val"] = p.val;
    return out;
  }

  static py::dict toDict(const Position3D &p) {
    py::dict out;
    out["i"] = p.i;
    out["j"] = p.j;
    out["k"] = p.k;
    out["val"] = p.val;
    return out;
  }

  // -- CombineLoadPosition ------------------------------------------------------
  static py::dict toDict(const CombineLoadPosition &c) {
    py::dict out;
    out["max_position"] = c.max_position;
    out["position"] = c.position;
    out["value"] = c.value;
    out["addition"] = c.Addition; // dict[str, dict[str, float]] (conversion
                                  // auto via pybind11/stl.h)
    return out;
  }

  // -- load_delivery ----------------------------------------------------------
  static py::dict toDict(const load_delivery &d) {
    py::dict out;
    out["load"] = d.load; // dict[str, dict[str, float]] (conversion auto via
                          // pybind11/stl.h)
    out["span"] = d.span;
    out["section"] = d.section;
    out["maximum_value"] = d.maximum_value;
    out["position"] = d.position;
    return out;
  }

  // -- Output::LoadEnvelope -----------------------------------------------------
  static py::dict toDict(const Output::LoadEnvelope &e) {
    py::dict out;
    out["point_load"] = toDict(e.pointLoad);
    out["rectangular_load"] = toDict(e.rectangularLoad);
    out["combined_load"] = toDict(e.combinedLoad);
    return out;
  }

  // -- Loading::CriticalSectionResult -------------------------------------------
  static py::dict toDict(const Loading::CriticalSectionResult &r) {
    py::dict out;
    out["point"] = toDict(r.point);
    out["rect"] = toDict(r.rect);
    out["combined"] = toDict(r.combined);
    return out;
  }

  // -- Configuration ------------------------------------------------------------
  static py::dict toDict(const Configuration &c) {
    py::dict out;
    out["spans"] = c.spans;
    out["steps"] = c.steps;
    out["inertie"] = c.Inertie;
    out["young_module"] = c.YoungModule;
    out["point_loads"] = toDictList(c.Point_LOAD);
    out["distrib_loads"] = toDictList(c.Rectangulare_LOAD);
    return out;
  }

  // -- ProjectPaths ---------------------------------------------------------------
  static py::dict toDict(const ProjectPaths &p) {
    py::dict out;
    out["root"] = p.root;
    out["input"] = p.input;
    out["influence_lines"] = p.influence_lines;
    out["critical_values"] = p.critical_values;
    out["load_envelopes"] = p.load_envelopes;
    out["load_positioning"] = p.load_positioning;
    out["env_global"] = p.env_global;
    out["env_global_point"] = p.env_global_point;
    out["env_global_dist"] = p.env_global_dist;
    out["env_global_combined"] = p.env_global_combined;
    out["env_critical"] = p.env_critical;
    out["env_critical_point"] = p.env_critical_point;
    out["env_critical_dist"] = p.env_critical_dist;
    out["env_critical_combined"] = p.env_critical_combined;
    out["pos_global"] = p.pos_global;
    out["pos_global_point"] = p.pos_global_point;
    out["pos_global_dist"] = p.pos_global_dist;
    out["pos_global_combined"] = p.pos_global_combined;
    out["pos_critical"] = p.pos_critical;
    out["pos_critical_point"] = p.pos_critical_point;
    out["pos_critical_dist"] = p.pos_critical_dist;
    out["pos_critical_combined"] = p.pos_critical_combined;
    return out;
  }

  // -- Listes de struct -> list[dict] --------------------------------------------
  // (utilisé pour Configuration.point_loads / distrib_loads,
  //  Loading.point_load_inputs / distrib_load_inputs, etc.)
  static py::list toDictList(const std::vector<load> &loads) {
    py::list out;
    for (const auto &l : loads)
      out.append(toDict(l));
    return out;
  }
};
