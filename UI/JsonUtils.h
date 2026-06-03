#pragma once
/**
 * JsonUtils.h
 * Utilitaires header-only pour le chargement des donnees JSON du projet.
 *
 * Arborescence attendue (sortie de Influence_Line_For_Continus_Beam) :
 *   <root>/
 *     01_Input/structural_model.json
 *     02_Influence_Lines/{bending_moment,shear_force,deflection,rotation,
 *                         support_moment,abscissa,shear_abscissa,node_lengths}.json
 *     03_Critical_Values/{bending_moment,...}.json
 *     04_Load_Envelopes/Global|Critical_Section/{Point_Load,...}/{curve}.json
 *     06_Plots/All|Maximum|Envelopes/{Point,Distributed,Combined}_Load/*.png
 *     07_Animations/Results|Curvature/GIF|MP4/*.gif|*.mp4
 */

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

// nlohmann/json — header-only, doit etre dans third_party/nlohmann/json.hpp
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json   = nlohmann::json;

// =============================================================================
//  Types alias
// =============================================================================
namespace jutils {

using Vec1D = std::vector<double>;
using Vec2D = std::vector<Vec1D>;
using Vec3D = std::vector<Vec2D>;

// =============================================================================
//  Lecture basique
// =============================================================================
inline json load(const fs::path& p)
{
    std::ifstream f(p);
    if (!f.is_open())
        throw std::runtime_error("Cannot open: " + p.string());
    json j;
    f >> j;
    return j;
}

inline json load_safe(const fs::path& p)
{
    try { return load(p); }
    catch (...) { return nullptr; }
}

// =============================================================================
//  Detection de la racine depuis un path.json
// =============================================================================
inline fs::path detect_root(const fs::path& dir)
{
    const auto p = dir / "path.json";
    if (!fs::exists(p)) return {};
    try {
        auto j = load(p);
        if (j.contains("configPath"))
            return fs::path(j["configPath"].get<std::string>());
    }
    catch (...) {}
    return {};
}

// =============================================================================
//  Noms des courbes (index -> nom de fichier / label)
// =============================================================================
inline const std::array<const char*, 4>& curve_files() {
    static const std::array<const char*, 4> arr = {
        "bending_moment.json", "shear_force.json",
        "deflection.json",     "rotation.json"
    };
    return arr;
}
inline const std::array<const char*, 4>& curve_labels() {
    static const std::array<const char*, 4> arr = {
        "Bending Moment", "Shear Force", "Deflection", "Rotation"
    };
    return arr;
}
inline const std::array<const char*, 3>& load_type_dirs() {
    static const std::array<const char*, 3> arr = {
        "Point_Load", "Distributed_Load", "Combined_Load"
    };
    return arr;
}
inline const std::array<const char*, 3>& load_type_labels() {
    static const std::array<const char*, 3> arr = {
        "Point Load", "Distributed Load", "Combined Load"
    };
    return arr;
}

// =============================================================================
//  Chemins standards
// =============================================================================
struct Paths {
    fs::path root;

    Paths() = default;
    explicit Paths(fs::path r) : root(std::move(r)) {}

    fs::path structural_model()  const { return root/"01_Input"/"structural_model.json"; }
    fs::path influence_lines()   const { return root/"02_Influence_Lines"; }
    fs::path critical_values()   const { return root/"03_Critical_Values"; }
    fs::path load_envelopes()    const { return root/"04_Load_Envelopes"; }
    fs::path plots()             const { return root/"06_Plots"; }
    fs::path animations()        const { return root/"07_Animations"; }

    fs::path il_file(const std::string& f) const { return influence_lines()/f; }
    fs::path cv_file(const std::string& f) const { return critical_values()/f; }

    fs::path env_file(const std::string& scope,
                      const std::string& load_type,
                      const std::string& curve) const {
        return load_envelopes()/scope/load_type/curve;
    }

    bool valid() const { return !root.empty() && fs::exists(root); }
};

// =============================================================================
//  Structures de resultat
// =============================================================================
struct CriticalValue {
    int    span    = 0;
    int    section = 0;
    int    alpha   = 0;
    double value   = 0.0;
    std::string curve;

    static CriticalValue from_json(const json& j, const std::string& name) {
        CriticalValue c;
        c.curve   = name;
        c.span    = j.value("span",    0);
        c.section = j.value("section", 0);
        c.alpha   = j.value("alpha",   0);
        c.value   = j.value("value",   0.0);
        return c;
    }
};

struct LoadEntry {
    double alpha    = 0.0;
    double value    = 0.0;
    double position = 0.0;
};

struct EnvelopeResult {
    std::string curve;
    std::string load_type;
    std::string scope;          // "Global" or "Critical_Section"
    double      maximum  = 0.0;
    int         span     = 0;
    int         section  = 0;
    double      position = 0.0;
    // load: { name -> { alpha, value, Position } }
    std::map<std::string, LoadEntry> loads;

    static EnvelopeResult from_json(const json& j,
                                    const std::string& curve,
                                    const std::string& lt,
                                    const std::string& scope)
    {
        EnvelopeResult r;
        r.curve    = curve;
        r.load_type= lt;
        r.scope    = scope;
        r.maximum  = j.value("maximum",  0.0);
        r.span     = j.value("span",     0);
        r.section  = j.value("section",  0);
        r.position = j.value("position", 0.0);
        if (j.contains("load") && j["load"].is_object()) {
            for (auto& [name, info] : j["load"].items()) {
                LoadEntry le;
                le.alpha    = info.value("alpha",    0.0);
                le.value    = info.value("value",    0.0);
                le.position = info.value("Position", 0.0);
                r.loads[name] = le;
            }
        }
        return r;
    }
};

struct StructuralModel {
    Vec1D       spans;
    Vec1D       young_modulus;
    Vec1D       inertia;
    double      step         = 1.0;
    Vec1D       node_lengths;
    int         n_spans      = 0;
    int         n_total_nodes= 0;

    static StructuralModel from_json(const json& j) {
        StructuralModel m;
        if (j.contains("spans"))        m.spans         = j["spans"].get<Vec1D>();
        if (j.contains("young_modulus"))m.young_modulus  = j["young_modulus"].get<Vec1D>();
        if (j.contains("inertia"))      m.inertia        = j["inertia"].get<Vec1D>();
        if (j.contains("step"))         m.step           = j["step"].get<double>();
        if (j.contains("node_lengths")) m.node_lengths   = j["node_lengths"].get<Vec1D>();
        if (j.contains("n_spans"))      m.n_spans        = j["n_spans"].get<int>();
        if (j.contains("n_total_nodes"))m.n_total_nodes  = j["n_total_nodes"].get<int>();
        return m;
    }
};

} // namespace jutils
