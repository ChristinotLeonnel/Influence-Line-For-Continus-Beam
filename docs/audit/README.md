# Audit technique et scientifique — LIPoutreContinue

**Date :** 2026-11-04  **Commit de référence :** `24aa7ff` (branche `main`, arbre propre)
**Portée :** intégralité du dépôt (C++, bindings, Python, build, CI, docs, exemples).
**Aucune modification du code source n'a été faite pour cet audit.**

| Fichier | Contenu |
|---|---|
| [01_architecture_actuelle.md](01_architecture_actuelle.md) | Inventaire des classes, responsabilités, dépendances, défauts de conception |
| [02_doublons.md](02_doublons.md) | Code dupliqué, mort ou concurrent |
| [03_numerique.md](03_numerique.md) | Vérification scientifique : ce qui est juste, ce qui est faux (cas reproductibles) |
| [04_performance_memoire.md](04_performance_memoire.md) | Mesures de temps et de mémoire |
| [05_resultats_rapports_doc.md](05_resultats_rapports_doc.md) | Résultats, notes de calcul, documentation, build/CI |
| [06_plan.md](06_plan.md) | Architecture cible et plan de modernisation priorisé |
| [reproduce/](reproduce/) | Scripts indépendants ayant produit tous les chiffres de ce rapport |

## État initial (étape A)

| Élément | Constat |
|---|---|
| Environnement | Windows 11, CMake 4.5.0-rc2, MSVC (générateur Visual Studio), Python 3.15.2, numpy, matplotlib 3.11, pytest 9.2 |
| Build CMake | **OK**, sans avertissement (`cmake -B build -DTSARALOHA_BUILD_EXAMPLES=ON`, pybind11 v2.13.6 récupéré par FetchContent) |
| Import après build | **KO sous Visual Studio** : le `.pyd` est placé dans `build/python/Tsaraloha/LIPoutreContinue/Release/`, les `.py` un niveau au-dessus → `import` retombe silencieusement sur le backend pur-Python (voir 05 §4.1) |
| Exemples C++ | `ex01`, `ex02` s'exécutent |
| Tests | **Aucun test unitaire ni d'intégration dans le dépôt.** Le seul test est la commande fumée de `cibuildwheel` |
| Installation locale | Le paquet installé dans le site-packages tourne sur le **backend pur-Python** (`_BACKEND == "python"`) |

## Synthèse

**Le cœur mathématique est juste.** Les lignes d'influence produites par
`Hyperstatique` (méthode des foyers) coïncident avec une résolution directe
indépendante des équations de Clapeyron à **≤ 9·10⁻¹⁶ en erreur relative**.
C'est vrai sur 6 configurations (1 à 4 travées, EI variables, pas non entier
0,37 m, encastrements par travée de longueur nulle). La réciprocité de
Maxwell-Betti est vérifiée à 2·10⁻¹⁶.

**Les erreurs se situent en aval : chargement, enveloppes, unités.** Résultats
mesurés (19/26 contrôles passent) :

