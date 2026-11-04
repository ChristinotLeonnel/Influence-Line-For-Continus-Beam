# 02 — Doublons, code mort et chemins concurrents

Méthode : recherche de chaque symbole dans C++, bindings, Python, stubs,
exemples et documentation. Rien n'est classé « mort » s'il est atteignable
depuis l'API publique Python ou C++.

## 1. Moteurs numériques concurrents (priorité 1)

| Implémentation | Fichier | Statut |
|---|---|---|
| Moteur C++ | `LIPoutreContinue/src/**` | Référence |
| Moteur pur-Python | [`_core_python.py`](../../python/Tsaraloha/LIPoutreContinue/_core_python.py) (1555 l.) | **Portage intégral** : Isostatique, Hyperstatique, Configuration, Loading, ProjectPaths, Output |

- Parité mesurée (`reproduce/backend_parity.py`) : **0 écart** à 10⁻⁹ sur 3 travées articulées (tenseurs, extrema, enveloppes). Les deux moteurs partagent donc les mêmes défauts N2 à N8.
- **Divergence :** avec un encastrement (`L=[0, …]`), le backend Python lève `ZeroDivisionError` ([_core_python.py:306](../../python/Tsaraloha/LIPoutreContinue/_core_python.py#L306)), alors que le C++ ignore la travée nulle ([Hyperstatique.cpp:52](../../LIPoutreContinue/src/core/Hyperstatique.cpp#L52)).
- Toute correction doit être faite deux fois. C'est la cause racine du risque de divergence.
- Le choix du backend est **silencieux** ([__init__.py:110-116](../../python/Tsaraloha/LIPoutreContinue/__init__.py#L110-L116)). Une extension compilée mal placée bascule le calcul sur un moteur 40× plus lent sans avertissement (c'est le cas après `cmake --build` sous Visual Studio, voir 05 §4.1).

## 2. Duplications internes au C++

| Doublon | Emplacements | Proposition |
|---|---|---|
| Calcul des abscisses d'essieux / de tronçons | [Loading.cpp:205-211](../../LIPoutreContinue/src/loading/Loading.cpp#L205-L211), [268-274](../../LIPoutreContinue/src/loading/Loading.cpp#L268-L274), [440-446](../../LIPoutreContinue/src/loading/Loading.cpp#L440-L446) ; **version divergente** ligne 420 | Une seule fonction `axlePositions(load)` |
| Boucle « max par section puis par travée » | Constructeur de `Loading` (×3) + `computeCriticalSection` (×3) | Un seul algorithme d'enveloppe paramétré |
| `BendingMoments`, `Rotation`, `Deflection` | [Hyperstatique.cpp:271-399](../../LIPoutreContinue/src/core/Hyperstatique.cpp#L271-L399) : trois copies de la même double boucle | Un noyau générique + 3 formules |
| `ShearForce()` / `ShearForceAbscissa()` | [Isostatique.cpp:68-88](../../LIPoutreContinue/src/core/Isostatique.cpp#L68-L88) : deux appels à `Eq_ShearForce` avec un booléen | Un seul calcul qui renvoie les deux |
| `leftLoadedSpan…` / `LeftSupportMoments` + `Span*SupportMoments` + `SpanLeftRight` | Hyperstatique | Trois représentations intermédiaires de la même information (`SupportMoment`) |
| Calcul de `node_lengths` | [Output.cpp:44-48](../../LIPoutreContinue/src/Output.cpp#L44-L48), [__init__.py `structural_model`](../../python/Tsaraloha/LIPoutreContinue/__init__.py) (×2) | Calculé une seule fois dans le modèle |
| Recherche de max 2D / 3D | [Utils.h:118-149](../../LIPoutreContinue/include/LIPoutreContinue/Utils.h#L118-L149) | Une recherche sur un tableau contigu |

## 3. Code mort ou vestigial

| Élément | Usages trouvés | Verdict |
|---|---|---|
| `getValue()` [Utils.h](../../LIPoutreContinue/include/LIPoutreContinue/Utils.h) | aucun | Mort, mais dans un en-tête public : **déprécié en 2.0**, suppression en 3.0 |
| `parseVector()`, `LoadParser()` [Utils.h](../../LIPoutreContinue/include/LIPoutreContinue/Utils.h) | aucun usage. **Correction de l'audit initial :** ils ne sont *pas* exposés dans les bindings, seulement cités dans une docstring. | Vestige de l'ancien format texte ; **déprécié en 2.0** (`[[deprecated]]`, en-tête C++ public) |
| `Configuration::loadFromData`, `YoungModule`, `Inertie`, `steps` | bindings + `_core_python` ; inutilisés par `Loading` | Classe de base artificielle ; à fusionner dans le futur modèle |
| `UpdatePositions` + `ProjectPaths::pos_*` + `05_Load_Positioning/` | bindings, docs ; aucun exemple fonctionnel ne l'alimente | Vestige du flux fichier ; à déprécier (garder une couche de compatibilité 1 version) |
| `Hyperstatique::SpanLeftSupportMoments`, `SpanRightSupportMoments`, `leftMomentsCache_`, `rightMomentsCache_` | usage interne transitoire | Libérables après construction (mémoire) |
| `writeMax(data, …)` paramètre `data` ignoré [Output.cpp:73-77](../../LIPoutreContinue/src/Output.cpp#L73-L77) | — | Nettoyage |
| `report.py` : 3 moteurs PDF (weasyprint → Chromium → pdfkit) [report.py:27-110](../../python/Tsaraloha/LIPoutreContinue/report.py#L27-L110) | `html_to_pdf` | Chemins concurrents ; un seul moteur documenté |

## 4. Fichiers hors périmètre ou à retirer du dépôt

| Fichier | Raison |
|---|---|
| `exemple/teste.py`, `exemple/exemple1.py`, `exemple2.py`, `exemple3.py` | Brouillons (`teste.py` ne concerne pas la librairie) ; doublons de `ex01..ex05` |
| `exemple/ex05_loading_exact_position.py` **et** `ex05_visualisation_complete.py` | Deux « ex05 » |
| `resultats_ex03/` (40 JSON) | Sorties générées versionnées : elles divergeront du code à la première correction. Elles pourraient devenir des *golden files* de régression, mais seulement après correction des enveloppes |
| `google_veo_script.md` | Marketing, sans lien avec la librairie ; déjà exclu du sdist |
| `setup_pure.py` | Second système de build concurrent de `pyproject.toml`, lié au backend pur-Python (décision 1) |

## 5. Conversions de formats inutiles

- C++ `vector<vector<vector<double>>>` → listes Python imbriquées → `numpy.array` dans `plot.py` et `report.py`. Il y a une copie à chaque accès de propriété, plus la conversion numpy.
- `CombineLoadPosition.Addition` : `map<string,map<string,double>>` → dict → JSON. Les clés sont fabriquées par concaténation de chaînes (« `12 1` »), alors qu'une structure typée suffirait.

## 6. Exécution (2.0)

| Élément | Action |
|---|---|
| `_core_python.py`, `setup_pure.py` | **Supprimés** (moteur dupliqué) |
| `exemple/teste.py`, `exemple1.py`, `exemple3.py` | **Supprimés** (brouillons sans valeur propre) |
| `exemple/exemple2.py` | Réécrit en `ex07_pont_beton_convois.py` (API 2.0), puis supprimé |
| `ex05_visualisation_complete.py` | Renommé `ex06_…` (doublon de numéro) |
| `resultats_ex03/` | **Supprimé** (sorties 1.x périmées, valeurs d'enveloppes fausses) ; non régénéré dans le dépôt |
| `UpdatePositions` | Déprécié (`DeprecationWarning`), conservé jusqu'en 3.0 |
| `parseVector`, `LoadParser`, `getValue` | `[[deprecated]]` |
| Recherche de position (3 copies + 1 divergente) | Une seule implémentation : `MovingLoadSearch` |
| `report.py` 3 moteurs PDF | Un seul (weasyprint) |
| `google_veo_script.md` | **Conservé** : contenu non technique de l'auteur, déjà exclu du sdist ; à déplacer à votre convenance |
