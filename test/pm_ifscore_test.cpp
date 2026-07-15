// pm_ifscore_test — validates the chunked-eval split of IFSCore's smoothing: the per-point
// evaluateIFSSmoothed() (fed precomputeIFSVertices()) must match computeIFSWavetable() at the
// wavetable sample points, and must fall back to the raw point eval at smoothing = 1. This is
// the DSP that IFSOsc's shape-modulation path relies on.

#include "IFSCore.hpp"

#include <cmath>
#include <cstdio>

static int g_fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", (msg)); ++g_fails; } } while (0)

int main()
{
    // {ifs1Squash, ifs2Squash, bx, by, startEndY, smoothing, iterations}
    const IFSShapeParams shapes[] = {
        {},                                             // struct defaults
        { 0.7f, -0.3f,  0.2f,  0.4f, -0.6f, 0.3f, 4},
        {-0.5f,  0.8f, -0.4f, -0.2f,  0.0f, 0.8f, 3},
        { 0.9f, -0.9f,  0.0f,  0.9f, -1.0f, 0.0f, 4},   // full Catmull-Rom
    };

    static float table[IFS_WAVETABLE_SIZE];
    int mism = 0;
    float worst = 0.0f;
    for (const auto& s : shapes) {
        computeIFSWavetable(s, table);
        IFSVertices v;
        precomputeIFSVertices(s, v);
        for (int i = 0; i < IFS_WAVETABLE_SIZE; i += 7) {
            const float x = static_cast<float>(i) / IFS_WAVETABLE_SIZE;
            const float d = std::fabs(evaluateIFSSmoothed(x, v) - table[i]);
            if (d > worst) worst = d;
            if (d > 1e-3f) ++mism;
        }
    }
    CHECK(mism == 0, "evaluateIFSSmoothed matches computeIFSWavetable at sample points");
    std::printf("IFSCore: sample-point mismatches=%d worst=%.2e\n", mism, worst);

    // smoothing = 1 → pure PWL, so smoothed eval == raw point eval.
    IFSShapeParams pwl{};
    pwl.smoothing = 1.0f;
    IFSVertices vr;
    precomputeIFSVertices(pwl, vr);
    int rawMism = 0;
    for (int i = 0; i < 64; ++i) {
        const float x = static_cast<float>(i) / 64.0f;
        if (std::fabs(evaluateIFSSmoothed(x, vr) - evaluateIFSPoint(x, pwl)) > 1e-5f) ++rawMism;
    }
    CHECK(rawMism == 0, "smoothing=1 eval equals raw point eval");

    if (g_fails == 0) std::printf("ALL PASS\n");
    return g_fails ? 1 : 0;
}
