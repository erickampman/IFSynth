// pm_selftest — exercises the Phase 1b foundation: the spec-derived UMP MIDI layer, the
// CLAP→UMP decoders, and the vendored AU-free leaf files. Prints "ALL PASS".

#include "CPPTypes.hpp"
#include "KPBuffPairHelper.hpp"
#include "ParamAddressMacros.h"
#include "clap_ump.h"
#include "pm_midi.h"

#include <cmath>
#include <cstdio>

static int g_fails = 0;
#define CHECK(cond, msg)                                                                           \
   do {                                                                                            \
      if (!(cond)) {                                                                               \
         std::printf("FAIL: %s\n", (msg));                                                         \
         ++g_fails;                                                                                \
      }                                                                                            \
   } while (0)

int main() {
   // --- param address (kind<<16 | instance<<8 | item) round-trip ---
   uint32_t addr = MODULE_PARAM_ADDRESS(0x42, 3, 7);
   CHECK(MODULE_KIND_FROM_ADDRESS(addr) == 0x42, "addr kind");
   CHECK(MODULE_INSTANCE_FROM_ADDRESS(addr) == 3, "addr instance");
   CHECK(MODULE_ITEM_FROM_ADDRESS(addr) == 7, "addr item");

   // --- KPBuffPair static buffers ---
   CHECK(KPBuffPairHelper::zeroes()->first->data[0] == 0.0f, "zeroes");
   CHECK(KPBuffPairHelper::ones()->second->data[MODULE_BUFF_SIZE - 1] == 1.0f, "ones");

   // --- UMP predicates ---
   auto on = pm::makeNoteOn(69, 0x8000);
   CHECK(pm::isNoteOn(on) && !pm::isNoteOff(on) && on.note == 69, "makeNoteOn");
   auto off = pm::makeNoteOff(69);
   CHECK(pm::isNoteOff(off) && !pm::isNoteOn(off), "makeNoteOff");
   auto on0 = pm::makeNoteOn(60, 1);
   on0.velocity = 0; // MIDI 2.0: a velocity-0 note-on is still a note-on (M2-104 §4.2.8)
   CHECK(pm::isNoteOn(on0) && !pm::isNoteOff(on0), "MIDI 2.0 vel0 note-on is a note-on");
   CHECK(pm::isNoteOff(pm::fromMidi1Bytes(0x90, 60, 0)), "MIDI 1.0 vel0 note-on decodes to a note-off");

   // --- MIDI 1.0 → 2.0 value scaling (M2-104 D.1.3 Min-Center-Max) ---
   CHECK(pm::scaleUp(0, 7, 16) == 0 && pm::scaleUp(64, 7, 16) == 0x8000 && pm::scaleUp(127, 7, 16) == 0xFFFF,
         "scaleUp 7->16: 0, centre and max land on 0, 0x8000, 0xFFFF");
   CHECK(pm::scaleUp(64, 7, 32) == 0x80000000u && pm::scaleUp(127, 7, 32) == 0xFFFFFFFFu,
         "scaleUp 7->32: centre and max exact");
   CHECK(pm::scaleUp(8192, 14, 32) == 0x80000000u && pm::scaleUp(16383, 14, 32) == 0xFFFFFFFFu,
         "scaleUp 14->32: bend centre and max exact");
   CHECK(pm::scaleUp(65, 7, 16) > 0x8200 && pm::scaleUp(126, 7, 16) < 0xFFFF,
         "scaleUp is monotonic above the centre");
   CHECK(pm::fromMidi1Bytes(0x90, 60, 127).velocity == 0xFFFF, "MIDI 1.0 velocity 127 is full scale");
   CHECK(pm::fromMidi1Bytes(0xA0, 60, 127).status == pm::kStatusPolyPressure
             && pm::fromMidi1Bytes(0xA0, 60, 127).data32 == 0xFFFFFFFFu,
         "MIDI 1.0 poly pressure decodes, full scale at 127");

   // --- decode a raw MIDI 2.0 note-on UMP word pair ---
   uint32_t words[2] = {(0x4u << 28) | (0x9u << 20) | (69u << 8) | 0u, (0xC000u << 16)};
   auto dec = pm::decodeUmp(words);
   CHECK(pm::isNoteOn(dec) && dec.note == 69 && dec.velocity == 0xC000, "decodeUmp note-on");

   // --- microtonal: attribute type 3 (7.9), note 69 + fraction 256 ---
   uint16_t attr = (uint16_t(69) << 9) | 256;
   double f = pm::pitchAttributeToFrequency(attr);
   double expect = pm::noteToFrequency(69) * std::pow(2.0, 256.0 / 6144.0);
   CHECK(std::fabs(f - expect) < 1e-6, "pitch attribute");

   // --- CLAP note event → UMP ---
   clap_event_note_t ce{};
   ce.channel = 1;
   ce.key = 64;
   ce.velocity = 1.0;
   auto cu = pm::fromClapNote(ce, true);
   CHECK(pm::isNoteOn(cu) && cu.note == 64 && cu.channel == 1 && cu.velocity == 65535,
         "fromClapNote on");
   CHECK(pm::isNoteOff(pm::fromClapNote(ce, false)), "fromClapNote off");

   // --- CLAP midi2 raw words → UMP ---
   clap_event_midi2_t cm{};
   cm.data[0] = words[0];
   cm.data[1] = words[1];
   auto cmu = pm::fromClapMidi2(cm);
   CHECK(pm::isNoteOn(cmu) && cmu.note == 69, "fromClapMidi2");

   if (g_fails == 0)
      std::printf("ALL PASS\n");
   return g_fails ? 1 : 0;
}
