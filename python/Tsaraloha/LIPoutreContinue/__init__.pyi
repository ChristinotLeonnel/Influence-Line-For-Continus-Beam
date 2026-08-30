"""Stubs de types pour Tsaraloha.LIPoutreContinue — voir __init__.py pour la documentation."""

from __future__ import annotations
from typing import Dict, List

__version__: str

# =============================================================================
#  Structures de données simples
# =============================================================================

class Load:
    """Une charge (ponctuelle ou répartie). Voir __init__.py pour les
    exemples et les messages d'erreur (Raises: TypeError / ValueError si
    `intensity`/`length` sont incohérents)."""

    intensity: List[float]
    length: List[float]
    name: str
    def __init__(
        self, intensity: List[float], length: List[float], name: str = ...
    ) -> None: ...

class Position1D:
    max_position: int
    value: float
    def __init__(self) -> None: ...

class Position2D:
    i: int
    j: int
    val: float
    def __init__(self) -> None: ...

class Position3D:
    i: int
    j: int
    k: int
    val: float
    def __init__(self) -> None: ...

class CombineLoadPosition:
    max_position: int
    position: float
    value: float
    addition: Dict[str, Dict[str, float]]
    def __init__(self) -> None: ...

class LoadDelivery:
    load: Dict[str, Dict[str, float]]
    span: int
    section: int
    maximum_value: float
    position: float
    def __init__(self) -> None: ...

class CriticalSectionResult:
    point: LoadDelivery
    rect: LoadDelivery
    combined: LoadDelivery

# =============================================================================
#  Isostatique — poutre simple (une seule travée)
# =============================================================================

class Isostatique:
    """Poutre isostatique simple. Raises: TypeError / ValueError si E, I,
    L ou steps ne sont pas des nombres strictement positifs — voir
    __init__.py."""

    E: float
    I: float
    L: float
    steps: float
    a: float
    b: float
    c: float
    node_positions: List[float]
    omega_second: List[float]
    omega_prime: List[float]

    def __init__(self, E: float, I: float, L: float, steps: float) -> None: ...
    def eq_shear_force(self, x: float, return_abscissa: bool) -> List[float]: ...
    def shear_force(self) -> List[List[float]]: ...
    def shear_force_abscissa(self) -> List[List[float]]: ...
    def eq_bending_moment(self, x: float) -> List[float]: ...
    def bending_moment(self) -> List[List[float]]: ...
    def eq_deflection(self, x: float) -> List[float]: ...
    def deflection(self) -> List[List[float]]: ...
    def eq_rotation(self, x: float) -> List[float]: ...
    def rotation(self) -> List[List[float]]: ...

# =============================================================================
#  Hyperstatique — poutre continue multi-travées
# =============================================================================

class Hyperstatique:
    """Poutre continue hyperstatique. Raises: TypeError / ValueError si
    E, I, L n'ont pas la même longueur ou si steps n'est pas strictement
    positif — voir __init__.py."""

    number_of_spans: int
    steps: float
    L_spans: List[float]
    E_spans: List[float]
    I_spans: List[float]
    a_spans: List[float]
    b_spans: List[float]
    c_spans: List[float]
    phy: List[float]
    phy_prime: List[float]
    support_moment: List[List[float]]
    span_node_positions: List[List[float]]
    total_nodes: int

    def __init__(
        self, E: List[float], I: List[float], L: List[float], steps: float
    ) -> None: ...
    def bending_moments(self) -> List[List[List[float]]]: ...
    def rotation(self) -> List[List[List[float]]]: ...
    def shear_force(self, get_all_abscissa: bool = ...) -> List[List[List[float]]]: ...
    def deflection(self) -> List[List[List[float]]]: ...
    def points_x_coordinates(self, positions: List[List[float]]) -> List[float]: ...

# =============================================================================
#  Configuration / Loading
# =============================================================================

