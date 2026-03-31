# beam_analysis — Structure complète des fichiers

## Arborescence finale

```
beam_analysis/src/
├── Application.cpp                   ← ex Aplication.cpp  (faute corrigée)
├── BeamUtils.h                       ← ex Utils.h
│
├── isostatic/
│   ├── IsostaticBeam.h               ← ex Isostatique.h
│   └── IsostaticBeam.cpp             ← ex Isostatique.cpp
│
├── continuous/
│   ├── ContinuousBeam.h              ← ex Hyperstatique.h
│   ├── ContinuousBeam.cpp            ← ex Hyperstatique.cpp
│   ├── BeamFormulas.h                ← ex fonctions static dans Hyperstatique.cpp
│   └── SpanResult.h                  ← ex SpanResult.h (champs renommés)
│
├── config/
│   ├── StructuralConfig.h            ← ex Input.h
│   └── StructuralConfig.cpp          ← ex Input.cpp
│
├── loading/
│   ├── LoadEnvelope.h                ← ex Loading.h
│   └── LoadEnvelope.cpp              ← ex Loading.cpp
│
└── io/
    ├── ResultsExporter.h             ← ex Output.h
    ├── ResultsExporter.cpp           ← ex Output.cpp
    ├── LoadPositioner.h              ← ex UpdatePositions.h
    ├── LoadPositioner.cpp            ← ex UpdatePositions.cpp
    ├── ProjectPaths.h                ← ex ProjectPaths.h (déplacé dans io/)
    └── JsonStreamWriter.h            ← ex JsonStreamWriter.h (déplacé dans io/)
```

---

## Tables de correspondance

### Fichiers

| Ancien fichier           | Nouveau fichier                     | Raison                                       |
|--------------------------|-------------------------------------|----------------------------------------------|
| `Aplication.cpp`         | `Application.cpp`                   | Faute de frappe corrigée                     |
| `Utils.h`                | `BeamUtils.h`                       | Évite collision avec des Utils.h tiers       |
| `Input.h/.cpp`           | `config/StructuralConfig.h/.cpp`    | Décrit le rôle réel (config du modèle)       |
| `Loading.h/.cpp`         | `loading/LoadEnvelope.h/.cpp`       | Évite ambiguïté avec "chargement de fichier" |
| `Output.h/.cpp`          | `io/ResultsExporter.h/.cpp`         | Décrit le pipeline d'export des résultats    |
| `UpdatePositions.h/.cpp` | `io/LoadPositioner.h/.cpp`          | Nom → rôle métier (positionner les charges)  |
| `ProjectPaths.h`         | `io/ProjectPaths.h`                 | Déplacé dans le bon sous-dossier             |
| `JsonStreamWriter.h`     | `io/JsonStreamWriter.h`             | Déplacé dans le bon sous-dossier             |

### `BeamUtils.h` (ex `Utils.h`) — structures et fonctions globales

| Ancien identifiant               | Nouvel identifiant          |
|----------------------------------|-----------------------------|
| `struct load`                    | `struct LoadCase`           |
| `load::Intensity`                | `LoadCase::intensities`     |
| `load::Length`                   | `LoadCase::positions`       |
| `struct CombineLoadPosition`     | `struct CombinedLoadResult` |
| `CombineLoadPosition::max_position` | `CombinedLoadResult::maxIndex` |
| `CombineLoadPosition::Addition`  | `CombinedLoadResult::breakdown` |
| `struct load_delivery`           | `struct LoadEnvelopeResult` |
| `load_delivery::maximum_value`   | `LoadEnvelopeResult::maxValue` |
| `Position1D::max_position`       | `Position1D::maxIndex`      |
| `MaxValueInVector()`             | `maxAbsInVector()`          |
| `Indice_of()`                    | `indexOf()`                 |
| `checkFileExists()`              | `fileExists()`              |
| `getConfigPath()`                | `getDefaultConfigPath()`    |
| `parseVector()`                  | `parseDoubleVector()`       |
| `getValue()`                     | `parseScalarValue()`        |
| `LoadParser()`                   | `parseLoadLine()`           |
| `write_structural_model_input()` | `writeDefaultConfigFile()`  |
| `delivery()`                     | `writeJsonFile()`           |
| `maximum_delivery()`             | `writeCriticalValueJson()`  |
| `loading_delivery()`             | `writeEnvelopeJson()`       |

