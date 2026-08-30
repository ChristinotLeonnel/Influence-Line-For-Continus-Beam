#pragma once
// =============================================================================
//  StructuralAnalysis.h — Point d'entrée unique de la librairie
// =============================================================================
//  Un seul #include pour un programme externe qui veut consommer la
//  librairie (calcul en mémoire et/ou export JSON à la demande).
//
//      #include <LIPoutreContinue/StructuralAnalysis.h>
//
//      std::vector<double> E{...}, I{...}, L{...};
//      double steps = 1;
//      Output out(E, I, L, steps, "/chemin/de/sortie");
//      out.compute();                 // résultats en RAM, rien sur disque
//      // ... utiliser out.BM, out.SF, out.Def, out.Rot, out.X, etc.
//      out.exportAll();                // optionnel : écrit le JSON
// =============================================================================

#include "LIPoutreContinue/Utils.h"
#include "LIPoutreContinue/core/Isostatique.h"
#include "LIPoutreContinue/core/Hyperstatique.h"
#include "LIPoutreContinue/loading/Input.h"
#include "LIPoutreContinue/loading/Loading.h"
#include "LIPoutreContinue/io/ProjectPaths.h"
#include "LIPoutreContinue/io/UpdatePositions.h"
#include "LIPoutreContinue/Output.h"
