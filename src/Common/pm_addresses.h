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

inline constexpr AUParameterAddress sinOsc1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindSinOsc, ModuleInstance1, 0);
inline constexpr AUParameterAddress sinOsc1PitchInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindSinOsc, ModuleInstance1, SinOscParamItemPitchInput);

inline constexpr AUParameterAddress ifsOsc1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, 0);
inline constexpr AUParameterAddress ifsOsc1PitchInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemPitchInput);

inline constexpr AUParameterAddress ifsEnv1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, 0);
inline constexpr AUParameterAddress ifsEnv1GateInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemGateInput);

inline constexpr AUParameterAddress ifsLFO1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance1, 0);

// Second IFS envelope / LFO instances — mod sources (not connected in the graph yet).
inline constexpr AUParameterAddress ifsEnv2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, 0);
inline constexpr AUParameterAddress ifsLFO2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance2, 0);

inline constexpr AUParameterAddress adsr1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindADSR, ModuleInstance1, 0);

inline constexpr AUParameterAddress combine1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance1, 0);
inline constexpr AUParameterAddress combine1Input1Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance1, CombineParamItemInput1);
inline constexpr AUParameterAddress combine1Input2Address =
   MODULE_PARAM_ADDRESS(ModuleParamKindCombine, ModuleInstance1, CombineParamItemInput2);

inline constexpr AUParameterAddress finalAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindFinalOutput, ModuleInstance1, 0);
inline constexpr AUParameterAddress finalInputAddress =
   MODULE_PARAM_ADDRESS(ModuleParamKindFinalOutput, ModuleInstance1, FinalOutputParamItemInput);

#endif // PM_ADDRESSES_H
