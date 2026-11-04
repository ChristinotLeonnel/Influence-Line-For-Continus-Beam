# Contribuer

## Règles non négociables

1. **Un seul moteur numérique (C++).** N'ajoutez pas de calcul de sollicitations en Python. Seuls sont admis en Python les contrôles indépendants (tests, vérifications de la note de calcul).
2. **Toute correction numérique s'accompagne d'un test** qui échoue avant et passe après. La valeur attendue doit être obtenue *indépendamment* du code : forme fermée, oracle `tests/_reference.py` ou force brute.
3. **Ne supprimez pas un test qui révèle un défaut.** Marquez-le `xfail(strict=True)` avec le numéro du défaut, puis retirez le marqueur dans le commit qui corrige.
4. **Toute modification d'une valeur publiée** est justifiée scientifiquement et inscrite dans `CHANGELOG.md`.
5. **Toute optimisation est mesurée avant et après**, dans la même session (`docs/audit/reproduce/bench.py`). Elle ne doit pas dégrader la précision : les tests d'oracle restent à 10⁻¹².
6. Pas de nouvelle dépendance sans nécessité démontrée.

## Environnement

```bash
cmake -S . -B build -DTSARALOHA_BUILD_TESTS=ON -DTSARALOHA_BUILD_EXAMPLES=ON
cmake --build build --config Release
ctest --test-dir build -C Release
PYTHONPATH=build/python python -m pytest          # PowerShell : $env:PYTHONPATH="build/python"
```

Les fichiers Python sont copiés dans `build/python` à chaque build, y compris quand seuls des `.py` ont changé.

## Où modifier quoi

| Sujet | Fichier |
|---|---|
| Équations d'une travée | `LIPoutreContinue/src/core/Isostatique.cpp` |
| Poutre continue | `LIPoutreContinue/src/core/Hyperstatique.cpp` |
| Recherche de position des charges | `LIPoutreContinue/src/loading/MovingLoadSearch.cpp` |
| Enveloppes par section | `LIPoutreContinue/src/loading/Loading.cpp` |
| Réactions, orchestration, export | `LIPoutreContinue/src/Output.cpp` |
| Exposition Python | `LIPoutreContinue/bindings/` |
| API de haut niveau | `python/Tsaraloha/LIPoutreContinue/api.py` |
| Note de calcul | `python/Tsaraloha/LIPoutreContinue/report.py` |

## Style

- C++20, avertissements `/W4` / `-Wall -Wextra` sans nouvel avertissement.
- Messages d'erreur : dire ce qui ne va pas, la valeur reçue et un exemple correct.
- Documentation et messages en français ; noms d'API publics en anglais (API 2.0).
