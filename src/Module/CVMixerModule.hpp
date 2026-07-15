//
//  CVMixerModule.hpp — combines mod sources into one CV output.
//
//  Inputs: an LFO connection, an Env connection, and a MIDI CC value. Each input can be
//  enabled/disabled independently; a disabled input is SKIPPED (not treated as 0). Mode picks
//  whether the enabled inputs are summed or multiplied. There is deliberately no per-input
//  level — the modulation depth lives on the destination mod-input side. IFSynth-specific (not
//  from PolygraphModular).
//

#pragma once
#include "Module.hpp"

const void InitCVMixerModule(uint32_t count, Module* module);
const void DeinitCVMixerModule(Module* module);
const KPBuffPair* ProcessCVMixerModule(uint64_t processCallID, uint32_t count, Module* module);
void MIDICVMixerModule(const pm::UmpMessage& message, Module* module);
