# 03 — Vérification scientifique et numérique

Tous les chiffres proviennent de [`reproduce/`](reproduce/), exécuté sur le
backend C++ construit depuis `24aa7ff` :

```
PYTHONPATH=<build>/python python docs/audit/reproduce/validate_engine.py
```

La référence est **indépendante** : résolution directe (numpy) du système des
trois moments de Clapeyron, puis superposition des solutions isostatiques
exactes. Elle ne partage aucun code avec la librairie.

## 1. Ce qui est correct (vérifié)

### 1.1 Lignes d'influence isostatiques — [Isostatique.cpp](../../LIPoutreContinue/src/core/Isostatique.cpp)
Pour une charge unité en `a` sur une travée `[0, L]`, section en `x` :

| Grandeur | Formule codée | Vérification |
|---|---|---|
| M | `a(1 − x/L)` si a ≤ x, sinon `x(1 − a/L)` | Formule classique |
| V | `−a/L` si a < x, `1 − a/L` si a > x, deux valeurs si a = x | Discontinuité unité correctement représentée |
| w | `−a(L−x)[x(2L−x) − a²] / (6EIL)` (a ≤ x) | Dérivée deux fois → M/EI : OK |
| θ | `−a[2L² − 6Lx + 3x² + a²] / (6EIL)` (a < x) | Égal à dw/dx (dérivé à la main) |
| ω′, ω″ | rotations d'extrémité `a b (L+b)/(6EIL)` et `a b (L+a)/(6EIL)` | Termes de charge de Clapeyron |

### 1.2 Poutre continue — [Hyperstatique.cpp](../../LIPoutreContinue/src/core/Hyperstatique.cpp)
Méthode des **foyers** : rapports φᵢ = bᵢ / (aᵢ + cᵢ₋₁ − bᵢ₋₁ φᵢ₋₁) (et φ′
symétrique depuis la droite), avec a = c = L/(3EI) et b = L/(6EI). La
superposition `M = M₀ + Mᵢ₋₁(1 − x/L) + Mᵢ x/L` et ses homologues en V, w, θ
sont cohérentes avec la convention « moment positif = fibre inférieure tendue,
flèche positive vers le haut ».

| Contrôle | Résultat |
|---|---|
| BM vs Clapeyron direct, 6 configurations (1–4 travées, EI variables, pas 0,37 m non entier, `[0, L]` encastré-appuyé, `[0, L, 0]` bi-encastré) | erreur relative max **3·10⁻¹⁶** |
| Flèche vs Clapeyron direct (mêmes cas) | **8,5·10⁻¹⁶** |
| 2 travées égales, P à mi-travée 1 : M_B = −3PL/32 | exact (−0,9375) |
| Bi-encastrée : M_A = −ab²/L² | exact (−1,47) |
| Réciprocité de Maxwell-Betti w(xᵢ; xⱼ) = w(xⱼ; xᵢ), 3 travées hétérogènes | asymétrie **2,2·10⁻¹⁶** |
| Parité backend C++ / backend Python (articulé) | 0 écart > 10⁻⁹ |

**Conclusion : le solveur de lignes d'influence est exact à la précision
machine. Il ne doit pas être réécrit, mais restructuré (mémoire, API).**

L'astuce « travée de longueur nulle = encastrement » est mathématiquement
valable : pour L = 0, la souplesse f = L/6EI est nulle, donc la rotation est
bloquée. Elle est cependant non documentée et produit des effets de bord sur
l'API (voir 01, `Hyperstatique`).

## 2. Défauts détectés

Chaque défaut est accompagné d'un cas minimal, de son explication et de la
correction proposée.

