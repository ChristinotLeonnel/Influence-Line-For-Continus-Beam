# Documentation Complète & Référence Exhaustive — Tsaraloha : LIPoutreContinue

Bienvenue dans le manuel de référence exhaustif de la sous-librairie **`Tsaraloha.LIPoutreContinue`** (faisant partie du framework d'ingénierie **`Tsaraloha`**).

Cette librairie haute performance (moteur C++20 avec interface Python pybind11) est dédiée à l'**analyse structurelle de poutres continues (isostatiques et hyperstatiques)**, au calcul des **lignes d'influence**, au passage de **charges mobiles complexes** (essieux ponctuels, convois routiers/ferroviaires, surcharges réparties variables) et à la détermination des **enveloppes de sollicitations maximales**.

---

## Sommaire

1. [Architecture & Principes de Conception](#1-architecture--principes-de-conception)
2. [Installation & Compilation](#2-installation--compilation)
3. [Unités & Théorie du Calcul](#3-unités--théorie-du-calcul)
4. [Référence Exhaustive des Classes & API](#4-référence-exhaustive-des-classes--api)
   - [4.1. Classe `Output` (Point d'Entrée Principal)](#41-classe-output-point-dentrée-principal)
   - [4.2. Classe `Load` (Modélisation des Charges Mobiles)](#42-classe-load-modélisation-des-charges-mobiles)
   - [4.3. Classe `Isostatique` (Poutre Simple)](#43-classe-isostatique-poutre-simple)
   - [4.4. Classe `Hyperstatique` (Moteur Multi-Travées)](#44-classe-hyperstatique-moteur-multi-travées)
   - [4.5. Classe `Loading` (Moteur de Chargement)](#45-classe-loading-moteur-de-chargement)
   - [4.6. Classe `UpdatePositions` (Repositionnement des Charges)](#46-classe-updatepositions-repositionnement-des-charges)
   - [4.7. Classe `ProjectPaths` (Gestionnaire d'Arborescence)](#47-classe-projectpaths-gestionnaire-darborescence)
   - [4.8. Structures de Données & Résultats (`Position1D/2D/3D`, `LoadDelivery`, ...)](#48-structures-de-données--résultats)
   - [4.9. Module `plot` (Visualisation Graphique)](#49-module-plot-visualisation-graphique)
5. [Référence de l'API C++ (`StructuralAnalysis.h`)](#5-référence-de-lapi-c-structuralanalysish)
6. [Arborescence et Formats d'Exports JSON / TXT](#6-arborescence-et-formats-dexports-json--txt)
7. [Guide Complet des Exemples Pas à Pas](#7-guide-complet-des-exemples-pas-à-pas)
8. [Diagnostic & Résolution des Erreurs](#8-diagnostic--résolution-des-erreurs)

---

## 1. Architecture & Principes de Conception

La librairie est structurée en deux couches complémentaires :

```
┌──────────────────────────────────────────────────────────────────────────┐
│                   API Publique Python (Tsaraloha)                        │
│   • Tsaraloha.LIPoutreContinue : Output, Isostatique, Hyperstatique,     │
│                                  Load, UpdatePositions, ProjectPaths     │
│   • Validation proactive des types & dimensions à la NumPy               │
└────────────────────────────────────┬─────────────────────────────────────┘
                                     │  (pybind11 bindings)
┌────────────────────────────────────▼─────────────────────────────────────┐
│                   Noyau Haute Performance C++20                          │
│   • Header unique : <LIPoutreContinue/StructuralAnalysis.h>              │
│   • Résolution matricielle optimisée (Clapeyron, Kahan, OpenMP ready)    │
│   • Exécution 100% en RAM : aucun accès disque automatique               │
│   • Sérialisation JSON nlohmann & exports texte structurés               │
└──────────────────────────────────────────────────────────────────────────┘
```

- **Calcul en RAM par défaut** : Instancier un objet ou appeler `compute()` ne crée aucun fichier sur disque. Les données sont conservées en mémoire pour un traitement instantané.
- **Exports explicites** : Les méthodes `export_all()`, `export_load_envelopes()`, etc., sont déclenchées uniquement sur demande.
- **Zéro fuite mémoire** : Encapsulation stricte des durées de vie entre Python et C++.

---

## 2. Installation & Compilation

### Installation standard via Pip
```bash
pip install .
```
`pip` gère la compilation C++20 via CMake et scikit-build-core automatiquement.

### Compilation manuelle avec CMake
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
export PYTHONPATH=build/python    # (Linux/macOS)
$env:PYTHONPATH="build/python"   # (Windows PowerShell)
```

---

## 3. Unités & Théorie du Calcul

### Système d'Unités

| Grandeur | Symbole | Unité SI / Recommandée |
| :--- | :--- | :--- |
| Longueur de travée / Abscisse | $L, x$ | Mètres ($\text{m}$) |
| Pas de discrétisation spatial | $\text{steps}$ | Mètres ($\text{m}$) |
| Module de Young (élasticité) | $E$ | Pascals ($\text{Pa}$ ou $\text{N/m}^2$, ex: $210 \times 10^9$) |
| Moment quadratique d'inertie | $I$ | Mètres puissance 4 ($\text{m}^4$) |
| Force concentrée ponctuelle | $P$ | Kilonewtons ($\text{kN}$) |
| Intensité de charge répartie | $q$ | Kilonewtons par mètre ($\text{kN/m}$) |
| Moment fléchissant | $M$ ou $\text{BM}$ | Kilonewton-mètres ($\text{kN}\cdot\text{m}$) |
| Effort tranchant | $V$ ou $\text{SF}$ | Kilonewtons ($\text{kN}$) |
| Déformée / Flèche | $w$ ou $\text{Def}$ | Mètres ($\text{m}$) |
| Rotation de section | $\theta$ ou $\text{Rot}$ | Radians ($\text{rad}$) |

---

## 4. Référence Exhaustive des Classes & API

---

### 4.1. Classe `Output` (Point d'Entrée Principal)

La classe `Output` gère la géométrie de la poutre continue, le calcul de l'ensemble des lignes d'influence, l'application des charges et l'export des résultats.

#### Constructeur
```python
Output(E, I, L, steps, root="")
```
- **`E`** (`list[float]`) : Module d'élasticité de chaque travée $[\text{Pa}]$.
- **`I`** (`list[float]`) : Moment d'inertie de chaque travée $[\text{m}^4]$.
- **`L`** (`list[float]`) : Longueur de chaque travée $[\text{m}]$.
- **`steps`** (`float`) : Pas spatial d'échantillonnage $[\text{m}]$ ($> 0$).
- **`root`** (`str` / `os.PathLike`, optionnel) : Dossier racine d'exportation des fichiers JSON/TXT.

---

#### Liste Complète des Méthodes de `Output`

| Méthode | Arguments | Retour | Description |
| :--- | :--- | :--- | :--- |
| **`compute(force=False)`** | `force: bool` | `None` | Calcule les lignes d'influence de toutes les sections ($M, V, w, \theta$) en mémoire. |
| **`set_loads(point_loads, distrib_loads)`** | `point_loads: list[Load]`, `distrib_loads: list[Load]` | `None` | Fournit les listes de charges mobiles au modèle (mode sans fichier). |
| **`compute_load_envelopes(force=False)`** | `force: bool` | `None` | Calcule en mémoire toutes les enveloppes de charge globales et critiques. Nécessite `set_loads()`. |
| **`export_influence_lines()`** | Aucun | `None` | Écrit les dossiers `01_Input/` et `02_Influence_Lines/` dans `root`. |
| **`export_critical_values()`** | Aucun | `None` | Écrit les valeurs maximales globales des lignes d'influence dans `03_Critical_Values/`. |
| **`export_load_envelopes()`** | Aucun | `None` | Calcule et écrit les enveloppes de charge dans `04_Load_Envelopes/`. |
| **`export_all()`** | Aucun | `None` | Exécute l'ensemble des exports (équivalent aux 3 fonctions d'export ci-dessus). |

---

#### Liste Complète des Attributs & Données Disponibles dans `Output`

##### 1. Géométrie et discrétisation
- **`X`** (`list[float]`) : Liste des coordonnées spatiales $x$ $[\text{m}]$ de l'ensemble des nœuds de la poutre complète.
- **`node_lengths`** (`list[float]`) : Abscisses cumulées $[\text{m}]$ des appuis (ex: `[0.0, 10.0, 22.0]`).
- **`number_of_spans`** (`int`) : Nombre total de travées de la poutre continue.
- **`steps`** (`float`) : Pas de calcul en mètres.
- **`E_spans`**, **`I_spans`**, **`L_spans`** (`list[float]`) : Propriétés physiques associées à chaque travée.
- **`paths`** (`ProjectPaths`) : Objet contenant l'arborescence complète des chemins d'export.
- **`is_computed`** (`bool`) : `True` si `compute()` a été exécuté.
- **`is_load_envelopes_computed`** (`bool`) : `True` si `compute_load_envelopes()` a été exécuté.

##### 2. Lignes d'influence 3D (disponibles après `compute()`)
Chaque tenseur 3D est indexé selon : `[index_travée][index_section][index_position_charge_alpha]`.
- **`BM`** (`list[list[list[float]]])`) : Moments fléchissants $[\text{kN}\cdot\text{m}]$.
- **`SF`** (`list[list[list[float]]])`) : Efforts tranchants $[\text{kN}]$.
- **`Def`** (`list[list[list[float]]])`) : Flèches / déformations verticales $[\text{m}]$.
- **`Rot`** (`list[list[list[float]]])`) : Rotations des sections $[\text{rad}]$.
- **`shear_force_all_abscissa`** (`list[list[list[float]]])`) : Abscisses associées aux lignes d'influence de l'effort tranchant.

##### 3. Extrema des Lignes d'Influence
- **`bending_moment_max_positions`** (`dict`) : Position et valeur du moment max : `{'i': travée, 'j': section, 'k': alpha, 'val': float}`.
- **`shear_force_max_positions`** (`dict`) : Position et valeur de l'effort tranchant maximal.
- **`deflection_max_positions`** (`dict`) : Position et valeur de la flèche maximale.
- **`rotation_max_positions`** (`dict`) : Position et valeur de la rotation maximale.
- **`support_moment_max_positions`** (`dict`) : Moment maximal sur les appuis intermédiaires : `{'i': appui, 'j': section, 'val': float}`.

##### 4. Enveloppes Générales de Charge (recherche sur TOUTE la poutre)
Disponibles après `compute_load_envelopes()`. Dictionnaires contenant `point_load`, `rectangular_load` et `combined_load` :
- **`bending_moment_general_envelope`** (`dict`) : Enveloppe générale du moment fléchissant.
- **`shear_force_general_envelope`** (`dict`) : Enveloppe générale de l'effort tranchant.
- **`deflection_general_envelope`** (`dict`) : Enveloppe générale de la flèche.
- **`rotation_general_envelope`** (`dict`) : Enveloppe générale de la rotation.

##### 5. Enveloppes Critiques de Charge (à la section du maximum global)
- **`bending_moment_critical_envelope`** (`dict`) : Enveloppe critique du moment fléchissant.
- **`shear_force_critical_envelope`** (`dict`) : Enveloppe critique de l'effort tranchant.
- **`deflection_critical_envelope`** (`dict`) : Enveloppe critique de la flèche.
- **`rotation_critical_envelope`** (`dict`) : Enveloppe critique de la rotation.

---

### 4.2. Classe `Load` (Modélisation des Charges Mobiles)

Décrit une charge mobile ponctuelle (essieu / convoi) ou répartie (uniforme / plurielle).

```python
Load(intensity, length, name="")
```

#### Liste des Attributs de `Load`
- **`intensity`** (`list[float]`) : Forces des essieux $[\text{kN}]$ ou intensités des tronçons réparties $[\text{kN/m}]$.
- **`length`** (`list[float]`) : Distances / entraxes $[\text{m}]$.
- **`name`** (`str`) : Identifiant / étiquette de la charge.

#### Liste des Méthodes de `Load`
- **`to_dict()`** : Convertit l'objet en dictionnaire Python `{'intensity': [...], 'length': [...], 'name': '...'}`.

#### Formats et Règles de Dimensionnement

```
1. CHARGE PONCTUELLE / CONVOI : len(length) == len(intensity)
   ┌──────────────────────────────────────────────────────────────────────┐
   │ Essieu unique    : intensity=[P],        length=[xP]                 │
   │ 2 essieux (P,Q)  : intensity=[P, Q],     length=[xP, xQ]            │
   │ 3 essieux (P,Q,R): intensity=[P, Q, R],  length=[xP, xQ, xR]        │
   └──────────────────────────────────────────────────────────────────────┘
   • xP : distance de référence (offset initial)
   • xQ, xR : distances entre essieux consécutifs (la dernière valeur de length n'est pas utilisée)

2. CHARGE RÉPARTIE : len(length) == len(intensity) + 1
   ┌──────────────────────────────────────────────────────────────────────┐
   │ Uniforme simple   : intensity=[q],           length=[depart, L_q]   │
   │ Uniforme plurielle: intensity=[q1, q2, qn],  length=[depart, L_q1,  │
   │                                                        L_q2, L_qn]  │
   └──────────────────────────────────────────────────────────────────────┘
   • depart : position de début de charge depuis l'appui gauche (0 si début de travée)
   • L_qi   : longueur du i-ème tronçon de charge
```

---

### 4.3. Classe `Isostatique` (Poutre Simple)

Calcul direct et indépendant pour une poutre bi-articulée sur 2 appuis simples.

```python
Isostatique(E, I, L, steps)
```

#### Attributs de `Isostatique`
- **`E`**, **`I`**, **`L`**, **`steps`** (`float`) : Propriétés physiques et maillage spatial.
- **`node_positions`** (`list[float]`) : Coordonnées des nœuds le long de la travée.
- **`omega_prime`** (`list[float]`) : Premier coefficient de flexibilité $\Omega'$.
- **`omega_second`** (`list[float]`) : Second coefficient de flexibilité $\Omega''$.
- **`a`**, **`b`**, **`c`** (`float`) : Constantes de flexibilité ($L/3EI$, $L/6EI$, $L/EI$).

#### Méthodes de `Isostatique`

| Méthode | Arguments | Retour | Description |
| :--- | :--- | :--- | :--- |
| **`bending_moment()`** | Aucun | `list[list[float]]` | Matrice 2D des lignes d'influence du moment $[\text{section}][\alpha]$. |
| **`shear_force()`** | Aucun | `list[list[float]]` | Matrice 2D des lignes d'influence de l'effort tranchant. |
| **`deflection()`** | Aucun | `list[list[float]]` | Matrice 2D des lignes d'influence de la flèche. |
| **`rotation()`** | Aucun | `list[list[float]]` | Matrice 2D des lignes d'influence de la rotation. |
| **`shear_force_abscissa()`** | Aucun | `list[list[float]]` | Abscisses associées aux discontinuités de l'effort tranchant. |
| **`eq_bending_moment(x)`** | `x: float` | `list[float]` | Ligne d'influence du moment fléchissant pour la section d'abscisse $x$. |
| **`eq_shear_force(x, return_abscissa=False)`** | `x: float`, `return_abscissa: bool` | `list[float]` | Ligne d'influence de l'effort tranchant pour la section d'abscisse $x$. |
| **`eq_deflection(x)`** | `x: float` | `list[float]` | Ligne d'influence de la flèche pour la section $x$. |
| **`eq_rotation(x)`** | `x: float` | `list[float]` | Ligne d'influence de la rotation pour la section $x$. |

---

### 4.4. Classe `Hyperstatique` (Moteur Multi-Travées)

Classe de base pour l'analyse d'une poutre continue à $N$ travées.

```python
Hyperstatique(E, I, L, steps)
```

#### Attributs de `Hyperstatique`
- **`E_spans`**, **`I_spans`**, **`L_spans`** (`list[float]`) : Vecteurs des propriétés par travée.
- **`steps`** (`float`) : Pas spatial.
- **`number_of_spans`** (`int`) : Nombre total de travées.
- **`span_node_positions`** (`list[list[float]]`) : Nœuds de discrétisation par travée.
- **`support_moment`** (`list[list[float]]`) : Matrice des moments hyperstatiques sur appuis.
- **`a_spans`**, **`b_spans`**, **`c_spans`** (`list[float]`) : Coefficients de flexibilité des travées.
- **`phy`**, **`phy_prime`** (`list[float]`) : Vecteurs de rotations d'appui.

#### Méthodes de `Hyperstatique`
- **`bending_moments()`** : Calcule et retourne la matrice 3D des moments fléchissants.
- **`shear_force(get_all_abscisse=False)`** : Calcule et retourne la matrice 3D des efforts tranchants.
- **`deflection()`** : Calcule et retourne la matrice 3D des flèches.
- **`rotation()`** : Calcule et retourne la matrice 3D des rotations.
- **`points_x_coordinates(liste)`** : Aplatit et convertit les positions en coordonnées $x$ globales.

---

### 4.5. Classe `Loading` (Moteur de Chargement)

Gère le déplacement et la convolution des convois de charges avec les lignes d'influence.

#### Attributs de `Loading`
- **`point_load`** (`LoadDelivery`) : Résultat d'enveloppe pour charges ponctuelles.
- **`rectangular_load`** (`LoadDelivery`) : Résultat d'enveloppe pour charges réparties.
- **`combined_load`** (`LoadDelivery`) : Résultat d'enveloppe combinée (ponctuelle + répartie).
- **`point_load_inputs`** (`list[Load]`) : Liste des charges ponctuelles appliquées.
- **`distrib_load_inputs`** (`list[Load]`) : Liste des charges réparties appliquées.

#### Méthodes de `Loading`
- **`one_point_load(intensity, span, section, alpha)`** -> `Position1D` : Calcule l'effet d'une charge ponctuelle unique à une position donnée.
- **`plural_point_load(intensity, length, span, section)`** -> `Position1D` : Calcule l'effet optimal d'un convoi de charges ponctuelles sur une section.
- **`one_rectangular_load(intensity, span, section, begin, end)`** -> `Position1D` : Calcule l'effet d'une charge répartie sur un tronçon $[begin, end]$.
- **`plural_rectangular_load(intensity, length, span, section)`** -> `Position1D` : Calcule l'effet optimal d'une charge répartie plurielle.
- **`combined_load_at(span, section)`** -> `CombineLoadPosition` : Superpose et maximise les effets combinés (ponctuel + réparti).
- **`compute_critical_section(span)`** -> `dict` : Détermine la section critique et les sollicitations associées pour une travée donnée.

---

### 4.6. Classe `UpdatePositions` (Repositionnement des Charges)

Met à jour et repositionne physiquement chaque charge sur le modèle à partir des enveloppes calculées.

```python
UpdatePositions(root, input_lines)
```

#### Attributs de `UpdatePositions`
- **`is_computed`** (`bool`) : `True` si les positions ont été recalculées en mémoire.
- **`results`** (`dict`) : Dictionnaire structuré des lignes de texte repositionnées :
  `results[scope][load_type][curve_name] = list[str]`
  *(où `scope` $\in \{\text{"Global"}, \text{"Critical\_Section"}\}$, `load_type` $\in \{\text{"Point\_Load"}, \text{"Distributed\_Load"}, \text{"Combined\_Load"}\}$).*

#### Méthodes de `UpdatePositions`
- **`compute(force=False)`** : Calcule le repositionnement des charges en RAM sans toucher au disque.
- **`write_all()`** : Génère et écrit les fichiers `.txt` correspondants dans `05_Load_Positioning/`.
- **`run()`** : Exécute séquentiellement `compute()` puis `write_all()`.

---

### 4.7. Classe `ProjectPaths` (Gestionnaire d'Arborescence)

Expose les chemins résolus d'exportation vers les dossiers de résultats.

```python
ProjectPaths(root)
```

#### Attributs de `ProjectPaths`
- **`root`** : Répertoire racine du projet.
- **`input`**, **`influence_lines`**, **`critical_values`**, **`load_envelopes`**, **`load_positioning`** : Chemins des 5 dossiers principaux.
- **`env_global`**, **`env_global_point`**, **`env_global_dist`**, **`env_global_combined`** : Chemins des enveloppes globales.
- **`env_critical`**, **`env_critical_point`**, **`env_critical_dist`**, **`env_critical_combined`** : Chemins des enveloppes à la section critique.
- **`pos_global`**, **`pos_critical`**, etc. : Chemins des fichiers de repositionnement.

#### Méthodes de `ProjectPaths`
- **`create_all()`** : Crée physiquement l'ensemble des répertoires sur disque.
- **`to_dict()`** : Retourne tous les chemins sous forme de dictionnaire Python.

---

### 4.8. Structures de Données & Résultats

#### 1. `Position1D`
- **`max_position`** (`int`) : Indice spatial de discrétisation du maximum.
- **`value`** (`float`) : Valeur maximale de la sollicitation.
- **`to_dict()`** -> `dict` `{ 'max_position': int, 'value': float }`.

#### 2. `Position2D`
- **`i`** (`int`) : Indice de l'appui / travée.
- **`j`** (`int`) : Indice de section.
- **`val`** (`float`) : Valeur de la sollicitation.
- **`to_dict()`** -> `dict` `{ 'i': int, 'j': int, 'val': float }`.

#### 3. `Position3D`
- **`i`** (`int`) : Indice de la travée critique ($0 \le i < N$).
- **`j`** (`int`) : Indice de la section critique le long de la travée.
- **`k`** (`int`) : Indice $\alpha$ de position de la charge unitaire mobile.
- **`val`** (`float`) : Amplitude de la sollicitation maximale.
- **`to_dict()`** -> `dict` `{ 'i': int, 'j': int, 'k': int, 'val': float }`.

#### 4. `CombineLoadPosition`
- **`max_position`** (`int`) : Indice de grille du maximum.
- **`position`** (`float`) : Abscisse globale en mètres $[\text{m}]$.
- **`value`** (`float`) : Valeur combinée maximale $[\text{kN}\cdot\text{m}]$ ou $[\text{kN}]$.
- **`addition`** (`dict`) : Détail des contributions individuelles de chaque charge.
- **`to_dict()`** -> `dict`.

#### 5. `LoadDelivery` (Détail d'une enveloppe de charge)
- **`span`** (`int`) : Travée la plus sollicitée.
- **`section`** (`int`) : Section la plus sollicitée.
- **`maximum_value`** (`float`) : Sollicitation maximale de l'enveloppe.
- **`position`** (`float`) : Position globale de la charge $[\text{m}]$.
- **`load`** (`dict[str, dict[str, float]]`) : Dictionnaire associant chaque nom de charge à ses caractéristiques optimales :
  - `'Position'` : Abscisse physique $[\text{m}]$ de la charge sur la poutre.
  - `'value'` : Contribution de cette charge $[\text{kN}\cdot\text{m}]$ ou $[\text{kN}]$.
  - `'alpha'` : Indice de discrétisation de position.
- **`to_dict()`** -> `dict`.


---

### 4.9. Module `plot` (Visualisation Graphique)

Le module `Tsaraloha.LIPoutreContinue.plot` fournit un ensemble de fonctions de haut niveau basées sur **Matplotlib** pour tracer des graphiques de qualité professionnelle, avec un thème sombre élégant et des rendus précis des charges.

#### Importation et Accès au Module

Par conception, pour éviter de charger inutilement de grosses dépendances graphiques comme `matplotlib` lors de simples calculs en tâche de fond ou d'exports JSON purs, **les fonctions de visualisation ne sont pas importées par défaut** dans le namespace principal de la librairie (`Tsaraloha.LIPoutreContinue`).

Pour y accéder, vous devez importer explicitement le sous-module de l'une des façons suivantes :

```python
# Option 1 (Recommandée) : Import direct des fonctions cibles
from Tsaraloha.LIPoutreContinue.plot import plot_output_full, plot_load_summary

# Option 2 : Import du module de tracé sous un alias
import Tsaraloha.LIPoutreContinue.plot as liplot

# Option 3 : Import classique de la librairie principale, puis import manuel du sous-module
import Tsaraloha.LIPoutreContinue as lipc
import Tsaraloha.LIPoutreContinue.plot as liplot
```

Une fois l'import réalisé, toutes les fonctions présentées ci-dessous deviennent accessibles dans votre script.

#### Fonctions de Visualisation Disponibles

| Fonction | Signature | Description |
| :--- | :--- | :--- |
| **`plot_isostatique_influence_lines`** | `(poutre, x, *, title="", figsize=(13, 7))` | Génère 4 panneaux (Moment $M$, Effort tranchant $V$, Flèche $w$ et Rotation $\theta$) pour une poutre isostatique à la section d'abscisse $x$. |
| **`plot_hyperstatique_influence_lines`** | `(hyper, BM, X, *, span=0, section=None, title="", figsize=(13, 5))` | Affiche les courbes de lignes d'influence du moment fléchissant pour les sections 1/4, 1/2, et 3/4 d'une travée spécifique (ou une section spécifique). |
| **`plot_support_moments`** | `(hyper, X, *, title="", figsize=(13, 5))` | Trace les lignes d'influence des moments sur tous les appuis intermédiaires. |
| **`plot_bm_envelopes`** | `(X, BM, *, span_lengths=None, title="", figsize=(13, 5))` | Affiche l'enveloppe des moments fléchissants à mi-travée de chaque travée. |
| **`plot_load_on_influence_line`** | `(X, li_curve, loads, alpha_opt, *, span_lengths=None, ylabel="Moment [kN.m]", title="", figsize=(13, 5), load_type="point")` | Superpose les charges sur la courbe de ligne d'influence à la position optimale donnée. |
| **`plot_load_summary`** | `(X, BM, loading, span, section, *, title="", figsize=(14, 12))` | Trace un résumé de chargement en 3 panneaux pour une section : charges ponctuelles optimales, charges réparties optimales, et toutes les charges combinées en action à leur position optimale globale. |
| **`plot_output_full`** | `(out, *, figsize=(14, 10), title="")` | Vue synthétique complète en 4 panneaux à partir d'un objet `Output` (moments sur appuis, moments mi-travée, efforts tranchants mi-travée, flèches mi-travée). |

#### Représentation Graphique Avancée des Charges
- **Appuis** : Représentés par des repères verticaux pointillés jaunes munis de triangles ▼ et d'étiquettes de position (ex: `A1 8.70 m`).
- **Convois (Charges Ponctuelles)** : Essieux individuels représentés par des flèches annotées avec leurs charges en $\text{kN}$.
- **Charges réparties (UDL) segmentées** : Chaque tronçon d'intensité constante est coloré sur la courbe de ligne d'influence avec des limites verticales nettes (interpolées mathématiquement) et représenté au-dessus par un rectangle hachuré dont la hauteur est proportionnelle à son intensité de charge en $\text{kN/m}$.

#### Exemple d'utilisation
```python
import matplotlib.pyplot as plt
import Tsaraloha.LIPoutreContinue as lipc
from Tsaraloha.LIPoutreContinue.plot import plot_load_summary, plot_output_full

# Initialisation et calcul
out = lipc.Output(E=[30e9]*3, I=[1.2e-3]*3, L=[10.0, 14.0, 10.0], steps=0.5)
out.compute()

# Configuration des charges
camion = lipc.Load(intensity=[60.0, 120.0], length=[0.0, 2.5], name="Camion")
surcharge = lipc.Load(intensity=[12.0, 20.0], length=[0.0, 6.0, 2.0], name="UDL")
out.set_loads(point_loads=[camion], distrib_loads=[surcharge])

# Tracé du résumé de chargement pour la travée 1, section centrale
loading_engine = lipc.Loading(
    curves=out.BM, position=out.X,
    span_node_positions=out.span_node_positions,
    spans=out.L_spans,
    point_loads=[camion], distrib_loads=[surcharge]
)

fig = plot_load_summary(out.X, out.BM, loading_engine, span=1, section=14)
plt.show()
```

---

## 5. Référence de l'API C++ (`StructuralAnalysis.h`)

Pour une intégration native en C++, inclure le header unique :

```cpp
#include <LIPoutreContinue/StructuralAnalysis.h>
#include <iostream>
#include <vector>

int main() {
    // 1. Définition des paramètres
    std::vector<double> E{210e9, 210e9};  // Pa
    std::vector<double> I{8e-4, 8e-4};    // m^4
    std::vector<double> L{10.0, 10.0};    // m
    double steps = 0.5;                   // m

    // 2. Initialisation de Output
    Output out(E, I, L, steps, "Export_Projet");

    // 3. Calcul en RAM des lignes d'influence
    out.compute();
    std::cout << "Moment max: " << out.BendingMomentMaxPositions.val << " kNm\n";

    // 4. Définition des charges mobiles
    load camion;
    camion.Intensity = {60.0, 120.0};
    camion.Length = {0.0, 2.5};
    camion.name = "Camion 2 essieux";

    load surcharge;
    surcharge.Intensity = {15.0};
    surcharge.Length = {0.0, 6.0};
    surcharge.name = "UDL 15kN/m";

    // 5. Calcul des enveloppes
    out.setLoads({camion}, {surcharge});
    out.computeLoadEnvelopes();

    // 6. Export complet sur disque
    out.exportAll();

    return 0;
}
```

---

## 6. Arborescence et Formats d'Exports JSON / TXT

Lors d'un appel à `export_all()`, les sous-dossiers suivants sont générés :

```
<root>/
├── 01_Input/
│   ├── Model/                 # Données géométriques et raideurs
│   └── Loading/               # Définitions des charges appliquées
├── 02_Influence_Lines/        # Lignes d'influence complètes
│   ├── BendingMoment/         # Fichiers JSON : BM_Span_X_Section_Y.json
│   ├── ShearForce/            # SF_Span_X_Section_Y.json
│   ├── Deflection/            # Def_Span_X_Section_Y.json
│   └── Rotation/              # Rot_Span_X_Section_Y.json
├── 03_Critical_Values/        # Extrema globaux
│   ├── BendingMoment.json     # { "span": i, "section": j, "alpha": k, "value": val }
│   ├── ShearForce.json
│   ├── Deflection.json
│   └── Rotation.json
├── 04_Load_Envelopes/         # Enveloppes de charge
│   ├── Global/                # Maximum sur toute la poutre
│   │   ├── Point_Load/
│   │   ├── Distributed_Load/
│   │   └── Combined_Load/
│   └── Critical_Section/      # À la section la plus sollicitée
│       ├── Point_Load/
│       ├── Distributed_Load/
│       └── Combined_Load/
└── 05_Load_Positioning/       # Fichiers descriptifs repositionnés
    ├── Global/
    └── Critical_Section/
```

---

## 7. Guide Complet des Exemples Pas à Pas

### Script Python Exhaustif

```python
import Tsaraloha.LIPoutreContinue as lipc

# ── 1. Initialisation de la structure ─────────────────────────────────────────
out = lipc.Output(
    E=[210e9, 210e9, 210e9],       # 3 travées de module 210 GPa
    I=[8e-4, 1.2e-3, 8e-4],        # Inerties en m^4
    L=[12.0, 16.0, 12.0],          # Longueurs : 12m, 16m, 12m
    steps=0.5,                     # Discrétisation tous les 50 cm
    root="./Resultats_Calcul",
)

# ── 2. Calcul des lignes d'influence ──────────────────────────────────────────
out.compute()
print("Coordonnées de tous les nœuds :", out.X)
print("Abscisses des appuis :", out.node_lengths)
print("Moment max global L.I. :", out.bending_moment_max_positions)

# ── 3. Définition des charges mobiles ─────────────────────────────────────────
# Convoi lourd de 3 essieux (60 kN, 120 kN, 120 kN)
convoi = lipc.Load(
    intensity=[60.0, 120.0, 120.0],
    length=[0.0, 2.0, 1.5],
    name="Convoi 3 essieux",
)

# Charge répartie uniforme de 15 kN/m sur 6 m
udl = lipc.Load(
    intensity=[15.0],
    length=[0.0, 6.0],
    name="Trottoir 15kN/m",
)

# ── 4. Calcul des enveloppes ──────────────────────────────────────────────────
out.set_loads(point_loads=[convoi], distrib_loads=[udl])
out.compute_load_envelopes()

# ── 5. Récupération des positions et valeurs optimales ────────────────────────
bm_env = out.bending_moment_general_envelope

print("\n--- ENVELOPPE DU MOMENT FLÉCHISSANT ---")
print(f"Travée critique : {bm_env['combined_load']['span']}")
print(f"Section critique : {bm_env['combined_load']['section']}")
print(f"Moment combiné maximal : {bm_env['combined_load']['maximum_value']:.2f} kN.m")

for load_name, data in bm_env['point_load']['load'].items():
    print(f"  • {load_name} : Position = {data['Position']:.2f} m (Contribution = {data['value']:.2f} kN.m)")

# ── 6. Export complet sur disque et repositionnement ──────────────────────────
out.export_all()

input_lines = [
    f"/Point/ {convoi.name} intensity: {convoi.intensity[0]} {convoi.intensity[1]} {convoi.intensity[2]} length: {convoi.length[0]} {convoi.length[1]} {convoi.length[2]}",
    f"/Distributed/ {udl.name} intensity: {udl.intensity[0]} length: {udl.length[0]} {udl.length[1]}",
]

up = lipc.UpdatePositions(root=out.paths.root, input_lines=input_lines)
up.run()
print("\n✓ Calculs et exports terminés avec succès dans ./Resultats_Calcul/")
```

---

## 8. Diagnostic & Résolution des Erreurs

| Message d'Erreur | Origine / Cause | Solution |
| :--- | :--- | :--- |
| `ValueError: E, I, L doivent décrire le même nombre de travées` | Tailles incompatibles entre les listes de rigidités | Vérifier que `len(E) == len(I) == len(L)` |
| `ValueError: Incohérence de dimensions pour Load` | Mauvais nombre d'éléments dans `length` | Ponctuelle : `len(length) == len(intensity)`. Répartie : `len(length) == len(intensity) + 1` |
| `RuntimeError: Output::exportLoadEnvelopes: aucune charge fournie` | Calcul d'enveloppe sans charges | Appeler `out.set_loads(point_loads, distrib_loads)` avant |
| `AttributeError: module ... has no attribute 'Outut'` | Faute de frappe dans le nom d'un symbole | Vérifier l'orthographe suggérée par le message |
| `ValueError: 'steps' doit être strictement positif` | Pas négatif ou nul | Choisir une valeur positive (ex. `0.5` ou `1.0`) |
