// pm_param_data.h — per-module parameter constant groups (converted from the Objective-C
// ParamGroupData.m). Each module's group is added as the module is ported. NOTE: for the DSP
// port these carry the correct item enums + default values (what addParams / the process
// functions read); full metadata fidelity (units, value strings, exact ranges for the CLAP
// params UI) is finished in L2 when the params extension is built.

#ifndef PM_PARAM_DATA_H
#define PM_PARAM_DATA_H

#include "pm_params.h"

extern const ModuleParamConstantsGroup sinOscConstantsGroup;
extern const ModuleParamConstantsGroup ifsOscConstantsGroup;
extern const ModuleParamConstantsGroup adsrConstantsGroup;
extern const ModuleParamConstantsGroup combineConstantsGroup;
extern const ModuleParamConstantsGroup finalConstantsGroup;
extern const ModuleParamConstantsGroup slewModuleConstantsGroup;
extern const ModuleParamConstantsGroup pitchConverterModuleConstantsGroup;

#endif // PM_PARAM_DATA_H
