# Changelog

## 2.0.0 — en cours

Version incompatible : **les valeurs des enveloppes de charge changent**. Chaque
changement corrige un défaut documenté, avec un cas reproductible, dans
[docs/audit/03_numerique.md](docs/audit/03_numerique.md), et il est verrouillé
par un test (`tests/test_regressions_v2.py`). Les lignes d'influence
elles-mêmes (BM, SF, Def, Rot, moments sur appuis) sont **inchangées** : elles
sont vérifiées exactes à 10⁻¹⁵ par rapport à une résolution directe des
équations de Clapeyron.

### Corrections numériques (valeurs modifiées)
- **N1 — Unités.** Les enveloppes de flèche et de rotation étaient 1000× trop petites, car E était en Pa et les charges en kN. Les entrées ne changent pas (E en Pa, charges en kN). Sorties : M en kN·m, V en kN, w en m, θ en rad. Les *lignes d'influence* `Def` et `Rot` restent exprimées pour une charge unité de 1 N (m/N, rad/N).
- **N2 — Enveloppes signées.** L'ancien calcul additionnait, pour chaque charge, la valeur signée de plus grand module, si bien que les signes opposés se compensaient. Désormais : E⁺ = Σ max et E⁻ = Σ min, rapportés séparément (`max`, `min`). `maximum_value` est l'extrême gouvernant, c'est-à-dire celui de plus grand module.
- **N3 — Entrée du convoi.** Toutes les positions sont maintenant explorées, y compris celles où les premiers essieux ont déjà quitté la poutre par la gauche.
- **N4 — Convention de `length`.** Une seule convention pour tous les calculs : `[d0, s1, s2, …]`, essieu i à `origine + d0 + s1 + … + si`. Toutes les valeurs sont utilisées. La charge « combinée » interprétait auparavant ces valeurs comme des abscisses absolues.
- **N5 — Charge combinée.** C'est désormais la superposition de charges **indépendantes**. Auparavant, toutes les charges étaient soudées à une même origine.
- **N6 — Charges réparties.** L'intégration est exacte sur les bornes réelles, tronçons partiellement hors poutre compris. L'ancienne version omettait le dernier intervalle et arrondissait les bornes au nœud le plus proche.
- **N7 — Effort tranchant.** Corrige le décalage d'un nœud entre la ligne d'influence (N+1 valeurs) et la grille.
- **N8 — Maillage.** Les nœuds sont calculés par `i·h` et non plus par accumulation, ce qui supprime l'élément parasite de ~10⁻¹⁵ m. Exemple : L = 6 m avec un pas de 0,1 m donne 61 nœuds au lieu de 62.
- **N9 — Essieux.** Les essieux sont placés à leur position exacte, sans arrondi au nœud.
- **Enveloppe « section critique ».** Elle est maintenant calculée à la section du maximum de la ligne d'influence, comme le dit la documentation, et non plus sur toute la travée.

### Ajouts
- **API de haut niveau** (`api.py`) : `Beam` (`add_span`, `add_support` 'pin'/'roller'/'fixed', `analyze`), `PointLoadTrain(axles, spacings)`, `DistributedLoad(q, length)`, `Results` (`envelope()`, `influence_line()`, `reaction_influence_line()`, `reactions()`, `maximum_bending_moment`…, `to_json()`, `plot()`, `report()`). Résultats en NumPy, avec unités et abscisses physiques. Elle n'effectue aucun calcul : tout est délégué au moteur C++. L'API 1.x (`Output`, `Load`, …) reste disponible.
- **Réactions d'appui** : lignes d'influence exactes `support_reaction` [appui][charge], déduites des moments sur appuis (ΣR = 1 vérifié), et enveloppes par appui `support_reaction_envelopes`. Exports `02_Influence_Lines/support_reaction.json` et `04_Load_Envelopes/support_reactions.json`.
- **Courbes enveloppes** E⁺(x), E⁻(x) le long de la poutre, pour M, V, w et θ et pour chaque type de charge : `section_envelopes`, export `04_Load_Envelopes/envelope_curves.json`.
- **Extrema des lignes d'influence** (max **et** min) repérés par leurs abscisses physiques `section_x` et `load_x` : `influence_line_extrema`.
- Module de calcul `MovingLoadSearch` (C++, `lipc::`) : recherche exacte réutilisable sur toute ligne d'influence.
- `load_delivery` / dict d'enveloppe : `max`, `min` (valeur, travée, section, `section_x`, détail par charge) et `section_x`.
- `Position1D.position` : abscisse exacte de l'origine de la charge, ou NaN si l'extrême correspond à la charge absente.
- `Loading.section_envelopes` : enveloppes E⁺ et E⁻ en chaque section ; `Loading.compute_at_section(span, section)` ; argument `scale`.
- Le JSON `04_Load_Envelopes/*` contient les blocs `max` et `min`. Les positions non définies sont écrites `null`.
- Validation des entrées dans le cœur C++ (`steps > 0`, E·I > 0, tailles cohérentes).

