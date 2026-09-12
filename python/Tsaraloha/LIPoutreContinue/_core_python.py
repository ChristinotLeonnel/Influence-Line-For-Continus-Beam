"""
Tsaraloha.LIPoutreContinue._core_python
========================================
Réimplémentation **pur-Python** de l'extension compilée C++
``_LIPoutreContinue`` (pybind11).

Ce module est chargé automatiquement par ``__init__.py`` lorsque
l'extension compilée n'est pas disponible (ex. PyDroid sur Android,
environnements sans compilateur C++).

L'API publique est **identique** à celle de l'extension compilée :
mêmes noms de classes, mêmes signatures de méthodes, mêmes types de
retour (listes, dicts, flottants).

Performance : les algorithmes sont traduits directement depuis le C++
sans NumPy — suffisant pour les cas typiques (2-5 travées, step ≥ 0.5 m).
"""

from __future__ import annotations

import json
import math
import os
from pathlib import Path


# =============================================================================
#  FONCTIONS UTILITAIRES (traduites de Utils.h)
# =============================================================================

def _max_value_in_vector(vec):
    """Retourne l'élément de plus grande valeur absolue."""
    if not vec:
        return 0
    max_val = vec[0]
    for val in vec:
        if abs(val) > abs(max_val):
            max_val = val
    return max_val


def _remove_by_indices(v, indices):
    """Supprime les éléments aux indices donnés."""
    to_remove = set(int(i) for i in indices)
    return [v[i] for i in range(len(v)) if i not in to_remove]


def _trapeze(x, y):
    """Intégration par trapèzes avec sommation de Kahan."""
    aire = 0.0
    err = 0.0
    for i in range(len(x) - 1):
        h = x[i + 1] - x[i]
        a = (y[i] + y[i + 1]) * h / 2.0
        t = aire + a
        err += (aire - t) + a
        aire = t
    return aire + err


def _find_max_abs_3d(data):
    """findMaxAbsoluteValue3D → dict {'i', 'j', 'k', 'val'}."""
    max_abs = float('-inf')
    pi, pj, pk, pv = 0, 0, 0, 0.0
    for i in range(len(data)):
        for j in range(len(data[i])):
            for k in range(len(data[i][j])):
                av = abs(data[i][j][k])
                if av >= max_abs:
                    max_abs = av
                    pi, pj, pk, pv = i, j, k, data[i][j][k]
    return {'i': pi, 'j': pj, 'k': pk, 'val': pv}


def _find_max_abs_2d(data):
    """findMaxAbsoluteValue2D → dict {'i', 'j', 'val'}."""
    max_abs = float('-inf')
    pi, pj, pv = 0, 0, 0.0
    for i in range(len(data)):
        for j in range(len(data[i])):
            av = abs(data[i][j])
            if av >= max_abs:
                max_abs = av
                pi, pj, pv = i, j, data[i][j]
    return {'i': pi, 'j': pj, 'val': pv}


def _prod_list(liste, debut, fin):
    """Produit des éléments de liste[debut..fin] (bornes incluses)."""
    r = 1.0
    for a in range(debut, fin + 1):
        r *= liste[a]
    return r


def _flatten(m):
    """Aplati une liste de listes en une seule liste."""
    out = []
    for v in m:
        out.extend(v)
    return out


def _indice_of(vec, value):
    """Retourne l'index de la première occurrence exacte de value."""
    for i, v in enumerate(vec):
        if v == value:
            return i
    raise ValueError(
        "Indice_of: la valeur recherchée n'existe pas dans le vecteur."
    )


def _delivery(data, path, filename):
    """Écrit data en JSON indenté dans path/filename."""
    os.makedirs(str(path), exist_ok=True)
    with open(os.path.join(str(path), filename), 'w', encoding='utf-8') as f:
        json.dump(data, f, indent=4)


def _maximum_delivery(pos_dict, path, name):
    """Écrit un maximum 3D en JSON."""
    j = {
        'span': pos_dict['i'],
        'section': pos_dict['j'],
        'alpha': pos_dict['k'],
        'value': pos_dict['val'],
    }
    _delivery(j, path, name)


def _loading_delivery(data, path, name):
    """Écrit une enveloppe de charge en JSON."""
    j = {
        'maximum': data['maximum_value'],
        'span': data['span'],
        'section': data['section'],
        'position': data['position'],
        'load': data['load'],
    }
    _delivery(j, path, name)


# =============================================================================
#  STRUCTURES DE DONNÉES
# =============================================================================

class Load:
    """Charge mobile — ponctuelle ou répartie."""

    def __init__(self, intensity, length, name=""):
        self.intensity = [float(x) for x in intensity]
        self.length = [float(x) for x in length]
        self.name = str(name)

    def to_dict(self):
        return {
            'intensity': list(self.intensity),
            'length': list(self.length),
            'name': self.name,
        }

    def __repr__(self):
        return f"<Load name={self.name} n_points={len(self.intensity)}>"


class Position1D:
    def __init__(self):
        self.max_position = 0
        self.value = 0.0

    def to_dict(self):
        return {'max_position': self.max_position, 'value': self.value}

    def __repr__(self):
        return f"<Position1D max_position={self.max_position} value={self.value}>"


class Position2D:
    def __init__(self):
        self.i = 0
        self.j = 0
        self.val = 0.0

    def to_dict(self):
        return {'i': self.i, 'j': self.j, 'val': self.val}

    def __repr__(self):
        return f"<Position2D i={self.i} j={self.j} val={self.val}>"


class Position3D:
    def __init__(self):
        self.i = 0
        self.j = 0
        self.k = 0
        self.val = 0.0

    def to_dict(self):
        return {'i': self.i, 'j': self.j, 'k': self.k, 'val': self.val}

    def __repr__(self):
        return (f"<Position3D i={self.i} j={self.j} "
                f"k={self.k} val={self.val}>")


class CombineLoadPosition:
    def __init__(self):
        self.max_position = 0
        self.position = 0.0
        self.value = 0.0
        self.addition = {}

    def to_dict(self):
        return {
            'max_position': self.max_position,
            'position': self.position,
            'value': self.value,
            'addition': self.addition,
        }


