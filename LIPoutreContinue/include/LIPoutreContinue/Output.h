#pragma once
#ifndef __OUTPUT__
#define __OUTPUT__

// =============================================================================
//  Output — Couche "librairie" au-dessus de Hyperstatique
// =============================================================================
//
//  MODE LIBRAIRIE — RIEN N'EST AUTOMATIQUE :
//
//    Output out(E, I, L, steps, root);   // ne calcule rien, n'écrit rien
//    out.compute();                      // calcule tout EN MÉMOIRE seulement
//    // ... un autre programme peut lire out.BM, out.SF, out.Def, out.Rot,
//    //     out.X, les *MaxPositions, etc. directement, sans jamais toucher
//    //     au disque.
//
//    // Écriture JSON UNIQUEMENT si demandée explicitement :
//    out.exportAll();                    // écrit tout (equivalent des 3 lignes ci-dessous)
//    // ou, étape par étape :
//    out.exportCriticalValues();         // 03_Critical_Values/
//    out.exportInfluenceLines();         // 01_Input/ + 02_Influence_Lines/
//    out.exportLoadEnvelopes();          // 04_Load_Envelopes/
//
//  Aucune de ces méthodes n'est appelée par le constructeur ni entre elles :
//  chaque étape est déclenchée uniquement par le code appelant.
// =============================================================================

#include "LIPoutreContinue/core/Hyperstatique.h"
#include "LIPoutreContinue/loading/Loading.h"
#include "LIPoutreContinue/io/ProjectPaths.h"
#include "LIPoutreContinue/Utils.h"

#include <vector>
#include <filesystem>

class Output : public Hyperstatique
{
public:
    Output(std::vector<double>& E,
           std::vector<double>& I,
           std::vector<double>& L,
           double& steps,
           std::filesystem::path root = "");

    // ── Étape 1 : calcul en mémoire, aucune écriture disque ────────────────
    // Obligatoire avant d'utiliser les résultats ci-dessous ou d'exporter.
    // Idempotente : un second appel sans force=true ne relance rien.
    void compute(bool force = false);

    bool isComputed() const { return computed_; }

    // ── Étape 2 : export JSON — chaque méthode appelle compute() si besoin,
    //    mais aucune n'est appelée automatiquement pour vous. ────────────────
    void exportCriticalValues();                      // 03_Critical_Values/
    void exportInfluenceLines();                       // 01_Input/ + 02_Influence_Lines/

    // Nécessite que setLoads() ait été appelé au préalable : les charges
    // sont toujours fournies par l'appelant, jamais lues d'un fichier.
    void exportLoadEnvelopes();                                    // 04_Load_Envelopes/
    void exportAll();                                              // les 3 ci-dessus

    // ── Mode « sans fichier » pour les charges ─────────────────────────────
    // Obligatoire avant exportLoadEnvelopes()/exportAll() : fournit les
    // charges directement, aucune lecture de path.json ni de
    // structural model input.txt.
    void setLoads(const std::vector<load>& pointLoads,
                  const std::vector<load>& distribLoads);

    // ── Étape 1bis : enveloppes de charge, calcul EN MÉMOIRE uniquement ────
    // Tout le calcul est fait par Loading (Loading.h/.cpp) ; Output se
    // contente de récolter les résultats dans les membres publics ci-dessous.
    // Idempotente comme compute(). Nécessite setLoads() au préalable.
    // exportLoadEnvelopes() l'appelle pour vous avant d'écrire sur disque —
    // vous pouvez aussi l'appeler seule si vous ne voulez jamais toucher
    // au disque.
    void computeLoadEnvelopes(bool force = false);

    bool isLoadEnvelopesComputed() const { return loadEnvelopesComputed_; }

    // Une enveloppe de charge : les 3 résultats produits par Loading
    // (ponctuelle / répartie / combinée), récoltés tels quels.
    struct LoadEnvelope {
        load_delivery pointLoad;
        load_delivery rectangularLoad;
        load_delivery combinedLoad;
    };

    // ── Résultats disponibles en mémoire après compute() ───────────────────
    std::vector<std::vector<std::vector<double>>> BM, SF, Def, Rot;
    std::vector<std::vector<std::vector<double>>> ShearForceAllAbscissa; // X_T
    std::vector<double> X;                // abscisses de tous les nœuds
    std::vector<double> NodeLengths;      // abscisses cumulées des appuis

    Position3D BendingMomentMaxPositions {0, 0, 0, 0.0};
    Position3D DeflectionMaxPositions    {0, 0, 0, 0.0};
    Position3D RotationMaxPositions      {0, 0, 0, 0.0};
    Position3D ShearForceMaxPositions    {0, 0, 0, 0.0};
    Position2D SupportMomentMaxPositions {0, 0, 0.0};

    // ── Enveloppes de charge, disponibles en mémoire après
    //    computeLoadEnvelopes() (ou exportLoadEnvelopes()) ───────────────────
    // Enveloppes GÉNÉRALES : recherche sur toute la poutre, une par type de
    // résultat structurel (moment fléchissant / effort tranchant /
    // déformée / rotation).
    LoadEnvelope BendingMomentGeneralLoadEnvelope;
    LoadEnvelope ShearForceGeneralLoadEnvelope;
    LoadEnvelope DeflectionGeneralLoadEnvelope;
    LoadEnvelope RotationGeneralLoadEnvelope;

    // Enveloppes CRITIQUES : uniquement à la section critique (là où le
    // maximum global de compute() a été trouvé), une par type de résultat
    // structurel.
    LoadEnvelope BendingMomentCriticalLoadEnvelope;
    LoadEnvelope ShearForceCriticalLoadEnvelope;
    LoadEnvelope DeflectionCriticalLoadEnvelope;
    LoadEnvelope RotationCriticalLoadEnvelope;

    // Chemins résolus — exposés pour que du code externe (une autre appli)
    // sache où lire/écrire sans redéfinir sa propre logique de chemins.
    ProjectPaths Paths;

private:
    bool computed_ = false;
    bool loadEnvelopesComputed_ = false;
    bool hasUserLoads_ = false;
    std::vector<load> userPointLoads_;
    std::vector<load> userDistribLoads_;
};

#endif // __OUTPUT__
