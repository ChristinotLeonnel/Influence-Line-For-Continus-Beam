"""Independent validation of the LIPoutreContinue engine (baseline audit).

Reference = Clapeyron three-moment equations solved directly with numpy, plus
exact simply-supported influence functions. Shares NO code with the library.
Run with PYTHONPATH pointing at the build tree holding the C++ backend.
"""
import sys, math
import numpy as np
import Tsaraloha.LIPoutreContinue as m

print("backend:", m._BACKEND)
RESULTS = []


def report(name, ok, detail=""):
    RESULTS.append((name, ok))
    print(f"[{'PASS' if ok else 'FAIL'}] {name} {detail}")


# ---------------------------------------------------------------- reference
def iso_M(x, a, L):   return a * (1 - x / L) if a <= x else x * (1 - a / L)
def iso_V(x, a, L, side):  # side = -1 just left of section, +1 just right
    if a < x or (a == x and side > 0): return -a / L
    return 1 - a / L
def iso_w(x, a, L, EI):
    if a <= x: return -(a * (L - x) / (6 * EI * L)) * (x * (2 * L - x) - a * a)
    return -(x * (L - a) / (6 * EI * L)) * (a * (2 * L - a) - x * x)
def theta_ends(a, L, EI):  # magnitudes of end rotations, unit load at a
    b = L - a
    return a * b * (L + b) / (6 * EI * L), a * b * (L + a) / (6 * EI * L)


def support_moments(Ls, EIs, span, a):
    """Moments at all n+1 supports for a unit load at a in span `span`."""
    n = len(Ls)
    f = [L / (6 * EI) if L > 0 else 0.0 for L, EI in zip(Ls, EIs)]
    N = n - 1
    if N == 0:
        return np.zeros(2)
    A = np.zeros((N, N)); r = np.zeros(N)
    for k in range(N):            # equation at interior support k+1
        i = k + 1
        A[k, k] = 2 * (f[i - 1] + f[i])
        if k > 0: A[k, k - 1] = f[i - 1]
        if k < N - 1: A[k, k + 1] = f[i]
    tl, tr = theta_ends(a, Ls[span], EIs[span])
    # span s lies between supports s and s+1
    if span >= 1: r[span - 1] -= tl          # support s: left-end rotation of span s
    if span + 1 <= N: r[span] -= tr          # support s+1: right-end rotation
    Mint = np.linalg.solve(A, r)
    return np.concatenate([[0.0], Mint, [0.0]])


def reference(Ls, Es, Is, nodes):
    """BM, Def tensors [s][sec][k] with load positions = concatenated nodes."""
    EIs = [E * I for E, I in zip(Es, Is)]
    act = [i for i, L in enumerate(Ls) if L > 0]
    loads = [(s, a) for s in act for a in nodes[act.index(s)]]
    SM = np.array([support_moments(Ls, EIs, s, a) for s, a in loads])  # [k][support]
    BM, DEF = [], []
    for si, s in enumerate(act):
        L, EI = Ls[s], EIs[s]
        bm_s, df_s = [], []
        for x in nodes[si]:
            rowM, rowW = [], []
            for k, (ls, a) in enumerate(loads):
                mA, mB = SM[k, s], SM[k, s + 1]
                M = mA * (1 - x / L) + mB * x / L
                w = -mA * x * (L - x) * (2 * L - x) / (6 * EI * L) - mB * x * (L - x) * (L + x) / (6 * EI * L)
                if ls == s:
                    M += iso_M(x, a, L); w += iso_w(x, a, L, EI)
                rowM.append(M); rowW.append(w)
            bm_s.append(rowM); df_s.append(rowW)
        BM.append(bm_s); DEF.append(df_s)
    return BM, DEF, SM


def max_rel(a, b):
    a = np.concatenate([np.ravel(np.array(x)) for x in a]); b = np.concatenate([np.ravel(np.array(x)) for x in b])
    scale = max(np.max(np.abs(b)), 1e-300)
    return np.max(np.abs(a - b)) / scale


