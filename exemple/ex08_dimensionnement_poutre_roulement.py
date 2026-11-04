"""
ex08_dimensionnement_poutre_roulement.py — Problème concret de bout en bout :
choisir le profilé d'une poutre de roulement de pont roulant.

Énoncé
------
Atelier de 24 m, portiques tous les 6 m. La poutre de roulement est continue
sur 4 travées (4 x 6 m), en acier S355. Le pont roulant (16 t) applique deux
galets de 95 kN espacés de 3,20 m, qui circulent sur toute la longueur.
Charges permanentes : poids propre du profilé + rail (0,50 kN/m).

Question : quel est le plus léger profilé HEB qui satisfait
  * ELU flexion   : M_Ed <= M_pl,Rd = W_pl * fy / gamma_M0
  * ELU tranchant : V_Ed <= V_pl,Rd = A_vz * fy / (sqrt(3) * gamma_M0)
  * ELS flèche    : flèche due au pont roulant <= L / 600 (par travée)
et quelles réactions faut-il reprendre sur les corbeaux des poteaux
(y compris un éventuel soulèvement) ?

Méthode (c'est là que les lignes d'influence servent)
-----------------------------------------------------
* Charge MOBILE (galets) : le moteur cherche, pour chaque section, la position
  la plus défavorable du convoi -> enveloppes Q+(x) et Q-(x).
* Charge PERMANENTE g : elle est fixe et couvre toute la poutre, donc son effet
  en une section est exactement  g x (aire de la ligne d'influence).
  (Une DistributedLoad est une charge *mobile* : son extremum serait un
  chargement partiel, pas la charge permanente.)
* Combinaison ELU section par section, avec gamma_G = 1,35 si G aggrave
  l'effet et 1,00 s'il le réduit :
      E_Ed+(x) = gamma_G * G(x) + 1,5 * phi * Q+(x)
      E_Ed-(x) = gamma_G * G(x) + 1,5 * phi * Q-(x)

Hypothèses simplificatrices (exemple pédagogique, à adapter à votre projet) :
coefficient dynamique unique phi = 1,25 ; propriétés HEB usuelles (catalogue,
à vérifier) ; NON vérifiés ici : déversement, effets horizontaux du pont,
interaction M-V, fatigue, résistance locale de l'âme sous galet.

Sorties : tableau de choix, détail du profilé retenu, figures PNG,
note de calcul HTML (enveloppes du pont roulant).
"""
from __future__ import annotations

import math

import numpy as np

import Tsaraloha.LIPoutreContinue as tsa

if not hasattr(tsa, "Beam"):   # version 1.x installée (PyPI) au lieu de la 2.0 du dépôt
    raise SystemExit(f"Tsaraloha {tsa.__version__} est installée ; cet exemple requiert la 2.0. "
                     'Depuis la racine du dépôt :  python -m pip install ".[plot]"')

_trapz = getattr(np, "trapezoid", None) or np.trapz   # NumPy 2.x / 1.x

# ── Données du problème ─────────────────────────────────────────────────────
SPANS = [6.0, 6.0, 6.0, 6.0]           # m
E = 210e9                               # Pa
FY = 355e3                              # kPa  (= 355 MPa ; kN/m² pour rester en kN, m)
GAMMA_M0 = 1.0
GAMMA_Q, PHI = 1.5, 1.25                # charge variable, coefficient dynamique
RAIL = 0.50                             # kN/m
WHEEL, WHEELBASE = 95.0, 3.20           # kN, m
DEFLECTION_LIMIT = 600                  # L / 600
STEP = 0.10                             # m

# HEB : masse [kg/m], I_y [cm⁴], W_pl,y [cm³], A_vz [cm²]
HEB = {
    "HEB 200": (61.3, 5696, 642.5, 24.83),
    "HEB 220": (71.5, 8091, 827.0, 27.92),
    "HEB 240": (83.2, 11260, 1053, 33.23),
    "HEB 260": (93.0, 14920, 1283, 37.59),
    "HEB 280": (103.0, 19270, 1534, 41.09),
    "HEB 300": (117.0, 25170, 1869, 47.43),
    "HEB 320": (127.0, 30820, 2149, 51.77),
}

