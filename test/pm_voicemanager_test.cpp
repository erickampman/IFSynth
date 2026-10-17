// pm_voicemanager_test — polyphony off Apple. A 3-note chord allocates 3 voices and produces
// audio; releasing all notes frees the voices and decays to silence. This is the layer that
// makes the synth polyphonic (a single Voice is inherently one note).

#include "VoiceManager.hpp"
#include "pm_addresses.h"
#include "pm_midi.h"
#include "pm_params.h"

#include <cmath>
#include <cstdio>
#include <cstring>

static int g_fails = 0;
#define CHECK(cond, msg)                                                                           \
   do {                                                                                            \
      if (!(cond)) {                                                                               \
         std::printf("FAIL: %s\n", (msg));                                                         \
         ++g_fails;                                                                                \
      }                                                                                            \
   } while (0)

static void setVMParam(VoiceManager& vm, MIDIParamItem item, float v) {
   vm.setVoiceManagerParameter(MODULE_PARAM_ADDRESS(ModuleParamKindMIDI, ModuleInstance1, item), v);
}

// VoiceManager's params are not zero-initialised — set sane defaults (the plugin does too).
static void configure(VoiceManager& vm) {
   setVMParam(vm, MIDIParamItemVoiceCount, 8);
   setVMParam(vm, MIDIParamItemChannelFilterValue, 0);
   setVMParam(vm, MIDIParamItemGroupFilterValue, 0);
   setVMParam(vm, MIDIParamItemVoiceStealAlgorithm, 0);
   setVMParam(vm, MIDIParamItemFreeRun, 0);
   setVMParam(vm, MIDIParamItemLegato, 0);
}

// Channel mode messages: CC 123 All Notes Off and CC 120 All Sound Off act on their own channel.
static void testChannelModeMessages(double sr, uint32_t N) {
   float L[512], R[512];
   auto render = [&](VoiceManager& vm, int blocks) {
      float p = 0.0f;
      for (int b = 0; b < blocks; ++b) {
         std::memset(L, 0, sizeof(L));
         std::memset(R, 0, sizeof(R));
         vm.process(N, L, R);
         for (uint32_t i = 0; i < N; ++i)
            p = std::fmax(p, std::fabs(L[i]));
      }
      return p;   // the loudest sample over those blocks
   };
   const int oneSecond = int(sr / N);

   {  // CC 123 releases only its channel's notes.
      VoiceManager vm(sr, N);
      configure(vm);
      vm.handleMIDIMessage(pm::makeNoteOn(60, 0x8000, 1));
      vm.handleMIDIMessage(pm::makeNoteOn(64, 0x8000, 2));
      render(vm, 4);
      vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB1, 123, 0));
      render(vm, oneSecond * 3);
      CHECK(vm.activeCount() == 1, "CC 123 on channel 2 releases channel 2's note only");
   }
   {  // ...including a note the sustain pedal is holding.
      VoiceManager vm(sr, N);
      configure(vm);
      vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB0, 64, 127));
      vm.handleMIDIMessage(pm::makeNoteOn(60, 0x8000));
      vm.handleMIDIMessage(pm::makeNoteOff(60));
      render(vm, oneSecond);
      CHECK(vm.activeCount() == 1, "the pedal is holding the note");
      vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB0, 123, 0));
      render(vm, oneSecond * 3);
      CHECK(vm.activeCount() == 0, "CC 123 releases a pedal-held note");
   }
   {  // CC 120 cuts its channel's voice at once, release included; another channel plays on.
      VoiceManager vm(sr, N);
      configure(vm);
      vm.handleMIDIMessage(pm::makeNoteOn(60, 0x8000, 1));
      render(vm, 8);
      vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB1, 120, 0));
      CHECK(vm.activeCount() == 0, "CC 120 drops its channel's voice immediately");
      CHECK(render(vm, 4) < 1e-6f, "CC 120 is silent at once, with no release tail");

      vm.handleMIDIMessage(pm::makeNoteOn(60, 0x8000, 1));
      vm.handleMIDIMessage(pm::makeNoteOn(67, 0x8000, 2));
      render(vm, 8);
      vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB1, 120, 0));
      CHECK(vm.activeCount() == 1, "CC 120 on channel 2 leaves channel 3's voice");
      CHECK(render(vm, 8) > 0.01f, "channel 3's note still sounds");
   }
   {  // After CC 120 the next note on that channel starts clean and sounds.
      VoiceManager vm(sr, N);
      configure(vm);
      vm.handleMIDIMessage(pm::makeNoteOn(60, 0x8000));
      render(vm, 8);
      vm.handleMIDIMessage(pm::fromMidi1Bytes(0xB0, 120, 0));
      vm.handleMIDIMessage(pm::makeNoteOn(62, 0x8000));
      CHECK(render(vm, 20) > 0.05f, "a note after CC 120 sounds");
   }
}

int main() {
   const double sr = 48000.0;
   const uint32_t N = 512;

   VoiceManager vm(sr, N);
   configure(vm);

   float L[512], R[512];
   auto render = [&]() {
      std::memset(L, 0, sizeof(L));
      std::memset(R, 0, sizeof(R));
      vm.process(N, L, R);
   };
   auto peak = [&]() {
      float p = 0.0f;
      for (uint32_t i = 0; i < N; ++i)
         p = std::fmax(p, std::fabs(L[i]));
      return p;
   };

   // Play a C-major chord: three distinct notes → three voices.
   vm.handleMIDIMessage(pm::makeNoteOn(60, 0x8000));
   vm.handleMIDIMessage(pm::makeNoteOn(64, 0x8000));
   vm.handleMIDIMessage(pm::makeNoteOn(67, 0x8000));
   CHECK(vm.activeCount() == 3, "3 voices active for a 3-note chord");

   float chordPeak = 0.0f;
   for (int b = 0; b < 20; ++b) {
      render();
      chordPeak = std::fmax(chordPeak, peak());
   }
   CHECK(chordPeak > 0.05f, "chord produces audio");

   // Release all notes → voices free, output decays.
   vm.handleMIDIMessage(pm::makeNoteOff(60));
   vm.handleMIDIMessage(pm::makeNoteOff(64));
   vm.handleMIDIMessage(pm::makeNoteOff(67));
   for (int b = 0; b < 200; ++b)
      render();
   CHECK(peak() < chordPeak * 0.1f, "chord decays to silence after release");
   CHECK(vm.activeCount() == 0, "all voices released");

   testChannelModeMessages(sr, N);

   std::printf("VoiceManager: chordPeak=%.3f active-after-release=%u\n", chordPeak,
               vm.activeCount());
   if (g_fails == 0)
      std::printf("ALL PASS\n");
   return g_fails ? 1 : 0;
}