| # | Défaut | Gravité | Exemple mesuré |
|---|---|---|---|
| N1 | Unités : E en Pa avec des charges en kN → flèches et rotations **×10⁻³** | **Critique** | 100 kN à mi-portée : 1,24·10⁻⁵ au lieu de 1,24·10⁻² m |
| N2 | Somme des maxima signés de charges indépendantes (pas d'enveloppe max⁺/min⁻) | **Critique** | x = 7,5 m : \|M\| = 82,8 au lieu de 201,6 kN·m |
| N3 | Le convoi n'entre jamais par la gauche (balayage `first_wall ≥ 0`) | **Critique** | Convoi [10 ; 100] kN : M = 25 au lieu de 250 kN·m |
| N4 | `length` interprété différemment par `CombinedLoad` et `PluralPointLoad` | **Critique** | Même convoi : 345,3 (ponctuel) contre 394,8 (combiné) |
| N5 | Combinée : toutes les charges sont soudées à une même origine | Élevée | P = 1 et q = 1 : 12,5 au lieu de 15,0 |
| N6 | Intégration UDL : le dernier intervalle n'est jamais intégré (`i < end`) | Élevée | qL²/8 : 12,25 au lieu de 12,5 |
| N7 | Effort tranchant : décalage d'un indice entre la ligne `SF` et la grille `X` | Moyenne | Position critique 4,0 m au lieu de 3,0 m |
| N8 | Maillage par accumulation `pas += steps` → nœud quasi dupliqué | Moyenne | L = 6, pas 0,1 : 62 nœuds, Δx min = 5·10⁻¹⁵ m |
| N9 | Charges arrondies au nœud le plus proche (erreur jusqu'à pas/2) | Moyenne | — |

**Performance et mémoire.** Le calcul des lignes d'influence est rapide
(16 ms pour N = 1004 nœuds). La mémoire croît en **O(N²)** : 503 MiB pour
N = 2205. Les enveloppes croissent en **≈O(N³)** : **55 s** pour N = 1004.
Le backend pur-Python est ~40× plus lent sur les enveloppes.

**Architecture.** Il existe **deux moteurs numériques complets** : le C++ et
`_core_python.py` (1555 lignes, portage ligne à ligne). Ils produisent des
résultats identiques (0 écart à 10⁻⁹), *bugs compris*, sauf pour les
encastrements, où le backend Python plante (`ZeroDivisionError`). Cela
contrevient directement à la règle « un seul moteur ».

## Décisions prises (2026-11-04)

| # | Décision |
|---|---|
| 1 | **Supprimer le backend pur-Python** et étendre les wheels C++ (Linux, macOS, ARM, Python 3.14 et 3.15) |
| 2 | **Entrées inchangées** (E en Pa, charges en kN) ; conversion SI interne ; sorties M en kN·m, V en kN, w en m |
| 3 | **Corriger les enveloppes** (N2–N7) par défaut, en changement incompatible documenté → version **2.0.0** |
| 4 | Retirer les mentions RDM6 de la note de calcul (phase P7) |

## Avancement

| Phase | Commit | Vérification |
|---|---|---|
| Audit + P0 (tests) + P1 (build) | `da9b0c8` | 62 tests + 11 `xfail(strict)` (N1–N9), chacun échouant sur sa valeur prédite ; `.pyd` correctement placé |
| P2 — Corrections numériques N1–N9 | `14c1b47` | Les 11 xfail passent et deviennent des tests de régression ; oracle de Clapeyron inchangé |
| P5 — Moteur unique (suppression du backend Python) | `a52515c` | `pip install .` dans un venv isolé : extension C++ seule ; ImportError explicite sinon |
| P3 — Mémoire et performance | `5ec0c9c` | A/B dans la même session : enveloppes ×18 à ×29, pic mémoire −38 à −43 % ([benchmarks](benchmarks.md)) |
| P4 — Résultats (réactions, courbes, extrema physiques) | `0ec7cef` | ΣR = 1, ΣR·x = x, formes fermées ; N6b (O(h²)) mesuré et documenté |
| P6 — API de haut niveau | `4c005f8` | Mêmes valeurs que l'API bas niveau (test d'égalité exacte) |
| P7 — Note de calcul | `87eed1c` | Contrôles exécutés à la génération ; mentions RDM6 supprimées |
| P8 — Nettoyage et dépréciations | `89cf539` | Exemples exécutés avec `-W error::DeprecationWarning` |
| P9 — Documentation | (ce commit) | Extraits du README exécutés (Python) et compilés dans `test_core` (C++) |

**Validation finale** (build propre, Windows / MSVC / Python 3.15) : 0 avertissement de compilation ; `ctest`
1/1 (6 contrôles) ; **116 tests pytest** sur le build CMake **et** sur la wheel installée dans un venv neuf ;
11/11 exemples (3 C++, 8 Python).

**Non vérifié localement :**
- build GCC / Linux et macOS ;
- `cibuildwheel@v3.1.4` et `cp314` ;
- PDF (weasyprint non installé).

Le premier passage de la CI (`tests.yml`, `wheels.yml`) tranchera ces points.

Lancer les tests :

```
cmake -S . -B build -DTSARALOHA_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release
PYTHONPATH=build/python python -m pytest        # PowerShell : $env:PYTHONPATH="build/python"
```

## Décisions requises avant la phase de modernisation (contexte)

1. **Backend pur-Python.** Il a été publié en v1.1.0 pour le support mobile.
   Options : (a) le supprimer et étendre les wheels C++ (Linux, macOS, ARM) ;
   (b) le garder comme repli, ce qui maintient deux moteurs ; (c) le
   transformer en oracle de validation utilisé uniquement par les tests.
2. **Convention d'unités** (N1) : imposer un système cohérent. Par exemple N,
   Pa, m, ou bien kN, kPa, m. Cela **modifie les valeurs** de flèche et de
   rotation produites.
3. **Sémantique des enveloppes** (N2–N5) : corriger fait changer les valeurs
   numériques des enveloppes. C'est justifié scientifiquement (voir 03), mais
   cela constitue un changement de comportement de l'API.
4. **Note de calcul** : elle se présente comme « conforme RDM6 / Logiciel :
   RDM6 » ([report.py:210](../../python/Tsaraloha/LIPoutreContinue/report.py#L210),
   [report.py:841](../../python/Tsaraloha/LIPoutreContinue/report.py#L841)).
   RDM6 est un logiciel tiers (IUT du Mans). Cette mention doit être retirée.