# ---------------------------------------------------------------- 1. solver
cases = {
    "1 span":                 ([10.0], [210e9], [8e-4], 0.5),
    "2 equal spans":          ([10.0, 10.0], [210e9] * 2, [8e-4] * 2, 1.0),
    "3 spans var. EI":        ([12.0, 16.0, 12.0], [35e9] * 3, [0.01, 0.0171, 0.01], 0.5),
    "4 spans, step 0.37":     ([5.0, 7.3, 11.1, 4.0], [30e9, 210e9, 30e9, 70e9], [1e-3, 2e-4, 3e-3, 5e-4], 0.37),
    "fixed-pinned (L0=0)":    ([0.0, 10.0], [210e9] * 2, [8e-4] * 2, 0.5),
    "fixed-fixed (0,L,0)":    ([0.0, 10.0, 0.0], [210e9] * 3, [8e-4] * 3, 0.5),
}
for name, (L, E, I, st) in cases.items():
    o = m.Output(E=E, I=I, L=list(L), steps=st); o.compute()
    BMr, DEFr, SMr = reference(L, E, I, o.span_node_positions)
    eBM = max_rel(o.BM, BMr); eD = max_rel(o.Def, DEFr)
    report(f"Clapeyron ref BM  [{name}]", eBM < 1e-9, f"rel.err={eBM:.2e}")
    report(f"Clapeyron ref Def [{name}]", eD < 1e-9, f"rel.err={eD:.2e}")

# closed forms
o = m.Output(E=[1.0] * 2, I=[1.0] * 2, L=[10.0, 10.0], steps=0.5); o.compute()
SMlib = np.array(o.support_moment)       # [support][k]
X = np.array(o.X)
k = int(np.argmin(np.abs(X - 5.0)))
report("2-span M_B (load mid span 1) = -3PL/32", abs(SMlib[1][k] - (-3 * 10 / 32)) < 1e-12, f"{SMlib[1][k]:.6f} vs {-3*10/32:.6f}")
o = m.Output(E=[1.0] * 3, I=[1.0] * 3, L=[0.0, 10.0, 0.0], steps=0.5); o.compute()
SMlib = np.array(o.support_moment); k = int(np.argmin(np.abs(np.array(o.X) - 3.0)))
a, b, Lb = 3.0, 7.0, 10.0
report("fixed-fixed M_A = -ab^2/L^2", abs(SMlib[0][k] + a * b * b / Lb**2) < 1e-12, f"{SMlib[0][k]:.6f} vs {-a*b*b/Lb**2:.6f}")

# Maxwell-Betti on deflection: w(x_i; load x_j) == w(x_j; load x_i)
o = m.Output(E=[30e9, 210e9, 30e9], I=[1e-3, 2e-4, 3e-3], L=[5.0, 7.3, 11.1], steps=0.37); o.compute()
W = np.vstack([np.array(s) for s in o.Def])          # rows=section (concat), cols=load
asym = np.max(np.abs(W - W.T)) / np.max(np.abs(W))
report("Maxwell-Betti deflection symmetry", asym < 1e-10, f"rel.asym={asym:.2e}")

# ---------------------------------------------------------------- 2. mesh
o = m.Output(E=[1.0], I=[1.0], L=[6.0], steps=0.1); o.compute()
x = np.array(o.span_node_positions[0]); d = np.diff(x)
report("mesh L=6 step=0.1: no sliver element", d.min() > 0.5 * 0.1, f"n={len(x)} min dx={d.min():.3e}")
o = m.Output(E=[1.0], I=[1.0], L=[1.0], steps=0.1); o.compute()
x = np.array(o.span_node_positions[0]); d = np.diff(x)
report("mesh L=1 step=0.1: no sliver element", d.min() > 0.5 * 0.1, f"n={len(x)} min dx={d.min():.3e}")

# input mutation (C++ API takes refs) — Python wrapper copies, check zero-span removal visible
o = m.Output(E=[1.0] * 2, I=[1.0] * 2, L=[0.0, 10.0], steps=1.0)
report("L_spans keeps declared spans", list(o.L_spans) == [0.0, 10.0], f"L_spans={list(o.L_spans)}")

# ---------------------------------------------------------------- 3. loading
def env(L, st, point=(), dist=(), what="bending_moment_general_envelope"):
    o = m.Output(E=[210e9] * len(L), I=[8e-4] * len(L), L=L, steps=st); o.compute()
    o.set_loads(point_loads=list(point), distrib_loads=list(dist)); o.compute_load_envelopes()
    return getattr(o, what)

# UDL over full simple span: M_max = qL^2/8 = 12.5 for q=1, L=10
e = env([10.0], 1.0, dist=[m.Load(intensity=[1.0], length=[0.0, 10.0], name="q")])

print("   UDL envelope dict keys:", list(e.keys()))
val = e["rectangular_load"]["maximum_value"]
report("UDL q=1 full span L=10: Mmax = 12.5", abs(val - 12.5) < 1e-6, f"got {val:.6f}")
e = env([10.0], 0.1, dist=[m.Load(intensity=[1.0], length=[0.0, 10.0], name="q")])
val = e["rectangular_load"]["maximum_value"]
report("UDL q=1 full span L=10, step 0.1: Mmax = 12.5", abs(val - 12.5) < 1e-3, f"got {val:.6f}")

