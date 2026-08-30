// =============================================================================
//  bindings.cpp — Bindings Python (pybind11) pour Tsaraloha
// =============================================================================
//
//  Expose : Isostatique, Hyperstatique, Loading, Configuration, Output,
//           ProjectPaths, UpdatePositions, ainsi que les structures de
//           données (Load, Position1D/2D/3D, CombineLoadPosition,
//           LoadDelivery, CriticalSectionResult).
//
//  PIÈGE ÉVITÉ — durée de vie des références :
//  Hyperstatique (et donc Output, qui en hérite) NE COPIE PAS E/I/L/steps :
//  il stocke des références (`std::vector<double>& E_spans;`, etc.) vers les
//  vecteurs fournis par l'appelant. En C++ pur, l'appelant garde ces
//  vecteurs vivants (voir examples/Aplication.cpp : `config` reste en vie
//  tout le temps que `Output LI(...)` existe).
//
//  Depuis Python, pybind11 convertit une liste Python en un
//  std::vector<double> TEMPORAIRE au moment de l'appel ; si on bindait le
//  vrai constructeur de Hyperstatique/Output directement, cette référence
//  deviendrait pendante dès le retour du constructeur → comportement
//  indéfini au premier accès.
//
//  Solution : PyHyperstatique / PyOutput possèdent leurs PROPRES copies de
//  E/I/L/steps, déclarées AVANT l'objet réel dans la classe (l'ordre
//  d'initialisation des membres suit l'ordre de déclaration, pas celui de
//  la liste d'initialisation), afin que ces vecteurs survivent aussi
//  longtemps que le wrapper — donc aussi longtemps que le sous-objet réel.
// =============================================================================

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include "LIPoutreContinue/Output.h"
#include "LIPoutreContinue/Utils.h"
#include "LIPoutreContinue/core/Hyperstatique.h"
#include "LIPoutreContinue/core/Isostatique.h"
#include "LIPoutreContinue/io/ProjectPaths.h"
#include "LIPoutreContinue/io/UpdatePositions.h"
#include "LIPoutreContinue/loading/Input.h"
#include "LIPoutreContinue/loading/Loading.h"

namespace py = pybind11;

// =============================================================================
//  Conversion load_delivery / LoadEnvelope → dict Python
//
//  Objectif : les données de chargement (enveloppes, section critique, ...)
//  doivent être directement manipulables côté Python — indexables comme un
//  dict, sérialisables en JSON (json.dumps), sans passer par un objet C++
//  opaque ni appeler explicitement une méthode de conversion.
// =============================================================================
inline py::dict loadDeliveryToDict(const load_delivery &d) {
  py::dict out;
  out["load"] = d.load; // dict[str, dict[str, float]] (conversion auto via
                        // pybind11/stl.h)
  out["span"] = d.span;
  out["section"] = d.section;
  out["maximum_value"] = d.maximum_value;
  out["position"] = d.position;
  return out;
}

inline py::dict loadEnvelopeToDict(const Output::LoadEnvelope &e) {
  py::dict out;
  out["point_load"] = loadDeliveryToDict(e.pointLoad);
  out["rectangular_load"] = loadDeliveryToDict(e.rectangularLoad);
  out["combined_load"] = loadDeliveryToDict(e.combinedLoad);
  return out;
}

// -- Convertisseurs supplémentaires (un par struct) --------------------------

inline py::dict loadToDict(const load &l) {
  py::dict out;
  out["intensity"] = l.Intensity;
  out["length"] = l.Length;
  out["name"] = l.name;
  return out;
}

inline py::dict position1DToDict(const Position1D &p) {
  py::dict out;
  out["max_position"] = p.max_position;
  out["value"] = p.value;
  return out;
}

inline py::dict position2DToDict(const Position2D &p) {
  py::dict out;
  out["i"] = p.i;
  out["j"] = p.j;
  out["val"] = p.val;
  return out;
}

inline py::dict position3DToDict(const Position3D &p) {
  py::dict out;
  out["i"] = p.i;
  out["j"] = p.j;
  out["k"] = p.k;
  out["val"] = p.val;
  return out;
}

inline py::dict combineLoadPositionToDict(const CombineLoadPosition &c) {
  py::dict out;
  out["max_position"] = c.max_position;
  out["position"] = c.position;
  out["value"] = c.value;
  out["addition"] = c.Addition;
  return out;
}

inline py::dict configurationToDict(const Configuration &c) {
  py::dict out;
  out["spans"] = c.spans;
  out["steps"] = c.steps;
  out["inertie"] = c.Inertie;
  out["young_module"] = c.YoungModule;
  py::list pt;
  for (const auto &l : c.Point_LOAD)
    pt.append(loadToDict(l));
  py::list dl;
  for (const auto &l : c.Rectangulare_LOAD)
    dl.append(loadToDict(l));
  out["point_loads"] = pt;
  out["distrib_loads"] = dl;
  return out;
}