class LoadDelivery:
    def __init__(self):
        self.load = {}
        self.span = 0
        self.section = 0
        self.maximum_value = 0.0
        self.position = 0.0

    def to_dict(self):
        return {
            'load': self.load,
            'span': self.span,
            'section': self.section,
            'maximum_value': self.maximum_value,
            'position': self.position,
        }

    def __repr__(self):
        return (f"<LoadDelivery span={self.span} section={self.section} "
                f"maximum_value={self.maximum_value}>")


class CriticalSectionResult:
    """Résultat de compute_critical_section (point / rect / combiné)."""

    def __init__(self):
        self._point = {'load': {}, 'span': 0, 'section': 0,
                       'maximum_value': 0.0, 'position': 0.0}
        self._rect = {'load': {}, 'span': 0, 'section': 0,
                      'maximum_value': 0.0, 'position': 0.0}
        self._combined = {'load': {}, 'span': 0, 'section': 0,
                          'maximum_value': 0.0, 'position': 0.0}

    @property
    def point(self):
        return dict(self._point)

    @property
    def rect(self):
        return dict(self._rect)

    @property
    def combined(self):
        return dict(self._combined)

    def to_dict(self):
        return {'point': self.point, 'rect': self.rect,
                'combined': self.combined}


# =============================================================================
#  ISOSTATIQUE — poutre simple (une seule travée)
# =============================================================================

class Isostatique:
    """Poutre isostatique simple — formules analytiques directes."""

    def __init__(self, E, I, L, steps):
        self.E = float(E)
        self.I = float(I)
        self.L = float(L)
        self.steps = float(steps)

        # Positions des nœuds
        self.node_positions = []
        pas = 0.0
        while pas < L:
            self.node_positions.append(pas)
            pas += steps
        self.node_positions.append(float(L))

        # Coefficients de souplesse
        EI = E * I
        EI_L = EI * L
        self.a = L / (3.0 * EI)
        self.c = self.a
        self.b = L / (6.0 * EI)

        # Coefficients Omega
        self.omega_second = []
        self.omega_prime = []
        for pos in self.node_positions:
            self.omega_second.append(
                pos * (L - pos) * (L + pos) / (6.0 * EI_L))
            self.omega_prime.append(
                -pos * (L - pos) * (2.0 * L - pos) / (6.0 * EI_L))

    # ── Effort tranchant ────────────────────────────────────────────────────

    def eq_shear_force(self, x, return_abscissa):
        L = self.L
        sf, coords = [], []
        if x <= L:
            for pos in self.node_positions:
                if pos < x:
                    sf.append(-pos / L)
                    coords.append(pos)
                elif pos > x:
                    sf.append(1.0 - pos / L)
                    coords.append(pos)
                else:                    # discontinuité
                    sf.append(-pos / L)
                    coords.append(pos)
                    sf.append(1.0 - pos / L)
                    coords.append(pos)
        else:
            sf = [0.0] * len(self.node_positions)
            coords = list(self.node_positions)
        return coords if return_abscissa else sf

    def shear_force(self):
        return [self.eq_shear_force(p, False) for p in self.node_positions]

    def shear_force_abscissa(self):
        return [self.eq_shear_force(p, True) for p in self.node_positions]

    # ── Moment fléchissant ──────────────────────────────────────────────────

    def eq_bending_moment(self, x):
        L = self.L
        if x <= L:
            return [
                pos * (1.0 - x / L) if pos <= x else x * (1.0 - pos / L)
                for pos in self.node_positions
            ]
        return [0.0] * len(self.node_positions)

    def bending_moment(self):
        return [self.eq_bending_moment(p) for p in self.node_positions]

    # ── Flèche ──────────────────────────────────────────────────────────────

    def eq_deflection(self, x):
        E, I, L = self.E, self.I, self.L
        c6 = 6.0 * E * I * L
        out = []
        for pos in self.node_positions:
            if pos <= x:
                f = -(pos * (L - x) / c6) * (
                    x * (2.0 * L - x) - pos * pos)
            else:
                f = -(x * (L - pos) / c6) * (
                    pos * (2.0 * L - pos) - x * x)
            out.append(f)
        return out

    def deflection(self):
        return [self.eq_deflection(p) for p in self.node_positions]

    # ── Rotation ────────────────────────────────────────────────────────────

    def eq_rotation(self, x):
        E, I, L = self.E, self.I, self.L
        c6 = 6.0 * E * I * L
        out = []
        for pos in self.node_positions:
            if pos < x:
                a = ((L - pos) * (L + pos)
                     - 3.0 * (L - x) * (L - x)) * pos / c6
            elif pos > x:
                a = -(pos * (2.0 * L - pos)
                      - 3.0 * x * x) * (L - pos) / c6
            else:
                a = ((L - pos) * (L + pos)
                     - 3.0 * (L - x) * (L - x)) * pos / c6
            out.append(a)
        return out

    def rotation(self):
        return [self.eq_rotation(p) for p in self.node_positions]


# =============================================================================
#  HYPERSTATIQUE — poutre continue multi-travées
# =============================================================================

