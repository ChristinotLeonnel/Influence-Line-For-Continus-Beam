# beam_analysis — Structure des fichiers

## Arborescence

```
beam_analysis/
├── src/
│   ├── isostatic/
│   │   ├── IsostaticBeam.h          # ex Isostatique.h
│   │   └── IsostaticBeam.cpp        # ex Isostatique.cpp
│   └── continuous/
│       ├── ContinuousBeam.h         # ex Hyperstatique.h
│       ├── ContinuousBeam.cpp       # ex Hyperstatique.cpp
│       ├── BeamFormulas.h           # ex fonctions static dans Hyperstatique.cpp
│       └── SpanResult.h             # ex SpanResult.h (inchangé dans son rôle)
```

---

## Table de correspondance — fichiers

| Ancien nom           | Nouveau nom              | Raison                                      |
|----------------------|--------------------------|---------------------------------------------|
| `Isostatique.h/.cpp` | `IsostaticBeam.h/.cpp`   | Nom anglais cohérent avec le reste du code  |
| `Hyperstatique.h`    | `ContinuousBeam.h`       | Décrit l'objet métier (poutre continue)     |
| `Hyperstatique.cpp`  | `ContinuousBeam.cpp`     | Idem                                        |
| *(dans le .cpp)*     | `BeamFormulas.h`         | Formules extraites → header réutilisable    |
| `SpanResult.h`       | `SpanResult.h` (déplacé) | Noms des champs mis en camelCase anglais    |

---

## Table de correspondance — identifiants

### Classe `IsostaticBeam` (ex `Isostatique`)
Classe inchangée fonctionnellement — seul le nom de la classe et du fichier change.

### Classe `ContinuousBeam` (ex `Hyperstatique`)

| Ancien identifiant                    | Nouvel identifiant                      |
|---------------------------------------|-----------------------------------------|
| `L_spans`                             | `spanLengths`                           |
| `a_spans / b_spans / c_spans`         | `flexCoeff_a / flexCoeff_b / flexCoeff_c` |
| `phy / phy_prime`                     | `transferCoeff / transferCoeff_prime`   |
| `SupportMoment`                       | `supportMoments`                        |
| `SpanNodePositions`                   | `spanNodePositions`                     |
| `number_of_spans`                     | `numberOfSpans`                         |
| `E_spans / I_spans`                   | `elasticModuli / inertiaMoments`        |
| `BendingMomentStatic`                 | `staticBendingMoment`                   |
| `RotationStatic`                      | `staticRotation`                        |
| `ShearForceAbscissaStatic`            | `staticShearForceAbscissa`              |
| `ShearForceStatic`                    | `staticShearForce`                      |
| `DeflectionStatic`                    | `staticDeflection`                      |
| `ShearForceAbscissa_`                 | `shearForceAbscissa_`                   |
| `Omega_Second_Spans`                  | `omegaSecond`                           |
| `Omega_Prime_Spans`                   | `omegaPrime`                            |
| `leftLoadedSpanSupportMoments()`      | `computeLeftLoadedSupportMoments()`     |
| `rightLoadedSpanSupportMoments()`     | `computeRightLoadedSupportMoments()`    |
| `LeftSupportMoments()`                | `leftSupportMomentsForSpan()`           |
| `RightSupportMoments()`               | `rightSupportMomentsForSpan()`          |
| `SpanLeftSupportMoments`              | `spanLeftMoments`                       |
| `SpanRightSupportMoments`             | `spanRightMoments`                      |
| `SpanLeftRight`                       | `spanBoundaryMoments`                   |
| `pointsXCoordinates()`                | `globalXCoordinates()`                  |
| `get_all_abscisse` (param)            | `getAllAbscissae` (param)               |

### Formules (`BeamFormulas.h`, ex fonctions `static` dans `Hyperstatique.cpp`)

| Ancien nom               | Nouveau nom                      |
|--------------------------|----------------------------------|
| `HypPartBendingMoment()` | `BeamFormula_BendingMoment()`    |
| `HypPartRotation()`      | `BeamFormula_Rotation()`         |
| `HypPartShearForce()`    | `BeamFormula_ShearForce()`       |
| `HypPartDeflection()`    | `BeamFormula_Deflection()`       |

### `SpanResult` — champs

| Ancien champ    | Nouveau champ        |
|-----------------|----------------------|
| `span_index`    | `spanIndex`          |
| `BM`            | `bendingMoment`      |
| `SF`            | `shearForce`         |
| `Def`           | `deflection`         |
| `Rot`           | `rotation`           |
| `max_BM/SF/...` | `maxBendingMoment/…` |
| `sec_BM/SF/...` | `maxBM_sectionIdx/…` |
| `alpha_BM/…`    | `maxBM_alphaIdx/…`   |