inline py::dict projectPathsToDict(const ProjectPaths &p) {
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

// =============================================================================
//  Wrappers "propriétaires" — voir note en tête de fichier.
// =============================================================================
struct PyHyperstatique {
  PyHyperstatique(std::vector<double> E, std::vector<double> I,
                  std::vector<double> L, double steps)
      : E_(std::move(E)), I_(std::move(I)), L_(std::move(L)), steps_(steps),
        impl(E_, I_, L_, steps_) {}

  // Ordre de déclaration = ordre d'initialisation : E_/I_/L_/steps_
  // DOIVENT être construits avant impl.
  std::vector<double> E_, I_, L_;
  double steps_;
  Hyperstatique impl;
};

struct PyOutput {
  PyOutput(std::vector<double> E, std::vector<double> I, std::vector<double> L,
           double steps, std::filesystem::path root = "")
      : E_(std::move(E)), I_(std::move(I)), L_(std::move(L)), steps_(steps),
        impl(E_, I_, L_, steps_, std::move(root)) {}

  std::vector<double> E_, I_, L_;
  double steps_;
  Output impl;
};

// =============================================================================
PYBIND11_MODULE(_LIPoutreContinue, m) {
  m.doc() =
      "Tsaraloha.LIPoutreContinue._LIPoutreContinue — extension compilée "
      "(pybind11) du sous-module LIPoutreContinue de Tsaraloha, "
      "librairie de calcul de poutres continues / isostatiques.\n\n"
      "Détail d'implémentation privé : n'importez pas ce module "
      "directement, utilisez `import Tsaraloha.LIPoutreContinue` (voir "
      "python/Tsaraloha/LIPoutreContinue/__init__.py pour l'API publique, "
      "sur le modèle de numpy).\n\n"
      "Types non exposés (détails d'implémentation interne, sans intérêt "
      "côté Python) : les fonctions libres de parsing texte (LoadParser, "
      "parseVector, ...) — utilisez des dict/list Python à la place.";

  // ── Structures de données (Utils.h) ─────────────────────────────────────
  py::class_<load>(m, "Load", R"doc(
Décrit une charge mobile — ponctuelle (convoi d'essieux) ou répartie
(uniforme / plurielle) — à passer à Output.set_loads().

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
PARAMÈTRES DU CONSTRUCTEUR
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

intensity : list[float]
    • Charge ponctuelle  → force de chaque essieu [kN]
      len(intensity) = nombre d'essieux du convoi
    • Charge répartie   → intensité de chaque tronçon [kN/m]
      len(intensity) = nombre de tronçons

length : list[float]
    • Charge ponctuelle  → distances entre essieux consécutifs [m]
      len(length) == len(intensity)  (la dernière valeur n'est pas utilisée)
    • Charge répartie   → [PositionDepart, L_q1, L_q2, ..., L_qn] [m]
      len(length) == len(intensity) + 1
      PositionDepart = 0 si la charge commence au début de la travée

name : str  (optionnel, défaut = "")
    Étiquette libre, utilisée dans les exports JSON.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
CHARGES PONCTUELLES — FORMAT
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  ┌──────────────────────────────────────────────────────────────────────┐
  │ Essieu unique    : intensity=[P],        length=[xP]                 │
  │ 2 essieux (P,Q)  : intensity=[P, Q],     length=[xP, xQ]            │
  │ 3 essieux (P,Q,R): intensity=[P, Q, R],  length=[xP, xQ, xR]        │
  └──────────────────────────────────────────────────────────────────────┘
  xP = distance entre le début de travée et le 1er essieu
  xQ = distance entre le 1er et le 2ème essieu
  xR = distance entre le 2ème et le 3ème essieu
  (la dernière valeur de length n'est pas utilisée par le moteur)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
CHARGES RÉPARTIES — FORMAT
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  ┌──────────────────────────────────────────────────────────────────────┐
  │ Uniforme simple   : intensity=[q],           length=[depart, L_q]   │
  │ Uniforme pluriel  : intensity=[q1,q2,…,qn],  length=[depart, L_q1,  │
  │                                                        L_q2,…,L_qn] │
  └──────────────────────────────────────────────────────────────────────┘
  depart   = position du début de la charge depuis l'appui gauche [m]
             (= 0 si la charge commence dès le début de la travée)
  L_qi     = longueur du i-ème tronçon de charge [m]

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
EXEMPLES
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# ── Essieu unique 50 kN ─────────────────────────────────────────
Load(intensity=[50.0], length=[0.0], name="Essieu")

# ── Convoi 6 essieux (tandem + tandem) ──────────────────────────
Load(
    intensity=[6.0, 12.0, 12.0, 6.0, 12.0, 12.0],   # kN par essieu
    length   =[2.25, 4.5,  1.5, 5.0,  4.5,  1.5],   # distances [m]
    name="Convoi BC",
)

# ── Charge uniforme 12 kN/m sur 4 m, depuis le début ───────────
Load(intensity=[12.0], length=[0.0, 4.0], name="UDL")

# ── Charge plurielle : 45 kN/m sur 3 m, puis 10 sur 5 m, puis 25 sur 2 m
Load(intensity=[45.0, 10.0, 25.0], length=[0.0, 3.0, 5.0, 2.0], name="UDL2")
)doc")
      .def(py::init([](std::vector<double> intensity,
                       std::vector<double> length, std::string name) {
             load l;
             l.Intensity = std::move(intensity);
             l.Length = std::move(length);
             l.name = std::move(name);
             return l;
           }),
           py::arg("intensity"), py::arg("length"),
           py::arg("name") = std::string())
      .def_readwrite("intensity", &load::Intensity)
      .def_readwrite("length", &load::Length)
      .def_readwrite("name", &load::name)
      .def("to_dict", &loadToDict,
           "Convertit en dict Python { 'intensity', 'length', 'name' }.")
      .def("__repr__", [](const load &l) {
        return "<Load name=" + l.name +
               " n_points=" + std::to_string(l.Intensity.size()) + ">";
      });

  py::class_<Position1D>(m, "Position1D")
      .def(py::init<>())
      .def_readwrite("max_position", &Position1D::max_position)
      .def_readwrite("value", &Position1D::value)
      .def("to_dict", &position1DToDict,
           "Convertit en dict Python { 'max_position', 'value' }.")
      .def("__repr__", [](const Position1D &p) {
        return "<Position1D max_position=" + std::to_string(p.max_position) +
               " value=" + std::to_string(p.value) + ">";
      });

  py::class_<Position2D>(m, "Position2D")
      .def(py::init<>())
      .def_readwrite("i", &Position2D::i)
      .def_readwrite("j", &Position2D::j)
      .def_readwrite("val", &Position2D::val)
      .def("to_dict", &position2DToDict,
           "Convertit en dict Python { 'i', 'j', 'val' }.")
      .def("__repr__", [](const Position2D &p) {
        return "<Position2D i=" + std::to_string(p.i) +
               " j=" + std::to_string(p.j) + " val=" + std::to_string(p.val) +
               ">";
      });

  py::class_<Position3D>(m, "Position3D")
      .def(py::init<>())
      .def_readwrite("i", &Position3D::i)
      .def_readwrite("j", &Position3D::j)
      .def_readwrite("k", &Position3D::k)
      .def_readwrite("val", &Position3D::val)
      .def("to_dict", &position3DToDict,
           "Convertit en dict Python { 'i', 'j', 'k', 'val' }.")
      .def("__repr__", [](const Position3D &p) {
        return "<Position3D i=" + std::to_string(p.i) +
               " j=" + std::to_string(p.j) + " k=" + std::to_string(p.k) +
               " val=" + std::to_string(p.val) + ">";
      });

  py::class_<CombineLoadPosition>(m, "CombineLoadPosition")
      .def(py::init<>())
      .def_readwrite("max_position", &CombineLoadPosition::max_position)
      .def_readwrite("position", &CombineLoadPosition::position)
      .def_readwrite("value", &CombineLoadPosition::value)
      .def_readwrite("addition", &CombineLoadPosition::Addition)
      .def("to_dict", &combineLoadPositionToDict,
           "Convertit en dict Python { 'max_position', 'position', 'value', "
           "'addition' }.");

  py::class_<load_delivery>(
      m, "LoadDelivery",
      "Détail d'une enveloppe de charge : valeur maximale, travée/section "
      "concernée, et détail par charge (alpha/valeur/position).")
      .def(py::init<>())
      .def_readwrite("load", &load_delivery::load)
      .def_readwrite("span", &load_delivery::span)
      .def_readwrite("section", &load_delivery::section)
      .def_readwrite("maximum_value", &load_delivery::maximum_value)
      .def_readwrite("position", &load_delivery::position)
      .def("to_dict", &loadDeliveryToDict,
           "Convertit en dict Python standard (load, span, section, "
           "maximum_value, position).")
      .def("__repr__", [](const load_delivery &d) {
        return "<LoadDelivery span=" + std::to_string(d.span) +
               " section=" + std::to_string(d.section) +
               " maximum_value=" + std::to_string(d.maximum_value) +
               " position=" + std::to_string(d.position) +
               " n_loads=" + std::to_string(d.load.size()) + ">";
      });

  // ── Isostatique (poutre simple, pas de dépendance de durée de vie) ──────
  py::class_<Isostatique>(
      m, "Isostatique",
      "Poutre isostatique simple (une seule travée). Contrairement à "
      "Hyperstatique/Output, E/I/L/steps sont copiés par valeur : aucun "
      "piège de durée de vie ici.")
      .def(py::init<double, double, double, double>(), py::arg("E"),
           py::arg("I"), py::arg("L"), py::arg("steps"))
      .def_readonly("E", &Isostatique::E)
      .def_readonly("I", &Isostatique::I)
      .def_readonly("L", &Isostatique::L)
      .def_readonly("steps", &Isostatique::steps)
      .def_readonly("a", &Isostatique::a)
      .def_readonly("b", &Isostatique::b)
      .def_readonly("c", &Isostatique::c)
      .def_readonly("node_positions", &Isostatique::nodePositions)
      .def_readonly("omega_second", &Isostatique::Omega_Second)
      .def_readonly("omega_prime", &Isostatique::Omega_Prime)
      .def("eq_shear_force", &Isostatique::Eq_ShearForce, py::arg("x"),
           py::arg("return_abscissa"))
      .def("shear_force", &Isostatique::ShearForce)
      .def("shear_force_abscissa", &Isostatique::ShearForceAbscissa)
      .def("eq_bending_moment", &Isostatique::Eq_BendingMoment, py::arg("x"))
      .def("bending_moment", &Isostatique::BendingMoment)
      .def("eq_deflection", &Isostatique::Eq_Deflection, py::arg("x"))
      .def("deflection", &Isostatique::Deflection)
      .def("eq_rotation", &Isostatique::Eq_Rotation, py::arg("x"))
      .def("rotation", &Isostatique::Rotation);

  // ── Hyperstatique (poutre continue multi-travées) ───────────────────────
  py::class_<PyHyperstatique>(
      m, "Hyperstatique",
      "Poutre continue hyperstatique (plusieurs travées). E/I/L sont "
      "copiés à la construction (voir note de durée de vie en tête de "
      "bindings.cpp) — le comportement observable est identique à la "
      "classe C++ d'origine.")
      .def(py::init<std::vector<double>, std::vector<double>,
                    std::vector<double>, double>(),
           py::arg("E"), py::arg("I"), py::arg("L"), py::arg("steps"))
      .def_property_readonly(
          "number_of_spans",
          [](PyHyperstatique &s) { return s.impl.number_of_spans; })
      .def_property_readonly("steps",
                             [](PyHyperstatique &s) { return s.impl.steps; })
      .def_property_readonly("L_spans",
                             [](PyHyperstatique &s) { return s.impl.L_spans; })
      .def_property_readonly("E_spans",
                             [](PyHyperstatique &s) { return s.impl.E_spans; })
      .def_property_readonly("I_spans",
                             [](PyHyperstatique &s) { return s.impl.I_spans; })
      .def_property_readonly("a_spans",
                             [](PyHyperstatique &s) { return s.impl.a_spans; })
      .def_property_readonly("b_spans",
                             [](PyHyperstatique &s) { return s.impl.b_spans; })
      .def_property_readonly("c_spans",
                             [](PyHyperstatique &s) { return s.impl.c_spans; })
      .def_property_readonly("phy",
                             [](PyHyperstatique &s) { return s.impl.phy; })
      .def_property_readonly(
          "phy_prime", [](PyHyperstatique &s) { return s.impl.phy_prime; })
      .def_property_readonly(
          "support_moment",
          [](PyHyperstatique &s) { return s.impl.SupportMoment; })
      .def_property_readonly(
          "span_node_positions",
          [](PyHyperstatique &s) { return s.impl.SpanNodePositions; })
      .def_property_readonly(
          "total_nodes", [](PyHyperstatique &s) { return s.impl.total_nodes_; })
      .def("bending_moments",
           [](PyHyperstatique &s) { return s.impl.BendingMoments(); })
      .def("rotation", [](PyHyperstatique &s) { return s.impl.Rotation(); })
      .def(
          "shear_force",
          [](PyHyperstatique &s, bool getAll) {
            return s.impl.ShearForce(getAll);
          },
          py::arg("get_all_abscissa") = false)
      .def("deflection", [](PyHyperstatique &s) { return s.impl.Deflection(); })
      .def(
          "points_x_coordinates",
          [](PyHyperstatique &s,
             const std::vector<std::vector<double>> &positions) {
            return s.impl.pointsXCoordinates(positions);
          },
          py::arg("positions"))
      .def("__repr__", [](PyHyperstatique &s) {
        return "<Hyperstatique number_of_spans=" +
               std::to_string(s.impl.number_of_spans) + ">";
      });

  // ── Configuration (chargement « sans fichier » des données d'entrée) ───
  py::class_<Configuration>(
      m, "Configuration",
      "Regroupe spans/steps/E/I/charges. Pratique pour construire les "
      "arguments d'un Output/Hyperstatique en une fois.")
      .def(py::init<>())
      .def_readwrite("spans", &Configuration::spans)
      .def_readwrite("steps", &Configuration::steps)
      .def_readwrite("inertie", &Configuration::Inertie)
      .def_readwrite("young_module", &Configuration::YoungModule)
      .def_readwrite("point_loads", &Configuration::Point_LOAD)
      .def_readwrite("distrib_loads", &Configuration::Rectangulare_LOAD)
      .def("load_from_data", &Configuration::loadFromData, py::arg("spans"),
           py::arg("steps"), py::arg("young_module"), py::arg("inertie"),
           py::arg("point_loads"), py::arg("distrib_loads"))
      .def("to_dict", &configurationToDict,
           "Convertit en dict Python { 'spans', 'steps', 'inertie', "
           "'young_module', "
           "'point_loads', 'distrib_loads' } (point_loads/distrib_loads : "
           "liste de dicts).");

  // ── Loading (enveloppes de charge) ──────────────────────────────────────
  // Note : CriticalSectionResult n'est plus le type de retour de
  // compute_critical_section() côté Python (voir plus bas, qui renvoie un
  // dict directement) — la classe reste enregistrée pour un usage avancé.
  py::class_<Loading::CriticalSectionResult>(
      m, "CriticalSectionResult",
      "Résultat de Loading.compute_critical_section() : détail de charge "
      "(point / réparti / combiné) à la section critique d'une travée.")
      .def_readonly("point", &Loading::CriticalSectionResult::point)
      .def_readonly("rect", &Loading::CriticalSectionResult::rect)
      .def_readonly("combined", &Loading::CriticalSectionResult::combined)
      .def(
          "to_dict",
          [](const Loading::CriticalSectionResult &r) {
            py::dict out;
            out["point"] = loadDeliveryToDict(r.point);
            out["rect"] = loadDeliveryToDict(r.rect);
            out["combined"] = loadDeliveryToDict(r.combined);
            return out;
          },
          "Convertit en dict Python { 'point': {...}, 'rect': {...}, "
          "'combined': {...} }.");

  py::class_<Loading>(
      m, "Loading",
      "Calcul des enveloppes de charge (ponctuelle / répartie / combinée) "
      "à partir d'une courbe d'influence (CURVES) déjà calculée — par "
      "exemple Output.BM, Output.SF, etc.")
      .def(py::init<std::vector<std::vector<std::vector<double>>>,
                    std::vector<double>, std::vector<std::vector<double>>,
                    const std::vector<double> &, const std::vector<load> &,
                    const std::vector<load> &>(),
           py::arg("curves"), py::arg("position"),
           py::arg("span_node_positions"), py::arg("spans"),
           py::arg("point_loads"), py::arg("distrib_loads"))
      // Renvoyés en dict Python directement manipulable (voir
      // loadDeliveryToDict) plutôt qu'en objet LoadDelivery.
      .def_property_readonly("rectangular_load",
                             [](Loading &self) {
                               return loadDeliveryToDict(self.Rectangular_load);
                             })
      .def_property_readonly(
          "point_load",
          [](Loading &self) { return loadDeliveryToDict(self.Point_load); })
      .def_property_readonly(
          "combined_load",
          [](Loading &self) { return loadDeliveryToDict(self.Combined_load); })
      .def_readwrite("spans", &Loading::spans)
      .def_readwrite("point_load_inputs", &Loading::Point_LOAD)
      .def_readwrite("distrib_load_inputs", &Loading::Rectangulare_LOAD)
      .def("one_point_load", &Loading::OnePointLoad, py::arg("intensity"),
           py::arg("span"), py::arg("section"), py::arg("alpha"))
      .def("plural_point_load", &Loading::PluralPointLoad, py::arg("intensity"),
           py::arg("length"), py::arg("span"), py::arg("section"))
      .def("one_rectangular_load", &Loading::OneRectangularLoad,
           py::arg("intensity"), py::arg("span"), py::arg("section"),
           py::arg("begin"), py::arg("end"))
      .def("plural_rectangular_load", &Loading::PluralRectangularLoad,
           py::arg("intensity"), py::arg("length"), py::arg("span"),
           py::arg("section"))
      .def("combined_load_at", &Loading::CombinedLoad, py::arg("span"),
           py::arg("section"))
      .def(
          "compute_critical_section",
          [](Loading &self, size_t span) {
            auto result = self.computeCriticalSection(span);
            py::dict out;
            out["point"] = loadDeliveryToDict(result.point);
            out["rect"] = loadDeliveryToDict(result.rect);
            out["combined"] = loadDeliveryToDict(result.combined);
            return out;
          },
          py::arg("span"),
          "Calcule, pour la travée donnée, la section critique et le "
          "détail de charge (point/réparti/combiné). Reproduit ce que "
          "fait Output.export_load_envelopes() en interne. Renvoie un "
          "dict Python { 'point': {...}, 'rect': {...}, 'combined': {...} }.");

  // ── ProjectPaths ─────────────────────────────────────────────────────────
  py::class_<ProjectPaths>(
      m, "ProjectPaths",
      "Registre centralisé des chemins d'export (voir arborescence dans "
      "LIPoutreContinue/io/ProjectPaths.h).")
      .def(py::init<std::filesystem::path>(), py::arg("root"))
      .def_readonly("root", &ProjectPaths::root)
      .def_readonly("input", &ProjectPaths::input)
      .def_readonly("influence_lines", &ProjectPaths::influence_lines)
      .def_readonly("critical_values", &ProjectPaths::critical_values)
      .def_readonly("load_envelopes", &ProjectPaths::load_envelopes)
      .def_readonly("load_positioning", &ProjectPaths::load_positioning)
      .def_readonly("env_global", &ProjectPaths::env_global)
      .def_readonly("env_global_point", &ProjectPaths::env_global_point)
      .def_readonly("env_global_dist", &ProjectPaths::env_global_dist)
      .def_readonly("env_global_combined", &ProjectPaths::env_global_combined)
      .def_readonly("env_critical", &ProjectPaths::env_critical)
      .def_readonly("env_critical_point", &ProjectPaths::env_critical_point)
      .def_readonly("env_critical_dist", &ProjectPaths::env_critical_dist)
      .def_readonly("env_critical_combined",
                    &ProjectPaths::env_critical_combined)
      .def_readonly("pos_global", &ProjectPaths::pos_global)
      .def_readonly("pos_global_point", &ProjectPaths::pos_global_point)
      .def_readonly("pos_global_dist", &ProjectPaths::pos_global_dist)
      .def_readonly("pos_global_combined", &ProjectPaths::pos_global_combined)
      .def_readonly("pos_critical", &ProjectPaths::pos_critical)
      .def_readonly("pos_critical_point", &ProjectPaths::pos_critical_point)
      .def_readonly("pos_critical_dist", &ProjectPaths::pos_critical_dist)
      .def_readonly("pos_critical_combined",
                    &ProjectPaths::pos_critical_combined)
      .def("create_all", &ProjectPaths::createAll,
           "Crée sur disque tous les sous-dossiers d'export (appel explicite).")
      .def("to_dict", &projectPathsToDict,
           "Convertit en dict Python avec tous les chemins (root, input, "
           "influence_lines, …).");

  // ── UpdatePositions ──────────────────────────────────────────────────────
  py::class_<UpdatePositions>(
      m, "UpdatePositions",
      "Reconstruit les positions physiques des charges à partir des "
      "enveloppes déjà exportées en JSON (04_Load_Envelopes/).")
      .def(py::init<std::filesystem::path, const std::vector<std::string> &>(),
           py::arg("root"), py::arg("input_lines"))
      .def("compute", &UpdatePositions::compute, py::arg("force") = false)
      .def_property_readonly("is_computed", &UpdatePositions::isComputed)
      .def_property_readonly("results", &UpdatePositions::results)
      .def("write_all", &UpdatePositions::writeAll,
           "Écrit 05_Load_Positioning/ sur disque (appel explicite).")
      .def("run", &UpdatePositions::run,
           "Raccourci historique : compute() + write_all().");

  // ── Output::LoadEnvelope — enveloppe (point/rect/combined) récoltée
  //    par Output.compute_load_envelopes() ───────────────────────────────
  py::class_<Output::LoadEnvelope>(
      m, "LoadEnvelope",
      "Une enveloppe de charge récoltée par Output : les 3 résultats "
      "produits par Loading (ponctuelle / répartie / combinée).\n\n"
      "Note : côté Python, Output.*_envelope renvoie déjà un dict "
      "directement manipulable (voir Output) — cette classe reste "
      "disponible pour un usage avancé, mais n'est plus le type de "
      "retour par défaut.")
      .def_readonly("point_load", &Output::LoadEnvelope::pointLoad)
      .def_readonly("rectangular_load", &Output::LoadEnvelope::rectangularLoad)
      .def_readonly("combined_load", &Output::LoadEnvelope::combinedLoad)
      .def("to_dict", &loadEnvelopeToDict,
           "Convertit en dict Python standard (point_load, "
           "rectangular_load, combined_load), chacun lui-même un dict.")
      .def("__repr__", [](const Output::LoadEnvelope &e) {
        return "<LoadEnvelope point_load.maximum_value=" +
               std::to_string(e.pointLoad.maximum_value) +
               " rectangular_load.maximum_value=" +
               std::to_string(e.rectangularLoad.maximum_value) +
               " combined_load.maximum_value=" +
               std::to_string(e.combinedLoad.maximum_value) + ">";
      });

  // ── Output — point d'entrée principal de la librairie ───────────────────
  py::class_<PyOutput>(
      m, "Output",
      "Point d'entrée principal : calcule (en mémoire) et exporte (sur "
      "demande explicite) les résultats d'une poutre continue.\n\n"
      "Exemple :\n"
      "    out = Tsaraloha.LIPoutreContinue.Output(E=[210e9]*3, I=[1e-6]*3, "
      "L=[20,25,20], "
      "steps=1.0, root=\"/tmp/out\")\n"
      "    out.compute()                 # calcul en mémoire uniquement\n"
      "    out.BM, out.SF, out.X, ...    # résultats directement exploitables\n"
      "    out.set_loads(point_loads, distrib_loads)\n"
      "    out.export_all()              # écrit le JSON (optionnel)")
      .def(py::init<std::vector<double>, std::vector<double>,
                    std::vector<double>, double, std::filesystem::path>(),
           py::arg("E"), py::arg("I"), py::arg("L"), py::arg("steps"),
           py::arg("root") = std::filesystem::path(""))
      // ── Étape 1 : calcul en mémoire ──────────────────────────────────
      .def(
          "compute", [](PyOutput &s, bool force) { s.impl.compute(force); },
          py::arg("force") = false)
      .def_property_readonly("is_computed",
                             [](PyOutput &s) { return s.impl.isComputed(); })
      // ── Étape 2 : export JSON (explicite) ────────────────────────────
      .def("export_critical_values",
           [](PyOutput &s) { s.impl.exportCriticalValues(); })
      .def("export_influence_lines",
           [](PyOutput &s) { s.impl.exportInfluenceLines(); })
      .def("export_load_envelopes",
           [](PyOutput &s) { s.impl.exportLoadEnvelopes(); })
      .def("export_all", [](PyOutput &s) { s.impl.exportAll(); })
      .def(
          "set_loads",
          [](PyOutput &s, const std::vector<load> &p,
             const std::vector<load> &d) { s.impl.setLoads(p, d); },
          py::arg("point_loads"), py::arg("distrib_loads"),
          R"doc(
Enregistre les charges mobiles à appliquer sur la poutre pour le calcul
des enveloppes (compute_load_envelopes).

Doit être appelé AVANT compute_load_envelopes().

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
PARAMÈTRES
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

point_loads : list[Load]
    Liste de charges ponctuelles (ou convois multi-essieux).

    Chaque Load décrit UN convoi :
      • intensity : list[float]  — force de chaque essieu [kN]
      • length    : list[float]  — distance entre essieux consécutifs [m]
                                   (len(length) == len(intensity),
                                    la dernière valeur est ignorée)
      • name      : str          — étiquette libre (ex. "Camion A")

    Règles de format :
      ┌─────────────────────────────────────────────────────────────────────┐
      │ Essieu unique    : intensity=[P],      length=[xP]                  │
      │ 2 essieux (P,Q)  : intensity=[P, Q],   length=[xP, xQ]              │
      │ 3 essieux        : intensity=[P,Q,R],  length=[xP, xQ, xR]          │
      └─────────────────────────────────────────────────────────────────────┘
      xP = distance entre le debut de travee et le premier essieu
      xQ = distance entre le premier essieu et le deuxième essieu
      xR = distance entre le deuxième essieu et le troisième essieu

distrib_loads : list[Load]
    Liste de charges réparties (uniformes ou trapézoïdales mobiles).

    Chaque Load décrit UNE charge répartie :
      • intensity : list[float]  — intensité au début et à la fin [kN/m]
                                   (len == 2 ; égaux → charge uniforme)
      • length    : list[float]  — [offset_début, longueur_chargée] [m]
                                   (toujours 2 valeurs)
      • name      : str          — étiquette libre (ex. "UDL")

    Règles de format :
      ┌─────────────────────────────────────────────────────────────────────┐
      │ Uniforme  : intensity=[q],    length=[PositionDepart = 0, L_q]          │
      │ UniformePluriel   : intensity=[q1, q2, q3...qn],  length=[PositionDepart = 0, L_q1, L_q2, ..., L_qn]│
      └─────────────────────────────────────────────────────────────────────┘
      NB : Si le q commence de debut ==> PositionDepart = 0
      
            
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
EXEMPLES
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# ── Essieu unique de 50 kN ──────────────────────────────────────
camion = lipc.Load(intensity=[50.0], length=[0.0], name="Essieu")

# ── Convoi 6 essieux : 6 + 12 + 12 + 6 + 12 + 12 kN, espacés de 2.25 + 4.5 + 1.5 + 5 + 4.5 + 1.5 m ─────
convoi = lipc.Load(
    intensity=[6.0, 12.0, 12.0, 6.0, 12.0, 12.0],
    length   =[2.25, 4.5,  1.5, 5.0,  4.5, 1.5],   # dernière valeur ignorée
    name="Convoi BC",
)

# ── Charge uniforme 12 kN/m sur 4 m ─────────────────────────────
udl = lipc.Load(intensity=[12.0, 12.0], length=[0.0, 4.0], name="UDL")

# ── Charge UniformePluriel 45 10 25  kN/m sur 6 m ───────────────────────
udl2 = lipc.Load(intensity=[45.0, 10.0, 25.0], length=[0.0, 3.0, 5.0, 2.0], name="UDL2")

out.set_loads(
    point_loads  =[camion, convoi],
    distrib_loads=[udl, udl2],
)
out.compute_load_envelopes()
)doc")
      // ── Étape 1bis : enveloppes de charge, calcul en mémoire ─────────
      .def(
          "compute_load_envelopes",
          [](PyOutput &s, bool force) { s.impl.computeLoadEnvelopes(force); },
          py::arg("force") = false,
          "Calcule (Loading fait tout le travail) et récolte les "
          "enveloppes de charge dans BM_envelope, BM_critical_envelope, "
          "etc. Aucune écriture disque. Nécessite set_loads() au "
          "préalable.")
      .def_property_readonly(
          "is_load_envelopes_computed",
          [](PyOutput &s) { return s.impl.isLoadEnvelopesComputed(); })
      // ── Résultats disponibles en mémoire après compute() ─────────────
      .def_property_readonly("BM", [](PyOutput &s) { return s.impl.BM; })
      .def_property_readonly("SF", [](PyOutput &s) { return s.impl.SF; })
      .def_property_readonly("Def", [](PyOutput &s) { return s.impl.Def; })
      .def_property_readonly("Rot", [](PyOutput &s) { return s.impl.Rot; })
      .def_property_readonly(
          "shear_force_all_abscissa",
          [](PyOutput &s) { return s.impl.ShearForceAllAbscissa; })
      .def_property_readonly("X", [](PyOutput &s) { return s.impl.X; })
      .def_property_readonly("node_lengths",
                             [](PyOutput &s) { return s.impl.NodeLengths; })
      .def_property_readonly("bending_moment_max_positions",
                             [](PyOutput &s) {
                               return position3DToDict(
                                   s.impl.BendingMomentMaxPositions);
                             })
      .def_property_readonly("deflection_max_positions",
                             [](PyOutput &s) {
                               return position3DToDict(
                                   s.impl.DeflectionMaxPositions);
                             })
      .def_property_readonly("rotation_max_positions",
                             [](PyOutput &s) {
                               return position3DToDict(
                                   s.impl.RotationMaxPositions);
                             })
      .def_property_readonly("shear_force_max_positions",
                             [](PyOutput &s) {
                               return position3DToDict(
                                   s.impl.ShearForceMaxPositions);
                             })
      .def_property_readonly("support_moment_max_positions",
                             [](PyOutput &s) {
                               return position2DToDict(
                                   s.impl.SupportMomentMaxPositions);
                             })
      // ── Enveloppes de charge — disponibles après compute_load_envelopes()
      //    (ou export_load_envelopes(), qui l'appelle pour vous) ────────
      //    Renvoyées directement en dict Python (indexable, sérialisable
      //    en JSON, sans conversion supplémentaire à faire côté appelant).
      // Générales (toute la poutre) :
      .def_property_readonly("bending_moment_general_envelope",
                             [](PyOutput &s) {
                               return loadEnvelopeToDict(
                                   s.impl.BendingMomentGeneralLoadEnvelope);
                             })
      .def_property_readonly("shear_force_general_envelope",
                             [](PyOutput &s) {
                               return loadEnvelopeToDict(
                                   s.impl.ShearForceGeneralLoadEnvelope);
                             })
      .def_property_readonly("deflection_general_envelope",
                             [](PyOutput &s) {
                               return loadEnvelopeToDict(
                                   s.impl.DeflectionGeneralLoadEnvelope);
                             })
      .def_property_readonly("rotation_general_envelope",
                             [](PyOutput &s) {
                               return loadEnvelopeToDict(
                                   s.impl.RotationGeneralLoadEnvelope);
                             })
      // Critiques (section critique uniquement) :
      .def_property_readonly("bending_moment_critical_envelope",
                             [](PyOutput &s) {
                               return loadEnvelopeToDict(
                                   s.impl.BendingMomentCriticalLoadEnvelope);
                             })
      .def_property_readonly("shear_force_critical_envelope",
                             [](PyOutput &s) {
                               return loadEnvelopeToDict(
                                   s.impl.ShearForceCriticalLoadEnvelope);
                             })
      .def_property_readonly("deflection_critical_envelope",
                             [](PyOutput &s) {
                               return loadEnvelopeToDict(
                                   s.impl.DeflectionCriticalLoadEnvelope);
                             })
      .def_property_readonly("rotation_critical_envelope",
                             [](PyOutput &s) {
                               return loadEnvelopeToDict(
                                   s.impl.RotationCriticalLoadEnvelope);
                             })
      .def_property_readonly(
          "paths", [](PyOutput &s) { return projectPathsToDict(s.impl.Paths); })
      // ── Champs hérités de Hyperstatique ──────────────────────────────
      .def_property_readonly("number_of_spans",
                             [](PyOutput &s) { return s.impl.number_of_spans; })
      .def_property_readonly("steps", [](PyOutput &s) { return s.impl.steps; })
      .def_property_readonly("L_spans",
                             [](PyOutput &s) { return s.impl.L_spans; })
      .def_property_readonly("E_spans",
                             [](PyOutput &s) { return s.impl.E_spans; })
      .def_property_readonly("I_spans",
                             [](PyOutput &s) { return s.impl.I_spans; })
      .def_property_readonly("support_moment",
                             [](PyOutput &s) { return s.impl.SupportMoment; })
      .def_property_readonly(
          "span_node_positions",
          [](PyOutput &s) { return s.impl.SpanNodePositions; })
      .def("__repr__", [](PyOutput &s) {
        return "<Output number_of_spans=" +
               std::to_string(s.impl.number_of_spans) + " computed=" +
               (s.impl.isComputed() ? std::string("True")
                                    : std::string("False")) +
               ">";
      });
}