class Configuration:
    spans: List[float]
    steps: float
    inertie: List[float]
    young_module: List[float]
    point_loads: List[Load]
    distrib_loads: List[Load]

    def __init__(self) -> None: ...
    def load_from_data(
        self,
        spans: List[float],
        steps: float,
        young_module: List[float],
        inertie: List[float],
        point_loads: List[Load],
        distrib_loads: List[Load],
    ) -> None: ...

class Loading:
    rectangular_load: LoadDelivery
    point_load: LoadDelivery
    combined_load: LoadDelivery
    spans: List[float]
    point_load_inputs: List[Load]
    distrib_load_inputs: List[Load]

    def __init__(
        self,
        curves: List[List[List[float]]],
        position: List[float],
        span_node_positions: List[List[float]],
        spans: List[float],
        point_loads: List[Load],
        distrib_loads: List[Load],
    ) -> None: ...
    def one_point_load(
        self, intensity: float, span: int, section: int, alpha: int
    ) -> float: ...
    def plural_point_load(
        self, intensity: List[float], length: List[float], span: int, section: int
    ) -> Position1D: ...
    def one_rectangular_load(
        self, intensity: float, span: int, section: int, begin: int, end: int
    ) -> float: ...
    def plural_rectangular_load(
        self, intensity: List[float], length: List[float], span: int, section: int
    ) -> Position1D: ...
    def combined_load_at(self, span: int, section: int) -> CombineLoadPosition: ...
    def compute_critical_section(self, span: int) -> CriticalSectionResult: ...

# =============================================================================
#  Chemins d'export / repositionnement des charges
# =============================================================================

class ProjectPaths:
    root: str
    input: str
    influence_lines: str
    critical_values: str
    load_envelopes: str
    load_positioning: str
    env_global: str
    env_global_point: str
    env_global_dist: str
    env_global_combined: str
    env_critical: str
    env_critical_point: str
    env_critical_dist: str
    env_critical_combined: str
    pos_global: str
    pos_global_point: str
    pos_global_dist: str
    pos_global_combined: str
    pos_critical: str
    pos_critical_point: str
    pos_critical_dist: str
    pos_critical_combined: str

    def __init__(self, root: str) -> None: ...
    def create_all(self) -> None: ...

class UpdatePositions:
    def __init__(self, root: str, input_lines: List[str]) -> None: ...
    def compute(self, force: bool = ...) -> None: ...
    @property
    def is_computed(self) -> bool: ...
    @property
    def results(self) -> Dict[str, Dict[str, Dict[str, List[str]]]]: ...
    def write_all(self) -> None: ...
    def run(self) -> None: ...

# =============================================================================
#  Output — point d'entrée principal de la librairie
# =============================================================================

class Output:
    """Point d'entrée principal. Raises: TypeError / ValueError sur des
    E/I/L/steps invalides (constructeur) ; RuntimeError enrichi par
    export_load_envelopes()/export_all() si set_loads() n'a pas été
    appelé au préalable — voir __init__.py pour les exemples complets."""

    BM: List[List[List[float]]]
    SF: List[List[List[float]]]
    Def: List[List[List[float]]]
    Rot: List[List[List[float]]]
    shear_force_all_abscissa: List[List[List[float]]]
    X: List[float]
    node_lengths: List[float]
    bending_moment_max_positions: Position3D
    deflection_max_positions: Position3D
    rotation_max_positions: Position3D
    shear_force_max_positions: Position3D
    support_moment_max_positions: Position2D
    paths: ProjectPaths
    number_of_spans: int
    steps: float
    L_spans: List[float]
    E_spans: List[float]
    I_spans: List[float]
    support_moment: List[List[float]]
    span_node_positions: List[List[float]]

    def __init__(
        self,
        E: List[float],
        I: List[float],
        L: List[float],
        steps: float,
        root: str = ...,
    ) -> None: ...
    def compute(self, force: bool = ...) -> None: ...
    @property
    def is_computed(self) -> bool: ...
    def export_critical_values(self) -> None: ...
    def export_influence_lines(self) -> None: ...
    def export_load_envelopes(self) -> None: ...
    def export_all(self) -> None: ...
    def set_loads(
        self, point_loads: List[Load], distrib_loads: List[Load]
    ) -> None: ...