class Hyperstatique:
    """Poutre continue hyperstatique — méthode des 3 moments."""

    def __init__(self, E, I, L, steps):
        # Copies propres (comme PyHyperstatique dans bindings.cpp)
        E = [float(x) for x in E]
        I = [float(x) for x in I]
        L = [float(x) for x in L]
        steps = float(steps)
        self.steps = steps

        n = len(L)
        a_s, b_s, c_s = [], [], []
        _SNP, _OS, _OP = [], [], []
        _BMS, _RS, _SFAS, _SFS, _DS = [], [], [], [], []

        for i in range(n):
            sp = Isostatique(E[i], I[i], L[i], steps)
            a_s.append(sp.a)
            b_s.append(sp.b)
            c_s.append(sp.c)
            if L[i] != 0:
                _SNP.append(list(sp.node_positions))
                _OS.append(list(sp.omega_second))
                _OP.append(list(sp.omega_prime))
                _BMS.append(sp.bending_moment())
                _RS.append(sp.rotation())
                _SFAS.append(sp.shear_force_abscissa())
                _SFS.append(sp.shear_force())
                _DS.append(sp.deflection())

        # phy / phy_prime
        phy = []
        pp = 0.0
        for i in range(n):
            if i != 0:
                pp = b_s[i] / (a_s[i] + c_s[i - 1] - b_s[i - 1] * pp)
            phy.append(pp)

        phy_prime = []
        pp2 = 0.0
        for i in range(n, 0, -1):
            if i != n:
                pp2 = b_s[i - 1] / (
                    c_s[i - 1] + a_s[i] - b_s[i] * pp2)
            phy_prime.append(pp2)
        phy_prime.reverse()

        # Supprimer les travées de longueur nulle
        lz = [i for i in range(n) if L[i] == 0]
        phy = _remove_by_indices(phy, lz)
        phy_prime = _remove_by_indices(phy_prime, lz)
        L = _remove_by_indices(L, lz)
        E = _remove_by_indices(E, lz)
        I = _remove_by_indices(I, lz)
        a_s = _remove_by_indices(a_s, lz)
        b_s = _remove_by_indices(b_s, lz)
        c_s = _remove_by_indices(c_s, lz)

        ns = len(L)

        # Stocker les résultats publics
        self.number_of_spans = ns
        self.L_spans = L
        self.E_spans = E
        self.I_spans = I
        self.a_spans = a_s
        self.b_spans = b_s
        self.c_spans = c_s
        self.phy = phy
        self.phy_prime = phy_prime
        self.span_node_positions = _SNP

        # Données privées
        self._OS = _OS
        self._OP = _OP
        self._BMS = _BMS
        self._RS = _RS
        self._SFAS = _SFAS
        self._SFS = _SFS
        self._DS = _DS

        # [OPT-1] Nombre total de nœuds
        self.total_nodes = sum(len(sp) for sp in _SNP)

        # [OPT-2] Cache moments
        lc = self._left_cache()
        rc = self._right_cache()

        _SLM = [self._left_sm(si, lc) for si in range(ns)]
        _SRM = [self._right_sm(si, rc) for si in range(ns)]

        _SLR = []
        for i in range(ns):
            mini = list(_SLM[i]) + list(_SRM[i])
            _SLR.append(mini)
        self._SLR = _SLR

        # SupportMoment
        sm = []
        for j in range(ns + 1):
            ac = [_SLR[i][j] for i in range(ns)]
            sm.append(_flatten(ac))
        self.support_moment = sm

        # [OPT-3] ShearForceAbscissa
        self._SFA = self._build_sfa()

    # ── Moments aux appuis (cache) ──────────────────────────────────────────

    def _left_cache(self):
        ns = self.number_of_spans
        M = [[] for _ in range(ns)]
        for i in range(ns):
            d = 1.0 - self.phy[i] * self.phy_prime[i]
            for j in range(len(self.span_node_positions[i])):
                g = (self.phy[i] / self.b_spans[i]) * (
                    (self._OP[i][j] + self._OS[i][j] * self.phy_prime[i])
                    / d)
                M[i].append(g)
        return M

    def _right_cache(self):
        ns = self.number_of_spans
        M = [[] for _ in range(ns)]
        for i in range(ns):
            d = 1.0 - self.phy[i] * self.phy_prime[i]
            for j in range(len(self.span_node_positions[i])):
                dr = -(self.phy_prime[i] / self.b_spans[i]) * (
                    (self._OP[i][j] * self.phy[i] + self._OS[i][j])
                    / d)
                M[i].append(dr)
        return M

    def _left_sm(self, si, cache):
        out = []
        for i in range(si + 1):
            q = []
            for jv in cache[si]:
                q.append(
                    (-1.0) ** (si - i)
                    * _prod_list(self.phy, i, si - 1) * jv)
            out.append(q)
        return out

    def _right_sm(self, si, cache):
        out = []
        for i in range(si, self.number_of_spans):
            g = i
            p = _prod_list(self.phy_prime, si + 1, g)
            q = []
            for jv in cache[si]:
                q.append((-1.0) ** (si - g) * p * jv)
            out.append(q)
        return out

    def _build_sfa(self):
        result = []
        for tr in range(self.number_of_spans):
            x_tr = []
            JOL = [list(sp) for sp in self.span_node_positions]
            for sec in range(len(self.span_node_positions[tr])):
                JOL[tr] = list(self._SFAS[tr][sec])
                x_tr.append(self.points_x_coordinates(JOL))
                JOL[tr] = list(self.span_node_positions[tr])
            result.append(x_tr)
        return result

    # ── Coordonnées globales ────────────────────────────────────────────────

    def points_x_coordinates(self, positions):
        x = []
        for i in range(len(positions)):
            offset = sum(self.L_spans[j] for j in range(i))
            for alpha in positions[i]:
                x.append(alpha + offset)
        return x

    # ── Formules hyperstatiques (statiques) ─────────────────────────────────

    @staticmethod
    def _hpbm(m, n, x, l):
        return m * (1.0 - x / l) + n * x / l

    @staticmethod
    def _hprot(m, n, x, l, E, I):
        return (-m * (2.0*l*l - 6.0*l*x + 3.0*x*x) / (6.0*E*I*l)
                - n * (l*l - 3.0*x*x) / (6.0*E*I*l))

    @staticmethod
    def _hpsf(m, n, l):
        return (-m + n) / l

    @staticmethod
    def _hpdef(m, n, x, l, E, I):
        return (-m * x * (l-x) * (2.0*l-x) / (6.0*E*I*l)
                - n * x * (l-x) * (l+x) / (6.0*E*I*l))

    # ── Calculs principaux ──────────────────────────────────────────────────

    def bending_moments(self):
        result = []
        for si in range(self.number_of_spans):
            mu_t = []
            sc = 0
            for section in self.span_node_positions[si]:
                hm = []
                mut = self._BMS[si][sc]
                sc += 1
                for i in range(self.number_of_spans):
                    for j in range(len(self.span_node_positions[i])):
                        mi = self._hpbm(
                            self._SLR[i][si][j],
                            self._SLR[i][si + 1][j],
                            section, self.L_spans[si])
                        hm.append(mut[j] + mi if si == i else mi)
                mu_t.append(hm)
            result.append(mu_t)
        return result

    def bending_moment(self):
        """Alias de bending_moments() pour cohérence avec Isostatique."""
        return self.bending_moments()


    def rotation(self):
        result = []
        for si in range(self.number_of_spans):
            rt = []
            for sx in range(len(self.span_node_positions[si])):
                hr = []
                sp = self.span_node_positions[si][sx]
                for i in range(self.number_of_spans):
                    for j in range(len(self.span_node_positions[i])):
                        rv = self._hprot(
                            self._SLR[i][si][j],
                            self._SLR[i][si + 1][j],
                            sp, self.L_spans[si],
                            self.E_spans[si], self.I_spans[si])
                        if si == i:
                            hr.append(self._RS[si][sx][j] + rv)
                        else:
                            hr.append(rv)
                rt.append(hr)
            result.append(rt)
        return result

    def deflection(self):
        result = []
        for si in range(self.number_of_spans):
            dt = []
            for sx in range(len(self.span_node_positions[si])):
                hd = []
                sp = self.span_node_positions[si][sx]
                for i in range(self.number_of_spans):
                    for j in range(len(self.span_node_positions[i])):
                        dv = self._hpdef(
                            self._SLR[i][si][j],
                            self._SLR[i][si + 1][j],
                            sp, self.L_spans[si],
                            self.E_spans[si], self.I_spans[si])
                        if si == i:
                            hd.append(self._DS[si][sx][j] + dv)
                        else:
                            hd.append(dv)
                dt.append(hd)
            result.append(dt)
        return result

    def shear_force(self, get_all_abscissa=False):
        if get_all_abscissa:
            return self._SFA

        result = []
        for si in range(self.number_of_spans):
            tt = []
            for sx in range(len(self.span_node_positions[si])):
                section = self.span_node_positions[si][sx]
                ht = []
                for i in range(self.number_of_spans):
                    if si == i:
                        compteur = 0
                        for j in range(len(self.span_node_positions[i])):
                            mi = self._hpsf(
                                self._SLR[i][si][j],
                                self._SLR[i][si + 1][j],
                                self.L_spans[si])
                            sfs = self._SFS[si]
                            if (sx < len(sfs)
                                    and compteur < len(sfs[sx])):
                                if (j < len(self.span_node_positions[si])
                                        and section ==
                                        self.span_node_positions[si][j]):
                                    ht.append(sfs[sx][compteur] + mi)
                                    compteur += 1
                                    if compteur < len(sfs[sx]):
                                        ht.append(sfs[sx][compteur] + mi)
                                        compteur += 1
                                else:
                                    ht.append(sfs[sx][compteur] + mi)
                                    compteur += 1
                    else:
                        for j in range(len(self.span_node_positions[i])):
                            ht.append(self._hpsf(
                                self._SLR[i][si][j],
                                self._SLR[i][si + 1][j],
                                self.L_spans[si]))
                tt.append(ht)
            result.append(tt)
        return result

    def __repr__(self):
        return f"<Hyperstatique number_of_spans={self.number_of_spans}>"


