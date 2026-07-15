// pm_module_specs.cpp — the trimmed v1 module registry. getModuleInitSpecs() lists exactly the
// six modules the fixed v1 voice instantiates (PitchConverter, Slew, SinOsc, ADSR envelope,
// Combine VCA, Final). Each entry: {address, param group, init/deinit/process/midi/param/reset/
// sampleRate procs}. Envelope is ADSR (the v1 choice).

#include "ModuleInitSpecs.hpp"

#include "ADSRModule.hpp"
#include "CVMixerModule.hpp"
#include "CombineModule.hpp"
#include "FinalModule.hpp"
#include "IFSEnvModule.hpp"
#include "IFSLFOModule.hpp"
#include "IFSOscModule.hpp"
#include "PitchConverterModule.hpp"
#include "SinOscillatorModule.hpp"
#include "SlewModule.hpp"
#include "pm_addresses.h"
#include "pm_param_data.h"

const std::vector<ModuleInitSpec>& getModuleInitSpecs() {
   static const std::vector<ModuleInitSpec> specs = {
      {pitchConverter1Address, &pitchConverterModuleConstantsGroup, InitPitchConverter,
       DeinitPitchConverter, ProcessPitchConverter, MIDIPitchConverter, nullptr, nullptr, nullptr},
      {slew1Address, &slewModuleConstantsGroup, InitSlewModule, DeinitSlewModule, ProcessSlewModule,
       nullptr, nullptr, nullptr, nullptr},
      {sinOsc1Address, &sinOscConstantsGroup, InitSinOscillator, DeinitSinOscillator,
       ProcessSinOscillator, MIDISinOscillator, nullptr, ResetSinOscillator, SampleRateSinOscillator},
      // IFSOsc is the active source (see buildDefaultPatch); SinOsc stays instantiated but
      // unconnected — available as a second source / future phase-mod source.
      {ifsOsc1Address, &ifsOscConstantsGroup, InitIFSOscModule, DeinitIFSOscModule,
       ProcessIFSOscModule, MIDIIFSOscModule, ParamIFSOscModule, ResetIFSOscModule,
       SampleRateIFSOscModule},
      {adsr1Address, &adsrConstantsGroup, InitADSRModule, DeinitADSRModule, ProcessADSRModule,
       MIDIProcADSRModule, ParamADSR, nullptr, SampleRateADSRModule},
      // IFSEnv is the active amplitude envelope (see buildDefaultPatch); ADSR stays instantiated
      // but unconnected.
      {ifsEnv1Address, &ifsEnvConstantsGroup, InitIFSEnvModule, DeinitIFSEnvModule,
       ProcessIFSEnvModule, MIDIIFSEnvModule, ParamIFSEnvModule, ResetIFSEnvModule,
       SampleRateIFSEnvModule},
      // IFSLFO is instantiated (params exposed, UI visible) but NOT connected in the graph yet
      // — it's a mod source awaiting the CV/mod system. Free-running, no MIDI/reset/sr procs.
      {ifsLFO1Address, &ifsLFOConstantsGroup, InitIFSLFOModule, DeinitIFSLFOModule,
       ProcessIFSLFOModule, nullptr, ParamIFSLFOModule, nullptr, nullptr},
      // Second env + LFO instances (mod sources, also unconnected). Same procs, instance-2
      // addresses distinguish them; each has its own state/params.
      {ifsEnv2Address, &ifsEnvConstantsGroup, InitIFSEnvModule, DeinitIFSEnvModule,
       ProcessIFSEnvModule, MIDIIFSEnvModule, ParamIFSEnvModule, ResetIFSEnvModule,
       SampleRateIFSEnvModule},
      {ifsLFO2Address, &ifsLFOConstantsGroup, InitIFSLFOModule, DeinitIFSLFOModule,
       ProcessIFSLFOModule, nullptr, ParamIFSLFOModule, nullptr, nullptr},
      // Two CV mixers (combine LFO+Env+CC). Inputs are wired in buildDefaultPatch; their outputs
      // await the destination mod-inputs, so they're dormant for now.
      {cvMixer1Address, &cvMixerConstantsGroup, InitCVMixerModule, DeinitCVMixerModule,
       ProcessCVMixerModule, MIDICVMixerModule, nullptr, nullptr, nullptr},
      {cvMixer2Address, &cvMixerConstantsGroup, InitCVMixerModule, DeinitCVMixerModule,
       ProcessCVMixerModule, MIDICVMixerModule, nullptr, nullptr, nullptr},
      {combine1Address, &combineConstantsGroup, nullptr, nullptr, ProcessCombineModule, nullptr,
       nullptr, nullptr, nullptr},
      {finalAddress, &finalConstantsGroup, InitFinalModule, DeinitFinalModule, ProcessFinalModule,
       MIDIFinalModule, nullptr, nullptr, SampleRateFinalModule},
   };
   return specs;
}
