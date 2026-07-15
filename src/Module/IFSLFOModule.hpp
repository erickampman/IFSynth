//
//  IFSLFOModule.hpp
//
//  IFS fractal LFO — a free-running modulation source (2-transform IFS shape, same as IFSOsc).
//  Ported from PolygraphModular, trimmed to the simple v1 set: rate + the five shape params +
//  smoothing/iterations (the mod matrix, FM/reset inputs and L/R phase are deferred). Not wired
//  into the audio graph yet — it awaits the CV/mod system. Same coalesced single-threaded
//  wavetable render as IFSOsc (no Apple GCD / Accelerate).
//

#pragma once
#include "Module.hpp"

const void InitIFSLFOModule(uint32_t count, Module* module);
const void DeinitIFSLFOModule(Module* module);
const void ParamIFSLFOModule(Module* module, AUParameterAddress address, AUValue value);
const KPBuffPair* ProcessIFSLFOModule(uint64_t processCallID, uint32_t count, Module* module);