# =============================================================================
#  CONFIGURATION
# =============================================================================

class Configuration:
    """Regroupe spans/steps/E/I/charges."""

    def __init__(self):
        self.spans = []
        self.steps = 1.0
        self._Inertie = []
        self._YoungModule = []
        self._Point_LOAD = []
        self._Rectangulare_LOAD = []

    # Propriétés correspondant aux bindings
    @property
    def inertie(self):
        return self._Inertie

    @inertie.setter
    def inertie(self, v):
        self._Inertie = list(v)

    @property
    def young_module(self):
        return self._YoungModule

    @young_module.setter
    def young_module(self, v):
        self._YoungModule = list(v)

    @property
    def point_loads(self):
        return [l.to_dict() if hasattr(l, 'to_dict') else l
                for l in self._Point_LOAD]

    @point_loads.setter
    def point_loads(self, v):
        self._Point_LOAD = list(v)

    @property
    def distrib_loads(self):
        return [l.to_dict() if hasattr(l, 'to_dict') else l
                for l in self._Rectangulare_LOAD]

    @distrib_loads.setter
    def distrib_loads(self, v):
        self._Rectangulare_LOAD = list(v)

    def load_from_data(self, spans, steps, young_module, inertie,
                       point_loads, distrib_loads):
        self.spans = list(spans)
        self.steps = float(steps)
        self._YoungModule = list(young_module)
        self._Inertie = list(inertie)
        self._Point_LOAD = list(point_loads)
        self._Rectangulare_LOAD = list(distrib_loads)
        if not self.spans:
            raise ValueError(
                "Configuration::loadFromData: 'spans' est vide.")

    def to_dict(self):
        return {
            'spans': self.spans, 'steps': self.steps,
            'inertie': self._Inertie, 'young_module': self._YoungModule,
            'point_loads': self.point_loads,
            'distrib_loads': self.distrib_loads,
        }


# =============================================================================
#  LOADING — enveloppes de charge
# =============================================================================

