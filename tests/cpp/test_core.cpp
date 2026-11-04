// =============================================================================
//  test_core.cpp — tests natifs du cœur C++ (sans Python, sans framework).
//  Construit avec -DTSARALOHA_BUILD_TESTS=ON, exécuté par `ctest`.
//  Code de retour non nul si un contrôle échoue.
// =============================================================================
#include "LIPoutreContinue/StructuralAnalysis.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_failures = 0;

static void check(bool ok, const char* name, double got, double expected) {
    std::printf("[%s] %s  got=%.15g expected=%.15g\n", ok ? "PASS" : "FAIL", name, got, expected);
    if (!ok) ++g_failures;
}

static bool close(double a, double b, double rel) {
    return std::abs(a - b) <= rel * std::max(std::abs(b), 1e-300);
}

static size_t nearest(const std::vector<double>& x, double v) {
    size_t k = 0;
    for (size_t i = 1; i < x.size(); ++i)
        if (std::abs(x[i] - v) < std::abs(x[k] - v)) k = i;
    return k;
}

int main() {
    // 1) Deux travées égales, P à mi-travée 1 : M_B = -3PL/32
    {
        std::vector<double> E{1.0, 1.0}, I{1.0, 1.0}, L{10.0, 10.0};
        double h = 0.5;
        Output out(E, I, L, h);
        out.compute();
        const size_t k = nearest(out.X, 5.0);
        check(close(out.SupportMoment[1][k], -3.0 * 10.0 / 32.0, 1e-13),
              "two-span M_B = -3PL/32", out.SupportMoment[1][k], -3.0 * 10.0 / 32.0);
    }
    // 2) Bi-encastrée (travées nulles aux extrémités) : M_A = -ab^2/L^2
    {
        std::vector<double> E{1.0, 1.0, 1.0}, I{1.0, 1.0, 1.0}, L{0.0, 10.0, 0.0};
        double h = 0.5;
        Output out(E, I, L, h);
        out.compute();
        const size_t k = nearest(out.X, 3.0);
        const double expected = -3.0 * 7.0 * 7.0 / 100.0;
        check(close(out.SupportMoment[0][k], expected, 1e-13),
              "fixed-fixed M_A = -ab^2/L^2", out.SupportMoment[0][k], expected);
    }
    // 3) Flèche isostatique à mi-portée : -L^3 / (48 EI)
    {
        std::vector<double> E{210e9}, I{8e-4}, L{10.0};
        double h = 0.5;
        Output out(E, I, L, h);
        out.compute();
        const size_t k = nearest(out.X, 5.0);
        const double expected = -1000.0 / (48.0 * 210e9 * 8e-4);
        check(close(out.Def[0][k][k], expected, 1e-13),
              "simple span midspan deflection", out.Def[0][k][k], expected);
    }
    // 4) Maxwell-Betti : w(xi; xj) == w(xj; xi) sur 3 travées hétérogènes
    {
        std::vector<double> E{30e9, 210e9, 30e9}, I{1e-3, 2e-4, 3e-3}, L{5.0, 7.3, 11.1};
        double h = 0.37;
        Output out(E, I, L, h);
        out.compute();
        std::vector<std::vector<double>> W;
        for (const auto& span : out.Def)
            for (const auto& row : span) W.push_back(row);
        double asym = 0.0, scale = 0.0;
        for (size_t i = 0; i < W.size(); ++i)
            for (size_t j = 0; j < W.size(); ++j) {
                asym = std::max(asym, std::abs(W[i][j] - W[j][i]));
                scale = std::max(scale, std::abs(W[i][j]));
            }
        check(asym <= 1e-12 * scale, "Maxwell-Betti symmetry (abs asym / scale)", asym / scale, 0.0);
    }

    std::printf("%d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
