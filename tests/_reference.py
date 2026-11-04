"""Independent reference solutions for continuous beams.

This module deliberately shares NO code with the library: it solves the
Clapeyron three-moment equations directly with numpy and superposes the exact
simply-supported influence functions. It is the oracle used by the tests to
validate the C++ engine; it is not a computation backend.

Conventions (identical to the library):
    sagging bending moment > 0, upward deflection > 0, unit load downward.
    A span of zero length models a fixed end (zero flexibility L/6EI).
"""
from __future__ import annotations

import numpy as np


# --------------------------------------------------------------------------
# Simply supported span [0, L], unit load at a, section at x
# --------------------------------------------------------------------------
def iso_moment(x: float, a: float, L: float) -> float:
    return a * (1.0 - x / L) if a <= x else x * (1.0 - a / L)


def iso_shear_pair(x: float, a: float, L: float) -> list[float]:
    """Shear at x for a load at a. At a == x the jump is returned as
    [value with load just left of x, value with load just right of x]."""
    if a < x:
        return [-a / L]
    if a > x:
        return [1.0 - a / L]
    return [-a / L, 1.0 - a / L]


def iso_deflection(x: float, a: float, L: float, EI: float) -> float:
    if a <= x:
        return -(a * (L - x) / (6 * EI * L)) * (x * (2 * L - x) - a * a)
    return -(x * (L - a) / (6 * EI * L)) * (a * (2 * L - a) - x * x)


def iso_rotation(x: float, a: float, L: float, EI: float) -> float:
    """d(iso_deflection)/dx, derived analytically."""
    if a <= x:
        return -a * (2 * L * L - 6 * L * x + 3 * x * x + a * a) / (6 * EI * L)
    return -(L - a) * (a * (2 * L - a) - 3 * x * x) / (6 * EI * L)


def end_rotations(a: float, L: float, EI: float) -> tuple[float, float]:
    """Magnitudes of the left/right end rotations of a simple span."""
    b = L - a
    return a * b * (L + b) / (6 * EI * L), a * b * (L + a) / (6 * EI * L)


# --------------------------------------------------------------------------
# Continuous beam
# --------------------------------------------------------------------------
def support_moments(spans, EIs, span: int, a: float) -> np.ndarray:
    """Moments at the n+1 supports for a unit load at `a` in span `span`
    (direct solve of the three-moment equations)."""
    n = len(spans)
    f = [L / (6 * EI) if L > 0 else 0.0 for L, EI in zip(spans, EIs)]
    N = n - 1
    if N == 0:
        return np.zeros(2)
    A = np.zeros((N, N))
    r = np.zeros(N)
    for k in range(N):  # equation written at interior support k + 1
        i = k + 1
        A[k, k] = 2 * (f[i - 1] + f[i])
        if k > 0:
            A[k, k - 1] = f[i - 1]
        if k < N - 1:
            A[k, k + 1] = f[i]
    tl, tr = end_rotations(a, spans[span], EIs[span])
    if span >= 1:
        r[span - 1] -= tl
    if span + 1 <= N:
        r[span] -= tr
    return np.concatenate([[0.0], np.linalg.solve(A, r), [0.0]])


class ContinuousBeamReference:
    """Influence tensors laid out like the library: [span][section][load]."""

    def __init__(self, spans, E, I, span_nodes):
        self.spans = list(spans)
        self.EIs = [e * i for e, i in zip(E, I)]
        self.active = [s for s, L in enumerate(self.spans) if L > 0]
        self.nodes = [list(n) for n in span_nodes]
        self.loads = [(s, a) for s, nodes in zip(self.active, self.nodes) for a in nodes]
        self.SM = np.array([support_moments(self.spans, self.EIs, s, a) for s, a in self.loads])

    def _tensor(self, fn):
        out = []
        for si, s in enumerate(self.active):
            L, EI = self.spans[s], self.EIs[s]
            rows = []
            for x in self.nodes[si]:
                row = []
                for k, (ls, a) in enumerate(self.loads):
                    mA, mB = self.SM[k, s], self.SM[k, s + 1]
                    row.extend(fn(x, a, L, EI, mA, mB, ls == s))
                rows.append(row)
            out.append(rows)
        return out

    def bending_moment(self):
        return self._tensor(lambda x, a, L, EI, mA, mB, own:
                            [mA * (1 - x / L) + mB * x / L + (iso_moment(x, a, L) if own else 0.0)])

    def shear_force(self):
        def f(x, a, L, EI, mA, mB, own):
            hyp = (mB - mA) / L
            return [v + hyp for v in iso_shear_pair(x, a, L)] if own else [hyp]
        return self._tensor(f)

    def deflection(self):
        def f(x, a, L, EI, mA, mB, own):
            hyp = (-mA * x * (L - x) * (2 * L - x) - mB * x * (L - x) * (L + x)) / (6 * EI * L)
            return [hyp + (iso_deflection(x, a, L, EI) if own else 0.0)]
        return self._tensor(f)

    def rotation(self):
        def f(x, a, L, EI, mA, mB, own):
            hyp = (-mA * (2 * L * L - 6 * L * x + 3 * x * x) - mB * (L * L - 3 * x * x)) / (6 * EI * L)
            return [hyp + (iso_rotation(x, a, L, EI) if own else 0.0)]
        return self._tensor(f)

    def support_moment_lines(self) -> np.ndarray:
        """[support][load] — same layout as Output.support_moment, restricted
        to the supports bounding non-zero spans."""
        idx = sorted({s for s in self.active} | {s + 1 for s in self.active})
        return self.SM[:, idx].T


def flat(t) -> np.ndarray:
    return np.concatenate([np.ravel(np.asarray(row, dtype=float)) for span in t for row in span])


def max_rel_error(actual, expected) -> float:
    a, b = flat(actual), flat(expected)
    assert a.shape == b.shape, f"shape mismatch {a.shape} vs {b.shape}"
    return float(np.max(np.abs(a - b)) / max(np.max(np.abs(b)), 1e-300))
