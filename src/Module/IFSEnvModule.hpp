//
//  IFSEnvModule.hpp
//
//  IFS fractal envelope generator — the voice's amplitude envelope (replaces ADSR in the
//  graph). Ported from PolygraphModular's IFSEnvModule (AUv3). Like the IFSOsc port, the Apple
//  GCD background wavetable render is replaced by a single-threaded coalesced render at the top
//  of the process block. The full param set is kept (gate, duration, the 3-transform shape,
//  smoothing/iterations, hold + legato modes); modulation inputs were never part of this module.
//

#pragma once
#include "Module.hpp"

const void InitIFSEnvModule(uint32_t count, Module* module);
const void DeinitIFSEnvModule(Module* module);
const void ResetIFSEnvModule(Module* module);
const void SampleRateIFSEnvModule(Module* module, double newSampleRate);
const void ParamIFSEnvModule(Module* module, AUParameterAddress address, AUValue value);
const KPBuffPair* ProcessIFSEnvModule(uint64_t processCallID, uint32_t count, Module* module);
void MIDIIFSEnvModule(const pm::UmpMessage& message, Module* module);