### N1 — Système d'unités incohérent (critique)
- **Où :** déclaration [Utils.h:13](../../LIPoutreContinue/include/LIPoutreContinue/Utils.h#L13) (« m, kN, Pa, m⁴ ») ; [DOCUMENTATION.md §3](../../DOCUMENTATION.md).
- **Cas :** L = 10 m, E = 210 GPa, I = 8·10⁻⁴ m⁴, P = 100 kN à mi-portée.
  Librairie : w = 1,240·10⁻⁵ ; exact PL³/48EI = **1,240·10⁻² m**. Rapport : **1000,0**.
- **Explication :** la ligne d'influence de flèche vaut L³/(48EI) en m/N lorsque
  EI est en N·m². La multiplier par une intensité exprimée en kN donne un
  résultat 10³ fois trop petit. M et V ne dépendent pas de E et sont donc justes.
- **Correction :** calcul interne entièrement en SI (N, m, Pa). La couche API
  accepte et restitue des unités explicites, et les rapports convertissent pour
  l'affichage (kN, kN·m, mm). C'est un changement de valeurs : il faut le
  documenter dans le CHANGELOG.

### N2 — Les enveloppes additionnent des maxima signés (critique)
- **Où :** [Loading.cpp:46-58](../../LIPoutreContinue/src/loading/Loading.cpp#L46-L58), `val += lo.value` ; `MaxValueInVector` renvoie la valeur signée de plus grande |·| ([Utils.h:81-89](../../LIPoutreContinue/include/LIPoutreContinue/Utils.h#L81-L89)).
- **Cas :** 2 travées de 10 m, charges indépendantes A = 100 kN et B = 2 × 60 kN à 9 m, section x = 7,5 m. Librairie : |M| = **82,8** kN·m. Enveloppe vraie, par force brute : **201,6** kN·m (`reproduce/envelope_sections.py`).
- **Explication :** par superposition linéaire, si les charges k sont placées
  indépendamment, l'effet extrême en une section est
  `E⁺ = Σₖ maxₚ Sₖ(p)` et `E⁻ = Σₖ minₚ Sₖ(p)`. La librairie retient pour
  chaque charge la valeur signée de plus grand module, puis additionne : un
  maximum positif et un minimum négatif s'annulent. Il n'y a de plus aucune
  enveloppe min/max séparée, alors que M⁺ en travée et M⁻ sur appui sont deux
  vérifications distinctes.
- **Correction :** calculer pour chaque section les paires (max⁺, min⁻) de chaque
  charge, avec leur position. Les combinaisons se font par sommes séparées.
  Exposer les enveloppes `max(x)` et `min(x)` en tout point.

### N3 — Le convoi n'entre jamais par la gauche (critique)
- **Où :** [Loading.cpp:214](../../LIPoutreContinue/src/loading/Loading.cpp#L214) : `for (auto& first_wall : POSITION)`, donc la position du premier essieu est toujours ≥ 0.
- **Cas :** travée de 10 m, convoi [10 kN ; 100 kN] avec un entraxe de 20 m. Le maximum est atteint avec 100 kN à mi-portée et 10 kN hors poutre : **250 kN·m**. Librairie : **25 kN·m** (seul le 1ᵉʳ essieu est jamais pris en compte).
- **Explication :** les positions balayées doivent couvrir tous les cas où
  au moins un essieu est sur la poutre, soit
  `p ∈ [x_min − longueur_convoi, x_max]`. Le sens de circulation inverse
  (convoi retourné) n'est pas non plus envisagé.
- **Correction :** balayage complet. Pour une ligne d'influence linéaire par
  morceaux, l'extremum est atteint lorsqu'un essieu est sur un nœud. La
  recherche exacte consiste donc à placer successivement chaque essieu sur
  chaque nœud, soit n_essieux × N configurations. Ajouter une option `reversible`.

### N4 — Deux conventions différentes pour `length` (critique)
- **Où :** `PluralPointLoad` cumule (`acc += length[i+1]`, [Loading.cpp:205-211](../../LIPoutreContinue/src/loading/Loading.cpp#L205-L211)) ; `CombinedLoad` lit `j.Length[compteur]` comme une **abscisse absolue** ([Loading.cpp:420](../../LIPoutreContinue/src/loading/Loading.cpp#L420)).
- **Cas :** 2 travées (8 + 10 m), convoi 100/100/50 kN, `length=[0, 2, 3]`, sans charge répartie. « Ponctuelle » : 345,3 ; « combinée » : **394,8** kN·m. Les deux résultats devraient être identiques.
- **Explication :** la version combinée place les essieux à 0, 2 et 3 m au lieu de 0, 2 et 5 m.
- **Note :** README et DOCUMENTATION affirment que « la dernière valeur de
  `length` n'est pas utilisée ». C'est faux : `PluralPointLoad` utilise toutes
  les valeurs.
- **Correction :** une seule fonction de positionnement des essieux, et une
  convention unique, documentée et testée.

### N5 — La « charge combinée » soude toutes les charges (élevée)
- **Où :** [Loading.cpp:407-481](../../LIPoutreContinue/src/loading/Loading.cpp#L407-L481) : une seule variable `first_wall` pour toutes les charges.
- **Cas :** travée de 10 m, P = 1 kN et q = 1 kN/m sur 10 m. Attendu (charges indépendantes) : 2,5 + 12,5 = **15,0**. Librairie : **12,5** (P à 1 m, q de 1 à 10 m, dont la partie au-delà de 10 m est tronquée).
- **Explication :** le modèle implicite est un « train » rigide unique, ce qui
  n'est pas ce que décrit l'API (deux listes indépendantes).
- **Correction :** combinaison par défaut = superposition d'enveloppes
  indépendantes (N2). Un `LoadGroup` explicite pour les convois réellement
  liés (charge ponctuelle + charge répartie d'accompagnement).

### N6 — Intégration des charges réparties tronquée (élevée)
- **Où :** [Loading.cpp:235](../../LIPoutreContinue/src/loading/Loading.cpp#L235) : `for (i = begin; i < end; ++i)`, donc le nœud `end` est exclu.
- **Cas :** travée de 10 m, q = 1 kN/m sur toute la portée, pas de 1 m : M_max = **12,25** au lieu de qL²/8 = **12,5** (−2 %). Avec un pas de 0,1 m, le résultat est 12,5 *par accident* : le dernier intervalle exclu est l'élément quasi nul créé par N8.
- **Erreurs associées :** les extrémités de chaque tronçon sont arrondies au nœud le plus proche (`MetersToPosition`), ce qui donne une erreur de longueur pouvant atteindre un pas complet par tronçon. De plus, la règle des trapèzes appliquée à une ligne d'influence cubique est en O(h²) : ordre mesuré **2,00** (`reproduce/convergence.py`), soit 1 % d'erreur sur M_B au pas de 1 m.

  | pas (m) | 2 | 1 | 0,5 | 0,25 | 0,125 |
  |---|---|---|---|---|---|
  | erreur relative M_B | 4,0·10⁻² | 1,0·10⁻² | 2,5·10⁻³ | 6,3·10⁻⁴ | 1,6·10⁻⁴ |
- **Correction :** intégrer l'interpolant entre les bornes *réelles* `[a, b]`,
  éléments partiels compris. Entre deux nœuds d'une même travée, chaque ligne
  d'influence est un polynôme de degré ≤ 3 en la position de la charge. La
  règle de Simpson (ou une quadrature de Gauss à 2 points) y est donc
  **exacte**. On élimine ainsi totalement l'erreur de discrétisation, à coût
  identique.

### N7 — Effort tranchant : décalage d'indice (moyenne)
- **Où :** [Output.cpp:163](../../LIPoutreContinue/src/Output.cpp#L163) passe `SF` avec `X`. Chaque ligne `SF[s][sec]` contient N + 1 valeurs (double valeur à la section), alors que `X` en contient N.
- **Cas :** travée de 10 m, pas de 1 m, section x = 3 m, charge de 100 kN. Valeur correcte (+70), mais position rapportée **4,0 m** au lieu de 3,0 m. Toutes les ordonnées situées à droite de la section sont décalées d'un nœud.
- **Correction :** représenter V par deux valeurs (V⁻, V⁺) au droit de la
  section, sur la même grille que les autres grandeurs. Il n'y a alors plus de
  tableau à longueur variable.

### N8 — Maillage par accumulation flottante (moyenne)
- **Où :** [Isostatique.cpp:12-17](../../LIPoutreContinue/src/core/Isostatique.cpp#L12-L17) : `while (pas < L) { push(pas); pas += steps; } push(L);`.
- **Cas :** L = 6 m, pas 0,1 m → **62 nœuds** (au lieu de 61), dernier élément de **5,3·10⁻¹⁵ m**. L = 1 m, pas 0,1 → 12 nœuds, élément de 1,1·10⁻¹⁶ m.
- **Conséquences :** nœud et section dupliqués, extrema en double, effets de
  compensation avec N6, coût inutile.
- **Correction :** `n = ceil(L/h − ε)`, `xᵢ = i·L/n` (pas uniforme ≤ h et
  extrémités exactes), ou `xᵢ = i·h` calculé par multiplication avec
  suppression du dernier nœud s'il est à moins de ε·h de L.

### N9 — Charges ponctuelles arrondies au nœud le plus proche (moyenne)
- **Où :** `MetersToPosition` ([Loading.cpp:159-180](../../LIPoutreContinue/src/loading/Loading.cpp#L159-L180)).
- **Explication :** pour un entraxe qui n'est pas multiple du pas (par exemple
  1,5 m avec un pas de 1 m), chaque essieu subit un décalage pouvant atteindre
  h/2, donc une erreur |S′|·h/2 par essieu. La géométrie du convoi n'est pas
  respectée.
- **Correction :** interpolation de la ligne d'influence à la position exacte
  de chaque essieu (cubique de Hermite exacte, puisque w et θ sont tous deux
  connus, ou évaluation analytique).

### N10 — Extrema réduits à des indices (moyenne)
- `findMaxAbsoluteValue3D` ([Utils.h:118](../../LIPoutreContinue/include/LIPoutreContinue/Utils.h#L118)) : uniquement max |·|, en cas d'égalité le *dernier* indice est retenu (`>=`), sans abscisse physique. L'extremum réel peut se trouver entre deux nœuds, d'où une erreur O(h²) sur la valeur.
- **Correction :** renvoyer `(valeur, x_section, x_charge, travée)` pour le max et
  pour le min, avec raffinement local (interpolation parabolique ou analytique)
  de l'extremum.

### N11 — Backend Python : encastrement impossible
- `L = [0, 10]` → `ZeroDivisionError` ([_core_python.py:306](../../python/Tsaraloha/LIPoutreContinue/_core_python.py#L306)). Le C++ fonctionne.

### N12 — Robustesse des entrées (C++)
- Aucun contrôle côté C++ : `steps ≤ 0` provoque une boucle infinie
  ([Isostatique.cpp:13](../../LIPoutreContinue/src/core/Isostatique.cpp#L13)),
  E·I = 0 produit des divisions par zéro et des NaN, et une liste de travées
  vide n'est pas détectée (seule `Configuration::loadFromData` vérifie).
- Le cas de la travée unique et des 2 travées dont une nulle est accepté, mais
  le comportement n'est pas spécifié (aucun test).

## 3. Points non couverts par la librairie actuelle

Ce sont des fonctionnalités demandées qui n'existent pas encore. Il ne s'agit
pas de défauts.

- **Réactions d'appui :** aucune ligne d'influence n'est fournie. On peut les obtenir exactement à partir des moments sur appuis : `Rᵢ = R⁰ᵢ + (Mᵢ₋₁ − Mᵢ)/Lᵢ + (Mᵢ₊₁ − Mᵢ)/Lᵢ₊₁`.
- **Contrôle d'équilibre** ΣR = ΣP : impossible sans réactions.
- **Diagrammes pour un chargement fixe donné** (M(x), V(x), w(x) sous un cas de charge statique) : non disponibles. Seules les lignes d'influence et les enveloppes existent.
- **Appuis élastiques, porte-à-faux, EI variable dans une travée :** non supportés. Les porte-à-faux sont fréquents en pratique.
