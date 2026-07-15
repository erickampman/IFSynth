// pm_module_specs.cpp — the trimmed v1 module registry. getModuleInitSpecs() lists exactly the
// six modules the fixed v1 voice instantiates (PitchConverter, Slew, SinOsc, ADSR envelope,
// Combine VCA, Final). Each entry: {address, param group, init/deinit/process/midi/param/reset/
// sampleRate procs}. Envelope is ADSR (the v1 choice).

#include "ModuleInitSpecs.hpp"

#include "ADSRModule.hpp"
#include "CombineModule.hpp"
#include "FinalModule.hpp"
#include "IFSEnvModule.hpp"
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
      {combine1Address, &combineConstantsGroup, nullptr, nullptr, ProcessCombineModule, nullptr,
       nullptr, nullptr, nullptr},
      {finalAddress, &finalConstantsGroup, InitFinalModule, DeinitFinalModule, ProcessFinalModule,
       MIDIFinalModule, nullptr, nullptr, SampleRateFinalModule},
   };
   return specs;
}
