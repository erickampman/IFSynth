//
//  IFSCore.cpp
//

#include "IFSCore.hpp"

#include <cmath>
#include <algorithm>
#include <cassert>
#include <complex>

// ------------------------------------------------------------
// Internal: evaluate the IFS attractor function f(x) at depth d.
//
// Uses the functional equation directly (no explicit vertex storage):
//   f(x) = sEY + (by-sEY)*(x/bx)       + s1*(f(x/bx) - sEY)     x ∈ [0,bx]
//   f(x) = by  + (sEY-by)*((x-bx)/(1-bx)) + s2*(f(...) - sEY)    x ∈ [bx,1]
//
// At depth 0 the base case is the linear tent (one iteration of PWL).
// Cost: O(depth) per call — 4 levels = 4 float ops of overhead.
// ------------------------------------------------------------

static float evaluateIFS(float x, int depth,
                          float bxNorm, float by, float sEY,
                          float s1, float s2)
{
    if (depth == 0) {
        // Base: linear tent from sEY → by → sEY
        if (x <= bxNorm) {
            return sEY + (by - sEY) * (x / bxNorm);
        } else {
            return by + (sEY - by) * ((x - bxNorm) / (1.0f - bxNorm));
        }
    }

    if (x <= bxNorm) {
        float xn = x / bxNorm;
        float fn = evaluateIFS(xn, depth - 1, bxNorm, by, sEY, s1, s2);
        return sEY + (by - sEY) * xn + s1 * (fn - sEY);
    } else {
        float xn = (x - bxNorm) / (1.0f - bxNorm);
        float fn = evaluateIFS(xn, depth - 1, bxNorm, by, sEY, s1, s2);
        return by + (sEY - by) * xn + s2 * (fn - sEY);
    }
}

// ------------------------------------------------------------
// Internal: collect the 2^n + 1 canonical vertex x-positions for n iterations.
// These are the breakpoints of the PWL approximation at the given depth.
// Fills `out` in left-to-right order; returns the count (= 2^n + 1).
// ------------------------------------------------------------

static void collectXPositionsRecursive(float xL, float xR, float bxNorm,
                                        int depth, float* out, int& idx)
{
    if (depth == 0) {
        out[idx++] = xL;
        return;
    }
    float xMid = xL + (xR - xL) * bxNorm;
    collectXPositionsRecursive(xL, xMid, bxNorm, depth - 1, out, idx);
    collectXPositionsRecursive(xMid, xR, bxNorm, depth - 1, out, idx);
}

static int collectVertexXPositions(float bxNorm, int iterations, float* out)
{
    int idx = 0;
    collectXPositionsRecursive(0.0f, 1.0f, bxNorm, iterations, out, idx);
    out[idx++] = 1.0f;   // rightmost endpoint
    return idx;           // = 2^iterations + 1
}

// ------------------------------------------------------------
// Internal: Cardinal spline evaluation for a single sample.
//
// P0..P3 are the four surrounding vertex y-values.
// t is the local parameter in [0,1] within the [P1, P2] segment.
// tension: 0 = Catmull-Rom (alpha = 0.5), 1 = linear.
// ------------------------------------------------------------

static inline float cardinalSpline(float P0, float P1, float P2, float P3,
                                   float t, float tension)
{
    // Cardinal spline tangent scale: alpha = (1-tension) * 0.5
    // At tension=0 this is standard Catmull-Rom.
    const float alpha = (1.0f - tension) * 0.5f;

    const float t2 = t * t;
    const float t3 = t2 * t;

    // Cubic Hermite basis with Cardinal tangents
    const float m1 = alpha * (P2 - P0);    // tangent at P1
    const float m2 = alpha * (P3 - P1);    // tangent at P2

    return ( 2.0f*t3 - 3.0f*t2 + 1.0f) * P1
         + (      t3 - 2.0f*t2 + t   ) * m1
         + (-2.0f*t3 + 3.0f*t2       ) * P2
         + (      t3 -      t2        ) * m2;
}