crane = tsa.PointLoadTrain(axles=[WHEEL, WHEEL], spacings=[WHEELBASE], name="Pont roulant 16 t")


# ── Effets de la charge permanente : g x aire des lignes d'influence ────────
def permanent_effects(res: tsa.Results, g: float) -> dict:
    """M_G, V_G aux sections de calcul et R_G par appui, pour g [kN/m] sur toute la poutre."""
    out, xs = res.output, res.x
    M, V = [], []
    for s in range(len(out.span_node_positions)):
        for j in range(len(out.span_node_positions[s])):
            M.append(g * _trapz(out.BM[s][j], xs))
            # la LI de V a un saut à la section : ses abscisses le contiennent (deux fois x_s)
            V.append(g * _trapz(out.SF[s][j], out.shear_force_all_abscissa[s][j]))
    R = [g * _trapz(res.reaction_influence_line(k)[1], xs) for k in range(len(res.support_positions))]
    return {"M": np.array(M), "V": np.array(V), "R": np.array(R)}


def uls(G: np.ndarray, q_max: np.ndarray, q_min: np.ndarray):
    """Enveloppes ELU max / min, gamma_G défavorable ou favorable section par section."""
    q = GAMMA_Q * PHI
    e_max = np.where(G > 0, 1.35, 1.0) * G + q * q_max
    e_min = np.where(G < 0, 1.35, 1.0) * G + q * q_min
    return e_max, e_min


def check(profile: str) -> dict:
    mass, I_cm4, Wpl_cm3, Av_cm2 = HEB[profile]
    I = I_cm4 * 1e-8
    beam = tsa.Beam.continuous(SPANS, E=E, I=I)
    res = beam.analyze(step=STEP, loads=[crane])
    g = mass * 9.81e-3 + RAIL                                     # kN/m

    G = permanent_effects(res, g)
    eM, eV, ew = res.envelope("M"), res.envelope("V"), res.envelope("w")
    assert len(eM.x) == len(G["M"]), "grille des enveloppes ≠ grille des lignes d'influence"

    M_max, M_min = uls(G["M"], eM.max, eM.min)
    V_max, V_min = uls(G["V"], eV.max, eV.min)
    M_Ed = max(M_max.max(), -M_min.min())
    V_Ed = max(V_max.max(), -V_min.min())
    M_Rd = Wpl_cm3 * 1e-6 * FY / GAMMA_M0                         # kN·m
    V_Rd = Av_cm2 * 1e-4 * FY / (math.sqrt(3) * GAMMA_M0)         # kN

    # ELS : flèche du pont roulant seul (phi = 1), travée par travée
    bounds = np.cumsum([0.0, *SPANS])
    w_ratio = 0.0
    for a, b in zip(bounds[:-1], bounds[1:]):
        inside = (ew.x >= a) & (ew.x <= b)
        w = np.max(np.abs(np.r_[ew.max[inside], ew.min[inside]]))
        w_ratio = max(w_ratio, w / ((b - a) / DEFLECTION_LIMIT))

    # Réactions : max (caractéristique et ELU) et soulèvement (G favorable)
    Rq = res.reactions()
    reactions = [{"x": r["x"],
                  "R_k_max": G["R"][i] + PHI * r["max"],
                  "R_Ed_max": 1.35 * G["R"][i] + GAMMA_Q * PHI * r["max"],
                  "R_Ed_min": 1.0 * G["R"][i] + GAMMA_Q * PHI * r["min"]} for i, r in enumerate(Rq)]

    return {"profile": profile, "res": res, "g": g, "M_Ed": M_Ed, "M_Rd": M_Rd, "V_Ed": V_Ed, "V_Rd": V_Rd,
            "eta_M": M_Ed / M_Rd, "eta_V": V_Ed / V_Rd, "eta_w": w_ratio, "reactions": reactions,
            "M_curves": (eM.x, M_max, M_min)}


