#!/usr/bin/env python3
"""
export_output_summary.py — exporte les résultats d'un `Output`
Tsaraloha.LIPoutreContinue dans un fichier JSON, en EXCLUANT les lignes
d'influence (courbes) : BM, SF, Def, Rot, ainsi que les autres données de
type "courbe" qui leur sont associées : shear_force_all_abscissa (X_T),
X (abscisses), node_lengths et support_moment.

Ce qui est conservé dans le JSON :
    - les positions/valeurs critiques : bending_moment_max_positions,
      deflection_max_positions, rotation_max_positions,
      shear_force_max_positions, support_moment_max_positions
    - la géométrie de la poutre : number_of_spans, steps, L_spans,
      E_spans, I_spans, span_node_positions
    - les chemins d'export : paths
    - is_computed

Usage :
    python export_output_summary.py
    python export_output_summary.py --output resultats.json
    python export_output_summary.py --E 210e9 210e9 --I 8e-4 8e-4 --L 10 10 --steps 1.0

Utilisable aussi comme module :

    from export_output_summary import output_to_dict
    import Tsaraloha.LIPoutreContinue as lipc

    out = lipc.Output(E=[210e9, 210e9], I=[8e-4, 8e-4], L=[10, 10], steps=1.0)
    out.compute()
    resume = output_to_dict(out)
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

import Tsaraloha.LIPoutreContinue as lipc

# =============================================================================
#  Attributs "courbe" (lignes d'influence) à exclure du JSON.
# =============================================================================
EXCLUDED_ATTRIBUTES = {
    "BM",                        # moments fléchissants — ligne d'influence
    "SF",                        # efforts tranchants — ligne d'influence
    "Def",                       # flèches — ligne d'influence
    "Rot",                       # rotations — ligne d'influence
    "shear_force_all_abscissa",  # X_T : abscisses SF, donnée "courbe"
    "X",                         # abscisses de tous les noeuds, donnée "courbe"
    "node_lengths",              # abscisses cumulées des appuis, donnée "courbe"
    "support_moment",            # moments sur appuis par itération, donnée "courbe"
}

# Tous les attributs publics exposés par `Output` (voir bindings.cpp),
# dans l'ordre où ils apparaîtront dans le JSON (une fois les exclus retirés).
ALL_OUTPUT_ATTRIBUTES = [
    "BM",
    "SF",
    "Def",
    "Rot",
    "shear_force_all_abscissa",
    "X",
    "node_lengths",
    "bending_moment_max_positions",
    "deflection_max_positions",
    "rotation_max_positions",
    "shear_force_max_positions",
    "support_moment_max_positions",
    "paths",
    "number_of_spans",
    "steps",
    "L_spans",
    "E_spans",
    "I_spans",
    "support_moment",
    "span_node_positions",
    "is_computed",
]

_PROJECT_PATHS_FIELDS = [
    "root", "input", "influence_lines", "critical_values",
    "load_envelopes", "load_positioning",
    "env_global", "env_global_point", "env_global_dist", "env_global_combined",
    "env_critical", "env_critical_point", "env_critical_dist", "env_critical_combined",
    "pos_global", "pos_global_point", "pos_global_dist", "pos_global_combined",
    "pos_critical", "pos_critical_point", "pos_critical_dist", "pos_critical_combined",
]


def _serialize(value: Any) -> Any:
    """
    Convertit récursivement les types pybind11 exposés par Tsaraloha
    (Position1D/2D/3D, ProjectPaths, ...) et les conteneurs Python en
    objets JSON-compatibles (dict/list/str/float/int/bool/None).
    """
    if value is None or isinstance(value, (int, float, str, bool)):
        return value
    if isinstance(value, (list, tuple)):
        return [_serialize(v) for v in value]
    if isinstance(value, dict):
        return {str(k): _serialize(v) for k, v in value.items()}

    if isinstance(value, lipc.ProjectPaths):
        return {f: str(getattr(value, f)) for f in _PROJECT_PATHS_FIELDS}

    # Position1D / Position2D / Position3D : structures simples exposant
    # leurs champs en lecture/écriture — on les détecte par les attributs
    # qu'elles portent plutôt que par isinstance (types C++ non ambigus
    # entre eux, mais on reste défensif).
    for attr_group in (
        ("i", "j", "k", "val"),          # Position3D
        ("i", "j", "val"),               # Position2D
        ("max_position", "value"),       # Position1D
    ):
        if all(hasattr(value, a) for a in attr_group):
            return {a: _serialize(getattr(value, a)) for a in attr_group}

    # Filet de sécurité : tout type non prévu est rendu sous forme texte
    # plutôt que de faire échouer tout l'export.
    return str(value)


def output_to_dict(out: "lipc.Output", *, exclude: set[str] = EXCLUDED_ATTRIBUTES) -> dict:
    """
    Construit un dict JSON-compatible à partir d'un `Output` déjà
    calculé, en excluant les attributs de `exclude` (par défaut : les
    lignes d'influence/courbes — voir EXCLUDED_ATTRIBUTES).

    Parameters
    ----------
    out : Tsaraloha.LIPoutreContinue.Output
        Un Output sur lequel `compute()` a déjà été appelé.
    exclude : set[str], optional
        Noms des attributs à ne pas inclure dans le JSON. Par défaut,
        BM/SF/Def/Rot et les autres données de type courbe.

    Raises
    ------
    RuntimeError
        Si `out.compute()` n'a pas été appelé au préalable.
    """
    if not out.is_computed:
        raise RuntimeError(
            "output_to_dict: l'objet Output n'a pas encore été calculé — "
            "appelez out.compute() avant output_to_dict(out)."
        )

    return {
        name: _serialize(getattr(out, name))
        for name in ALL_OUTPUT_ATTRIBUTES
        if name not in exclude
    }


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Exporte un résumé JSON d'un Output Tsaraloha (sans les lignes d'influence).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--output", "-o", type=Path, default=Path("output_summary.json"),
        help="Chemin du fichier JSON à écrire (défaut : output_summary.json)",
    )
    parser.add_argument("--E", type=float, nargs="+", default=[210e9, 210e9],
                         help="Modules d'élasticité par travée, ex. --E 210e9 210e9")
    parser.add_argument("--I", type=float, nargs="+", default=[8e-4, 8e-4],
                         help="Moments d'inertie par travée, ex. --I 8e-4 8e-4")
    parser.add_argument("--L", type=float, nargs="+", default=[10, 10],
                         help="Longueurs des travées (m), ex. --L 10 10")
    parser.add_argument("--steps", type=float, default=1.0,
                         help="Pas de discrétisation (m), défaut : 1.0")
    parser.add_argument("--root", type=str, default="",
                         help="Dossier racine d'export (optionnel)")
    return parser.parse_args()


def main() -> None:
    args = _parse_args()

    out = lipc.Output(E=args.E, I=args.I, L=args.L, steps=args.steps, root=args.root)
    out.compute()

    data = output_to_dict(out)

    args.output.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")
    print(
        f"Résumé écrit dans {args.output} ({len(data)} champs).\n"
        f"Exclus (lignes d'influence) : {', '.join(sorted(EXCLUDED_ATTRIBUTES))}"
    )


if __name__ == "__main__":
    main()