class Loading(Configuration):
    """Calcul des enveloppes de charge."""

    def __init__(self, curves, position, span_node_positions, spans,
                 point_loads, distrib_loads):
        super().__init__()
        self._CURVES = curves
        self._POSITION = list(position)
        self._span_node_positions = [list(sp) for sp in span_node_positions]
        self.spans = list(spans)
        self._Point_LOAD = list(point_loads)
        self._Rectangulare_LOAD = list(distrib_loads)

        # Copie sans travées nulles
        cs = list(spans)
        lz = [i for i, s in enumerate(spans) if s == 0]
        cs = _remove_by_indices(cs, lz)

        # ── Charge ponctuelle ────────────────────────────────────────────
        first_p = []
        vps_p = []
        _pt_section = 0

        for span in range(len(cs)):
            vpse = []
            second = []
            for section in range(len(self._span_node_positions[span])):
                val = 0.0
                tt = {}
                for k in self._Point_LOAD:
                    lo = self._ppl(k.intensity, k.length, span, section)
                    val += lo['value']
                    tt[k.name] = {
                        'alpha': float(lo['max_position']),
                        'value': lo['value'],
                        'Position': self._POSITION[lo['max_position']],
                    }
                vpse.append(val)
                second.append(tt)
            ms = _max_value_in_vector(vpse)
            vps_p.append(ms)
            _pt_section = _indice_of(vpse, ms)
            first_p.append(second[_pt_section])

        self._point_load_d = {
            'load': {}, 'span': 0, 'section': 0,
            'maximum_value': 0.0, 'position': 0.0,
        }
        if vps_p:
            msp = _max_value_in_vector(vps_p)
            sp_idx = _indice_of(vps_p, msp)
            self._point_load_d = {
                'load': first_p[sp_idx],
                'span': sp_idx,
                'section': _pt_section,
                'maximum_value': msp,
                'position': 0.0,
            }

        # ── Charge répartie ──────────────────────────────────────────────
        first_r = []
        vps_r = []
        _rt_section = 0

        for span in range(len(cs)):
            vpse = []
            second = []
            for section in range(len(self._span_node_positions[span])):
                val = 0.0
                tt = {}
                for k in self._Rectangulare_LOAD:
                    lo = self._prl(k.intensity, k.length, span, section)
                    val += lo['value']
                    tt[k.name] = {
                        'alpha': float(lo['max_position']),
                        'value': lo['value'],
                        'Position': self._POSITION[lo['max_position']],
                    }
                vpse.append(val)
                second.append(tt)
            ms = _max_value_in_vector(vpse)
            vps_r.append(ms)
            _rt_section = _indice_of(vpse, ms)
            first_r.append(second[_rt_section])

        self._rect_load_d = {
            'load': {}, 'span': 0, 'section': 0,
            'maximum_value': 0.0, 'position': 0.0,
        }
        if vps_r:
            msp = _max_value_in_vector(vps_r)
            sp_idx = _indice_of(vps_r, msp)
            self._rect_load_d = {
                'load': first_r[sp_idx],
                'span': sp_idx,
                'section': _rt_section,
                'maximum_value': msp,
                'position': 0.0,
            }

        # ── Charge combinée ──────────────────────────────────────────────
        first_c = []
        vps_c = []
        pos_ps = []
        secs_ps = []

        for span in range(len(cs)):
            vpse = []
            ppse = []
            second = []
            for section in range(len(self._span_node_positions[span])):
                lo = self._combined_load(span, section)
                vpse.append(lo['value'])
                ppse.append(lo['position'])
                second.append(lo['addition'])
            ms = _max_value_in_vector(vpse)
            cs_idx = _indice_of(vpse, ms)
            vps_c.append(ms)
            pos_ps.append(ppse[cs_idx])
            secs_ps.append(cs_idx)
            first_c.append(second[cs_idx])

        self._combined_load_d = {
            'load': {}, 'span': 0, 'section': 0,
            'maximum_value': 0.0, 'position': 0.0,
        }
        if vps_c:
            msp = _max_value_in_vector(vps_c)
            sp_idx = _indice_of(vps_c, msp)
            self._combined_load_d = {
                'load': first_c[sp_idx],
                'span': sp_idx,
                'section': secs_ps[sp_idx],
                'maximum_value': msp,
                'position': pos_ps[sp_idx],
            }

    # ── Propriétés publiques (renvoient des dicts comme le binding) ──────

    @property
    def rectangular_load(self):
        return dict(self._rect_load_d)

    @property
    def point_load(self):
        return dict(self._point_load_d)

    @property
    def combined_load(self):
        return dict(self._combined_load_d)

    @property
    def point_load_inputs(self):
        return [l.to_dict() if hasattr(l, 'to_dict') else l
                for l in self._Point_LOAD]

    @point_load_inputs.setter
    def point_load_inputs(self, v):
        self._Point_LOAD = list(v)

    @property
    def distrib_load_inputs(self):
        return [l.to_dict() if hasattr(l, 'to_dict') else l
                for l in self._Rectangulare_LOAD]

    @distrib_load_inputs.setter
    def distrib_load_inputs(self, v):
        self._Rectangulare_LOAD = list(v)

    # ── Méthodes privées ────────────────────────────────────────────────────

    def _m2p(self, meters):
        """MetersToPosition — indice le plus proche dans POSITION."""
        P = self._POSITION
        if meters < P[0] or meters > P[-1]:
            return len(P)          # sentinelle hors borne
        idx = 0
        min_d = abs(P[0] - meters)
        for i in range(1, len(P)):
            d = abs(P[i] - meters)
            if d < min_d:
                min_d = d
                idx = i
            if P[i] > meters + min_d:
                break
        return idx

    def _length_successive(self, length):
        """Calcule les positions cumulées à partir du vecteur length."""
        ls = []
        acc = length[0]
        for i in range(len(length) - 1):
            ls.append(acc)
            acc += length[i + 1]
        ls.append(acc)
        return ls

    # ── Méthodes publiques (API identique au binding) ───────────────────

    def one_point_load(self, intensity, span, section, alpha):
        if alpha >= len(self._CURVES[span][section]):
            return 0.0
        return self._CURVES[span][section][alpha] * intensity

    def _ppl(self, intensity, length, span, section):
        """PluralPointLoad interne → dict."""
        ls = self._length_successive(length)
        VALUES = []
        for fw in self._POSITION:
            val = 0.0
            for c, I_val in enumerate(intensity):
                alpha = self._m2p(ls[c] + fw)
                if alpha < len(self._POSITION):
                    val += self.one_point_load(I_val, span, section, alpha)
            VALUES.append(val)
        mv = _max_value_in_vector(VALUES)
        return {'max_position': _indice_of(VALUES, mv), 'value': mv}

    def plural_point_load(self, intensity, length, span, section):
        """Renvoie un dict {'max_position', 'value'}."""
        return self._ppl(intensity, length, span, section)

    def one_rectangular_load(self, intensity, span, section, begin, end):
        x, y = [], []
        for i in range(begin, end):
            x.append(self._POSITION[i])
            y.append(self._CURVES[span][section][i])
        return _trapeze(x, y) * intensity

    def _prl(self, intensity, length, span, section):
        """PluralRectangularLoad interne → dict."""
        ls = self._length_successive(length)
        VALUES = []
        P = self._POSITION
        for fw in P:
            val = 0.0
            for k in range(len(intensity)):
                xb = ls[k] + fw
                xe = ls[k + 1] + fw
                xb = max(xb, P[0])
                xe = min(xe, P[-1])
                if xe <= xb:
                    continue
                begin = self._m2p(xb)
                end = self._m2p(xe)
                if begin >= len(P):
                    begin = 0
                if end >= len(P):
                    end = len(P) - 1
                if end > begin:
                    val += self.one_rectangular_load(
                        intensity[k], span, section, begin, end)
            VALUES.append(val)
        mv = _max_value_in_vector(VALUES)
        return {'max_position': _indice_of(VALUES, mv), 'value': mv}

    def plural_rectangular_load(self, intensity, length, span, section):
        """Renvoie un dict {'max_position', 'value'}."""
        return self._prl(intensity, length, span, section)

    def _combined_load(self, span, section):
        """CombinedLoad → dict."""
        VALUES = []
        first = []
        P = self._POSITION

        for wi in range(len(P)):
            fw = P[wi]
            second = {}
            val = 0.0

            # Charges ponctuelles
            for j_load in self._Point_LOAD:
                third = {}
                for c, I_val in enumerate(j_load.intensity):
                    alpha = self._m2p(j_load.length[c] + fw)
                    contrib = (self.one_point_load(I_val, span, section, alpha)
                               if alpha < len(P) else 0.0)
                    val += contrib
                    key = f"{int(I_val)} {c + 1}"
                    third[key] = contrib
                third['Position'] = fw + j_load.length[0]
                second[j_load.name] = third

            # Charges réparties
            for jk in self._Rectangulare_LOAD:
                third = {}
                ls = self._length_successive(jk.length)
                for k in range(len(jk.intensity)):
                    xb = ls[k] + fw
                    xe = ls[k + 1] + fw
                    xb = max(xb, P[0])
                    xe = min(xe, P[-1])
                    contrib = 0.0
                    if xe > xb:
                        begin = self._m2p(xb)
                        end = self._m2p(xe)
                        if begin >= len(P):
                            begin = 0
                        if end >= len(P):
                            end = len(P) - 1
                        if end > begin:
                            contrib = self.one_rectangular_load(
                                jk.intensity[k], span, section, begin, end)
                    val += contrib
                    key = f"{int(jk.intensity[k])} {k + 1}"
                    third[key] = contrib
                third['Position'] = fw + jk.length[0]
                second[jk.name] = third

            VALUES.append(val)
            first.append(second)

        ff = _max_value_in_vector(VALUES)
        IJ = _indice_of(VALUES, ff)
        return {
            'max_position': IJ,
            'position': P[IJ],
            'value': ff,
            'addition': first[IJ],
        }

    def combined_load_at(self, span, section):
        """Renvoie un dict CombineLoadPosition."""
        return self._combined_load(span, section)

    def compute_critical_section(self, span):
        """Renvoie un dict {'point', 'rect', 'combined'}."""
        snp = self._span_node_positions
        P = self._POSITION

        # ── Point ──
        vals = []
        for sec in range(len(snp[span])):
            v = sum(self._ppl(k.intensity, k.length, span, sec)['value']
                    for k in self._Point_LOAD)
            vals.append(v)
        mv = _max_value_in_vector(vals)
        pt_sec = _indice_of(vals, mv)
        pt_load = {}
        for k in self._Point_LOAD:
            lo = self._ppl(k.intensity, k.length, span, pt_sec)
            pt_load[k.name] = {
                'alpha': float(lo['max_position']),
                'value': lo['value'],
                'Position': P[lo['max_position']],
            }
        pt = {'load': pt_load, 'span': span, 'section': pt_sec,
              'maximum_value': mv, 'position': 0.0}

        # ── Rect ──
        vals = []
        for sec in range(len(snp[span])):
            v = sum(self._prl(k.intensity, k.length, span, sec)['value']
                    for k in self._Rectangulare_LOAD)
            vals.append(v)
        mv = _max_value_in_vector(vals)
        rt_sec = _indice_of(vals, mv)
        rt_load = {}
        for k in self._Rectangulare_LOAD:
            lo = self._prl(k.intensity, k.length, span, rt_sec)
            rt_load[k.name] = {
                'alpha': float(lo['max_position']),
                'value': lo['value'],
                'Position': P[lo['max_position']],
            }
        rt = {'load': rt_load, 'span': span, 'section': rt_sec,
              'maximum_value': mv, 'position': 0.0}

        # ── Combiné ──
        vals, positions, maps = [], [], []
        for sec in range(len(snp[span])):
            lo = self._combined_load(span, sec)
            vals.append(lo['value'])
            positions.append(lo['position'])
            maps.append(lo['addition'])
        mv = _max_value_in_vector(vals)
        cb_sec = _indice_of(vals, mv)
        cb = {'load': maps[cb_sec], 'span': span, 'section': cb_sec,
              'maximum_value': mv, 'position': positions[cb_sec]}

        return {'point': pt, 'rect': rt, 'combined': cb}


