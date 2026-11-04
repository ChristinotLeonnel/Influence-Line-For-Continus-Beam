# Benchmarks avant / après

**Protocole :** [`reproduce/bench.py`](reproduce/bench.py). Les deux builds
(1.x = `24aa7ff`, 2.0 = branche courante) sont exécutés **alternativement dans
la même session**, sur le même poste (Windows 11, MSVC Release, Python 3.15).
Chaque exécution tourne dans un processus séparé.

Chargement : convoi de 4 essieux (60/120/120/60 kN) et charge répartie de 8 m.

Les temps absolus varient d'une session à l'autre selon la charge du poste
(facteur ~1,3–1,4 observé). Seuls les rapports mesurés dans une même session
sont significatifs.

| Modèle | N | Enveloppes 1.x | Enveloppes 2.0 | Gain | RSS pic 1.x | RSS pic 2.0 |
|---|---|---|---|---|---|---|
| 20+30+30+20 m, pas 0,25 | 404 | 7,39 / 7,45 s | 0,41 / 0,43 s | ×18 | 33,7 MiB | 19,0 MiB |
| 20+30+30+20 m, pas 0,1 | 1004 | 70,27 s | 2,46 s | ×29 | 176,1 MiB | 109,7 MiB |

`compute()` (lignes d'influence) : inchangé, de 20 à 30 ms pour N = 1004.

## Origine des gains
- **Algorithme.** La recherche se fait aux points de rupture de l'interpolant, avec une recherche dichotomique. Auparavant : balayage de toutes les positions de départ et `MetersToPosition` linéaire, soit O(N³). Il n'y a plus de `map<string,…>` alloué dans la boucle chaude.
- **Mémoire.** `Output` fait emprunter les tenseurs à `Loading` au lieu de les copier (une copie N² en moins par grandeur). L'enveloppe générale et l'enveloppe à la section critique partagent la même instance.

## Ce qui n'a pas encore été optimisé (P4)
- Les tenseurs exposés `BM/SF/Def/Rot` restent en O(N²) (≈ 39 Mo chacun pour N = 2205). Ils sont reconvertis en listes Python à chaque accès (0,03 s et ~160 Mo d'objets Python pour N = 2205). Le correctif suppose un stockage contigu et des vues NumPy, ce qui change le type renvoyé : il est prévu avec le nouveau système de résultats.
- `std::async` lance un thread par travée et par grandeur. `compute()` ne prend que ~25 ms pour N = 1004, donc aucune action n'est justifiée par la mesure.
