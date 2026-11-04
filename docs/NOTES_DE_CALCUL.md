# Notes de calcul

```python
res = beam.analyze(step=0.25, loads=[truck, lane])
res.report("note.html",                       # ou "note.pdf" (pip install "Tsaraloha[report]")
           project_title="Pont de la rivière", project_ref="AFF-2026-014",
           author="N. Ingénieur", company="Bureau d'études",
           convergence=True)                  # étude de convergence : 2 calculs de plus
```

## Contenu

| § | Partie | Origine des données |
|---|---|---|
| — | Page de garde : projet, référence, auteur, date, version de la librairie | paramètres + `__version__` |
| 1 | Structure, schéma coté, géométrie et matériaux, conditions d'appui | `Beam` |
| 2 | Hypothèses, conventions, équations fondamentales | texte fixe (modèle du moteur) |
| 3 | Méthode numérique, pas, nombre de nœuds par travée | `Results.step`, maillage du moteur |
| 4 | Charges mobiles | charges passées à `analyze()` |
| 5 | Extrema des lignes d'influence (section et position de la charge) + figure | `influence_line_extrema` |
| 6 | Enveloppes M, V, w (figures) ; tableau max/min par type de charge | `section_envelopes`, enveloppes générales |
| 7 | Positions critiques : contribution et origine de chaque charge | détail `load` des extrêmes |
| 8 | Réactions d'appui max/min | `support_reaction_envelopes` |
| 9 | **Vérifications exécutées** : ΣR = 1, ΣR·x = x (appuis simples), Maxwell-Betti, continuité, flèche nulle aux appuis, domaine de validité \|w\|/L ≤ 1/100 | `report.run_checks()` sur les résultats |
| 10 | Étude de convergence (2h, h, h/2) avec écart relatif | ré-exécution du moteur |
| 11 | Conclusion (conformité des vérifications) et limites du modèle | résultat des contrôles |

## Règles

- **Aucune valeur saisie à la main.** Tout chiffre provient du moteur ou d'une vérification exécutée pendant la génération.
- Une vérification non conforme est affichée **NON CONFORME**, et la conclusion indique que les résultats ne doivent pas être utilisés en l'état.
- La note ne contient aucune vérification réglementaire (résistance, ELS, coefficients partiels).
- Sortie HTML autonome, figures en SVG intégré (matplotlib optionnel ; sans lui, seul le schéma de la poutre est tracé). PDF via weasyprint uniquement.

## Contrôles seuls

```python
from Tsaraloha.LIPoutreContinue.report import run_checks
for c in run_checks(res):
    print(c["name"], c["measure"], "<", c["threshold"], "OK" if c["passed"] else "NON CONFORME")
```
