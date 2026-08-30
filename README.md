# Tsaraloha

Librairie chapeau destinée à regrouper plusieurs sous-librairies de calcul.
Chaque sous-librairie vit dans son propre sous-paquet (`Tsaraloha.<NomSousLib>`).

> 📖 **Documentation complète & Guide de référence** : Consultez [DOCUMENTATION.md](file:///e:/Book/Dev/Ligne%20Influence/DOCUMENTATION.md) pour la théorie, les équations, les formats JSON et l'ensemble de l'API C++ / Python.

## Sous-librairies

### LIPoutreContinue

Calcul de poutres continues — isostatique / hyperstatique, lignes
d'influence, enveloppes de charge (ponctuelle, répartie, combinée),
export JSON structuré.

Deux façons de l'utiliser :

- **En C++** : librairie coeur `LIPoutreContinue_core`, un seul header à
  inclure (`#include <LIPoutreContinue/StructuralAnalysis.h>`).
- **En Python** : sous-paquet `Tsaraloha.LIPoutreContinue`, structuré
  comme numpy — l'API publique pure-Python
  (`Tsaraloha/LIPoutreContinue/__init__.py`) enveloppe une extension
  compilée privée (`Tsaraloha.LIPoutreContinue._LIPoutreContinue`), jamais
  importée directement.

## Installation (Python)

```bash
pip install .
```

`pip` compile automatiquement l'extension C++ (via
[scikit-build-core](https://scikit-build-core.readthedocs.io) + CMake +
pybind11) — aucune étape manuelle requise.

## Usage rapide (Python)

```python
import Tsaraloha.LIPoutreContinue as lipc

# Poutre continue à 2 travées de 10 m, E=210 GPa, I=8e-4 m^4, pas de 1 m
out = lipc.Output(E=[210e9, 210e9], I=[8e-4, 8e-4], L=[10, 10], steps=1.0)

# Étape 1 : calcul en mémoire uniquement — rien n'est écrit sur disque
out.compute()
print(out.BM, out.SF, out.Def, out.Rot)      # lignes d'influence
print(out.bending_moment_max_positions)       # position/valeur du max

# Étape 2 (optionnelle) : enveloppes de charge — nécessite set_loads()
out.set_loads(
    point_loads=[lipc.Load(intensity=[50.0], length=[0.0], name="Camion")],
    distrib_loads=[lipc.Load(intensity=[12.0], length=[0.0, 4.0], name="UDL")],
)


# Étape 3 (optionnelle) : export JSON structuré sur disque
out.paths  # lipc.ProjectPaths — chemins résolus (01_Input/ ... 05_Load_Positioning/)
out.export_all()
```

Rien n'est automatique : `compute()` et les `export_*()` sont toujours des
appels explicites, jamais déclenchés par le constructeur.

## Structure des données d'entrée (Inputs)

### 1. Géométrie et matériaux de la structure

La géométrie de la poutre continue et ses propriétés de section sont définies par les paramètres `E`, `I`, `L` et `steps` :

| Paramètre | Type Python | Type C++ | Unité | Description | Contraintes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`E`** | `list[float]` (ou `float`) | `std::vector<double>` (ou `double`) | $\text{Pa}$ ($\text{N/m}^2$) | Module d'élasticité (Young) de chaque travée | $E_i > 0$, même taille que `I` et `L` |
| **`I`** | `list[float]` (ou `float`) | `std::vector<double>` (ou `double`) | $\text{m}^4$ | Moment quadratique d'inertie de chaque travée | $I_i > 0$, même taille que `E` et `L` |
| **`L`** | `list[float]` (ou `float`) | `std::vector<double>` (ou `double`) | $\text{m}$ | Longueur de chaque travée | $L_i > 0$, même taille que `E` et `I` |
| **`steps`** | `float` | `double` | $\text{m}$ | Pas de discrétisation / maillage le long de la poutre | $\text{steps} > 0$ |
| **`root`** *(opt.)* | `str` / `Path` | `std::string` / `path` | — | Répertoire de destination pour les exports JSON | Optionnel (`""` par défaut) |

> **Règle de dimensionnement** : Pour une poutre continue à $N$ travées avec `Output` ou `Hyperstatique`, les listes `E`, `I` et `L` doivent impérativement comporter exactement $N$ éléments (`len(E) == len(I) == len(L) == N`). Pour une poutre simple à une seule travée avec `Isostatique`, `E`, `I` et `L` sont de simples scalaires (`float`).

```python
# Exemple pour une poutre à 3 travées (12 m, 16 m, 12 m)
E = [210e9, 210e9, 210e9]  # 210 GPa par travée
I = [8.5e-4, 1.2e-3, 8.5e-4]  # Inerties variables en m^4
L = [12.0, 16.0, 12.0]  # Longueurs en mètres
steps = 0.5  # Échantillonnage tous les 50 cm
```

### 2. Définition des charges (`Load`)

La classe `Load` (ou `struct load` en C++) décrit une charge mobile — soit **ponctuelle** (essieu unique ou convoi d'essieux), soit **répartie** (uniforme ou plurielle) — destinée à `Output.set_loads()`.

```python
lipc.Load(intensity=[...], length=[...], name="NomDeLaCharge")
```

#### Paramètres du constructeur

- **`intensity`** (`list[float]`) :
  - **Charge ponctuelle** : force de chaque essieu $[\text{kN}]$. `len(intensity)` = nombre d'essieux du convoi.
  - **Charge répartie** : intensité de chaque tronçon $[\text{kN/m}]$. `len(intensity)` = nombre de tronçons.
- **`length`** (`list[float]`) :
  - **Charge ponctuelle** : distances entre essieux consécutifs $[\text{m}]$. `len(length) == len(intensity)` (la dernière valeur n'est pas utilisée par le moteur).
  - **Charge répartie** : `[PositionDepart, L_q1, L_q2, ..., L_qn]` $[\text{m}]$. `len(length) == len(intensity) + 1` (`PositionDepart = 0` si la charge débute au début de la travée).
- **`name`** (`str`, optionnel) : Étiquette libre affichée dans les exports JSON (ex. `"Essieu"`, `"Convoi BC"`, `"UDL"`).

---

#### Format des Charges Ponctuelles

| Type de charge | `intensity` | `length` |
| :--- | :--- | :--- |
| **Essieu unique** | `[P]` | `[xP]` |
| **2 essieux ($P$, $Q$)** | `[P, Q]` | `[xP, xQ]` |
| **3 essieux ($P$, $Q$, $R$)** | `[P, Q, R]` | `[xP, xQ, xR]` |

- `xP` : distance entre le début de travée et le 1er essieu (offset initial)
- `xQ` : distance entre le 1er et le 2ème essieu
- `xR` : distance entre le 2ème et le 3ème essieu *(la dernière valeur de `length` n'est pas utilisée par le moteur)*

```python
# ── Essieu unique 50 kN ─────────────────────────────────────────
essieu = lipc.Load(intensity=[50.0], length=[0.0], name="Essieu")

# ── Convoi 6 essieux (tandem + tandem) ──────────────────────────
convoi_bc = lipc.Load(
    intensity=[6.0, 12.0, 12.0, 6.0, 12.0, 12.0],  # kN par essieu
    length=[2.25, 4.5, 1.5, 5.0, 4.5, 1.5],  # distances [m]
    name="Convoi BC",
)
```

---

#### Format des Charges Réparties

| Type de charge | `intensity` | `length` |
| :--- | :--- | :--- |
| **Uniforme simple** | `[q]` | `[depart, L_q]` |
| **Uniforme plurielle** | `[q1, q2, ..., qn]` | `[depart, L_q1, L_q2, ..., L_qn]` |

- `depart` : position du début de la charge depuis l'appui gauche $[\text{m}]$ (`= 0` si la charge commence dès l'appui)
- `L_qi` : longueur du $i$-ème tronçon de charge $[\text{m}]$

```python
# ── Charge uniforme 12 kN/m sur 4 m, depuis le début ───────────
udl = lipc.Load(intensity=[12.0], length=[0.0, 4.0], name="UDL")

# ── Charge plurielle : 45 kN/m sur 3 m, puis 10 sur 5 m, puis 25 sur 2 m
udl_plurielle = lipc.Load(
    intensity=[45.0, 10.0, 25.0],
    length=[0.0, 3.0, 5.0, 2.0],
    name="UDL2",
)
```

---

### 3. Application des charges (`set_loads`)

Les charges sont passées au modèle via la méthode `set_loads(point_loads, distrib_loads)` :

```python
out.set_loads(
    point_loads=[essieu, convoi_bc],  # Liste de Load ponctuels / convois
    distrib_loads=[udl, udl_plurielle],  # Liste de Load réparties
)
```



### Plus d'exemples (Python)

Charge ponctuelle seule :

```python
out.set_loads(
    point_loads=[lipc.Load(intensity=[80.0], length=[0.0], name="Essieu")],
    distrib_loads=[],
)
out.export_load_envelopes()
```

Charge répartie seule (ex. poids propre) :

```python
udl = lipc.Load(intensity=[12.0], length=[0.0, 4.0], name="Poids propre")
out.set_loads(point_loads=[], distrib_loads=[udl])
out.export_load_envelopes()
```

`Isostatique` seul, indépendamment de `Output` (une seule travée) :

```python
from Tsaraloha.LIPoutreContinue import Isostatique

travee = Isostatique(E=210e9, I=8e-4, L=10.0, steps=1.0)
print(travee.bending_moment())
```

Recharger des positions de charge déjà exportées :

```python
up = lipc.UpdatePositions(root="/chemin/de/sortie", input_lines=["..."])
up.run()  # compute() + write_all()
```

### Gestion des erreurs

L'API Python (`Output`, `Hyperstatique`, `Isostatique`, `Load`) valide ses
arguments *avant* d'appeler l'extension compilée, sur le modèle des
messages d'erreur de numpy : chaque erreur dit ce qui ne va pas, avec les
valeurs concrètes fournies, et un exemple d'appel correct.

```python
>>> lipc.Output(E=[210e9, 210e9], I=[8e-4], L=[10, 10], steps=1.0)
ValueError: E, I, L doivent décrire le même nombre de travées (même
longueur) ; reçu len(E)=2, len(I)=1, len(L)=2.

>>> lipc.Isostatique(E=210e9, I=8e-4, L=-10.0, steps=1.0)
ValueError: 'L' doit être strictement positif, reçu -10.0.

>>> out.export_load_envelopes()   # sans avoir appelé set_loads() avant
RuntimeError: Output::exportLoadEnvelopes: aucune charge fournie —
appelez setLoads(point_loads, distrib_loads) avant [...]

  Levée par : Output.export_load_envelopes()
  Exemple minimal :
      out.set_loads(
          point_loads=[Load(intensity=[50.0], length=[0.0], name="Camion")],
          distrib_loads=[],
      )
      out.export_load_envelopes()
```

Une faute de frappe sur un nom exposé suggère aussi la bonne orthographe
(même logique que `numpy` depuis sa version 1.25) :

```python
>>> lipc.Outut
AttributeError: module 'Tsaraloha.LIPoutreContinue' has no attribute
'Outut'. Did you mean: 'Output'?
```

Voir les docstrings de `Tsaraloha/LIPoutreContinue/__init__.py` (chaque
classe a une section `Examples`/`Raises`, sur le modèle numpydoc) et
`Tsaraloha/LIPoutreContinue/_validation.py` pour le détail des
vérifications effectuées.

## Usage rapide (C++)

```cpp
#include <LIPoutreContinue/StructuralAnalysis.h>

std::vector<double> E{210e9, 210e9}, I{8e-4, 8e-4}, L{10, 10};
double steps = 1.0;

Output out(E, I, L, steps, "/chemin/de/sortie");
out.compute();                 // résultats en RAM, rien sur disque
// ... utiliser out.BM, out.SF, out.Def, out.Rot, out.X, etc.
out.exportAll();                // optionnel : écrit le JSON
```

## Build depuis les sources

```bash
cmake -B build
cmake --build build --config Release -j
# -> build/python/Tsaraloha/LIPoutreContinue/_LIPoutreContinue<...>.so (Linux/macOS) ou .pyd (Windows)
PYTHONPATH=build/python python3 -c "import Tsaraloha.LIPoutreContinue as lipc; print(lipc.__version__)"
```

Options CMake :

| Option                       | Défaut | Description                              |
|-------------------------------|--------|-------------------------------------------|
| `TSARALOHA_BUILD_PYTHON`      | `ON`   | Construit le(s) module(s) Python pybind11  |
| `TSARALOHA_BUILD_EXAMPLES`    | `OFF`  | Construit `examples/` (si présent)         |

## Structure du dépôt

```
Tsaraloha/
├── CMakeLists.txt                     # build C++ + extension(s) Python
├── pyproject.toml                     # pip install . (scikit-build-core)
├── include/nlohmann/                  # tiers partagé entre sous-librairies
├── LIPoutreContinue/                  # sous-librairie : LIPoutreContinue
│   ├── include/LIPoutreContinue/      # headers publics de la lib C++
│   ├── src/                           # implémentation C++
│   └── bindings/bindings.cpp          # bindings pybind11 (extension privée _LIPoutreContinue)
└── python/Tsaraloha/
    ├── __init__.py                    # racine du namespace Tsaraloha
    └── LIPoutreContinue/               # paquet Python public de la sous-lib
        ├── __init__.py                # (__init__.py, .pyi, py.typed)
        ├── __init__.pyi
        └── py.typed
```

D'autres sous-librairies pourront être ajoutées plus tard en suivant le
même schéma : un dossier `<NomSousLib>/` à la racine + un sous-paquet
`python/Tsaraloha/<NomSousLib>/`.