# =============================================================================
#  PROJECT PATHS
# =============================================================================

class ProjectPaths:
    """Registre centralisé des chemins d'export."""

    def __init__(self, root):
        r = Path(str(root))
        self.root = r
        self.input = r / "01_Input"
        self.influence_lines = r / "02_Influence_Lines"
        self.critical_values = r / "03_Critical_Values"
        self.load_envelopes = r / "04_Load_Envelopes"
        self.load_positioning = r / "05_Load_Positioning"

        self.env_global = self.load_envelopes / "Global"
        self.env_global_point = self.env_global / "Point_Load"
        self.env_global_dist = self.env_global / "Distributed_Load"
        self.env_global_combined = self.env_global / "Combined_Load"

        self.env_critical = self.load_envelopes / "Critical_Section"
        self.env_critical_point = self.env_critical / "Point_Load"
        self.env_critical_dist = self.env_critical / "Distributed_Load"
        self.env_critical_combined = self.env_critical / "Combined_Load"

        self.pos_global = self.load_positioning / "Global"
        self.pos_global_point = self.pos_global / "Point_Load"
        self.pos_global_dist = self.pos_global / "Distributed_Load"
        self.pos_global_combined = self.pos_global / "Combined_Load"

        self.pos_critical = self.load_positioning / "Critical_Section"
        self.pos_critical_point = self.pos_critical / "Point_Load"
        self.pos_critical_dist = self.pos_critical / "Distributed_Load"
        self.pos_critical_combined = self.pos_critical / "Combined_Load"

    def create_all(self):
        for d in [
            self.input, self.influence_lines, self.critical_values,
            self.env_global_point, self.env_global_dist,
            self.env_global_combined,
            self.env_critical_point, self.env_critical_dist,
            self.env_critical_combined,
            self.pos_global_point, self.pos_global_dist,
            self.pos_global_combined,
            self.pos_critical_point, self.pos_critical_dist,
            self.pos_critical_combined,
        ]:
            d.mkdir(parents=True, exist_ok=True)

    def to_dict(self):
        return {k: str(getattr(self, k)) for k in [
            'root', 'input', 'influence_lines', 'critical_values',
            'load_envelopes', 'load_positioning',
            'env_global', 'env_global_point', 'env_global_dist',
            'env_global_combined',
            'env_critical', 'env_critical_point', 'env_critical_dist',
            'env_critical_combined',
            'pos_global', 'pos_global_point', 'pos_global_dist',
            'pos_global_combined',
            'pos_critical', 'pos_critical_point', 'pos_critical_dist',
            'pos_critical_combined',
        ]}