// ------------------------------------------------------------
// Public entry point
// ------------------------------------------------------------

void computeIFSWavetable(const IFSShapeParams& p, float* wavetable)
{
    // Normalise bx from [-1,1] to (0,1).
    // Clamp slightly away from the endpoints to keep the IFS non-degenerate.
    const float bxNorm = std::clamp((p.bx + 1.0f) * 0.5f, 0.01f, 0.99f);
    const float by     = p.by;
    const float sEY    = p.startEndY;
    const float s1     = std::clamp(p.ifs1Squash, -0.99f, 0.99f);
    const float s2     = std::clamp(p.ifs2Squash, -0.99f, 0.99f);
    const int   iters  = std::clamp(p.iterations, 1, IFS_MAX_ITERATIONS);
    const float smooth = std::clamp(p.smoothing,  0.0f, 1.0f);

    const int N = IFS_WAVETABLE_SIZE;

    // --- Step 1: raw fractal wavetable (sampled point-by-point) ---
    // At smoothing=1 this is the only thing we use.

    float rawBuf[IFS_WAVETABLE_SIZE];
    for (int i = 0; i < N; ++i) {
        float x = static_cast<float>(i) / static_cast<float>(N);
        rawBuf[i] = evaluateIFS(x, iters, bxNorm, by, sEY, s1, s2);
    }

    if (smooth >= 0.9999f) {
        // Pure PWL — just copy raw.
        for (int i = 0; i < N; ++i) wavetable[i] = rawBuf[i];
        return;
    }

    // --- Step 2: Cardinal-spline wavetable through the IFS vertices ---
    // Compute the 2^n + 1 vertex positions and y-values.

    float vx[IFS_MAX_VERTICES];
    float vy[IFS_MAX_VERTICES];

    const int nVerts = collectVertexXPositions(bxNorm, iters, vx);

    for (int k = 0; k < nVerts; ++k) {
        vy[k] = evaluateIFS(vx[k], iters, bxNorm, by, sEY, s1, s2);
    }

    // Enforce boundary conditions explicitly (floating-point may drift slightly).
    vy[0]         = sEY;
    vy[nVerts-1]  = sEY;

    // Render Cardinal spline into smoothBuf.
    float smoothBuf[IFS_WAVETABLE_SIZE];

    int seg = 0; // current vertex segment index

    for (int i = 0; i < N; ++i) {
        float x = static_cast<float>(i) / static_cast<float>(N);

        // Advance segment pointer (vertices are in ascending x order).
        while (seg < nVerts - 2 && x >= vx[seg + 1]) ++seg;

        // Local parameter within this segment.
        float segLen = vx[seg + 1] - vx[seg];
        float t = (segLen > 1e-6f) ? (x - vx[seg]) / segLen : 0.0f;

        // Gather four surrounding y-values with clamped boundary phantom points.
        float P0 = vy[std::max(seg - 1, 0)];
        float P1 = vy[seg];
        float P2 = vy[seg + 1];
        float P3 = vy[std::min(seg + 2, nVerts - 1)];

        smoothBuf[i] = cardinalSpline(P0, P1, P2, P3, t, 0.0f); // tension=0 → Catmull-Rom
    }

    // --- Step 3: blend raw ↔ smooth according to smoothing param ---
    // smoothing=0 → full Catmull-Rom, smoothing=1 → full PWL.

    for (int i = 0; i < N; ++i) {
        wavetable[i] = smoothBuf[i] + smooth * (rawBuf[i] - smoothBuf[i]);
    }
}

// ============================================================
// Band-limited mip pyramid (ported from PM's IFSCore)
// ============================================================

