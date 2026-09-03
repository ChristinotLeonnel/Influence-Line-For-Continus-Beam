"""
Tsaraloha — librairie chapeau regroupant plusieurs sous-librairies de
calcul, chacune dans son propre sous-paquet.

Sous-librairies disponibles :

    Tsaraloha.LIPoutreContinue
        Calcul de poutres continues (isostatiques et hyperstatiques) :
        lignes d'influence, enveloppes de charge, export JSON.
        Voir Tsaraloha/LIPoutreContinue/__init__.py pour l'API détaillée.

Ce fichier reste volontairement minimal : il ne réexporte rien
automatiquement (chaque sous-librairie a ses propres dépendances
compilées, inutile de toutes les charger si l'appelant n'a besoin que
d'une seule). Importer explicitement le sous-paquet voulu :

    >>> import Tsaraloha.LIPoutreContinue as lipc
    >>> out = lipc.Output(E=[210e9]*2, I=[8e-4]*2, L=[10, 10], steps=1.0)

Lorsqu'une nouvelle sous-librairie sera ajoutée à Tsaraloha, elle
prendra place ici de la même façon, sous
Tsaraloha/<NomDeLaSousLib>/__init__.py.
"""

from __future__ import annotations

__version__ = "1.0.4"

__all__: list[str] = []
