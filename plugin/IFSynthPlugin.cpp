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
        // Gain is applied in run(); every other param drives a VoiceManager module item
        // (propagates to all voices, live).
        if (index != kParamGain && fVoiceManager)
            fVoiceManager->setParameter(vmAddress(index), value);
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
        case kParamEnvStartEndY:  return MODULE_PARAM_ADDRESS(ModuleParamKindIFSEnv, ModuleInstance1, IFSEnvParamItemStartEndY);
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
        default:                  return 0;
        }
    }

    // (Re)build the VoiceManager for the current sample rate and push all defaults into it.
    void initVoiceManager()
    {
        fVoiceManager = std::make_unique<VoiceManager>(getSampleRate(), MODULE_BUFF_SIZE);

        // VoiceManager params aren't zero-initialised — set the v1 voice-allocation config.
        setConfig(MIDIParamItemVoiceCount, 8);
        setConfig(MIDIParamItemChannelFilterValue, 0);
        setConfig(MIDIParamItemGroupFilterValue, 0);
        setConfig(MIDIParamItemVoiceStealAlgorithm, VoiceStealAlgorithmOldest);
        setConfig(MIDIParamItemFreeRun, 0);
        setConfig(MIDIParamItemLegato, 0);

        // Push every non-gain param so the engine matches the exposed parameter values.
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (i != kParamGain)
                fVoiceManager->setParameter(vmAddress(i), fValues[i]);
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
