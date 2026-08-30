"""
Tsaraloha.LIPoutreContinue._validation — validations d'entrée et
enrichissement de messages d'erreur, en pur Python, AVANT de traverser la
frontière pybind11.

Pourquoi ce fichier ?
---------------------
L'extension compilée (pybind11) lève des erreurs C++
(``std::invalid_argument``, ``std::runtime_error``, ...) automatiquement
traduites en ``ValueError``/``RuntimeError`` Python, mais avec des messages
parfois très courts (ex. ``"Error: No spans provided!"``) et sans contexte
sur *comment* corriger l'appel.

On reprend ici la philosophie des messages d'erreur de numpy : dire
(1) ce qui ne va pas, (2) avec les valeurs concrètes fournies, et (3) un
exemple minimal montrant l'appel correct. Par exemple, numpy :

    >>> import numpy as np
    >>> np.zeros(3) + np.zeros(4)
    Traceback (most recent call last):
        ...
    ValueError: operands could not be broadcast together with shapes (3,) (4,)

    >>> np.asarray("abc", dtype=float)
    Traceback (most recent call last):
        ...
    ValueError: could not convert string to float: 'abc'

Ce module ne fait AUCUN calcul physique : il se contente de vérifier la
forme/le type des arguments avant de les transmettre à l'extension
compilée, et d'enrichir les exceptions qui en reviennent. Les fonctions
sont volontairement indépendantes de pybind11 pour rester testables sans
avoir besoin de l'extension compilée.
"""

from __future__ import annotations

import numbers
import textwrap
from typing import Any, Sequence

__all__ = [
    "as_float_sequence",
    "check_matching_lengths",
    "check_positive_number",
    "check_type",
    "enrich",
]


def _type_name(value: Any) -> str:
    return type(value).__name__


def check_type(name: str, value: Any, expected: type | tuple[type, ...],
               *, example: str | None = None) -> Any:
    """Vérifie que `value` est bien du (des) type(s) attendu(s)."""
    if not isinstance(value, expected):
        expected_str = (
            expected.__name__ if isinstance(expected, type)
            else " ou ".join(t.__name__ for t in expected)
        )
        hint = f"\nExemple attendu : {name}={example}" if example else ""
        raise TypeError(
            f"'{name}' doit être de type {expected_str}, reçu "
            f"{_type_name(value)!r} : {value!r}.{hint}"
        )
    return value


def as_float_sequence(name: str, value: Any, *, example: str,
                       allow_empty: bool = False) -> list:
    """
    Vérifie que `value` est une séquence (list/tuple) de nombres réels et
    la convertit en ``list[float]``.

    Lève ``TypeError`` si `value` n'est pas une séquence de nombres (ex.
    une chaîne, un scalaire, ou une liste contenant un élément non
    numérique), ``ValueError`` si elle est vide alors qu'au moins un
    élément est requis.

    Exemples de messages produits (mêmes principes que numpy) :

        >>> as_float_sequence("E", "210e9", example="[210e9, 210e9]")
        Traceback (most recent call last):
            ...
        TypeError: 'E' doit être une séquence de nombres (list ou tuple), reçu 'str' : '210e9'.
        Exemple attendu : E=[210e9, 210e9]

        >>> as_float_sequence("E", [210e9, "oops"], example="[210e9, 210e9]")
        Traceback (most recent call last):
            ...
        TypeError: 'E' doit contenir uniquement des nombres (int/float) ; l'élément d'index 1 est de type 'str' : 'oops'.
        Exemple attendu : E=[210e9, 210e9]
    """
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise TypeError(
            f"'{name}' doit être une séquence de nombres (list ou tuple), "
            f"reçu {_type_name(value)!r} : {value!r}.\n"
            f"Exemple attendu : {name}={example}"
        )
    if not allow_empty and len(value) == 0:
        raise ValueError(
            f"'{name}' ne peut pas être vide (au moins une valeur est "
            f"requise — une travée, un point de charge, ...).\n"
            f"Exemple attendu : {name}={example}"
        )
    out: list = []
    for i, item in enumerate(value):
        if isinstance(item, bool) or not isinstance(item, numbers.Real):
            raise TypeError(
                f"'{name}' doit contenir uniquement des nombres (int/float) ; "
                f"l'élément d'index {i} est de type {_type_name(item)!r} : "
                f"{item!r}.\n"
                f"Exemple attendu : {name}={example}"
            )
        out.append(float(item))
    return out


def check_matching_lengths(**named_sequences: Sequence[Any]) -> None:
    """
    Vérifie que toutes les séquences nommées ont la même longueur, sur le
    modèle du message de « broadcast » de numpy quand les tailles ne
    correspondent pas.

        >>> check_matching_lengths(E=[1, 2], I=[1, 2, 3], L=[1, 2])
        Traceback (most recent call last):
            ...
        ValueError: E, I, L doivent décrire le même nombre de travées (même longueur) ; reçu len(E)=2, len(I)=3, len(L)=2.
    """
    lengths = {name: len(seq) for name, seq in named_sequences.items()}
    if len(set(lengths.values())) > 1:
        details = ", ".join(f"len({name})={n}" for name, n in lengths.items())
        raise ValueError(
            f"{', '.join(named_sequences)} doivent décrire le même nombre "
            f"de travées (même longueur) ; reçu {details}."
        )


def check_positive_number(name: str, value: Any) -> float:
    """
    Vérifie que `value` est un nombre réel strictement positif et le
    convertit en ``float``.

        >>> check_positive_number("steps", -1.0)
        Traceback (most recent call last):
            ...
        ValueError: 'steps' doit être strictement positif, reçu -1.0.
    """
    if isinstance(value, bool) or not isinstance(value, numbers.Real):
        raise TypeError(
            f"'{name}' doit être un nombre (int/float), reçu "
            f"{_type_name(value)!r} : {value!r}."
        )
    if value <= 0:
        raise ValueError(f"'{name}' doit être strictement positif, reçu {value!r}.")
    return float(value)


def enrich(exc: Exception, *, where: str, example: str) -> Exception:
    """
    Ré-emballe une exception venant de l'extension compilée en y ajoutant
    du contexte et un exemple minimal, SANS changer son type (une
    ``ValueError`` reste une ``ValueError``, etc.) — même logique que
    numpy, qui conserve toujours ``ValueError``/``TypeError`` mais explicite
    systématiquement la cause dans le message.

    Utilisation typique ::

        try:
            ...
        except RuntimeError as exc:
            raise enrich(exc, where="Output.export_load_envelopes()",
                         example="out.set_loads(...); out.export_load_envelopes()") from exc
    """
    message = (
        f"{exc}\n\n"
        f"  Levée par : {where}\n"
        f"  Exemple minimal :\n{textwrap.indent(example, '      ')}"
    )
    return type(exc)(message)