# Convoy with light leading axle: [10 kN, 100 kN] spaced 20 m on 10 m span.
# Max midspan moment: 100 kN at midspan, 10 kN off the beam  -> 250 kN.m
e = env([10.0], 0.5, point=[m.Load(intensity=[10.0, 100.0], length=[0.0, 20.0], name="T")])
val = e["point_load"]["maximum_value"]
report("convoy [10,100]@20m on L=10: Mmax = 250", abs(val - 250) < 1e-6, f"got {val:.3f}")

# Combined (no UDL) must equal point-only result
L3 = [8.0, 10.0]
pl = [m.Load(intensity=[100.0, 100.0, 50.0], length=[0.0, 2.0, 3.0], name="T")]
e = env(L3, 0.5, point=pl)
report("combined == point when no UDL", abs(e["point_load"]["maximum_value"] - e["combined_load"]["maximum_value"]) < 1e-9,
       f"point={e['point_load']['maximum_value']:.4f} combined={e['combined_load']['maximum_value']:.4f}")

# Two identical single-axle loads listed separately must give 2x one load (same IL, same position)
e1 = env([10.0, 10.0], 0.5, point=[m.Load(intensity=[100.0], length=[0.0], name="A")])
e2 = env([10.0, 10.0], 0.5, point=[m.Load(intensity=[100.0], length=[0.0], name="A"),
                                    m.Load(intensity=[100.0], length=[0.0], name="B")])
report("2 identical loads = 2 x 1 load", abs(e2["point_load"]["maximum_value"] - 2 * e1["point_load"]["maximum_value"]) < 1e-9,
       f"{e2['point_load']['maximum_value']:.3f} vs {2*e1['point_load']['maximum_value']:.3f}")

# Opposite-sign trap: brute-force true worst |M| at every section for 2 independent single loads
# placed anywhere (they may sit at different points) vs library.
o = m.Output(E=[1.0] * 2, I=[1.0] * 2, L=[10.0, 10.0], steps=0.5); o.compute()
BM = [np.array(s) for s in o.BM]
true_best = 0.0
for s in BM:
    for row in s:
        true_best = max(true_best, abs(row.max() * 100 + row.max() * 50), abs(row.min() * 100 + row.min() * 50))
o.set_loads(point_loads=[m.Load(intensity=[100.0], length=[0.0], name="A"), m.Load(intensity=[50.0], length=[0.0], name="B")], distrib_loads=[])
o.compute_load_envelopes()
got = abs(o.bending_moment_general_envelope["point_load"]["maximum_value"])
report("2 independent loads: |M| envelope = brute force", abs(got - true_best) < 1e-9, f"got {got:.3f} true {true_best:.3f}")

# Shear: 2-axle convoy on simple span, brute force with exact shear IL at every section
L, st = 10.0, 1.0
P = [100.0, 100.0]; sp = 3.0
o = m.Output(E=[1.0], I=[1.0], L=[L], steps=st); o.compute()
o.set_loads(point_loads=[m.Load(intensity=P, length=[0.0, sp], name="T")], distrib_loads=[]); o.compute_load_envelopes()
got = o.shear_force_general_envelope["point_load"]["maximum_value"]
nodes = o.span_node_positions[0]; best = 0.0
for x in nodes:
    for side in (-1, 1):
        for a0 in nodes:
            v = sum(Pi * iso_V(x, a0 + off, L, side) for Pi, off in zip(P, [0, sp]) if 0 <= a0 + off <= L)
            if abs(v) > abs(best): best = v
report("shear envelope 2-axle convoy = brute force", abs(abs(got) - abs(best)) < 1e-9, f"got {got:.3f} true {best:.3f}")

# ---------------------------------------------------------------- 4. units
o = m.Output(E=[210e9], I=[8e-4], L=[10.0], steps=0.5); o.compute()
o.set_loads(point_loads=[m.Load(intensity=[100.0], length=[0.0], name="P")], distrib_loads=[]); o.compute_load_envelopes()
w = o.deflection_general_envelope["point_load"]["maximum_value"]
true_w = 100e3 * 10**3 / (48 * 210e9 * 8e-4)  # P in N
print(f"   deflection 100 kN midspan: library={w:.6e}  true(m)={true_w:.6e}  ratio={true_w/abs(w):.1f}")
report("deflection unit consistency (kN loads, Pa E)", abs(abs(w) - true_w) / true_w < 1e-6)

print("\nSUMMARY:", sum(ok for _, ok in RESULTS), "/", len(RESULTS), "pass")
