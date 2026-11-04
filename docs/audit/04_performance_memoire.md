# 04 — Performance et mémoire (mesures de référence)

**Machine :** poste Windows 11 de l'utilisateur, build MSVC Release, Python 3.15.
**Script :** [`reproduce/bench.py`](reproduce/bench.py). Une exécution par
processus ; « RSS pic » = `peak_wset` (psutil) moins le RSS initial. Le
chargement utilisé pour les enveloppes est un convoi de 4 essieux plus une
charge répartie de 8 m.

## 1. Mesures

| Backend | Travées (m) | pas | N nœuds | `compute()` | 1 accès `out.BM` | Enveloppes | RSS pic |
|---|---|---|---|---|---|---|---|
| C++ | 10+10 | 0,5 | 42 | 1 ms | <1 ms | 0,03 s | 1,3 MiB |
| C++ | 12+16+12 | 0,25 | 163 | 1 ms | <1 ms | 0,57 s | 7,6 MiB |
| C++ | 20+30+30+20 | 0,25 | 404 | 4 ms | 3 ms | 4,70 s | 33 MiB |
| C++ | 20+30+30+20 | 0,1 | 1004 | 15 ms | 17 ms | **54,8 s** | 177 MiB |
| C++ | 40+50+50+40+40 | 0,1 | 2205 | 89 ms | 110 ms | (non mesuré) | **503 MiB** (sans enveloppes) |
| Python | 10+10 | 0,5 | 42 | 3 ms | — | 0,64 s | 1,7 MiB |
| Python | 12+16+12 | 0,25 | 163 | 42 ms | — | **24,6 s** | 7,3 MiB |
| Python | 20+30+30+20 | 0,25 | 404 | 234 ms | — | (non mesuré, > 5 min estimé) | 40 MiB |

Ces chiffres ne sont pas des benchmarks de référence publiables : un seul
poste, une seule exécution par cas. Ils servent de **base de comparaison
avant/après**.

## 2. Analyse

### 2.1 Mémoire : O(N²) par grandeur, 9 tenseurs N×N
Les tenseurs exposés BM, SF, Def, Rot (et l'abscisse SF) sont chacun
`[S][N_s][N_total]`, soit environ N² doubles. S'y ajoutent, en interne dans
`Hyperstatique`, BendingMomentStatic, RotationStatic, ShearForceStatic,
ShearForceAbscissaStatic et DeflectionStatic. Ce sont des blocs N_s² par
travée, gardés **pendant toute la vie de l'objet** alors qu'ils ne servent
qu'à l'assemblage.

Stockage en `vector<vector<vector<double>>>` : une allocation par ligne, soit
N allocations par grandeur, sans contiguïté, ce qui est défavorable au cache
et empêche le SIMD.

**Constat clé :** toute l'information hyperstatique tient dans la matrice des
moments sur appuis `SupportMoment[S+1][N]`. Avec les formules isostatiques
fermées, n'importe quelle ligne d'influence (section x quelconque, y compris
hors nœuds) s'évalue en O(N). La mémoire *nécessaire* est donc O(S·N), contre
O(N²) aujourd'hui. Pour N = 2205 et S = 5, cela représente 6 × 2205 doubles
≈ 100 kB au lieu de 500 MiB. Les tenseurs complets peuvent rester disponibles
*à la demande* pour la compatibilité.

### 2.2 Enveloppes : O(S·N_s · N · n_essieux · N), soit ≈ O(N³)
- Pour chaque section (N), chaque position de départ (N) et chaque essieu,
  `MetersToPosition` effectue un **balayage linéaire** de la grille (O(N)
  jusqu'à la sortie anticipée).
- `CombinedLoad` alloue un `map<string, map<string,double>>` **par position
  de départ et par section** (N² maps, plus des `std::to_string`).
- `computeCriticalSection` refait intégralement la recherche sur une travée.
- Les 4 grandeurs sont traitées en parallèle par `std::async`, et chaque
  `Loading` copie **par valeur** le tenseur complet `CURVES` (constructeur
  `Loading(std::vector<…> CURVES, …)`), soit 4 copies N² supplémentaires.

**Gains attendus, sans perte de précision :**
1. Indexation directe sur grille uniforme par travée ou recherche dichotomique : O(1) ou O(log N) au lieu de O(N).
2. Évaluation par sommes de préfixes (intégrales cumulées) pour les charges réparties : O(1) par tronçon au lieu d'O(N).
3. Mémoriser l'argmax au lieu de `Indice_of` après coup, et pas de `map` dans la boucle chaude.
4. Passage par `const&` (ou `span`) des tenseurs.

Ces mesures ramènent la complexité à O(N² · n_essieux). L'ordre de grandeur
attendu pour N = 1004 se situe entre la centaine de millisecondes et la
seconde. Cet objectif doit être *mesuré* après implémentation et n'est pas
garanti ici.

### 2.3 Frontière Python/C++
Chaque accès à `out.BM` reconvertit l'intégralité du tenseur en listes
imbriquées (110 ms et des centaines de Mo d'objets Python pour N = 2205).
`plot.py` et `report.py` font ensuite `np.array(...)` par-dessus. Exposer des
`numpy.ndarray` partageant le tampon C++ (protocole buffer pybind11, sans
copie) supprime les deux coûts.

### 2.4 Parallélisme
`std::async(std::launch::async)` crée un thread par tâche, jusqu'à 4 × S
threads dans `compute()` et 8 dans `computeLoadEnvelopes()`. Sur des tâches de
quelques millisecondes, le coût de création des threads domine. **À
re-mesurer après correction algorithmique** : il faut décider sur benchmark
s'il faut garder un parallélisme (par section, avec un pool) ou rester
séquentiel. Aucun ajout d'OpenMP n'est justifié à ce stade.

## 3. Protocole de mesure proposé pour la suite
- Trois modèles figés : S (2 travées, N ≈ 40), M (4 travées, N ≈ 400), L (5 travées, N ≈ 2000).
- Mesures : temps `compute`, temps enveloppes, RSS pic, temps d'un accès aux résultats, temps d'export JSON.
- Exécution avant et après chaque optimisation sur la même machine. La médiane de 5 exécutions est consignée dans `docs/audit/benchmarks.md`.
