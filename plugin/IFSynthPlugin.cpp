// IFSynthPlugin.cpp — DSP side: DPF Plugin wrapper around the vendored PolygraphModular
// voice engine. DPF hands us MIDI 1.0 events + a parameter interface and generates the CLAP
// (VST3/LV2) wrapper; all the actual synthesis is the same VoiceManager the CLAP build used.
//
//   MIDI events  → pm::UmpMessage → VoiceManager (polyphony / stealing / mono)
//   ADSR params  → VoiceManager::setParameter() (propagates to every voice, live)
//   run()        → chunked VoiceManager::process() + master gain

#include "DistrhoPlugin.hpp"

#include <algorithm>
#include <memory>

#include "IFSynthParams.h"

#include "ParamAddressMacros.h"
#include "VoiceManager.hpp"
#include "pm_addresses.h"
#include "pm_midi.h"
#include "pm_params.h"

START_NAMESPACE_DISTRHO

using namespace ifsynth;

class IFSynthPlugin : public Plugin
{
public:
    IFSynthPlugin()
        : Plugin(kParamCount, 0, 0) // params, programs, states
    {
        for (uint32_t i = 0; i < kParamCount; ++i)
            fValues[i] = kParams[i].def;
        initVoiceManager();
    }

protected:
    /* -- information ------------------------------------------------------ */
    const char* getLabel()       const override { return "IFSynth"; }
    const char* getDescription() const override
    {
        return "A polyphonic synth built on the PolygraphModular voice engine.";
    }
    const char* getMaker()    const override { return "example"; }
    const char* getHomePage() const override { return "https://example.com/ifsynth"; }
    const char* getLicense()  const override { return "MIT"; }
    uint32_t    getVersion()  const override { return d_version(0, 3, 0); }
    int64_t     getUniqueId() const override { return d_cconst('I', 'F', 'S', 'y'); }

