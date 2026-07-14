//
//  IFSCore.hpp
//
//  Shared IFS fractal waveform computation for IFSOscModule and IFSLFOModule.
//
//  The waveform is defined by two affine contractive maps T1, T2 on [0,1]×[-1,1]:
//
//    T1(x, y) = ( bx * x,
//                 sEY + (by - sEY)*x + ifs1Squash*(y - sEY) )
//
//    T2(x, y) = ( bx + (1-bx)*x,
//                 by + (sEY - by)*x + ifs2Squash*(y - sEY) )
//
//  where bx  = (params.bx + 1) / 2  (normalised to [0,1])
//        sEY = params.startEndY
//        by  = params.by
//
//  Both maps are contractive for |ifs1Squash| < 1 and |ifs2Squash| < 1,
//  which is guaranteed by the param range [-0.99, 0.99].
//
//  The attractor is the unique continuous function f:[0,1]→[-1,1] satisfying:
//    f(x) = T1_y(f(x/bx))             for x ∈ [0, bx]
//    f(x) = T2_y(f((x-bx)/(1-bx)))   for x ∈ [bx, 1]
//  with boundary conditions f(0) = f(1) = sEY, f(bx) = by.
//

#pragma once

#include <cstdint>

// ------------------------------------------------------------
// Wavetable constants
// ------------------------------------------------------------

static constexpr int IFS_WAVETABLE_SIZE  = 2048;
static constexpr int IFS_MAX_ITERATIONS  = 4;
// 2^IFS_MAX_ITERATIONS + 1 = 17 vertices maximum
static constexpr int IFS_MAX_VERTICES    = (1 << IFS_MAX_ITERATIONS) + 1;

// ------------------------------------------------------------
// Shape parameter bundle (shared between osc and LFO state)
// ------------------------------------------------------------

struct IFSShapeParams {
    float ifs1Squash  =  0.5f;   // T1 y-scaling,  range (-1, 1) excl.
    float ifs2Squash  = -0.5f;   // T2 y-scaling,  range (-1, 1) excl.
    float bx          =  0.0f;   // B x-position,  range [-1, 1]  → normalised to (0,1)
    float by          =  0.5f;   // B y-position,  range [-1, 1]
    float startEndY   = -1.0f;   // A and C y-coordinate, range [-1, 1]
    float smoothing   =  0.5f;   // 0 = Catmull-Rom, 1 = linear PWL
    int   iterations  =  4;      // 3 or 4
};

// ------------------------------------------------------------
// Core rendering entry point
// ------------------------------------------------------------

/// Fill `wavetable` (IFS_WAVETABLE_SIZE floats) with one period of the
/// IFS fractal waveform described by `p`.
///
/// `wavetable` must be pre-allocated by the caller.
void computeIFSWavetable(const IFSShapeParams& p, float* wavetable);

/// Evaluate the IFS attractor at a single phase position x ∈ [0, 1).
///
/// Returns the raw PWL fractal value — no Catmull-Rom smoothing is applied.
/// Used for per-sample computation when shape parameters are audio-rate modulated.
float evaluateIFSPoint(float x, const IFSShapeParams& p);

// ------------------------------------------------------------
// IFS Envelope — 3-transform variant
// ------------------------------------------------------------

/// Maximum depth used for Catmull-Rom vertex collection in the envelope.
/// 3^5 + 1 = 244 vertices, safely under IFS_WAVETABLE_SIZE (2048).
static constexpr int IFS_ENV_MAX_SMOOTH_DEPTH = 5;
static constexpr int IFS_ENV_MAX_VERTICES     = 248; // 3^5+1 = 244, with margin

/// Shape parameter bundle for IFSEnv (3 affine maps, 2 intermediate points).
struct IFSEnvShapeParams {
    float blueX      = 0.33f;  // Blue point x-position  [0, 1]
    float blueY      = 1.0f;   // Blue point y-value      [-1, 1]
    float greenX     = 0.67f;  // Green point x-position  [0, 1]
    float greenY     = 0.0f;   // Green point y-value      [-1, 1]
    float startEndY  = -1.0f;  // Start and end y-value   [-1, 1]
    float ifs1Squash = 0.5f;   // T1 y-scaling            (-1, 1) excl.
    float ifs2Squash = -0.5f;  // T2 y-scaling            (-1, 1) excl.
    float ifs3Squash = 0.5f;   // T3 y-scaling            (-1, 1) excl.
    float smoothing  = 0.5f;   // 0 = Catmull-Rom, 1 = linear PWL
    int   iterations = 3;      // 1–8 actual iterations
    int   holdMode   = 0;      // 0 = None, 1 = Blue, 2 = Green
};

/// Fill `wavetable` (IFS_WAVETABLE_SIZE floats) with the IFS envelope shape.
/// Blue and Green x-positions are sorted internally; any segment narrower than
/// 0.01 is treated as linear (no fractal detail) to avoid spline instability.
void computeIFSEnvWavetable(const IFSEnvShapeParams& p, float* wavetable);