// In-place iterative radix-2 FFT (sign = -1 forward, +1 inverse; inverse is left
// unnormalised — the caller divides). IFS_WAVETABLE_SIZE is a power of two.
static void fftRadix2(std::complex<double>* a, int n, int sign)
{
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        double ang = sign * 2.0 * M_PI / len;
        std::complex<double> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (int k = 0; k < len / 2; ++k) {
                std::complex<double> u = a[i + k];
                std::complex<double> v = a[i + k + len / 2] * w;
                a[i + k]           = u + v;
                a[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

void computeIFSWavetableMips(const IFSShapeParams& p,
                             float mips[IFS_MIP_LEVELS][IFS_WAVETABLE_SIZE])
{
    const int N = IFS_WAVETABLE_SIZE;

    // Level 0 is the full waveform (respects smoothing) — render it first.
    computeIFSWavetable(p, mips[0]);

    // thread_local (not stack): keeps a ~64 KB scratch footprint off the audio-thread
    // stack when a render happens inline during process().
    static thread_local std::complex<double> spec[IFS_WAVETABLE_SIZE];
    static thread_local std::complex<double> work[IFS_WAVETABLE_SIZE];

    for (int i = 0; i < N; ++i) spec[i] = std::complex<double>(mips[0][i], 0.0);
    fftRadix2(spec, N, -1);

    // Each higher level keeps harmonics 1..H, brick-walling the rest. Brick-wall
    // truncation of a periodic waveform is exact (a shorter Fourier series), so no
    // windowing/Gibbs concern.
    for (int l = 1; l < IFS_MIP_LEVELS; ++l) {
        const int H = (N / 2) >> l;                 // top retained harmonic
        for (int k = 0; k < N; ++k) {
            work[k] = (k <= H || k >= N - H) ? spec[k] : std::complex<double>(0.0, 0.0);
        }
        fftRadix2(work, N, +1);                      // inverse (unnormalised)
        const double inv = 1.0 / N;
        for (int i = 0; i < N; ++i) mips[l][i] = static_cast<float>(work[i].real() * inv);
    }
}

// ------------------------------------------------------------
// Single-point evaluator (for per-sample shape modulation)
// ------------------------------------------------------------

float evaluateIFSPoint(float x, const IFSShapeParams& p)
{
    const float bxNorm = std::clamp((p.bx + 1.0f) * 0.5f, 0.01f, 0.99f);
    const float s1     = std::clamp(p.ifs1Squash, -0.99f, 0.99f);
    const float s2     = std::clamp(p.ifs2Squash, -0.99f, 0.99f);
    const int   iters  = std::clamp(p.iterations, 1, IFS_MAX_ITERATIONS);
    // Wrap x to [0, 1) — caller may pass a phase-modulated value slightly outside range.
    x = x - std::floor(x);
    return evaluateIFS(x, iters, bxNorm, p.by, p.startEndY, s1, s2);
}

// ------------------------------------------------------------
// Chunked smoothed evaluation (see IFSVertices in the header). Mirrors the vertex derivation
// and Cardinal-spline blend of computeIFSWavetable(), but split so the shape-dependent part is
// done once per chunk and the per-sample cost is just a segment lookup + spline + blend.
// ------------------------------------------------------------

void precomputeIFSVertices(const IFSShapeParams& p, IFSVertices& out)
{
    out.bxNorm = std::clamp((p.bx + 1.0f) * 0.5f, 0.01f, 0.99f);
    out.by     = p.by;
    out.sEY    = p.startEndY;
    out.s1     = std::clamp(p.ifs1Squash, -0.99f, 0.99f);
    out.s2     = std::clamp(p.ifs2Squash, -0.99f, 0.99f);
    out.iters  = std::clamp(p.iterations, 1, IFS_MAX_ITERATIONS);
    out.smooth = std::clamp(p.smoothing, 0.0f, 1.0f);

    out.nVerts = collectVertexXPositions(out.bxNorm, out.iters, out.vx);
    for (int k = 0; k < out.nVerts; ++k)
        out.vy[k] = evaluateIFS(out.vx[k], out.iters, out.bxNorm, out.by, out.sEY, out.s1, out.s2);
    out.vy[0]              = out.sEY;
    out.vy[out.nVerts - 1] = out.sEY;
}

float evaluateIFSSmoothed(float x, const IFSVertices& v)
{
    x = x - std::floor(x);
    const float raw = evaluateIFS(x, v.iters, v.bxNorm, v.by, v.sEY, v.s1, v.s2);
    if (v.smooth >= 0.9999f)
        return raw;

    int seg = 0;
    while (seg < v.nVerts - 2 && x >= v.vx[seg + 1]) ++seg;
    const float segLen = v.vx[seg + 1] - v.vx[seg];
    const float t = (segLen > 1e-6f) ? (x - v.vx[seg]) / segLen : 0.0f;
    const float P0 = v.vy[std::max(seg - 1, 0)];
    const float P1 = v.vy[seg];
    const float P2 = v.vy[seg + 1];
    const float P3 = v.vy[std::min(seg + 2, v.nVerts - 1)];
    const float sm = cardinalSpline(P0, P1, P2, P3, t, 0.0f);
    return sm + v.smooth * (raw - sm);
}

// ============================================================
// IFS Envelope — 3-transform implementation
// ============================================================

// Internal 3-transform recursive evaluator.
// x1 < x2, and all three segment widths (x1, x2-x1, 1-x2) are >= 0.01 (guaranteed by caller).
//
// Continuity (fixed 2026-09-14): each segment adds its recursive fractal detail as
// s * (fn(xn) - y0). The sub-fractal fn starts AND ends at y0 (fn(0) == y0 exactly, and with this
// same anchoring fn(1) == y3 == y0 since an envelope's ends are both startEndY), so the added term
// is zero at both xn==0 and xn==1 — i.e. it vanishes at every segment boundary. This is what keeps
// the whole envelope CONTINUOUS at the Blue/Green junctions while leaving the segment endpoints
// (y0,y1,y2,y3) and the fractal wiggle's magnitude untouched. The previous port anchored segments
// 2 and 3 to y1 and y2 respectively, which left a step of s2*(y0-y1) at x1 and s3*(y0-y2) at x2 —
// a real discontinuity that clicked through a VCA (and that Match-Y legato exposed as a pop when it
// matched a level inside one of those steps' gaps). y0 is the correct anchor because it's the value
// fn actually returns to at both of its own ends; anchoring to the segment's own start value only
// happens to work for segment 1 (whose start IS y0).
static float evaluateIFS3(float x, int depth,
                           float x1, float x2,
                           float y0, float y1, float y2, float y3,
                           float s1, float s2, float s3)
{
    if (x <= x1) {
        float xn = x / x1;
        if (depth == 0) return y0 + (y1 - y0) * xn;
        float fn = evaluateIFS3(xn, depth - 1, x1, x2, y0, y1, y2, y3, s1, s2, s3);
        return y0 + (y1 - y0) * xn + s1 * (fn - y0);
    }
    if (x <= x2) {
        float w  = x2 - x1;
        float xn = (x - x1) / w;
        if (depth == 0) return y1 + (y2 - y1) * xn;
        float fn = evaluateIFS3(xn, depth - 1, x1, x2, y0, y1, y2, y3, s1, s2, s3);
        return y1 + (y2 - y1) * xn + s2 * (fn - y0);
    }
    {
        float w  = 1.0f - x2;
        float xn = (x - x2) / w;
        if (depth == 0) return y2 + (y3 - y2) * xn;
        float fn = evaluateIFS3(xn, depth - 1, x1, x2, y0, y1, y2, y3, s1, s2, s3);
        return y2 + (y3 - y2) * xn + s3 * (fn - y0);
    }
}

// Collect 3^depth vertex x-positions (left boundaries only; caller appends 1.0).
static void collectEnvVerticesRecursive(float xL, float xR,
                                         float x1, float x2,
                                         int depth, float* out, int& idx)
{
    if (depth == 0) { out[idx++] = xL; return; }   // leaf only — keeps count at 3^depth
    float mid1 = xL + (xR - xL) * x1;
    float mid2 = xL + (xR - xL) * x2;
    collectEnvVerticesRecursive(xL,   mid1, x1, x2, depth - 1, out, idx);
    collectEnvVerticesRecursive(mid1, mid2, x1, x2, depth - 1, out, idx);
    collectEnvVerticesRecursive(mid2, xR,   x1, x2, depth - 1, out, idx);
}

static int collectEnvVertexXPositions(float x1, float x2, int depth, float* out)
{
    int idx = 0;
    collectEnvVerticesRecursive(0.0f, 1.0f, x1, x2, depth, out, idx);
    out[idx++] = 1.0f;
    return idx;   // = 3^depth + 1
}

void computeIFSEnvWavetable(const IFSEnvShapeParams& p, float* wavetable)
{
    // Sort Blue/Green by x so x1 < x2; y-values follow.
    float x1, x2, y1, y2;
    if (p.blueX <= p.greenX) {
        x1 = p.blueX;  y1 = p.blueY;
        x2 = p.greenX; y2 = p.greenY;
    } else {
        x1 = p.greenX; y1 = p.greenY;
        x2 = p.blueX;  y2 = p.blueY;
    }

    // Clamp so every segment is at least 0.01 wide.
    x1 = std::clamp(x1, 0.01f, 0.98f);
    x2 = std::clamp(x2, x1 + 0.01f, 0.99f);

    const float y0 = p.startEndY;
    const float y3 = p.startEndY;
    const float s1 = std::clamp(p.ifs1Squash, -0.99f, 0.99f);
    const float s2 = std::clamp(p.ifs2Squash, -0.99f, 0.99f);
    const float s3 = std::clamp(p.ifs3Squash, -0.99f, 0.99f);
    const int   iters  = std::clamp(p.iterations, 1, 8);
    const float smooth = std::clamp(p.smoothing,  0.0f, 1.0f);

    const int N = IFS_WAVETABLE_SIZE;

    // --- Step 1: raw fractal wavetable ---
    float rawBuf[IFS_WAVETABLE_SIZE];
    for (int i = 0; i < N; ++i) {
        float x = static_cast<float>(i) / static_cast<float>(N);
        rawBuf[i] = evaluateIFS3(x, iters, x1, x2, y0, y1, y2, y3, s1, s2, s3);
    }

    if (smooth >= 0.9999f) {
        std::copy_n(rawBuf, N, wavetable);
        return;
    }

    // --- Step 2: Catmull-Rom through IFS vertices (depth capped at IFS_ENV_MAX_SMOOTH_DEPTH) ---
    const int smoothDepth = std::min(iters, IFS_ENV_MAX_SMOOTH_DEPTH);

    float vx[IFS_ENV_MAX_VERTICES];
    float vy[IFS_ENV_MAX_VERTICES];
    const int nVerts = collectEnvVertexXPositions(x1, x2, smoothDepth, vx);

    for (int k = 0; k < nVerts; ++k) {
        vy[k] = evaluateIFS3(vx[k], iters, x1, x2, y0, y1, y2, y3, s1, s2, s3);
    }
    vy[0]         = y0;
    vy[nVerts - 1] = y3;

    float smoothBuf[IFS_WAVETABLE_SIZE];
    int seg = 0;
    for (int i = 0; i < N; ++i) {
        float x = static_cast<float>(i) / static_cast<float>(N);
        while (seg < nVerts - 2 && x >= vx[seg + 1]) ++seg;
        float segLen = vx[seg + 1] - vx[seg];
        float t      = (segLen > 1e-6f) ? (x - vx[seg]) / segLen : 0.0f;
        float P0 = vy[std::max(seg - 1, 0)];
        float P1 = vy[seg];
        float P2 = vy[seg + 1];
        float P3 = vy[std::min(seg + 2, nVerts - 1)];
        smoothBuf[i] = cardinalSpline(P0, P1, P2, P3, t, 0.0f);
    }

    // --- Step 3: blend raw ↔ smooth ---
    for (int i = 0; i < N; ++i) {
        wavetable[i] = smoothBuf[i] + smooth * (rawBuf[i] - smoothBuf[i]);
    }
}
