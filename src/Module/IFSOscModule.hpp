//
//  IFSOscModule.hpp
//
//  IFS fractal wavetable oscillator — the active sound source in IFSynth's voice.
//  Ported from PolygraphModular's IFSOscModule (AUv3). This IFSynth port is trimmed to the
//  simple v1 param set (no modulation matrix) and replaces the Apple GCD background wavetable
//  render with a single-threaded, coalesced render at the top of the process block.
//

#pragma once
#include "Module.hpp"
#include "ModuleSharedState.hpp"

// One shared band-limited mip pyramid per placed module (voice-independent). Listed as the module's
// sharedStateCreateProc in its ModuleInitSpec; defined in IFSOscModule.cpp.
ModuleSharedState* CreateIFSOscSharedState();

const void InitIFSOscModule(uint32_t count, Module* module);
const void DeinitIFSOscModule(Module* module);
const void ResetIFSOscModule(Module* module);
const void SampleRateIFSOscModule(Module* module, double newSampleRate);
const void ParamIFSOscModule(Module* module, AUParameterAddress address, AUValue value);
const KPBuffPair* ProcessIFSOscModule(uint64_t processCallID, uint32_t count, Module* module);
void MIDIIFSOscModule(const pm::UmpMessage& message, Module* module);
