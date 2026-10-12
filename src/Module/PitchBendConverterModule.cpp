//
//  PitchBendConverterModule.cpp
//
//  MIDI 2.0 pitch bend → a bipolar octaves signal (patched into ModalBank's second pitch input).
//  Vendored from PolygraphModularUnified's PitchBendConverterModule; the extensions below were made
//  in PlainOldSynth and ported here:
//
//  - Per-note pitch bend (UMP status 0x6). One instance lives in every voice, and the voice only
//    passes it per-note messages for the note it is sounding (VoiceStateManager::handleMIDIMessage),
//    so this module simply applies whatever per-note bend reaches it. Range: PerNoteSemitones
//    (default ±48, the MPE convention).
//  - Channel bend tracked PER CHANNEL. Channel bends reach every voice, including idle ones; each
//    voice remembers all sixteen channels (× groups) and applies the one its note was played on. With
//    everything on one channel that is the ordinary behaviour; with a note per channel (MPE, or a
//    MIDI 2.0 controller) each note follows its own channel's bend. Range: QuarterTones / 2 semitones
//    (default ±2).
//  - Per-note pitch bend sensitivity (MIDI 2.0 registered controller bank 0 / index 7) overrides the
//    per-note range for the channel it arrives on — a controller such as KeyboardEDO announces its
//    range this way and scales its bends to match, so ignoring it bends by the wrong amount (±2 sent,
//    ±48 heard). Unlike the rest of this state it survives reset(): it is configuration the
//    controller sent once, not processing state, and some hosts call reset() on transport stop.
//  - De-zippering. Bend arrives as discrete events; a step in pitch is audible, so a change of target
//    ramps linearly over kRampSeconds. Every note-on snaps instead: the note must start at its bent
//    pitch, and in StruckSynth even a mono legato note re-strikes the bank, so there is no sounding
//    pitch worth gliding from. (PlainOldSynth ramps on legato via Module::legatoContinuation, which
//    StruckSynth does not have.)
//
//  Per-note bend resets to centre on every note-on (a new note, a steal or a mono retrigger) and on a
//  per-note management message with the Reset flag. Per-note bend sent BEFORE its note-on is
//  dropped: no voice owns that note yet.
//

#include "PitchBendConverterModule.hpp"
#include "pm_midi.h"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace {

constexpr double kRampSeconds = 0.002;
constexpr int    kGroups      = 16;
constexpr int    kChannels    = 16;

// Registered controller bank 0 / index 7: per-note pitch bend sensitivity.
constexpr uint8_t kRpnBankPitch = 0;
constexpr uint8_t kRpnPerNotePitchBendSensitivity = 7;

struct PitchBendConverterHelperStruct {
    PitchBendConverterHelperStruct() {
        for (auto& group : perNoteRange) std::fill(std::begin(group), std::end(group), -1.0f);
    }
    float   channelBend[kGroups][kChannels] = {};   // -1..+1, last channel bend per group/channel
    float   perNoteRange[kGroups][kChannels];       // semitones from RC 7; < 0 = not set (param default)
    float   perNoteBend = 0.0f;                     // -1..+1, for the note this voice is playing
    uint8_t group = 0, channel = 0;                 // where this voice's note was played

    float    current = 0.0f;     // octaves, as output
    float    rampTarget = 0.0f;  // octaves the ramp is heading to
    float    rampStep = 0.0f;
    uint32_t rampRemaining = 0;
    bool     snap = true;        // jump straight to the target on the next process()
};

float targetOctaves(const PitchBendConverterHelperStruct& st, Module* module) {
    // The QuarterTones param is the channel range in quarter tones, so divide by 2 for semitones.
    const float channelSemis = module->paramByEnum(PitchBendConverterParamItemQuarterTones) * 0.5f;
    const float rc7 = st.perNoteRange[st.group][st.channel];
    const float perNoteSemis = rc7 >= 0.0f ? rc7
                                           : module->paramByEnum(PitchBendConverterParamItemPerNoteSemitones);
    const float semis = st.channelBend[st.group][st.channel] * channelSemis
                      + st.perNoteBend * perNoteSemis;
    return semis / 12.0f;
}

} // namespace

const void InitPitchBendConverter(uint32_t /*count*/, Module* module)
{
    module->state = new PitchBendConverterHelperStruct();
}

const void DeinitPitchBendConverter(Module* module)
{
    delete static_cast<PitchBendConverterHelperStruct*>(module->state);
    module->state = nullptr;
}

const void ResetPitchBendConverter(Module* module)
{
    auto* st = static_cast<PitchBendConverterHelperStruct*>(module->state);
    PitchBendConverterHelperStruct fresh;
    std::copy(&st->perNoteRange[0][0], &st->perNoteRange[0][0] + kGroups * kChannels,
              &fresh.perNoteRange[0][0]);   // configuration, not processing state: keep it
    *st = fresh;
}

const KPBuffPair* ProcessPitchBendConverter(uint64_t /*processCallID*/, uint32_t count, Module* module)
{
    auto* st = static_cast<PitchBendConverterHelperStruct*>(module->state);
    float* outL = module->output.first->data;
    float* outR = module->output.second->data;

    // Recomputed every block, so a range change applies (ramped) without waiting for the next bend.
    const float target = targetOctaves(*st, module);
    if (st->snap) {
        st->snap = false;
        st->current = st->rampTarget = target;
        st->rampRemaining = 0;
    } else if (target != st->rampTarget) {
        st->rampTarget = target;
        st->rampRemaining = std::max<uint32_t>(1, uint32_t(std::lround(module->mSampleRate * kRampSeconds)));
        st->rampStep = (target - st->current) / float(st->rampRemaining);
    }

    if (st->rampRemaining == 0) {
        std::fill_n(outL, count, st->current);
    } else {
        for (uint32_t i = 0; i < count; ++i) {
            if (st->rampRemaining > 0) {
                st->current = (--st->rampRemaining == 0) ? st->rampTarget : st->current + st->rampStep;
            }
            outL[i] = st->current;
        }
    }
    std::copy_n(outL, count, outR);

    return &module->output;
}

void MIDIPitchBendConverter(const pm::UmpMessage& message, Module* module)
{
    if (!pm::isChannelVoice2(message)) return;
    auto* st = static_cast<PitchBendConverterHelperStruct*>(module->state);
    const uint8_t group   = message.group & 0x0F;
    const uint8_t channel = message.channel & 0x0F;

    switch (message.status) {
    case pm::kStatusNoteOn:
        if (!pm::isNoteOn(message)) break;   // velocity 0 = note-off
        st->group = group;
        st->channel = channel;
        st->perNoteBend = 0.0f;
        st->snap = true;
        break;
    case pm::kStatusPitchBend:
        st->channelBend[group][channel] = float(pm::pitchBendToUnit(message.data32));
        break;
    case pm::kStatusRegController:
        // Per-note pitch bend sensitivity. The value has the MIDI 1.0 data-entry layout in its top 14
        // bits: semitones in bits 31-25, cents in bits 24-18.
        if (message.note == kRpnBankPitch && message.ccIndex == kRpnPerNotePitchBendSensitivity) {
            const uint32_t data14 = message.ccData >> 18;
            st->perNoteRange[group][channel] = float(data14 >> 7) + float(data14 & 0x7F) / 100.0f;
        }
        break;
    case pm::kStatusPerNotePitchBend:
        st->perNoteBend = float(pm::pitchBendToUnit(message.data32));
        break;
    case pm::kStatusPerNoteManagement:
        if (message.optionFlags & pm::kPerNoteMgmtReset) st->perNoteBend = 0.0f;
        break;
    default:
        break;
    }
}