### Note de calcul (réécrite)
- `Results.report(path)` : note HTML autonome (figures SVG), ou PDF via weasyprint. Sections : page de garde, structure, géométrie et matériaux, appuis, hypothèses, équations (Clapeyron, foyers, réactions), méthode et discrétisation, charges, lignes d'influence, enveloppes, positions critiques, réactions, **vérifications exécutées** (ΣR = 1, ΣR·x = x, Maxwell-Betti, continuité, flèche nulle aux appuis), étude de convergence optionnelle (`convergence=True`), conclusion et limites.
- Toute valeur provient du moteur ou d'une vérification exécutée lors de la génération. La version, le pas, le nombre de nœuds et les données d'entrée sont reproduits.
- Les mentions « RDM6 » sont supprimées : la note ne s'attribue plus à un logiciel tiers. Le PDF passe par un seul moteur (weasyprint) ; les solutions de repli Chromium et pdfkit sont supprimées.
- `Output.generate_calculation_note()` / `generate_calculation_pdf()` sont conservées (`DeprecationWarning`) et délèguent au nouveau générateur. `logo_svg` et `curve_plotter` sont ignorés.

### Comportements modifiés
- `Loading.one_point_load` lève `IndexError` hors de la grille, au lieu d'afficher un message et de renvoyer 0.
- `Loading.one_rectangular_load(begin, end)` intègre jusqu'au nœud `end` **inclus**.
- Le détail de la charge combinée (`load` / `addition`) a la même forme que les autres : `{nom: {value, Position, alpha}}`. Les anciennes clés `"<intensité> <rang>"` sont supprimées.
- Les charges portant le même nom sont distinguées (`"Camion #2"`).

### Performance
- Enveloppes : **×18 (N = 404) à ×29 (N = 1004)**, avec une recherche désormais exacte. Pic mémoire −38 à −43 % : `Loading` n'effectue plus de copie du tenseur N² (constructeur `BorrowCurves`). Les deux versions ont été mesurées alternativement, sur le même poste et dans la même session, voir [docs/audit/benchmarks.md](docs/audit/benchmarks.md).

### Suppression du second moteur
- **Le backend pur-Python (`_core_python.py`) et `setup_pure.py` sont supprimés.** C'était une copie complète du moteur, ~40× plus lente sur les enveloppes, qui ne gérait pas les encastrements et qui aurait dû recevoir chaque correction en double. Il n'y a plus qu'un seul moteur numérique : l'extension C++.
- Si l'extension n'est pas chargeable, `import Tsaraloha.LIPoutreContinue` lève une `ImportError` explicite, au lieu de basculer silencieusement sur l'autre moteur. `_BACKEND` vaut toujours `"cpp"`.
- Wheels C++ pour Windows x64, Linux x86_64/aarch64 et macOS x86_64/arm64, CPython 3.9 à 3.14. La suite de tests complète est exécutée sur chaque wheel. Sur une plateforme sans wheel (par exemple Pydroid/Android), l'installation demande un compilateur C++20.

### Build / tests
- Le `.pyd` est désormais placé à côté du paquet avec les générateurs multi-configuration (Visual Studio).
- Ajout de la suite pytest (`tests/`), d'un test natif C++ (`ctest`) et de la CI Linux + Windows.
