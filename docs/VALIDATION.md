# Guide de validation scientifique

Ce document décrit **ce qui est calculé**, **comment c'est vérifié** et **avec quelle précision**. Chaque
affirmation renvoie à un test exécutable (`tests/`) ou à un script de `docs/audit/reproduce/`.

## 1. Modèle

Poutre d'Euler-Bernoulli, élasticité linéaire, petits déplacements ; déformation d'effort tranchant négligée ;
EI constant par travée. Appuis verticaux indéformables. Un encastrement est modélisé par une travée de longueur
nulle, de souplesse L/6EI = 0.

**Conventions.** M > 0 : fibre inférieure tendue. w > 0 vers le haut. R > 0 vers le haut. Charge unité
descendante.

### Travée isostatique (charge unité en a, section x, b = L − a)

| | a ≤ x | a > x |
|---|---|---|
| M | a(1 − x/L) | x(1 − a/L) |
| V | −a/L (a < x) | 1 − a/L ; les deux valeurs pour a = x |
| w | −a(L−x)[x(2L−x) − a²]/(6EIL) | −x(L−a)[a(2L−a) − x²]/(6EIL) |
| θ = dw/dx | −a(2L² − 6Lx + 3x² + a²)/(6EIL) | −(L−a)[a(2L−a) − 3x²]/(6EIL) |

### Poutre continue

Équation des trois moments à l'appui i (avec aᵢ = cᵢ = Lᵢ/3EIᵢ, bᵢ = Lᵢ/6EIᵢ ; ω′ < 0 et ω″ > 0 sont les
rotations d'extrémité de la travée isostatique) :

    bᵢ Mᵢ₋₁ + (cᵢ + aᵢ₊₁) Mᵢ + bᵢ₊₁ Mᵢ₊₁ = ω′ᵢ₊₁ − ω″ᵢ

Le système est résolu par la méthode des foyers : φᵢ = bᵢ / (aᵢ + cᵢ₋₁ − bᵢ₋₁ φᵢ₋₁), φ′ symétrique. Les
sollicitations s'obtiennent ensuite par superposition :

    M = M₀ + Mᵢ₋₁(1 − x/L) + Mᵢ x/L          V = V₀ + (Mᵢ − Mᵢ₋₁)/L
    Rᵢ = R⁰ᵢ + (Mᵢ₋₁ − Mᵢ)/Lᵢ₋₁ + (Mᵢ₊₁ − Mᵢ)/Lᵢ

### Charges mobiles

La ligne d'influence est l'interpolant linéaire par morceaux de ses valeurs nodales.

- **Convoi** : g(p) = Σ Pᵢ f(p + dᵢ) est linéaire par morceaux. Les extrema sont donc atteints lorsqu'un essieu est sur un nœud ; ces positions sont évaluées aux limites à gauche et à droite.
- **Charge répartie** : G(p) = Σ qₖ[F(p + cₖ₊₁) − F(p + cₖ)], où F est la primitive. Les extrema sont aux points de rupture ou là où G′ s'annule.
- **Charges indépendantes** : E⁺ = Σ max, E⁻ = Σ min, chaque charge pouvant être absente.

## 2. Campagne de validation

| Contrôle | Référence | Résultat | Test |
|---|---|---|---|
| M, V, w, θ, moments sur appuis : 7 configurations (1 à 4 travées, EI variables, pas 0,37 non entier, encastrements) | Résolution directe numpy des équations de Clapeyron (`tests/_reference.py`, aucun code partagé) | erreur relative ≤ 10⁻¹⁵ (tolérance 10⁻¹²) | `test_influence_lines.py` |
| M_B = −3PL/32, −Pa(L²−a²)/4L² ; bi-encastrée −ab²/L², −a²b/L² ; w = −PL³/48EI | formes fermées | 10⁻¹³ | idem |
| Maxwell-Betti w(xᵢ;xⱼ) = w(xⱼ;xᵢ) | réciprocité | 2·10⁻¹⁶ | idem, `test_core` (C++) |
| Continuité du moment aux appuis, w = 0 aux appuis | physique | exact | idem |
| ΣR = 1 et ΣR·x = x pour toute position | équilibre | 10⁻¹² / 10⁻⁹ | `test_results.py` |
| R_B = 11P/16 ; bi-encastrée R_A = b²(3a+b)/L³ | formes fermées | 10⁻¹³ | idem |
| Enveloppes : 2 charges indépendantes, chaque section | force brute (balayage fin, entrée et sortie partielles) | 10⁻⁹ | `test_regressions_v2.py` |
| Charge répartie, optimum entre deux nœuds (pas 0,7 m) | intégrale analytique | 10⁻¹² | `test_envelopes.py` |
| Bi-encastrée sous P mobile : M_min = −4PL/27 | forme fermée | 10⁻¹² | idem |
| Rotation maximale d'extrémité PL²/(9√3 EI) | forme fermée | 10⁻⁴ (pas 0,05) | idem |
| Unités : w = PL³/48EI pour P en kN | forme fermée | 10⁻⁹ | `test_regressions_v2.py` |
| Défauts 1.x N1–N9 | voir [audit/03](audit/03_numerique.md) | corrigés | `test_regressions_v2.py` |

Total : 116 tests Python et 1 exécutable de tests C++ (6 contrôles), exécutés en CI sous Linux et Windows.

## 3. Précision et convergence

- Lignes d'influence : **exactes aux nœuds** (erreur d'arrondi uniquement).
- Enveloppes de M et V isostatiques : exactes, car les lignes d'influence sont linéaires par morceaux.
- Enveloppes des grandeurs hyperstatiques (moments sur appuis, réactions, w, θ) : erreur **O(h²)**, car leurs lignes d'influence sont cubiques entre les nœuds. Ordre mesuré : 2,00 sur l'intégrale de M_B (`reproduce/convergence.py`) et sur la réaction centrale (`test_reaction_envelope_udl_converges_quadratically`).
- Localisation de l'extrême : il est recherché aux sections de calcul, d'où une décroissance O(h²) à constante irrégulière. La note de calcul publie l'**écart relatif entre pas successifs** (option `convergence=True`).

**Recommandation.** Pas ≤ L_min/40, valeur par défaut de `Beam.analyze()`. Vérifiez que l'écart relatif h → h/2
est négligeable pour votre usage.

## 4. Reproduire

```bash
PYTHONPATH=build/python python -m pytest -v
PYTHONPATH=build/python python docs/audit/reproduce/convergence.py
```