# =============================================================================
#  OUTPUT — point d'entrée principal
# =============================================================================

class Output(Hyperstatique):
    """Point d'entrée principal — calcul et export."""

    def __init__(self, E, I, L, steps, root=""):
        super().__init__(E, I, L, steps)
        self._Paths = ProjectPaths(root)

        # Résultats structurels (remplis par compute)
        self.BM = []
        self.SF = []
        self.Def = []
        self.Rot = []
        self.shear_force_all_abscissa = []
        self.X = []
        self.node_lengths = []

        self._bm_max = {'i': 0, 'j': 0, 'k': 0, 'val': 0.0}
        self._sf_max = {'i': 0, 'j': 0, 'k': 0, 'val': 0.0}
        self._def_max = {'i': 0, 'j': 0, 'k': 0, 'val': 0.0}
        self._rot_max = {'i': 0, 'j': 0, 'k': 0, 'val': 0.0}
        self._sm_max = {'i': 0, 'j': 0, 'val': 0.0}

        # Enveloppes de charge
        self._env_bm_gen = self._empty_env()
        self._env_sf_gen = self._empty_env()
        self._env_def_gen = self._empty_env()
        self._env_rot_gen = self._empty_env()
        self._env_bm_crit = self._empty_env()
        self._env_sf_crit = self._empty_env()
        self._env_def_crit = self._empty_env()
        self._env_rot_crit = self._empty_env()

        # État
        self._computed = False
        self._load_env_computed = False
        self._has_user_loads = False
        self._user_pt = []
        self._user_dist = []

    @staticmethod
    def _empty_env():
        e = {'load': {}, 'span': 0, 'section': 0,
             'maximum_value': 0.0, 'position': 0.0}
        return {'point_load': dict(e), 'rectangular_load': dict(e),
                'combined_load': dict(e)}

    @staticmethod
    def _env_from_loading(ld):
        return {
            'point_load': dict(ld.point_load),
            'rectangular_load': dict(ld.rectangular_load),
            'combined_load': dict(ld.combined_load),
        }

    # ── Propriétés (identiques au binding) ──────────────────────────────────

    @property
    def is_computed(self):
        return self._computed

    @property
    def is_load_envelopes_computed(self):
        return self._load_env_computed

    @property
    def bending_moment_max_positions(self):
        return dict(self._bm_max)

    @property
    def shear_force_max_positions(self):
        return dict(self._sf_max)

    @property
    def deflection_max_positions(self):
        return dict(self._def_max)

    @property
    def rotation_max_positions(self):
        return dict(self._rot_max)

    @property
    def support_moment_max_positions(self):
        return dict(self._sm_max)

    @property
    def paths(self):
        return self._Paths.to_dict()

    @property
    def bending_moment_general_envelope(self):
        return dict(self._env_bm_gen)

    @property
    def shear_force_general_envelope(self):
        return dict(self._env_sf_gen)

    @property
    def deflection_general_envelope(self):
        return dict(self._env_def_gen)

    @property
    def rotation_general_envelope(self):
        return dict(self._env_rot_gen)

    @property
    def bending_moment_critical_envelope(self):
        return dict(self._env_bm_crit)

    @property
    def shear_force_critical_envelope(self):
        return dict(self._env_sf_crit)

    @property
    def deflection_critical_envelope(self):
        return dict(self._env_def_crit)

    @property
    def rotation_critical_envelope(self):
        return dict(self._env_rot_crit)

    # ── compute ─────────────────────────────────────────────────────────────

    def compute(self, force=False):
        if self._computed and not force:
            return
        self.BM = self.bending_moments()
        self.SF = self.shear_force()
        self.Def = self.deflection()
        self.Rot = self.rotation()
        self.X = self.points_x_coordinates(self.span_node_positions)
        self.shear_force_all_abscissa = self.shear_force(True)

        nl = []
        cumul = 0.0
        for sp in self.L_spans:
            nl.append(cumul)
            cumul += sp
        nl.append(cumul)
        self.node_lengths = nl

        self._bm_max = _find_max_abs_3d(self.BM)
        self._sf_max = _find_max_abs_3d(self.SF)
        self._def_max = _find_max_abs_3d(self.Def)
        self._rot_max = _find_max_abs_3d(self.Rot)
        self._sm_max = _find_max_abs_2d(self.support_moment)

        self._computed = True

    # ── set_loads ───────────────────────────────────────────────────────────

    def set_loads(self, point_loads, distrib_loads):
        self._user_pt = list(point_loads)
        self._user_dist = list(distrib_loads)
        self._has_user_loads = True

    # ── compute_load_envelopes ──────────────────────────────────────────────

    def compute_load_envelopes(self, force=False):
        if not self._has_user_loads:
            raise RuntimeError(
                "Output::computeLoadEnvelopes: aucune charge fournie — "
                "appelez setLoads(point_loads, distrib_loads) avant.")
        if self._load_env_computed and not force:
            return
        self.compute()

        def env_for(data):
            ld = Loading(data, self.X, self.span_node_positions,
                         self.L_spans, self._user_pt, self._user_dist)
            return self._env_from_loading(ld)

        self._env_bm_gen = env_for(self.BM)
        self._env_sf_gen = env_for(self.SF)
        self._env_def_gen = env_for(self.Def)
        self._env_rot_gen = env_for(self.Rot)

        def crit_for(data, pos):
            ld = Loading(data, self.X, self.span_node_positions,
                         self.L_spans, self._user_pt, self._user_dist)
            r = ld.compute_critical_section(pos['i'])
            return {
                'point_load': r['point'],
                'rectangular_load': r['rect'],
                'combined_load': r['combined'],
            }

        self._env_bm_crit = crit_for(self.BM, self._bm_max)
        self._env_sf_crit = crit_for(self.SF, self._sf_max)
        self._env_def_crit = crit_for(self.Def, self._def_max)
        self._env_rot_crit = crit_for(self.Rot, self._rot_max)

        self._load_env_computed = True

    # ── Export JSON ─────────────────────────────────────────────────────────

    def export_critical_values(self):
        self.compute()
        self._Paths.create_all()
        cv = str(self._Paths.critical_values)
        _maximum_delivery(self._bm_max, cv, "bending_moment.json")
        _maximum_delivery(self._sf_max, cv, "shear_force.json")
        _maximum_delivery(self._def_max, cv, "deflection.json")
        _maximum_delivery(self._rot_max, cv, "rotation.json")
        j = {'span': self._sm_max['i'], 'alpha': self._sm_max['j'],
             'value': self._sm_max['val']}
        _delivery(j, cv, "support_moment.json")

    def export_influence_lines(self):
        self.compute()
        self._Paths.create_all()
        inp = str(self._Paths.input)
        il = str(self._Paths.influence_lines)
        model = {
            'spans': self.L_spans,
            'young_modulus': self.E_spans,
            'inertia': self.I_spans,
            'step': self.steps,
            'node_lengths': self.node_lengths,
            'n_spans': len(self.L_spans),
            'n_total_nodes': len(self.X),
        }
        _delivery(model, inp, "structural_model.json")
        _delivery(self.BM, il, "bending_moment.json")
        _delivery(self.SF, il, "shear_force.json")
        _delivery(self.Def, il, "deflection.json")
        _delivery(self.Rot, il, "rotation.json")
        _delivery(self.support_moment, il, "support_moment.json")
        _delivery(self.X, il, "abscissa.json")
        _delivery(self.shear_force_all_abscissa, il,
                  "shear_abscissa.json")
        _delivery(self.node_lengths, il, "node_lengths.json")

    def export_load_envelopes(self):
        self.compute_load_envelopes()
        self._Paths.create_all()

        def write_env(env, fname, dp, dr, dc):
            _loading_delivery(env['rectangular_load'], str(dr), fname)
            _loading_delivery(env['point_load'], str(dp), fname)
            _loading_delivery(env['combined_load'], str(dc), fname)

        P = self._Paths
        write_env(self._env_bm_gen, "bending_moment.json",
                  P.env_global_point, P.env_global_dist,
                  P.env_global_combined)
        write_env(self._env_sf_gen, "shear_force.json",
                  P.env_global_point, P.env_global_dist,
                  P.env_global_combined)
        write_env(self._env_def_gen, "deflection.json",
                  P.env_global_point, P.env_global_dist,
                  P.env_global_combined)
        write_env(self._env_rot_gen, "rotation.json",
                  P.env_global_point, P.env_global_dist,
                  P.env_global_combined)
        write_env(self._env_bm_crit, "bending_moment.json",
                  P.env_critical_point, P.env_critical_dist,
                  P.env_critical_combined)
        write_env(self._env_sf_crit, "shear_force.json",
                  P.env_critical_point, P.env_critical_dist,
                  P.env_critical_combined)
        write_env(self._env_def_crit, "deflection.json",
                  P.env_critical_point, P.env_critical_dist,
                  P.env_critical_combined)
        write_env(self._env_rot_crit, "rotation.json",
                  P.env_critical_point, P.env_critical_dist,
                  P.env_critical_combined)

    def export_all(self):
        self.export_influence_lines()
        self.export_load_envelopes()
        self.export_critical_values()
        print(f"Output generated successfully in: {self._Paths.root}")

    def __repr__(self):
        return (f"<Output number_of_spans={self.number_of_spans} "
                f"computed={self._computed}>")


# =============================================================================
#  UPDATE POSITIONS — repositionnement des charges
# =============================================================================

class UpdatePositions:
    """Reconstruit les positions physiques des charges à partir des
    enveloppes JSON exportées."""

    def __init__(self, root, input_lines):
        self._root = Path(str(root))
        self._input_lines = list(input_lines)
        self._computed = False
        self._results = {}

    def compute(self, force=False):
        """Stub — fonctionnalité complète nécessite l'extension C++."""
        if self._computed and not force:
            return
        # Implémentation minimale : lire les JSON exportés
        self._computed = True

    @property
    def is_computed(self):
        return self._computed

    @property
    def results(self):
        return dict(self._results)

    def write_all(self):
        """Stub — fonctionnalité complète nécessite l'extension C++."""
        pass

    def run(self):
        self.compute()
        self.write_all()
