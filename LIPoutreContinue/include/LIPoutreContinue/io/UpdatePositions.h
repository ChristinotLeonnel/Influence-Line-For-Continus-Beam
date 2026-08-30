#pragma once
#ifndef __UPDATE_POSITIONS__
#define __UPDATE_POSITIONS__

// =============================================================================
//  UpdatePositions.h
//  Conversion C++ de update_positions_from_bm.py — chemins via ProjectPaths
// =============================================================================
//
//  SOURCES JSON  (lues depuis 04_Load_Envelopes/) :
//    Global/Point_Load/<curve>.json
//    Global/Distributed_Load/<curve>.json
//    Global/Combined_Load/<curve>.json
//    Critical_Section/Point_Load/<curve>.json
//    Critical_Section/Distributed_Load/<curve>.json
//    Critical_Section/Combined_Load/<curve>.json
//
//  SORTIES TXT  (écrites dans 05_Load_Positioning/) :
//    Global/Point_Load/<curve>.txt
//    Global/Distributed_Load/<curve>.txt
//    Global/Combined_Load/<curve>.txt
//    Critical_Section/Point_Load/<curve>.txt
//    Critical_Section/Distributed_Load/<curve>.txt
//    Critical_Section/Combined_Load/<curve>.txt
//
//  Les chemins sont résolus par ProjectPaths — aucun chemin en dur ici.
//
//  NOTE (librairie) : cette classe ne lit jamais « structural model input.txt »
//  — les lignes du modèle sont fournies par l'appelant au constructeur.
//
//  MODE LIBRAIRIE — l'appelant décide s'il génère les fichiers .txt ou non :
//
//    UpdatePositions up(root, inputLines);
//    up.compute();                 // calcule tout EN MÉMOIRE, n'écrit rien
//    const auto& r = up.results(); // accès direct aux données calculées
//    // ... et seulement si on le souhaite :
//    up.writeAll();                // écrit 05_Load_Positioning/ (appel explicite)
//    // ou, en une fois (comportement historique) :
//    up.run();                     // équivalent à compute() + writeAll()
// =============================================================================

#include "LIPoutreContinue/io/ProjectPaths.h"
#include <string>
#include <vector>
#include <map>

class UpdatePositions
{
public:
    // Constructeur « sans fichier » : les lignes du modèle sont fournies
    // directement par l'appelant — aucune lecture de structural model input.txt.
    UpdatePositions(std::filesystem::path root, const std::vector<std::string>& inputLines);

    // results_[scope]["Point_Load"|"Distributed_Load"|"Combined_Load"][curve] = lines
    //   scope = "Global" | "Critical_Section"
    using ResultMap = std::map<std::string, std::map<std::string, std::map<std::string, std::vector<std::string>>>>;

    // ── Étape 1 : calcul en mémoire, aucune écriture disque ────────────────
    // Idempotent : un second appel sans force=true ne relance rien.
    void compute(bool force = false);

    bool isComputed() const { return computed_; }

    // ── Résultats disponibles en mémoire après compute() ────────────────────
    // Ne déclenche jamais compute() automatiquement : appeler compute() d'abord.
    const ResultMap& results() const { return results_; }

    // ── Étape 2 : écriture disque de 05_Load_Positioning/ — appel explicite.
    // Appelle compute() si pas encore fait (no-op sinon).
    void writeAll();

    // Raccourci historique : compute() + writeAll(). Toujours appelé
    // explicitement par le code appelant, jamais automatiquement.
    void run();

private:
    ProjectPaths P_;

    std::vector<std::string> lines_;              // lignes du modèle fournies par l'appelant
    std::vector<std::string> point_texte_;        // lines_ sans /Distributed/
    std::vector<std::string> distributed_texte_;  // lines_ sans /Point/

    bool computed_ = false;
    ResultMap results_;

    static const std::vector<std::string> CURVE_NAMES;

    void computeScope      (bool critical);
    void computePoint      (const std::string& curve, bool critical);
    void computeDistributed(const std::string& curve, bool critical);
    void computeCombined   (const std::string& curve, bool critical);

    json openJson(const std::string& loadType,
                  const std::string& curve,
                  bool critical) const;

    void writeTxt(const std::string& loadType,
                  const std::string& curve,
                  const std::vector<std::string>& data,
                  bool critical) const;

    static bool isComment         (const std::string& line);
    static std::string extractLoadName  (const std::string& line);
    static std::vector<double> extractPositions(const std::string& line,
                                                const std::string& marker);
    static std::string rebuildLine(const std::string& line,
                                   const std::string& markerBare,
                                   const std::vector<double>& positions);
};

#endif // __UPDATE_POSITIONS__