    /* -- init ------------------------------------------------------------- */
    void initAudioPort(bool input, uint32_t index, AudioPort& port) override
    {
        port.groupId = kPortGroupStereo; // 0 inputs; two output channels form a stereo pair
        Plugin::initAudioPort(input, index, port);
    }

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        if (index >= kParamCount)
            return;
        const ParamSpec& s = kParams[index];
        parameter.hints      = kParameterIsAutomatable
                             | (s.logarithmic ? kParameterIsLogarithmic : 0u)
                             | (s.integer     ? kParameterIsInteger     : 0u);
        parameter.name       = s.name;
        parameter.symbol     = s.symbol;
        parameter.unit       = s.unit;
        parameter.ranges.min = s.min;
        parameter.ranges.max = s.max;
        parameter.ranges.def = s.def;
    }

    float getParameterValue(uint32_t index) const override
    {
        return index < kParamCount ? fValues[index] : 0.0f;
    }

    void setParameterValue(uint32_t index, float value) override
    {
        if (index >= kParamCount)
            return;
        fValues[index] = value;
        applyParam(index, value);
    }

    /* -- lifecycle -------------------------------------------------------- */
    void sampleRateChanged(double) override { initVoiceManager(); }
    void activate()               override { if (fVoiceManager) fVoiceManager->reset(); }

    /* -- process ---------------------------------------------------------- */
    void run(const float**, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override
    {
        float* outL = outputs[0];
        float* outR = outputs[1];
        const float gain = fValues[kParamGain];

        uint32_t pos = 0, ev = 0;
        while (pos < frames)
        {
            // Dispatch every MIDI event scheduled at or before the current frame.
            for (; ev < midiEventCount && midiEvents[ev].frame <= pos; ++ev)
                dispatchMidi(midiEvents[ev]);

            uint32_t next = frames;
            if (ev < midiEventCount && midiEvents[ev].frame < frames)
                next = midiEvents[ev].frame;

            renderSegment(outL, outR, pos, next, gain);
            pos = next;
        }
    }

private:
    std::unique_ptr<VoiceManager> fVoiceManager;
    float                         fValues[kParamCount];

    // Map a DPF param index onto its VoiceManager module item address (gain excepted).
    static AUParameterAddress vmAddress(uint32_t paramIndex)
    {
        switch (paramIndex)
        {
        // IFS oscillator
        case kParamOscPitch:      return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemPitch);
        case kParamOscFine:       return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemPitchFine);
        case kParamOscBx:         return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemBx);
        case kParamOscBy:         return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemBy);
        case kParamOscStartEndY:  return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemStartEndY);
        case kParamOscSquash1:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemIFS1Squash);
        case kParamOscSquash2:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemIFS2Squash);
        case kParamOscSmoothing:  return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemSmoothing);
        case kParamOscIterations: return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemIterations);
        // IFS envelope
        case kParamEnvDuration:   return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemDuration);
        case kParamEnvBlueX:      return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemBlueX);
        case kParamEnvBlueY:      return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemBlueY);
        case kParamEnvGreenX:     return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemGreenX);
        case kParamEnvGreenY:     return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemGreenY);
        case kParamEnvSquash1:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemIFS1Squash);
        case kParamEnvSquash2:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemIFS2Squash);
        case kParamEnvSquash3:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemIFS3Squash);
        case kParamEnvSmoothing:  return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemSmoothing);
        case kParamEnvIterations: return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemIterations);
        case kParamEnvHoldMode:   return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemHoldMode);
        // IFS LFO
        case kParamLfoRate:       return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance1, IFSLFOParamItemPitch);
        case kParamLfoBx:         return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance1, IFSLFOParamItemBx);
        case kParamLfoBy:         return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance1, IFSLFOParamItemBy);
        case kParamLfoStartEndY:  return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance1, IFSLFOParamItemStartEndY);
        case kParamLfoSquash1:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance1, IFSLFOParamItemIFS1Squash);
        case kParamLfoSquash2:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance1, IFSLFOParamItemIFS2Squash);
        case kParamLfoSmoothing:  return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance1, IFSLFOParamItemSmoothing);
        case kParamLfoIterations: return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance1, IFSLFOParamItemIterations);
        // IFS envelope 2 (instance 2)
        case kParamEnv2Duration:   return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemDuration);
        case kParamEnv2BlueX:      return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemBlueX);
        case kParamEnv2BlueY:      return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemBlueY);
        case kParamEnv2GreenX:     return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemGreenX);
        case kParamEnv2GreenY:     return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemGreenY);
        case kParamEnv2Squash1:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemIFS1Squash);
        case kParamEnv2Squash2:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemIFS2Squash);
        case kParamEnv2Squash3:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemIFS3Squash);
        case kParamEnv2Smoothing:  return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemSmoothing);
        case kParamEnv2Iterations: return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemIterations);
        case kParamEnv2HoldMode:   return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance2, IFSEnvParamItemHoldMode);
        // IFS LFO 2 (instance 2)
        case kParamLfo2Rate:       return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance2, IFSLFOParamItemPitch);
        case kParamLfo2Bx:         return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance2, IFSLFOParamItemBx);
        case kParamLfo2By:         return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance2, IFSLFOParamItemBy);
        case kParamLfo2StartEndY:  return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance2, IFSLFOParamItemStartEndY);
        case kParamLfo2Squash1:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance2, IFSLFOParamItemIFS1Squash);
        case kParamLfo2Squash2:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance2, IFSLFOParamItemIFS2Squash);
        case kParamLfo2Smoothing:  return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance2, IFSLFOParamItemSmoothing);
        case kParamLfo2Iterations: return MODULE_PARAM_ADDRESS(ModuleParamKindIFSLFO, ModuleInstance2, IFSLFOParamItemIterations);
        // CV mixer 1 (instance 1)
        case kParamMix1EnableLFO: return MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, CVMixerParamItemEnableLFO);
        case kParamMix1EnableEnv: return MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, CVMixerParamItemEnableEnv);
        case kParamMix1EnableCC:  return MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, CVMixerParamItemEnableCC);
        case kParamMix1CCNumber:  return MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, CVMixerParamItemCCNumber);
        case kParamMix1Mode:      return MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance1, CVMixerParamItemMode);
        // CV mixer 2 (instance 2)
        case kParamMix2EnableLFO: return MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance2, CVMixerParamItemEnableLFO);
        case kParamMix2EnableEnv: return MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance2, CVMixerParamItemEnableEnv);
        case kParamMix2EnableCC:  return MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance2, CVMixerParamItemEnableCC);
        case kParamMix2CCNumber:  return MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance2, CVMixerParamItemCCNumber);
        case kParamMix2Mode:      return MODULE_PARAM_ADDRESS(ModuleParamKindCVMixer, ModuleInstance2, CVMixerParamItemMode);
        // IFSOsc mod depths (the module ModLevel params)
        case kParamOscPitchModDepth: return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemPitchModLevel);
        case kParamOscBxModDepth:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemBxModLevel);
        case kParamOscByModDepth:    return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemByModLevel);
        case kParamOscSq1ModDepth:   return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemSquash1ModLevel);
        case kParamOscSq2ModDepth:   return MODULE_PARAM_ADDRESS(ModuleParamKindIFSOsc, ModuleInstance1, IFSOscParamItemSquash2ModLevel);
        // Low-pass filter
        case kParamLpfCutoff:     return MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemPitch);
        case kParamLpfResonance:  return MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemQ);
        case kParamLpfKeyTrack:      return MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemFreqModLevel2);
        case kParamLpfCutoffModDepth:return MODULE_PARAM_ADDRESS(ModuleParamKindButterworthLowPassFilter, ModuleInstance1, LPFParamItemFreqModLevel);
        default:                  return 0;
        }
    }

    // Mod-source dropdown index (0 = None) → the source module's base address.
    static AUParameterAddress modSourceAddr(int idx)
    {
        switch (idx)
        {
        case 1: return ifsLFO1Address;
        case 2: return ifsLFO2Address;
        case 3: return ifsEnv1Address;
        case 4: return ifsEnv2Address;
        case 5: return cvMixer1Address;
        case 6: return cvMixer2Address;
        default: return 0; // None
        }
    }

    // Handle the routing params (dropdowns → connections). Returns true if handled.
    bool applyModRouting(uint32_t index, float value)
    {
        AUParameterAddress destInput = 0;
        switch (index)
        {
        case kParamOscPitchModSrc: destInput = ifsOsc1PitchModInputAddress;   break;
        case kParamOscBxModSrc:    destInput = ifsOsc1BxModInputAddress;      break;
        case kParamOscByModSrc:    destInput = ifsOsc1ByModInputAddress;      break;
        case kParamOscSq1ModSrc:   destInput = ifsOsc1Squash1ModInputAddress; break;
        case kParamOscSq2ModSrc:   destInput = ifsOsc1Squash2ModInputAddress; break;
        case kParamLpfCutoffModSrc: destInput = lpf1FreqModInputAddress;      break;
        case kParamVCASource:
            // The amplitude VCA always has an env: 0 = Env 1, 1 = Env 2.
            fVoiceManager->setModSource(combine1Input2Address,
                                        value < 0.5f ? ifsEnv1Address : ifsEnv2Address);
            return true;
        default:
            return false;
        }
        fVoiceManager->setModSource(destInput, modSourceAddr(static_cast<int>(value + 0.5f)));
        return true;
    }

    static bool isMono(float voiceModeValue)
    {
        return static_cast<int>(voiceModeValue + 0.5f) == 0; // 0 = Mono, 1 = poly
    }

    // Voice mode: Mono = 1 voice + VoiceManager legato (engages the last-note note-stack) + the
    // envelopes' FindMatchingY level-matching (click-free retrigger). Poly = 10 voices, none of
    // that. Glide (applySlew) also keys off mono.
    void applyVoiceMode(float value)
    {
        if (!fVoiceManager)
            return;
        const bool mono = isMono(value);
        setConfig(MIDIParamItemVoiceCount, mono ? 1.0f : 10.0f);
        setConfig(MIDIParamItemLegato,     mono ? 1.0f : 0.0f);
        const float envLegato = mono ? 1.0f : 0.0f; // 1 = FindMatchingY
        fVoiceManager->setParameter(ifsEnv1LegatoModeAddress, envLegato);
        fVoiceManager->setParameter(ifsEnv2LegatoModeAddress, envLegato);
        applySlew();
    }

    // Pitch glide (Slew Rise == Fall). Only active in mono; in poly it is forced near-instant so
    // each freshly allocated voice doesn't audibly scoop from its rest pitch.
    void applySlew()
    {
        if (!fVoiceManager)
            return;
        const float rate = isMono(fValues[kParamVoiceMode]) ? fValues[kParamSlewRate] : 1e-8f;
        fVoiceManager->setParameter(slew1RiseAddress, rate);
        fVoiceManager->setParameter(slew1FallAddress, rate);
    }

    // Route one param to the engine: gain is applied in run(); routing params change connections;
    // everything else is a module param.
    void applyParam(uint32_t index, float value)
    {
        if (index == kParamGain || !fVoiceManager)
            return;
        if (index == kParamSlewRate)  { applySlew(); return; }
        if (index == kParamVoiceMode) { applyVoiceMode(value); return; }
        if (applyModRouting(index, value))
            return;
        const AUParameterAddress a = vmAddress(index);
        if (a != 0)
            fVoiceManager->setParameter(a, value);
    }

    // (Re)build the VoiceManager for the current sample rate and push all defaults into it.
    void initVoiceManager()
    {
        fVoiceManager = std::make_unique<VoiceManager>(getSampleRate(), MODULE_BUFF_SIZE);

        // VoiceManager params aren't zero-initialised. Voice count + legato are driven by the
        // Voices param (applyVoiceMode, pushed in the loop below); set the rest here.
        setConfig(MIDIParamItemChannelFilterValue, 0);
        setConfig(MIDIParamItemGroupFilterValue, 0);
        setConfig(MIDIParamItemVoiceStealAlgorithm, VoiceStealAlgorithmOldest);
        setConfig(MIDIParamItemFreeRun, 0);

        // Push every non-gain param so the engine matches the exposed parameter values
        // (module params + the mod-routing connections).
        for (uint32_t i = 0; i < kParamCount; ++i)
            applyParam(i, fValues[i]);
    }

    void setConfig(MIDIParamItem item, float v)
    {
        fVoiceManager->setVoiceManagerParameter(
            MODULE_PARAM_ADDRESS(ModuleParamKindMIDI, ModuleInstance1, item), v);
    }

    void dispatchMidi(const MidiEvent& me)
    {
        const uint8_t* d = me.size > MidiEvent::kDataSize ? me.dataExt : me.data;
        const pm::UmpMessage m = pm::fromMidi1Bytes(d[0],
                                                    me.size > 1 ? d[1] : 0,
                                                    me.size > 2 ? d[2] : 0);
        if (pm::isChannelMessage(m))
            fVoiceManager->handleMIDIMessage(m);
    }

    // Render [start,end) by chunking to the module block size; VoiceManager zeroes + sums voices.
    void renderSegment(float* outL, float* outR, uint32_t start, uint32_t end, float gain)
    {
        for (uint32_t p = start; p < end;)
        {
            const uint32_t chunk = std::min<uint32_t>(end - p, MODULE_BUFF_SIZE);
            fVoiceManager->process(chunk, outL + p, outR + p);
            for (uint32_t i = p; i < p + chunk; ++i)
            {
                outL[i] *= gain;
                outR[i] *= gain;
            }
            p += chunk;
        }
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IFSynthPlugin)
};

Plugin* createPlugin()
{
    return new IFSynthPlugin();
}

END_NAMESPACE_DISTRHO