# ── 1. Balayage du catalogue ────────────────────────────────────────────────
print(f"{'Profilé':9s} {'kg/m':>6s} {'M_Ed/M_Rd':>10s} {'V_Ed/V_Rd':>10s} {'w/w_lim':>8s}  verdict")
chosen = None
for name in HEB:
    c = check(name)
    ok = max(c["eta_M"], c["eta_V"], c["eta_w"]) <= 1.0
    worst = max(("flexion", c["eta_M"]), ("tranchant", c["eta_V"]), ("flèche", c["eta_w"]), key=lambda t: t[1])
    print(f"{name:9s} {HEB[name][0]:6.1f} {c['eta_M']:10.2f} {c['eta_V']:10.2f} {c['eta_w']:8.2f}  "
          f"{'OK' if ok else 'insuffisant (' + worst[0] + ')'}")
    if ok and chosen is None:
        chosen = c

if chosen is None:
    raise SystemExit("Aucun profilé du catalogue ne convient : élargissez HEB.")

# ── 2. Détail du profilé retenu ─────────────────────────────────────────────
c, res = chosen, chosen["res"]
print(f"\n=> Profilé retenu : {c['profile']}  (g = {c['g']:.2f} kN/m)")
print(f"   Flexion   : M_Ed = {c['M_Ed']:.1f} kN·m  <=  M_pl,Rd = {c['M_Rd']:.1f} kN·m   ({c['eta_M']:.0%})")
print(f"   Tranchant : V_Ed = {c['V_Ed']:.1f} kN    <=  V_pl,Rd = {c['V_Rd']:.1f} kN     ({c['eta_V']:.0%})")
print(f"   Flèche    : {c['eta_w']:.0%} de L/{DEFLECTION_LIMIT}")
m = res.maximum_bending_moment
print(f"   Position critique des galets (M+ max, sans G) : origine du convoi en "
      f"{next(iter(m.loads.values()))['position']:.2f} m, section x = {m.x:.2f} m")

print("\n   Réactions sur corbeaux [kN] (positif = vers le haut)")
print(f"   {'x [m]':>6s} {'R_k max':>9s} {'R_Ed max':>9s} {'R_Ed min':>9s}")
for r in c["reactions"]:
    flag = "  <-- SOULÈVEMENT : prévoir un ancrage" if r["R_Ed_min"] < 0 else ""
    print(f"   {r['x']:6.1f} {r['R_k_max']:9.1f} {r['R_Ed_max']:9.1f} {r['R_Ed_min']:9.1f}{flag}")

# ── 3. Figures et note de calcul ────────────────────────────────────────────
try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from Tsaraloha.LIPoutreContinue import plot

    res.dashboard().savefig("ex08_synthese.png", dpi=150)
    res.plot_critical("M", which="min").savefig("ex08_position_critique_appui.png", dpi=150)

    x, M_max, M_min = c["M_curves"]
    ax = plot.envelope(res, "M", title=f"{c['profile']} — enveloppes ELU (G + 1,5·φ·Q) et M_pl,Rd")
    ax.lines[0].set_label("pont roulant seul (max)")
    ax.lines[1].set_label("pont roulant seul (min)")
    ax.plot(x, M_max, color="black", lw=1.4, label="ELU max")
    ax.plot(x, M_min, color="black", lw=1.4, ls="--", label="ELU min")
    for k, v in enumerate((c["M_Rd"], -c["M_Rd"])):
        ax.axhline(v, color="#7f8c8d", lw=1.0, ls="-.", label="± M_pl,Rd" if k == 0 else None)
    ax.legend(loc="upper right", fontsize=8, frameon=False)
    ax.figure.savefig("ex08_elu_moment.png", dpi=150)
    plt.close("all")
    print("\nFigures : ex08_synthese.png, ex08_position_critique_appui.png, ex08_elu_moment.png")
except ImportError:
    print("\n(matplotlib absent : figures non produites)")

res.report("ex08_note_pont_roulant.html", project_title=f"Poutre de roulement 4 x 6 m — {c['profile']}")
print("Note de calcul (enveloppes du pont roulant) : ex08_note_pont_roulant.html")
