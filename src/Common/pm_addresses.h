// pm_addresses.h — the v1 voice's module + input addresses. Rather than vendor the large
// ExtensionParameterAddresses.h, we define just the handful the fixed v1 patch needs. Each is
// MODULE_PARAM_ADDRESS(kind, instance, item) — the same packed value used as the CLAP clap_id.

#ifndef PM_ADDRESSES_H
#define PM_ADDRESSES_H

#include "ParamAddressMacros.h"
#include "pm_compat.h"
#include "pm_params.h"

inline constexpr AUParameterAddress pitchConverter1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindPitchConverter, ModuleInstance1, 0);

inline constexpr AUParameterAddress slew1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSlew, ModuleInstance1, 0);
inline constexpr AUParameterAddress slew1InputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSlew, ModuleInstance1, SlewParamItemInput);
// Pitch glide: one knob drives Rise and Fall together (symmetric slew).
inline constexpr AUParameterAddress slew1RiseAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSlew, ModuleInstance1, SlewParamItemRise);
inline constexpr AUParameterAddress slew1FallAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSlew, ModuleInstance1, SlewParamItemFall);

inline constexpr AUParameterAddress sinOsc1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSinOsc, ModuleInstance1, 0);
inline constexpr AUParameterAddress sinOsc1PitchInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSinOsc, ModuleInstance1, SinOscParamItemPitchInput);
// Osc 2 pitch mod-input port (a source's output connects here; depth is FreqModLevel).
inline constexpr AUParameterAddress sinOsc1PitchModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSinOsc, ModuleInstance1, SinOscParamItemModPitch);

inline constexpr AUParameterAddress ifsOsc1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, 0);
inline constexpr AUParameterAddress ifsOsc1PitchInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemPitchInput);
// Hard-sync input (the sine osc's output feeds this; per-channel zero-crossing resets phase).
inline constexpr AUParameterAddress ifsOsc1SyncInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemSyncInput);
// IFSOsc mod-input ports (a source's output connects here; depth is the matching ModLevel).
inline constexpr AUParameterAddress ifsOsc1PitchModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemPitchModInput);
inline constexpr AUParameterAddress ifsOsc1BxModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemBxModInput);
inline constexpr AUParameterAddress ifsOsc1ByModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemByModInput);
inline constexpr AUParameterAddress ifsOsc1Squash1ModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemSquash1ModInput);
inline constexpr AUParameterAddress ifsOsc1Squash2ModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemSquash2ModInput);

inline constexpr AUParameterAddress ifsEnv1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, 0);
inline constexpr AUParameterAddress ifsEnv1GateInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemGateInput);
// LegatoMode: 0 = NoteSteal (retrigger), 1 = FindMatchingY (jump to the earliest phase whose
// level matches the current output). Mono mode turns this on so retriggers don't click.
inline constexpr AUParameterAddress ifsEnv1LegatoModeAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemLegatoMode);

inline constexpr AUParameterAddress ifsLFO1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance1, 0);

// Second IFS envelope / LFO instances — mod sources (not connected in the graph yet).
inline constexpr AUParameterAddress ifsEnv2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, 0);
inline constexpr AUParameterAddress ifsEnv2LegatoModeAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemLegatoMode);
inline constexpr AUParameterAddress ifsLFO2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance2, 0);

// CV mixers (2). Base address + the two connection-input ports.
inline constexpr AUParameterAddress cvMixer1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, 0);
inline constexpr AUParameterAddress cvMixer1LFOInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, CVMixerParamItemLFOInput);
inline constexpr AUParameterAddress cvMixer1EnvInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, CVMixerParamItemEnvInput);
inline constexpr AUParameterAddress cvMixer2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance2, 0);
inline constexpr AUParameterAddress cvMixer2LFOInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance2, CVMixerParamItemLFOInput);
inline constexpr AUParameterAddress cvMixer2EnvInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance2, CVMixerParamItemEnvInput);

// Low-pass filter (24 dB/oct), per voice, inserted between the IFS oscillator and the VCA.
inline constexpr AUParameterAddress lpf1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, 0);
inline constexpr AUParameterAddress lpf1InputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemInput);
inline constexpr AUParameterAddress lpf1FreqModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemModFreqInput);
// Second freq-mod input, hardwired to the note pitch for key tracking (its level = FreqModLevel2).
inline constexpr AUParameterAddress lpf1KeyTrackInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemModFreqInput2);
inline constexpr AUParameterAddress lpf1QModInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemModQInput);

inline constexpr AUParameterAddress adsr1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindADSR, ModuleInstance1, 0);

inline constexpr AUParameterAddress combine1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance1, 0);
inline constexpr AUParameterAddress combine1Input1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance1, CombineParamItemInput1);
inline constexpr AUParameterAddress combine1Input2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance1, CombineParamItemInput2);

// Level — scales the sine osc's audible mix level (its raw output separately drives IFS sync).
inline constexpr AUParameterAddress level1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindLevel, ModuleInstance1, 0);
inline constexpr AUParameterAddress level1InputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindLevel, ModuleInstance1, LevelParamItemInput);

// Combine 2 — the oscillator mixer (Add): SinOsc + IFSOsc summed, then into the LPF.
inline constexpr AUParameterAddress combine2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance2, 0);
inline constexpr AUParameterAddress combine2Input1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance2, CombineParamItemInput1);
inline constexpr AUParameterAddress combine2Input2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance2, CombineParamItemInput2);
inline constexpr AUParameterAddress combine2TypeAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance2, CombineParamItemType);

inline constexpr AUParameterAddress finalAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindFinalOutput, ModuleInstance1, 0);
inline constexpr AUParameterAddress finalInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindFinalOutput, ModuleInstance1, FinalOutputParamItemInput);

// Second pitch inputs: the pitch-bend CV, which bypasses the Slew (glide must not smear a bend).
inline constexpr AUParameterAddress sinOsc1PitchMod2InputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSinOsc, ModuleInstance1, SinOscParamItemModPitch2);
inline constexpr AUParameterAddress sinOsc1PitchMod2LevelAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSinOsc, ModuleInstance1, SinOscParamItemFreqModLevel2);
inline constexpr AUParameterAddress ifsOsc1PitchMod2InputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemPitchMod2Input);

// --- Velocity → loudness (the VCA's third factor, shaped by the Velocity dropdown) ----------
inline constexpr AUParameterAddress velocity1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindVelocityConverter, ModuleInstance1, 0);
inline constexpr AUParameterAddress velocity1MinAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindVelocityConverter, ModuleInstance1, VelocityParamItemMin);
inline constexpr AUParameterAddress velocity1CurveAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindVelocityConverter, ModuleInstance1, VelocityParamItemCurve);
inline constexpr AUParameterAddress combine1Input3Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance1, CombineParamItemInput3);

// --- Pitch bend converter (channel bend + MIDI 2.0 per-note bend → octaves) ---------------
inline constexpr AUParameterAddress pitchBend1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindPitchBendConverter, ModuleInstance1, 0);

// The performance-control modulation sources: "Pressure" (channel pressure per channel, overridden per
// note by poly pressure) and "Timbre" (CC74 per channel, overridden by MIDI 2.0 per-note controller
// #74, which CLAP's Brightness note expression arrives as).
inline constexpr AUParameterAddress channelPressure1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindChannelPressure, ModuleInstance1, 0);
inline constexpr AUParameterAddress timbre1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindControlChangeConverter, ModuleInstance1, 0);
inline constexpr uint8_t kTimbreCC = 74;   // MPE's "slide"; MIDI 2.0 registered per-note controller #74

#endif // PM_ADDRESSES_H