### `StructuralConfig` (ex `Configuration`)

| Ancien identifiant  | Nouvel identifiant  |
|---------------------|---------------------|
| `spans`             | `spanLengths`       |
| `steps`             | `stepSize`          |
| `Inertie`           | `inertiaMoments`    |
| `YoungModule`       | `elasticModuli`     |
| `Point_LOAD`        | `pointLoads`        |
| `Rectangulare_LOAD` | `distributedLoads`  |

### `LoadEnvelope` (ex `Loading`)

| Ancien identifiant          | Nouvel identifiant          |
|-----------------------------|-----------------------------|
| `CURVES`                    | `influenceCurves_`          |
| `POSITION`                  | `globalAbscissae_`          |
| `SpanNodePositions`         | `spanNodes_`                |
| `Rectangular_load`          | `distributedLoadResult`     |
| `Point_load`                | `pointLoadResult`           |
| `Combined_load`             | `combinedLoadResult`        |
| `MetersToPosition()`        | `metersToIndex()`           |
| `OnePointLoad()`            | `onePointLoad()`            |
| `PluralPointLoad()`         | `pluralPointLoad()`         |
| `OneRectangularLoad()`      | `oneRectangularLoad()`      |
| `PluralRectangularLoad()`   | `pluralRectangularLoad()`   |
| `CombinedLoad()`            | `combinedLoad()`            |

### `ResultsExporter` (ex `Output`)

| Ancien identifiant           | Nouvel identifiant        |
|------------------------------|---------------------------|
| `BendingMomentMaxPositions`  | `bendingMomentCritical`   |
| `DeflectionMaxPositions`     | `deflectionCritical`      |
| `RotationMaxPositions`       | `rotationCritical`        |
| `ShearForceMaxPositions`     | `shearForceCritical`      |
| `SupportMomentMaxPositions`  | `supportMomentCritical`   |

### `LoadPositioner` (ex `UpdatePositions`)

| Ancien identifiant       | Nouvel identifiant          |
|--------------------------|-----------------------------|
| `lines_`                 | `allLines_`                 |
| `point_texte_`           | `pointOnlyLines_`           |
| `distributed_texte_`     | `distOnlyLines_`            |
| `openJson()`             | `readEnvelopeJson()`        |
| `writeTxt()`             | `writePositioningFile()`    |
| `extractPositions()`     | `extractOffsets()`          |

### `ContinuousBeam` (ex `Hyperstatique`) — rappel du lot précédent

| Ancien identifiant                  | Nouvel identifiant                       |
|-------------------------------------|------------------------------------------|
| `L_spans`                           | `spanLengths`                            |
| `a_spans / b_spans / c_spans`       | `flexCoeff_a / flexCoeff_b / flexCoeff_c`|
| `phy / phy_prime`                   | `transferCoeff / transferCoeff_prime`    |
| `SupportMoment`                     | `supportMoments`                         |
| `SpanNodePositions`                 | `spanNodePositions`                      |
| `number_of_spans`                   | `numberOfSpans`                          |
| `E_spans / I_spans`                 | `elasticModuli / inertiaMoments`         |
| `pointsXCoordinates()`              | `globalXCoordinates()`                   |
| `HypPartBendingMoment()`            | `BeamFormula_BendingMoment()`            |
| `HypPartRotation()`                 | `BeamFormula_Rotation()`                 |
| `HypPartShearForce()`               | `BeamFormula_ShearForce()`               |
| `HypPartDeflection()`               | `BeamFormula_Deflection()`               |
