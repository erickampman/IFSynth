// test/ifs_clap_host_test.cpp — drives the ACTUAL built IFSynth.clap through the CLAP C API, with no
// DAW and no GUI. This is the end-to-end proof of the thing that matters most here: that a MIDI 2.0
// NoteOn carrying a 7.9-pitch attribute survives the plugin's note port, reaches the oscillators, and
// comes back out at the right (microtonal) frequency.
//
// It is also the only check that exercises note-port dialect negotiation — the reason this plugin
// implements CLAP directly instead of using DPF's wrapper, which drops the event outright and
// advertises only the MIDI/MPE dialects.
//
// The pitch assertion is a RATIO between two renderings of the same note that differ only in the
// 7.9 fraction (0 vs 256 = half a semitone), which is the quantity the MIDI 2.0 attribute controls.
// That makes it robust to the pitch estimator locking onto a harmonic: an octave error cancels in
// the ratio, so only the fractional pitch shows up. Each measurement gets a fresh plugin instance.
//
// Usage: ifs-clap-host-test /path/to/IFSynth.clap
// Prints "ALL PASS" on success.

#include <clap/clap.h>

#include <dlfcn.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "IFSynthParams.h"   // parameter ids/enums; header-only, nothing is linked from it

using namespace ifsynth;

namespace {

int gFailures = 0;

void check(bool ok, const std::string& what) {
    std::printf("  [%s] %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) ++gFailures;
}

constexpr double kSampleRate = 48000.0;

// --- minimal host -------------------------------------------------------------------------------
void hostNoop(const clap_host_t*) {}
const void* hostGetExtension(const clap_host_t*, const char*) { return nullptr; }

// --- minimal event list -------------------------------------------------------------------------
struct EventList {
    const clap_event_header_t** events = nullptr;
    uint32_t count = 0;

    static uint32_t size(const clap_input_events_t* list) {
        return static_cast<const EventList*>(list->ctx)->count;
    }
    static const clap_event_header_t* get(const clap_input_events_t* list, uint32_t index) {
        return static_cast<const EventList*>(list->ctx)->events[index];
    }
};

// --- pitch estimate -----------------------------------------------------------------------------
// Normalized autocorrelation over the sustained part of the note. The first lag whose correlation is
// within 5% of the best is the fundamental period (its multiples are also peaks, at larger lags).
double estimatePitch(const std::vector<float>& samples, double minHz, double maxHz) {
    const size_t start = samples.size() * 3 / 10;     // skip the attack transient
    const size_t count = samples.size() - start;
    if (count < 256) return 0.0;
    const size_t minLag = std::max<size_t>(2, size_t(kSampleRate / maxHz));
    const size_t maxLag = std::min(count - 1, size_t(kSampleRate / minHz));
    if (maxLag <= minLag) return 0.0;

    double best = 0.0;
    std::vector<double> correlation(maxLag + 1, 0.0);
    for (size_t lag = minLag; lag <= maxLag; ++lag) {
        double dot = 0.0, energyA = 0.0, energyB = 0.0;
        for (size_t i = 0; i + lag < count; ++i) {
            const double a = samples[start + i];
            const double b = samples[start + i + lag];
            dot += a * b;
            energyA += a * a;
            energyB += b * b;
        }
        correlation[lag] = dot / (std::sqrt(energyA * energyB) + 1e-30);
        best = std::max(best, correlation[lag]);
    }
    if (best <= 0.0) return 0.0;
    // The first lag that comes near the best correlation is the fundamental period (later ones are
    // its multiples). Climb from there to that peak's top, then interpolate the peak with a parabola:
    // returning the first qualifying whole-sample lag read sharp, by up to a few percent.
    for (size_t lag = minLag; lag <= maxLag; ++lag) {
        if (correlation[lag] < 0.95 * best) continue;
        while (lag < maxLag && correlation[lag + 1] > correlation[lag]) ++lag;
        double period = double(lag);
        if (lag > minLag && lag < maxLag) {
            const double y0 = correlation[lag - 1], y1 = correlation[lag], y2 = correlation[lag + 1];
            const double denom = y0 - 2.0 * y1 + y2;
            if (denom < 0.0) period += 0.5 * (y0 - y2) / denom;
        }
        return kSampleRate / period;
    }
    return 0.0;
}

// --- state save/load plumbing -------------------------------------------------------------------
struct StateSink {
    std::string bytes;
    static int64_t write(const clap_ostream_t* stream, const void* buffer, uint64_t size) {
        static_cast<StateSink*>(stream->ctx)->bytes.append(static_cast<const char*>(buffer), size_t(size));
        return int64_t(size);
    }
};

struct StateSource {
    std::string bytes;
    size_t pos = 0;
    static int64_t read(const clap_istream_t* stream, void* buffer, uint64_t size) {
        auto* s = static_cast<StateSource*>(stream->ctx);
        const size_t remaining = s->bytes.size() - s->pos;
        const size_t n = std::min<size_t>(size_t(size), remaining);
        std::memcpy(buffer, s->bytes.data() + s->pos, n);
        s->pos += n;
        return int64_t(n);
    }
};

// sendParam()/readParam() are defined after render(), below.

// --- the library + plugin under test ------------------------------------------------------------
struct Library {
    void* handle = nullptr;
    const clap_plugin_entry_t* entry = nullptr;
    const clap_plugin_factory_t* factory = nullptr;
    const clap_plugin_descriptor_t* descriptor = nullptr;
    clap_host_t host{};
};

bool openLibrary(const std::string& path, Library& lib) {
    std::string binary = path;
    if (path.size() > 5 && path.compare(path.size() - 5, 5, ".clap") == 0) {
        const size_t slash = path.find_last_of('/');
        const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
        binary = path + "/Contents/MacOS/" + name.substr(0, name.size() - 5);
    }
    lib.handle = dlopen(binary.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (lib.handle == nullptr) lib.handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (lib.handle == nullptr) return false;

    lib.entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib.handle, "clap_entry"));
    if (lib.entry == nullptr || !lib.entry->init(path.c_str())) return false;

    lib.factory = static_cast<const clap_plugin_factory_t*>(
        lib.entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (lib.factory == nullptr) return false;
    lib.descriptor = lib.factory->get_plugin_descriptor(lib.factory, 0);
    if (lib.descriptor == nullptr) return false;

    lib.host = clap_host_t{};
    lib.host.clap_version = CLAP_VERSION_INIT;
    lib.host.name = "ifs-clap-host-test";
    lib.host.vendor = "test";
    lib.host.url = "";
    lib.host.version = "1";
    lib.host.get_extension = hostGetExtension;
    lib.host.request_restart = hostNoop;
    lib.host.request_process = hostNoop;
    lib.host.request_callback = hostNoop;
    return true;
}

struct Instance {
    const clap_plugin_t* plugin = nullptr;

    bool create(Library& lib) {
        plugin = lib.factory->create_plugin(lib.factory, &lib.host, lib.descriptor->id);
        return plugin != nullptr && plugin->init(plugin)
            && plugin->activate(plugin, kSampleRate, 1, 4096);
    }
    void destroy() {
        if (plugin != nullptr) { plugin->deactivate(plugin); plugin->destroy(plugin); }
        plugin = nullptr;
    }
};

struct StereoAudio {
    std::vector<float> left, right;
};

// Collects what a plugin pushes to out_events: every NOTE_END with the absolute frame it arrived at,
// and whether any block's events went backwards in time (CLAP requires them in order).
struct OutputSink {
    struct End { clap_event_note_t event; uint64_t frame; };
    std::vector<End> ends;
    uint64_t blockStart = 0;
    uint32_t lastTime = 0;
    bool outOfOrder = false;
    static bool tryPush(const clap_output_events_t* list, const clap_event_header_t* header) {
        auto* sink = static_cast<OutputSink*>(list->ctx);
        if (header->time < sink->lastTime) sink->outOfOrder = true;
        sink->lastTime = header->time;
        if (header->space_id == CLAP_CORE_EVENT_SPACE_ID && header->type == CLAP_EVENT_NOTE_END)
            sink->ends.push_back({ *reinterpret_cast<const clap_event_note_t*>(header),
                                   sink->blockStart + header->time });
        return true;
    }
};

StereoAudio renderStereo(const clap_plugin_t* plugin, uint32_t frames,
                         const clap_event_header_t** events, uint32_t eventCount,
                         OutputSink* sink = nullptr) {
    StereoAudio audio;
    audio.left.assign(frames, 0.0f);
    audio.right.assign(frames, 0.0f);

    EventList holder;
    holder.events = events;
    holder.count = eventCount;
    clap_input_events_t in{};
    in.ctx = &holder;
    in.size = EventList::size;
    in.get = EventList::get;

    float* channels[2] = { audio.left.data(), audio.right.data() };
    clap_audio_buffer_t out{};
    out.data32 = channels;
    out.data64 = nullptr;
    out.channel_count = 2;
    out.latency = 0;
    out.constant_mask = 0;

    clap_process_t process{};
    process.steady_time = -1;
    process.frames_count = frames;
    process.transport = nullptr;
    process.audio_inputs = nullptr;
    process.audio_inputs_count = 0;
    process.audio_outputs = &out;
    process.audio_outputs_count = 1;
    process.in_events = &in;
    clap_output_events_t outEvents{};
    outEvents.ctx = sink;
    outEvents.try_push = OutputSink::tryPush;
    process.out_events = sink != nullptr ? &outEvents : nullptr;
    if (sink != nullptr) sink->lastTime = 0;

    plugin->process(plugin, &process);
    return audio;
}

std::vector<float> render(const clap_plugin_t* plugin, uint32_t frames,
                         const clap_event_header_t** events, uint32_t eventCount) {
    return renderStereo(plugin, frames, events, eventCount).left;
}

// Send one parameter value and render a tiny block so the plugin stores/applies it.
void sendParam(const clap_plugin_t* plugin, uint32_t index, double value) {
    clap_event_param_value_t event{};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.param_id = index;
    event.value = value;
    const clap_event_header_t* headers[] = { &event.header };
    render(plugin, 64, headers, 1);
}

double readParam(const clap_plugin_t* plugin, uint32_t index) {
    const auto* params = static_cast<const clap_plugin_params_t*>(
        plugin->get_extension(plugin, CLAP_EXT_PARAMS));
    if (params == nullptr || params->get_value == nullptr) return -1.0;
    double value = 0.0;
    return params->get_value(plugin, index, &value) ? value : -1.0;
}

// Install a patch that is clean to measure pitch on: only the IFS oscillator, mono spread, filter
// wide open, no glide, a long envelope. Everything that could colour or confuse the measurement is
// pinned down; the note's pitch (and the 7.9 attribute) is the only variable left.
void installTestPatch(const clap_plugin_t* plugin) {
    struct Holder { clap_event_param_value_t event{}; };
    std::vector<Holder> storage;

    auto add = [&storage](uint32_t index, double value) {
        Holder holder{};
        holder.event.header.size = sizeof(clap_event_param_value_t);
        holder.event.header.time = 0;
        holder.event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        holder.event.header.type = CLAP_EVENT_PARAM_VALUE;
        holder.event.param_id = index;   // the plugin's clap_id is the 0-based parameter index
        holder.event.value = value;
        storage.push_back(holder);
    };

    add(kParamVoiceMode, 1.0);       // poly: pitch slew is forced instant, no glide scoop
    add(kParamSlewRate, 0.0001);
    add(kParamSpread, 0.0);          // mono: one unambiguous fundamental
    add(kParamOscPitch, 0.0);
    add(kParamOscFine, 0.0);
    add(kParamOscIterations, 1.0);   // richest IFS waveform; still perfectly periodic
    add(kParamIfsSyncMode, 0.0);     // no hard sync: the IFS osc runs at the note pitch
    add(kParamIfsLevel, 1.0);
    add(kParamSinLevel, 0.0);        // measure the IFS oscillator alone
    add(kParamLpfCutoff, 5.4);       // wide open
    add(kParamLpfResonance, 1.0);
    add(kParamLpfKeyTrack, 0.0);
    add(kParamEnvDuration, 10.0);    // long envelopes: the note must still be ringing when measured
    add(kParamEnv2Duration, 10.0);

    std::vector<const clap_event_header_t*> headers;
    headers.reserve(storage.size());
    for (Holder& holder : storage) headers.push_back(&holder.event.header);
    render(plugin, 256, headers.data(), uint32_t(headers.size()));
}

// A MIDI 2.0 channel-voice-2 NoteOn UMP with a 7.9-pitch attribute (attributeType 3).
clap_event_midi2_t makeUmpNoteOn(uint8_t note, uint16_t velocity, uint8_t attributeType,
                                 uint16_t attribute) {
    const uint32_t words[2] = {
        (0x4u << 28) | (0x9u << 20) | (uint32_t(note) << 8) | uint32_t(attributeType),
        (uint32_t(velocity) << 16) | uint32_t(attribute),
    };
    clap_event_midi2_t event{};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_MIDI2;
    event.port_index = 0;
    std::memcpy(event.data, words, sizeof(words));
    return event;
}

clap_event_note_t makeClapNoteOn(int16_t key, double velocity) {
    clap_event_note_t event{};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_NOTE_ON;
    event.port_index = 0;
    event.key = key;
    event.channel = 0;
    event.velocity = velocity;
    return event;
}

clap_event_midi_t makeMidi1(uint8_t status, uint8_t data1, uint8_t data2) {
    clap_event_midi_t event{};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_MIDI;
    event.port_index = 0;
    event.data[0] = status; event.data[1] = data1; event.data[2] = data2;
    return event;
}

// Render events (a note, plus anything that shapes it) through a fresh instance with the test patch,
// and report the measured pitch. sineOnly swaps the IFS oscillator for Osc 2 (the sine), so a check
// can prove something reaches both oscillators.
double pitchOfEvents(Library& lib, const clap_event_header_t** events, uint32_t eventCount,
                     double expectedHz, const char* what, bool sineOnly = false) {
    Instance inst;
    if (!inst.create(lib)) {
        check(false, std::string(what) + ": plugin instance created");
        return 0.0;
    }
    installTestPatch(inst.plugin);
    if (sineOnly) {
        sendParam(inst.plugin, kParamIfsLevel, 0.0);
        sendParam(inst.plugin, kParamSinLevel, 1.0);
    }

    const std::vector<float> audio = render(inst.plugin, uint32_t(kSampleRate * 1.5), events, eventCount);

    bool finite = true;
    float peak = 0.0f;
    for (float s : audio) {
        if (!std::isfinite(s)) finite = false;
        peak = std::max(peak, std::fabs(s));
    }
    const double pitch = estimatePitch(audio, expectedHz * 0.4, expectedHz * 2.4);
    std::printf("        %-28s expected ~%8.3f Hz | measured %8.3f Hz | peak %.4f\n",
                what, expectedHz, pitch, double(peak));

    check(finite, std::string(what) + ": output is finite");
    check(peak > 0.01f, std::string(what) + ": produces audio");
    inst.destroy();
    return pitch;
}

double pitchOfNote(Library& lib, const clap_event_header_t* noteEvent, double expectedHz,
                   const char* what) {
    const clap_event_header_t* events[] = { noteEvent };
    return pitchOfEvents(lib, events, 1, expectedHz, what);
}

clap_event_note_expression_t makeNoteExpression(clap_note_expression id, int32_t noteId, int16_t key,
                                                 double value) {
    clap_event_note_expression_t event{};
    event.header.size = sizeof(event);
    event.header.time = 0;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_NOTE_EXPRESSION;
    event.expression_id = id;
    event.note_id = noteId;
    event.port_index = 0;
    event.channel = 0;
    event.key = key;
    event.value = value;
    return event;
}

// A MIDI 2.0 per-note pitch bend UMP (status 0x6). value is the 32-bit bend, 0x80000000 = centre.
clap_event_midi2_t makeUmpPerNoteBend(uint8_t note, uint32_t value) {
    clap_event_midi2_t event = makeUmpNoteOn(note, 0, 0, 0);
    event.data[0] = (0x4u << 28) | (0x6u << 20) | (uint32_t(note) << 8);
    event.data[1] = value;
    return event;
}

// The 32-bit bend value for `semitones` of a ±range bend. Clamped: a full-scale bend computes 2^32,
// which wraps to 0 (full down) in a uint32_t.
uint32_t bendValue(double semitones, double range) {
    const double v = 2147483648.0 + semitones / range * 2147483648.0;
    return uint32_t(std::clamp(std::llround(v), 0LL, 4294967295LL));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s /path/to/IFSynth.clap\n", argv[0]);
        return 2;
    }

    Library lib;
    check(openLibrary(argv[1], lib), "dlopen + clap_entry + factory + descriptor");
    if (lib.handle == nullptr) { std::printf("FAILURES: %d\n", gFailures); return 1; }

    std::printf("descriptor: id=%s name=%s vendor=%s\n",
                lib.descriptor->id, lib.descriptor->name, lib.descriptor->vendor);
    check(std::strcmp(lib.descriptor->id, "com.example.ifsynth") == 0, "descriptor id");
    check(std::strcmp(lib.descriptor->name, "IFSynth") == 0, "descriptor name");

    // --- audio ports -----------------------------------------------------------------------------
    {
        Instance inst;
        check(inst.create(lib), "plugin instance created");
        const auto* ports = static_cast<const clap_plugin_audio_ports_t*>(
            inst.plugin->get_extension(inst.plugin, CLAP_EXT_AUDIO_PORTS));
        check(ports != nullptr, "audio-ports extension present");
        if (ports != nullptr) {
            check(ports->count(inst.plugin, true) == 0, "no audio inputs");
            check(ports->count(inst.plugin, false) == 1, "one audio output");
            clap_audio_port_info_t info{};
            if (ports->get(inst.plugin, 0, false, &info))
                check(info.channel_count == 2, "output is stereo");
            else
                check(false, "audio port info readable");
        }
        inst.destroy();
    }

    // --- the note port must offer (and prefer) the MIDI 2.0 dialect ------------------------------
    // This is the negotiation that makes a MIDI-2.0-capable host send raw UMP notes at all; without
    // it CLAP_EVENT_MIDI2 never arrives and the 7.9-pitch path is dead code.
    {
        Instance inst;
        check(inst.create(lib), "plugin instance created (note ports)");
        const auto* notes = static_cast<const clap_plugin_note_ports_t*>(
            inst.plugin->get_extension(inst.plugin, CLAP_EXT_NOTE_PORTS));
        check(notes != nullptr, "note-ports extension present");
        if (notes != nullptr) {
            check(notes->count(inst.plugin, true) == 1, "one note input port");
            clap_note_port_info_t info{};
            if (notes->get(inst.plugin, 0, true, &info)) {
                check((info.supported_dialects & CLAP_NOTE_DIALECT_CLAP) != 0,
                      "advertises the CLAP note dialect");
                check((info.supported_dialects & CLAP_NOTE_DIALECT_MIDI) != 0,
                      "advertises the MIDI 1.0 dialect");
                check((info.supported_dialects & CLAP_NOTE_DIALECT_MIDI2) != 0,
                      "advertises the MIDI 2.0 dialect");
                check(info.preferred_dialect == CLAP_NOTE_DIALECT_MIDI2,
                      "prefers the MIDI 2.0 dialect");
            } else {
                check(false, "note port info readable");
            }
        }
        inst.destroy();
    }

    // --- the gui extension is wired (DPF is now the GUI layer only) ------------------------------
    {
        Instance inst;
        check(inst.create(lib), "plugin instance created (gui)");
        const auto* gui = static_cast<const clap_plugin_gui_t*>(
            inst.plugin->get_extension(inst.plugin, CLAP_EXT_GUI));
        check(gui != nullptr, "gui extension present");
        if (gui != nullptr) {
            const char* api = nullptr;
            bool floating = true;
            check(gui->get_preferred_api(inst.plugin, &api, &floating) && api != nullptr && !floating,
                  "gui prefers an embedded (non-floating) window API");
            check(gui->is_api_supported(inst.plugin, api, false), "preferred gui API is supported");
            if (gui->create(inst.plugin, api, false)) {
                uint32_t w = 0, h = 0;
                check(gui->get_size(inst.plugin, &w, &h) && w > 0 && h > 0,
                      "gui reports a default size");
                gui->destroy(inst.plugin);
            } else {
                check(false, "gui create() succeeds for the preferred API");
            }
        }
        check(inst.plugin->get_extension(inst.plugin, CLAP_EXT_TIMER_SUPPORT) != nullptr,
              "timer-support extension present (pumps the UI event loop)");
        inst.destroy();
    }

    // --- params extension ------------------------------------------------------------------------
    {
        Instance inst;
        check(inst.create(lib), "plugin instance created (params)");
        const auto* params = static_cast<const clap_plugin_params_t*>(
            inst.plugin->get_extension(inst.plugin, CLAP_EXT_PARAMS));
        check(params != nullptr, "params extension present");
        if (params != nullptr) {
            check(params->count(inst.plugin) == uint32_t(kParamCount), "parameter count matches");
            clap_param_info_t info{};
            if (params->get_info(inst.plugin, kParamIfsSyncMode, &info)) {
                check(info.id == kParamIfsSyncMode, "param id is the 0-based index (DPF-compatible)");
                check((info.flags & CLAP_PARAM_IS_ENUM) != 0, "Sync is flagged as an enum");
                char text[128] = {};
                if (params->value_to_text(inst.plugin, kParamIfsSyncMode, 0.0, text, sizeof(text)))
                    check(std::strcmp(text, "Off") == 0, "enum value_to_text returns the label");
                else
                    check(false, "value_to_text works");
            } else {
                check(false, "param info readable");
            }
            if (params->get_info(inst.plugin, kParamIfsLevel, &info))
                check((info.flags & CLAP_PARAM_IS_ENUM) == 0, "IFS level is not an enum");
        }
        inst.destroy();
    }

    // --- state save/load, including the legacy DPF-blob fallback ---------------------------------
    // The raw entry point implements CLAP state itself, so this is real coverage: a round trip must
    // restore values, and the NUL-separated blob DPF's generated wrapper used to write must still
    // load (that is what keeps sessions saved before this switch working).
    {
        Instance inst;
        check(inst.create(lib), "plugin instance created (state)");
        const auto* state = static_cast<const clap_plugin_state_t*>(
            inst.plugin->get_extension(inst.plugin, CLAP_EXT_STATE));
        check(state != nullptr && state->save != nullptr && state->load != nullptr,
              "state extension present");

        if (state != nullptr) {
            sendParam(inst.plugin, kParamEnvDuration, 5.0);
            check(std::fabs(readParam(inst.plugin, kParamEnvDuration) - 5.0) < 1e-6,
                  "parameter set before saving");

            StateSink sink;
            clap_ostream_t out{};
            out.ctx = &sink;
            out.write = StateSink::write;
            check(state->save(inst.plugin, &out), "state save succeeds");
            check(!sink.bytes.empty(), "state save produced bytes");

            sendParam(inst.plugin, kParamEnvDuration, 1.0);   // clobber it
            check(std::fabs(readParam(inst.plugin, kParamEnvDuration) - 1.0) < 1e-6,
                  "parameter changed after saving");

            StateSource source;
            source.bytes = sink.bytes;
            clap_istream_t in{};
            in.ctx = &source;
            in.read = StateSource::read;
            check(state->load(inst.plugin, &in), "state load succeeds");
            check(std::fabs(readParam(inst.plugin, kParamEnvDuration) - 5.0) < 1e-6,
                  "state round trip restores the value");

            // DPF's old auto-state: NUL-separated "__dpf_parameters_begin__" pairs, then \xfe.
            std::string legacy = "__dpf_parameters_begin__";
            legacy.push_back('\0');
            legacy += "envDur";           legacy.push_back('\0');
            legacy += "3";                legacy.push_back('\0');
            legacy += "__dpf_parameters_end__";
            legacy.push_back('\0');
            legacy.push_back('\xfe');
            StateSource legacySource;
            legacySource.bytes = legacy;
            clap_istream_t legacyIn{};
            legacyIn.ctx = &legacySource;
            legacyIn.read = StateSource::read;
            check(state->load(inst.plugin, &legacyIn), "legacy DPF state blob loads");
            check(std::fabs(readParam(inst.plugin, kParamEnvDuration) - 3.0) < 1e-6,
                  "legacy DPF state blob restores the value");

            // Data that is neither format must be rejected, not silently defaulted.
            StateSource junk;
            junk.bytes = "not our state at all";
            clap_istream_t junkIn{};
            junkIn.ctx = &junk;
            junkIn.read = StateSource::read;
            check(!state->load(inst.plugin, &junkIn), "foreign state data is rejected");

            // state_load on an active plugin only stores the values (it runs on the main thread);
            // they must still reach the engine at the top of the next process() block.
            installTestPatch(inst.plugin);
            auto peakAfterLoading = [&](const char* text) {
                StateSource s;
                s.bytes = text;
                clap_istream_t sIn{};
                sIn.ctx = &s;
                sIn.read = StateSource::read;
                if (!state->load(inst.plugin, &sIn)) return -1.0f;
                clap_event_note_t note = makeClapNoteOn(60, 1.0);
                const clap_event_header_t* events[] = { &note.header };
                float peak = 0.0f;
                for (float v : render(inst.plugin, 4096, events, 1)) peak = std::max(peak, std::fabs(v));
                return peak;
            };
            check(peakAfterLoading("IFSynth 1\nifsLevel 0\nsinLevel 0\n") < 1e-4f,
                  "state loaded while active reaches the engine (silenced)");
            check(peakAfterLoading("IFSynth 1\nifsLevel 1\n") > 0.01f,
                  "state loaded while active reaches the engine (audible again)");
        }
        inst.destroy();
    }

    // --- MIDI 2.0 7.9-pitch, end to end through the real plugin -----------------------------------
    std::printf("MIDI 2.0 7.9-pitch notes through the real plugin:\n");
    const double kC4Hz = 261.6255653;
    const uint16_t kNote = 60;
    const uint16_t attributeZero = uint16_t((kNote << 9) | 0);
    const uint16_t attributeQuarterTone = uint16_t((kNote << 9) | 256);   // 256/512 semitone

    const clap_event_midi2_t noteOnZero = makeUmpNoteOn(uint8_t(kNote), 0xFFFF, 0x3, attributeZero);
    const clap_event_midi2_t noteOnQuarter = makeUmpNoteOn(uint8_t(kNote), 0xFFFF, 0x3, attributeQuarterTone);

    const double hzZero = pitchOfNote(lib, &noteOnZero.header, kC4Hz, "7.9 fraction 0 (C4)");
    const double hzQuarter = pitchOfNote(lib, &noteOnQuarter.header,
                                         kC4Hz * std::pow(2.0, 0.5 / 12.0),
                                         "7.9 fraction 256 (+50c)");

    if (hzZero > 0.0 && hzQuarter > 0.0) {
        const double ratio = hzQuarter / hzZero;
        const double expectedRatio = std::pow(2.0, 0.5 / 12.0);   // 1.029302
        const double errorPercent = 100.0 * std::fabs(ratio - expectedRatio) / expectedRatio;
        std::printf("        ratio %.6f (expected %.6f, error %.3f%%)\n",
                    ratio, expectedRatio, errorPercent);
        check(errorPercent < 0.3,
              "the 7.9 fraction moves the measured pitch by half a semitone");
    } else {
        check(false, "both 7.9-pitch notes produced a measurable pitch");
    }

    // --- pitch bend through the real plugin -------------------------------------------------------
    // Proves the bend CV reaches both oscillators' second pitch input at full depth. Measured against
    // the unbent C4 above, so only the interval matters.
    std::printf("Pitch bend through the real plugin:\n");
    auto checkInterval = [&](double hz, double semitones, const char* what) {
        const double expected = std::pow(2.0, semitones / 12.0);
        check(hzZero > 0.0 && std::fabs(hz / hzZero - expected) / expected < 0.003, what);
    };
    {
        const clap_event_midi2_t noteOn = makeUmpNoteOn(uint8_t(kNote), 0xFFFF, 0, 0);
        // No registered controller 7: per-note bend uses the channel's range, ±2.
        const clap_event_midi2_t bend = makeUmpPerNoteBend(uint8_t(kNote), 0xC0000000u);  // half way up
        const clap_event_header_t* events[] = { &noteOn.header, &bend.header };
        const double up1 = kC4Hz * std::pow(2.0, 1.0 / 12.0);
        checkInterval(pitchOfEvents(lib, events, 2, up1, "per-note bend +1 (IFS)"), 1.0,
                      "with no RC 7, a half-way per-note bend is +1 semitone on the IFS oscillator");
        checkInterval(pitchOfEvents(lib, events, 2, up1, "per-note bend +1 (Osc 2)", true), 1.0,
                      "with no RC 7, a half-way per-note bend is +1 semitone on Osc 2");
    }
    {
        // Registered controller 7 can also widen the range: ±48, then +3.
        clap_event_midi2_t range = makeUmpNoteOn(0, 0, 0, 0);
        range.data[0] = (0x4u << 28) | (0x2u << 20) | (0u << 8) | 7u;
        range.data[1] = 48u << 25;
        const clap_event_midi2_t noteOn = makeUmpNoteOn(uint8_t(kNote), 0xFFFF, 0, 0);
        const clap_event_midi2_t bend = makeUmpPerNoteBend(uint8_t(kNote), bendValue(3.0, 48.0));
        const clap_event_header_t* events[] = { &range.header, &noteOn.header, &bend.header };
        checkInterval(pitchOfEvents(lib, events, 3, kC4Hz * std::pow(2.0, 3.0 / 12.0),
                                    "per-note bend +3 at RC7 = 48"), 3.0,
                      "registered controller 7 = 48 widens the per-note range: +3 of ±48");
    }
    {
        // A bend sent for another key must not touch the sounding note.
        const clap_event_midi2_t noteOn = makeUmpNoteOn(uint8_t(kNote), 0xFFFF, 0, 0);
        const clap_event_midi2_t bend = makeUmpPerNoteBend(uint8_t(kNote + 1), 0xFFFFFFFFu);
        const clap_event_header_t* events[] = { &noteOn.header, &bend.header };
        checkInterval(pitchOfEvents(lib, events, 2, kC4Hz, "per-note bend, other key"), 0.0,
                      "a per-note bend for another key leaves C4 alone");
    }
    {
        // Notes match on note + channel + group: the same key's per-note bend on channel 2 is for a
        // different note.
        const clap_event_midi2_t noteOn = makeUmpNoteOn(uint8_t(kNote), 0xFFFF, 0, 0);
        clap_event_midi2_t bend = makeUmpPerNoteBend(uint8_t(kNote), 0xFFFFFFFFu);
        bend.data[0] |= 1u << 16;   // channel 2
        const clap_event_header_t* events[] = { &noteOn.header, &bend.header };
        checkInterval(pitchOfEvents(lib, events, 2, kC4Hz, "per-note bend, other channel"), 0.0,
                      "a per-note bend on another channel leaves C4 alone");
    }
    {
        // KeyboardEDO's setup: announce a ±2 per-note range (registered controller 7), then bend
        // half way up, which it means as +1 semitone.
        clap_event_midi2_t range = makeUmpNoteOn(0, 0, 0, 0);
        range.data[0] = (0x4u << 28) | (0x2u << 20) | (0u << 8) | 7u;
        range.data[1] = 2u << 25;
        const clap_event_midi2_t noteOn = makeUmpNoteOn(uint8_t(kNote), 0xFFFF, 0, 0);
        const clap_event_midi2_t bend = makeUmpPerNoteBend(uint8_t(kNote), 0xC0000000u);  // half way up
        const clap_event_header_t* events[] = { &range.header, &noteOn.header, &bend.header };
        checkInterval(pitchOfEvents(lib, events, 3, kC4Hz * std::pow(2.0, 1.0 / 12.0),
                                    "per-note bend at RC7 = 2"), 1.0,
                      "registered controller 7 sets the per-note range: half way at ±2 is +1 semitone");
    }
    {
        const clap_event_midi_t bend = makeMidi1(0xE0, 0x00, 0x60);   // half way up: +1 of ±2
        const clap_event_midi_t noteOn = makeMidi1(0x90, uint8_t(kNote), 100);
        const clap_event_header_t* events[] = { &bend.header, &noteOn.header };
        const double up1 = kC4Hz * std::pow(2.0, 1.0 / 12.0);
        checkInterval(pitchOfEvents(lib, events, 2, up1, "MIDI 1.0 channel bend +1"), 1.0,
                      "a MIDI 1.0 channel bend of +1 semitone moves C4 to C#4");
        checkInterval(pitchOfEvents(lib, events, 2, up1, "MIDI 1.0 channel bend +1 (Osc 2)", true), 1.0,
                      "a MIDI 1.0 channel bend of +1 semitone moves Osc 2 to C#4");
    }
    {
        // Glide must not smear a bend. Mono, and the note starts with the test patch's near-zero glide
        // (the first mono note after load glides up from the voice's rest pitch, A440). 0.1 s later
        // the glide becomes 2 s and the bend arrives: it must still land at once.
        const uint32_t later = uint32_t(kSampleRate * 0.1);
        clap_event_param_value_t mono{}, glide{};
        for (clap_event_param_value_t* e : { &mono, &glide }) {
            e->header.size = sizeof(*e);
            e->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            e->header.type = CLAP_EVENT_PARAM_VALUE;
        }
        mono.param_id = kParamVoiceMode;  mono.value = 0.0;
        glide.param_id = kParamSlewRate;  glide.value = 2.0;  glide.header.time = later;
        const clap_event_midi2_t noteOn = makeUmpNoteOn(uint8_t(kNote), 0xFFFF, 0, 0);
        clap_event_midi2_t bend = makeUmpPerNoteBend(uint8_t(kNote), 0xC0000000u);   // +1 of ±2
        bend.header.time = later;
        const clap_event_header_t* events[] = { &mono.header, &noteOn.header, &glide.header, &bend.header };
        checkInterval(pitchOfEvents(lib, events, 4, kC4Hz * std::pow(2.0, 1.0 / 12.0),
                                    "per-note bend +1, mono, 2 s glide"), 1.0,
                      "a bend bypasses the glide");
    }

    // --- Pitch 7.25 (registered per-note controller #3) -------------------------------------------
    // An absolute pitch for the sounding note, exact at any size (unlike a bend, clamped to its
    // range); both bends then offset from it.
    std::printf("Pitch 7.25 through the real plugin:\n");
    {
        const clap_event_midi2_t noteOn = makeUmpNoteOn(uint8_t(kNote), 0xFFFF, 0, 0);
        clap_event_midi2_t tune = makeUmpNoteOn(uint8_t(kNote), 0, 0, 0);
        tune.data[0] = (0x4u << 28) | (0x0u << 20) | (uint32_t(kNote) << 8) | 3u;
        tune.data[1] = uint32_t((kNote + 7) * 33554432.0 + 0.5 * 33554432.0);   // G4 + 50 cents
        {
            const clap_event_header_t* events[] = { &noteOn.header, &tune.header };
            checkInterval(pitchOfEvents(lib, events, 2, kC4Hz * std::pow(2.0, 7.5 / 12.0),
                                        "Pitch 7.25 = 67.5"), 7.5,
                          "Pitch 7.25 retunes the sounding C4 to 67.5 (G4 + 50 cents)");
            checkInterval(pitchOfEvents(lib, events, 2, kC4Hz * std::pow(2.0, 7.5 / 12.0),
                                        "Pitch 7.25 = 67.5 (Osc 2)", true), 7.5,
                          "Pitch 7.25 retunes Osc 2 too");
        }
        {
            const clap_event_midi2_t bend = makeUmpPerNoteBend(uint8_t(kNote), 0xC0000000u);  // +1
            const clap_event_header_t* events[] = { &noteOn.header, &tune.header, &bend.header };
            checkInterval(pitchOfEvents(lib, events, 3, kC4Hz * std::pow(2.0, 8.5 / 12.0),
                                        "Pitch 7.25 = 67.5, bend +1"), 8.5,
                          "a per-note bend offsets from the Pitch 7.25 pitch");
        }
        {
            // Not persistent: the next note-on on that key starts at its own pitch.
            clap_event_midi2_t noteOff = makeUmpNoteOn(uint8_t(kNote), 0, 0, 0);
            noteOff.data[0] = (0x4u << 28) | (0x8u << 20) | (uint32_t(kNote) << 8);
            clap_event_midi2_t again = noteOn;
            noteOff.header.time = 100;
            again.header.time = 200;
            const clap_event_header_t* events[] = { &noteOn.header, &tune.header,
                                                    &noteOff.header, &again.header };
            checkInterval(pitchOfEvents(lib, events, 4, kC4Hz, "Pitch 7.25, then a new note-on"), 0.0,
                          "Pitch 7.25 lasts only for the sounding note");
        }
    }

    // --- MPE switch through the real plugin ---------------------------------------------------------
    // The same member-channel bend with the switch off and on: the parameter must reach the engine, and
    // it must be a two-state Off/On enum.
    std::printf("MPE switch:\n");
    {
        Instance probe;
        if (probe.create(lib)) {
            const auto* params = static_cast<const clap_plugin_params_t*>(
                probe.plugin->get_extension(probe.plugin, CLAP_EXT_PARAMS));
            clap_param_info_t info{};
            char text[64] = {};
            check(params != nullptr && params->get_info(probe.plugin, kParamMpe, &info)
                      && (info.flags & CLAP_PARAM_IS_ENUM) != 0 && info.default_value == 0.0
                      && params->value_to_text(probe.plugin, kParamMpe, 1.0, text, sizeof(text))
                      && std::string(text) == "On",
                  "MPE is an Off/On enum parameter, off by default");
            probe.destroy();
        }
        // Semitones a list of MIDI 1.0 events (ending in a note-on) sounds above the unbent C4.
        auto semitonesOf = [&](bool mpe, std::vector<clap_event_midi_t> midi) {
            Instance inst;
            if (!inst.create(lib)) return 0.0;
            installTestPatch(inst.plugin);
            sendParam(inst.plugin, kParamMpe, mpe ? 1.0 : 0.0);
            std::vector<const clap_event_header_t*> events;
            for (const auto& e : midi) events.push_back(&e.header);
            const std::vector<float> audio = render(inst.plugin, uint32_t(kSampleRate * 1.5),
                                                    events.data(), uint32_t(events.size()));
            inst.destroy();
            return 12.0 * std::log2(estimatePitch(audio, 100.0, 2000.0) / hzZero);
        };
        const clap_event_midi_t memberBend = makeMidi1(0xE1, 0x00, 0x44);   // 8192 + 512, channel 2
        const clap_event_midi_t memberNote = makeMidi1(0x91, uint8_t(kNote), 100);
        const double off = semitonesOf(false, { memberBend, memberNote });
        const double on  = semitonesOf(true,  { memberBend, memberNote });
        std::printf("        member bend: %.3f st with MPE off, %.3f st with MPE on\n", off, on);
        check(std::fabs(off - 0.125) < 0.02 && std::fabs(on - 3.0) < 0.02,
              "MPE on reads a member-channel bend at ±48 (+3 st), off at ±2 (+0.125 st)");

        // The master channel (1) bends every member note, at its own ±2, on top of the member bend.
        const clap_event_midi_t masterBend = makeMidi1(0xE0, 0x00, 0x60);   // half way up: +1 of ±2
        const double both = semitonesOf(true, { masterBend, memberBend, memberNote });
        std::printf("        master +1 and member +3: %.3f st\n", both);
        check(std::fabs(both - 4.0) < 0.02, "MPE: the master channel's bend adds to a member note's");

        // RPN 0 sets a channel's bend range: ±12 on channel 1, then half way up is +6.
        const double rpn = semitonesOf(false, { makeMidi1(0xB0, 101, 0), makeMidi1(0xB0, 100, 0),
                                                makeMidi1(0xB0, 6, 12),  makeMidi1(0xB0, 38, 0),
                                                masterBend, makeMidi1(0x90, uint8_t(kNote), 100) });
        std::printf("        RPN 0 = 12, half way up: %.3f st\n", rpn);
        check(std::fabs(rpn - 6.0) < 0.02, "RPN 0 sets the channel bend range (±12: half way is +6)");

        // Released notes (MPE §3.3.3: control of a note ceases once Note Off has occurred). The test
        // patch's 10 s envelope rings ~5 s after note-off, so the pitch is measured in the tail.
        // Note-off at 0.05 s, then the bend at 0.1 s.
        auto at = [](clap_event_midi_t e, double seconds) {
            e.header.time = uint32_t(seconds * kSampleRate);
            return e;
        };
        const clap_event_midi_t memberOff = at(makeMidi1(0x81, uint8_t(kNote), 0), 0.05);
        const double frozen = semitonesOf(true, { memberNote, memberOff,
                                                  at(makeMidi1(0xE1, 0x7F, 0x7F), 0.1) });   // full up
        std::printf("        MPE member bend after note-off: %.3f st\n", frozen);
        check(std::fabs(frozen) < 0.02, "MPE: a member-channel bend after note-off does not move the released note");
        const double master = semitonesOf(true, { memberNote, memberOff, at(masterBend, 0.1) });
        std::printf("        MPE master bend after note-off: %.3f st\n", master);
        check(std::fabs(master - 1.0) < 0.02, "MPE: the master channel still bends a released note");
        const double wheel = semitonesOf(false, { makeMidi1(0x90, uint8_t(kNote), 100),
                                                  at(makeMidi1(0x80, uint8_t(kNote), 0), 0.05),
                                                  at(masterBend, 0.1) });
        std::printf("        MPE off, channel bend after note-off: %.3f st\n", wheel);
        check(std::fabs(wheel - 1.0) < 0.02, "MPE off: a pitch wheel still bends a releasing note");
    }
    {
        // A released note ignores a MIDI 2.0 per-note bend addressed by key; a CLAP note named by its
        // id keeps taking expression until NOTE_END.
        const clap_event_midi2_t noteOn = makeUmpNoteOn(uint8_t(kNote), 0xFFFF, 0, 0);
        clap_event_midi2_t noteOff = makeUmpNoteOn(uint8_t(kNote), 0, 0, 0);
        noteOff.data[0] = (0x4u << 28) | (0x8u << 20) | (uint32_t(kNote) << 8);
        noteOff.header.time = uint32_t(0.05 * kSampleRate);
        clap_event_midi2_t bend = makeUmpPerNoteBend(uint8_t(kNote), 0xC0000000u);   // +1
        bend.header.time = uint32_t(0.1 * kSampleRate);
        const clap_event_header_t* events[] = { &noteOn.header, &noteOff.header, &bend.header };
        checkInterval(pitchOfEvents(lib, events, 3, kC4Hz, "per-note bend after note-off"), 0.0,
                      "a released note ignores a per-note bend addressed by key");

        clap_event_note_t clapOn = makeClapNoteOn(int16_t(kNote), 1.0);
        clapOn.note_id = 5;
        clap_event_note_t clapOff = clapOn;
        clapOff.header.type = CLAP_EVENT_NOTE_OFF;
        clapOff.header.time = uint32_t(0.05 * kSampleRate);
        clap_event_note_expression_t tuning =
            makeNoteExpression(CLAP_NOTE_EXPRESSION_TUNING, 5, int16_t(kNote), 2.0);
        tuning.header.time = uint32_t(0.1 * kSampleRate);
        const clap_event_header_t* clapEvents[] = { &clapOn.header, &clapOff.header, &tuning.header };
        checkInterval(pitchOfEvents(lib, clapEvents, 3, kC4Hz * std::pow(2.0, 2.0 / 12.0),
                                    "CLAP tuning after note-off"), 2.0,
                      "a released CLAP note still takes a tuning expression by id");
    }

    // --- note end + voice-info ---------------------------------------------------------------------
    // A note rings on through the IFS envelope's release after note-off; CLAP_EVENT_NOTE_END must
    // come when the voice falls silent, not at the note-off, and only for notes that arrived as CLAP
    // note events.
    std::printf("Note end and voice info:\n");
    {
        Instance inst;
        check(inst.create(lib), "fresh instance");
        installTestPatch(inst.plugin);
        sendParam(inst.plugin, kParamEnvDuration, 0.5);   // default Green hold: ~0.28 s of release
        sendParam(inst.plugin, kParamEnv2Duration, 0.5);

        clap_event_note_t noteOn = makeClapNoteOn(int16_t(kNote), 1.0);
        noteOn.note_id = 42;
        clap_event_note_t noteOff = noteOn;
        noteOff.header.type = CLAP_EVENT_NOTE_OFF;
        noteOff.header.time = 64;
        const clap_event_midi_t midiOn = makeMidi1(0x90, 72, 100);
        const clap_event_midi_t midiOff = [] { auto e = makeMidi1(0x80, 72, 0); e.header.time = 64; return e; }();
        const clap_event_header_t* first[] = { &noteOn.header, &midiOn.header, &noteOff.header, &midiOff.header };

        OutputSink sink;
        const uint32_t block = 512;
        for (uint32_t b = 0; b < uint32_t(4.0 * kSampleRate) / block; ++b) {
            sink.blockStart = uint64_t(b) * block;
            if (b == 0) renderStereo(inst.plugin, block, first, 4, &sink);
            else        renderStereo(inst.plugin, block, nullptr, 0, &sink);
        }
        check(sink.ends.size() == 1, "exactly one NOTE_END, for the CLAP note only (got "
                                         + std::to_string(sink.ends.size()) + ")");
        if (!sink.ends.empty()) {
            const auto& end = sink.ends[0];
            std::printf("        NOTE_END at %.3f s (note-off at %.4f s)\n",
                        double(end.frame) / kSampleRate, 64.0 / kSampleRate);
            check(end.event.note_id == 42 && end.event.key == int16_t(kNote) && end.event.channel == 0
                      && end.event.port_index == 0,
                  "NOTE_END echoes the note-on's id, key, channel and port");
            check(double(end.frame) / kSampleRate > 0.1, "NOTE_END waits for the release, not the note-off");
        }
        check(!sink.outOfOrder, "output events stay in time order within each block");

        const auto* voiceInfo = static_cast<const clap_plugin_voice_info_t*>(
            inst.plugin->get_extension(inst.plugin, CLAP_EXT_VOICE_INFO));
        check(voiceInfo != nullptr, "voice-info extension is offered");
        if (voiceInfo != nullptr) {
            clap_voice_info_t info{};
            check(voiceInfo->get(inst.plugin, &info) && info.voice_count == 10 && info.voice_capacity == 10
                      && (info.flags & CLAP_VOICE_INFO_SUPPORTS_OVERLAPPING_NOTES) != 0,
                  "poly: 10 voices, overlapping notes supported");
            sendParam(inst.plugin, kParamVoiceMode, 0.0);
            check(voiceInfo->get(inst.plugin, &info) && info.voice_count == 1 && info.voice_capacity == 10,
                  "mono: 1 voice");
        }
        inst.destroy();
    }

    // --- CLAP note expressions + note ids, as Bitwig sends them ---------------------------------------
    // Tuning arrives as Pitch 7.25 (an absolute pitch), so it is exact far beyond the ±2 bend range.
    std::printf("CLAP note expressions (Bitwig):\n");
    {
        clap_event_note_t noteOn = makeClapNoteOn(int16_t(kNote), 1.0);
        noteOn.note_id = 7;
        const clap_event_note_expression_t tuning =
            makeNoteExpression(CLAP_NOTE_EXPRESSION_TUNING, 7, int16_t(kNote), 24.0);
        const clap_event_header_t* events[] = { &noteOn.header, &tuning.header };
        checkInterval(pitchOfEvents(lib, events, 2, kC4Hz * 4.0, "tuning +24 (key + id)"), 24.0,
                      "a +24 semitone tuning expression sounds two octaves up");
    }
    {
        clap_event_note_t noteOn = makeClapNoteOn(int16_t(kNote), 1.0);
        noteOn.note_id = 11;
        const clap_event_note_expression_t tuning =
            makeNoteExpression(CLAP_NOTE_EXPRESSION_TUNING, 11, -1, 7.0);   // id only, no key
        const clap_event_header_t* events[] = { &noteOn.header, &tuning.header };
        const double up7 = kC4Hz * std::pow(2.0, 7.0 / 12.0);
        checkInterval(pitchOfEvents(lib, events, 2, up7, "tuning +7 (id only)"), 7.0,
                      "an expression naming only the note id reaches that note (+7 semitones)");
        checkInterval(pitchOfEvents(lib, events, 2, up7, "tuning +7 (id only, Osc 2)", true), 7.0,
                      "a tuning expression retunes Osc 2 too");
    }
    {
        // The same key with another note id is another note: the expression must not reach it.
        clap_event_note_t noteOn = makeClapNoteOn(int16_t(kNote), 1.0);
        noteOn.note_id = 12;
        const clap_event_note_expression_t tuning =
            makeNoteExpression(CLAP_NOTE_EXPRESSION_TUNING, 13, int16_t(kNote), 7.0);
        const clap_event_header_t* events[] = { &noteOn.header, &tuning.header };
        checkInterval(pitchOfEvents(lib, events, 2, kC4Hz, "tuning for another note id"), 0.0,
                      "a tuning expression for another note id leaves C4 alone");
    }

    // --- velocity: scaling and the Velocity (loudness) dropdown ------------------------------------
    std::printf("Velocity:\n");
    {
        auto peakFor = [&](const clap_event_header_t* note, double velocityChoice) {
            Instance inst;
            if (!inst.create(lib)) return -1.0f;
            installTestPatch(inst.plugin);
            sendParam(inst.plugin, kParamVelocity, velocityChoice);
            const clap_event_header_t* events[] = { note };
            float peak = 0.0f;
            for (float v : render(inst.plugin, 4096, events, 1)) peak = std::max(peak, std::fabs(v));
            inst.destroy();
            return peak;
        };
        const clap_event_midi_t vel127 = makeMidi1(0x90, uint8_t(kNote), 127);
        const clap_event_midi_t vel64  = makeMidi1(0x90, uint8_t(kNote), 64);
        const clap_event_note_t clapFull = makeClapNoteOn(int16_t(kNote), 1.0);
        const float a = peakFor(&vel127.header, 3.0), b = peakFor(&clapFull.header, 3.0);
        std::printf("        peak at Full: MIDI 1.0 vel 127 %.6f | CLAP vel 1.0 %.6f\n", double(a), double(b));
        check(a > 0.01f && a == b, "MIDI 1.0 velocity 127 is full scale, exactly as CLAP velocity 1.0");

        // Velocity 64 upscales to exactly half; the dropdown's shapes put it at these levels.
        const char* names[] = { "Off", "Light", "Medium", "Full" };
        const double expectedDb[] = { 0.0, 20.0 * std::log10(0.75), 20.0 * std::log10(0.2 + 0.8 * std::pow(0.5, 1.5)),
                                      20.0 * std::log10(0.25) };
        for (int c = 0; c < 4; ++c) {
            const double db = 20.0 * std::log10(double(peakFor(&vel64.header, c)) / double(peakFor(&vel127.header, c)));
            std::printf("        %-6s velocity 64: %+.2f dB (expected %+.2f)\n", names[c], db, expectedDb[c]);
            check(std::fabs(db - expectedDb[c]) < 0.1,
                  std::string("Velocity ") + names[c] + ": velocity 64 sounds at the expected level");
        }
    }

    // --- a MIDI 2.0 note-on with velocity 0 is a note-on (M2-104 4.2.8) ------------------------------
    // It used to count as a note-off, so the note never sounded.
    std::printf("MIDI 2.0 velocity 0:\n");
    {
        const clap_event_midi2_t noteOn = makeUmpNoteOn(uint8_t(kNote), 0, 0, 0);
        const double hz = pitchOfNote(lib, &noteOn.header, kC4Hz, "MIDI 2.0 note-on, velocity 0");
        checkInterval(hz, 0.0, "a MIDI 2.0 note-on with velocity 0 plays its note");
    }

    // --- the MIDI-1.0 / CLAP dialects still work -------------------------------------------------
    std::printf("Other note dialects (must keep working):\n");
    {
        const clap_event_note_t clapNote = makeClapNoteOn(int16_t(kNote), 1.0);
        const double hz = pitchOfNote(lib, &clapNote.header, kC4Hz, "CLAP note-on (C4)");
        check(hz > 0.0, "CLAP note dialect reaches the engine");
    }
    {
        const clap_event_midi_t midi1 = makeMidi1(0x90, uint8_t(kNote), 100);
        const double hz = pitchOfNote(lib, &midi1.header, kC4Hz, "MIDI 1.0 note-on (C4)");
        check(hz > 0.0, "MIDI 1.0 dialect reaches the engine");
    }

    if (lib.handle != nullptr) {
        lib.entry->deinit();
        dlclose(lib.handle);
    }

    if (gFailures == 0) {
        std::printf("ALL PASS\n");
        return 0;
    }
    std::printf("FAILURES: %d\n", gFailures);
    return 1;
}
