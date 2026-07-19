// IFSynthUI.cpp — UI side. A grid of module panels (wide layout). Three module rows of two
// panels each: Oscillator | Low-Pass Filter, Envelope 1 | Envelope 2, LFO 1 | LFO 2. Then a CV
// mixer row (Mixer 1 | Mixer 2) and a bottom row: the wide IFS Osc modulation matrix + a narrow
// Amp box in the lower right (VCA-envelope source over the master gain). Pad panels put a shrunk
// fractal pad on the left + a single row of knobs/dropdowns on the right; the pads edit
// fractal-shape params directly, scalars are knobs, enums are dropdowns. The UI is the callback
// for all of them, routing by param index.

#include "DistrhoUI.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Dropdown.hpp"
#include "GroupBox.hpp"
#include "IFSControl.hpp"
#include "IFSEnvControl.hpp"
#include "IFSynthParams.h"
#include "Knob.hpp"
#include "PopupMenu.hpp"
#include "PresetDump.hpp" // dev-only JSON patch dumper (no-op unless IFSYNTH_PRESET_DUMP=1)
#include "Slider.hpp"
#include "TabBar.hpp"
#include "Toggle.hpp"

START_NAMESPACE_DISTRHO

using namespace ifsynth;
using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::Dropdown;
using DGL_NAMESPACE::GroupBox;
using DGL_NAMESPACE::IFSControl;
using DGL_NAMESPACE::IFSEnvControl;
using DGL_NAMESPACE::Knob;
using DGL_NAMESPACE::NanoSubWidget;
using DGL_NAMESPACE::PopupMenu;
using DGL_NAMESPACE::Slider;
using DGL_NAMESPACE::SubWidget;
using DGL_NAMESPACE::TabBar;
using DGL_NAMESPACE::Toggle;

#if IFSYNTH_PRESET_DUMP
// Dev-only "DUMP" button: a plain left-click writes the patch JSON. A button (not a keystroke or
// modifier-click) so it works identically in every host on macOS and Linux — Ctrl-click is a
// right-click on macOS, and hosts often don't forward key events to a plugin UI at all.
class DumpButton : public NanoSubWidget
{
public:
    DumpButton(Widget* parent, std::function<void()> onClick)
        : NanoSubWidget(parent), fOnClick(std::move(onClick)) { fHasFont = loadSharedResources(); }
protected:
    void onNanoDisplay() override
    {
        const float w = getWidth(), h = getHeight();
        beginPath(); roundedRect(0, 0, w, h, h * 0.22f);
        fillColor(Color(70, 44, 44)); fill();
        strokeColor(Color(205, 120, 120)); strokeWidth(1.0f); stroke();
        if (fHasFont)
        {
            fontFace(NANOVG_DEJAVU_SANS_TTF); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fontSize(h * 0.6f); fillColor(Color(235, 205, 205)); // scale text with the button
            text(w * 0.5f, h * 0.5f, "DUMP", nullptr);
        }
    }
    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button == 1 && ev.press
            && ev.pos.getX() >= 0 && ev.pos.getX() < getWidth()
            && ev.pos.getY() >= 0 && ev.pos.getY() < getHeight())
        {
            fOnClick();
            return true;
        }
        return false;
    }
private:
    std::function<void()> fOnClick;
    bool fHasFont = false;
};
#endif

class IFSynthUI : public UI,
                  public Knob::Callback,
                  public IFSControl::Callback,
                  public Dropdown::Callback,
                  public PopupMenu::Callback,
                  public Toggle::Callback,
                  public Slider::Callback,
                  public TabBar::Callback
{
public:
    IFSynthUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        // Lock the aspect ratio to the design ratio (980:900) so no host can hand the plugin
        // a disproportionate canvas — layout() stretches rows/pads to fill whatever it's
        // given, so an unconstrained host resize (previously keepAspectRatio=false, with a
        // minimum pair that didn't even match 980:900) is what let a host open the window
        // taller than intended.
        setGeometryConstraints(784, 576, true, false); // matches the 980:720 design ratio

        // Per-module accent colours, following PM's category scheme (sound=green, filter=purple,
        // control signals=amber, combiner=orange, modulation=teal).
        fOscGroup  = makeGroup("IFS Oscillator", colSound());
        fLpfGroup  = makeGroup("Low-Pass Filter", colFilter());
        // Env / LFO / Mixer are tab groups: the two instances share a rect + title, and a TabBar
        // switches which is visible, so the shared title is the module name (tabs say 1/2).
        fEnv1Group = makeGroup("IFS Envelope", colEnv());
        fEnv2Group = makeGroup("IFS Envelope", colEnv());
        fLfo1Group = makeGroup("IFS LFO", colLfo());
        fLfo2Group = makeGroup("IFS LFO", colLfo());
        fMix1Group = makeGroup("CV Mixer", colComb());
        fMix2Group = makeGroup("CV Mixer", colComb());
        fOscModGroup = makeGroup("IFS Osc Modulation  (source + depth)", colNeutral());
        fLpfModGroup = makeGroup("LPF Modulation", colNeutral());
        fAmpGroup  = makeGroup("Amp", colComb());

        fOscPad = std::make_unique<IFSControl>(fOscGroup.get(), this);
        fOscPad->setParamIds(kParamOscBx, kParamOscBy, kParamOscStartEndY, kParamOscSquash1, kParamOscSquash2);

        fEnv1Pad = std::make_unique<IFSEnvControl>(fEnv1Group.get(), this);
        fEnv1Pad->setParamIds(kParamEnvBlueX, kParamEnvBlueY, kParamEnvGreenX, kParamEnvGreenY,
                              kParamEnvSquash1, kParamEnvSquash2, kParamEnvSquash3);
        fEnv2Pad = std::make_unique<IFSEnvControl>(fEnv2Group.get(), this);
        fEnv2Pad->setParamIds(kParamEnv2BlueX, kParamEnv2BlueY, kParamEnv2GreenX, kParamEnv2GreenY,
                              kParamEnv2Squash1, kParamEnv2Squash2, kParamEnv2Squash3);

        fLfo1Pad = std::make_unique<IFSControl>(fLfo1Group.get(), this);
        fLfo1Pad->setParamIds(kParamLfoBx, kParamLfoBy, kParamLfoStartEndY, kParamLfoSquash1, kParamLfoSquash2);
        fLfo1Pad->setIterationBase(1);
        fLfo2Pad = std::make_unique<IFSControl>(fLfo2Group.get(), this);
        fLfo2Pad->setParamIds(kParamLfo2Bx, kParamLfo2By, kParamLfo2StartEndY, kParamLfo2Squash1, kParamLfo2Squash2);
        fLfo2Pad->setIterationBase(1);

        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            if (isPadParam(i))
                continue;
            if (isDropdownParam(i))
            {
                auto d = std::make_unique<Dropdown>(groupFor(i), this);
                d->setId(i);
                d->setLabel(kParams[i].name);
                d->setChoices(choicesFor(i));
                d->setIndex(static_cast<int>(kParams[i].def));
                fDropdowns[i] = std::move(d);
                continue;
            }
            if (isToggleParam(i))
            {
                auto t = std::make_unique<Toggle>(groupFor(i), this);
                t->setId(i);
                t->setLabel(kParams[i].name);
                t->setState(kParams[i].def > 0.5f);
                fToggles[i] = std::move(t);
                continue;
            }
            if (isSliderParam(i))
            {
                auto s = std::make_unique<Slider>(groupFor(i), this);
                s->setId(i);
                s->setLabel(kParams[i].name);
                s->setRange(kParams[i].min, kParams[i].max);
                s->setValue(kParams[i].def, false);
                fSliders[i] = std::move(s);
                continue;
            }
            const ParamSpec& s = kParams[i];
            auto knob = std::make_unique<Knob>(groupFor(i), this);
            knob->setId(i);
            knob->setLabel(s.name);
            knob->setAccentColor(accentFor(i));
            knob->configure(s.min, s.max, s.logarithmic, s.def);
            knob->setRealValue(s.def, false);
            knob->setFormatter(formatterFor(i));
            fKnobs[i] = std::move(knob);
        }

        // Tab bars for the Env / LFO / Mixer tab groups (created after the panels so they draw on
        // top of the title strip). Instance 2 of each starts hidden.
        fEnvTab = makeTab(kTabEnv, colEnv());
        fLfoTab = makeTab(kTabLfo, colLfo());
        fMixTab = makeTab(kTabMix, colComb());
        fEnv2Group->hide();
        fLfo2Group->hide();
        fMix2Group->hide();

#if IFSYNTH_PRESET_DUMP
        fDumpBtn = std::make_unique<DumpButton>(this, [this] {
            const std::string p = dumpPresetToFile(fMirror);
            std::fprintf(stderr, "[IFSynth] patch %s %s\n",
                         p.empty() ? "dump FAILED" : "dumped to", p.c_str());
        });
#endif
        fMenu = std::make_unique<PopupMenu>(this, this); // created last → draws on top

        pushPadDefaults();
        layout();
        updateGlideEnabled();
#if IFSYNTH_PRESET_DUMP
        for (uint32_t i = 0; i < kParamCount; ++i) fMirror[i] = kParams[i].def;
#endif
    }

protected:
    /* -- host -> UI ------------------------------------------------------- */
    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= kParamCount)
            return;
        if (fKnobs[index])     { fKnobs[index]->setRealValue(value, false); fKnobs[index]->repaint(); }
        if (fDropdowns[index]) fDropdowns[index]->setIndex(static_cast<int>(value));
        if (fToggles[index])   fToggles[index]->setState(value > 0.5f);
        if (fSliders[index])   fSliders[index]->setValue(value, false);
        routeToPad(index, value);
        recordMirror(index, value);
        if (index == kParamVoiceMode) updateGlideEnabled();
    }

    /* -- toggle -> host --------------------------------------------------- */
    void toggleChanged(Toggle* t, bool on) override
    {
        const uint32_t id = t->getId();
        editParameter(id, true);
        setParameterValue(id, on ? 1.0f : 0.0f);
        editParameter(id, false);
        recordMirror(id, on ? 1.0f : 0.0f);
    }

    /* -- slider -> host --------------------------------------------------- */
    void sliderDragStarted(Slider* s) override  { editParameter(s->getId(), true); }
    void sliderDragFinished(Slider* s) override { editParameter(s->getId(), false); }
    void sliderValueChanged(Slider* s, float v) override { setParameterValue(s->getId(), v); recordMirror(s->getId(), v); }

    /* -- knob -> host ----------------------------------------------------- */
    void knobDragStarted(Knob* k) override  { editParameter(k->getId(), true); }
    void knobDragFinished(Knob* k) override { editParameter(k->getId(), false); }
    void knobValueChanged(Knob* k, float v) override { setParameterValue(k->getId(), v); recordMirror(k->getId(), v); }

    /* -- pad -> host (all pads share IFSControl::Callback) ---------------- */
    void ifsDragStarted(uint32_t id) override  { editParameter(id, true); }
    void ifsDragFinished(uint32_t id) override { editParameter(id, false); }
    void ifsValueChanged(uint32_t id, float v) override { setParameterValue(id, v); recordMirror(id, v); }

    /* -- dropdown + menu -------------------------------------------------- */
    void dropdownClicked(Dropdown* d) override
    {
        int x, y, w, h;
        d->boxRect(x, y, w, h); // y = box top, h = box height (window-absolute)
        fMenu->openFor(d->getId(), d->choices(), d->index(), x, y + h, y, w,
                       static_cast<int>(getHeight()));
    }
    void menuSelected(PopupMenu* menu, int index) override
    {
        const uint32_t id = menu->tag();
        editParameter(id, true);
        setParameterValue(id, static_cast<float>(index));
        editParameter(id, false);
        recordMirror(id, static_cast<float>(index));
        if (fDropdowns[id]) fDropdowns[id]->setIndex(index);
        routeToPad(id, static_cast<float>(index));
        if (id == kParamVoiceMode) updateGlideEnabled();
    }

    /* -- drawing / layout ------------------------------------------------- */
    void onNanoDisplay() override
    {
        beginPath(); rect(0, 0, getWidth(), getHeight()); fillColor(24, 26, 31); fill();
    }
    void onResize(const ResizeEvent& ev) override { UI::onResize(ev); layout(); }

#if IFSYNTH_PRESET_DUMP
    // Dev-only: press 'P' to write the current patch as JSON to $HOME.
    bool onKeyboard(const KeyboardEvent& ev) override
    {
        if (ev.press && ev.key == 'p')
        {
            const std::string path = dumpPresetToFile(fMirror);
            std::fprintf(stderr, "[IFSynth] patch %s %s\n",
                         path.empty() ? "dump FAILED" : "dumped to", path.c_str());
            return true;
        }
        return UI::onKeyboard(ev);
    }
#endif

private:
    // Panels declared before their child controls so the children are destroyed first.
    std::unique_ptr<GroupBox>      fOscGroup, fLpfGroup, fEnv1Group, fEnv2Group, fLfo1Group, fLfo2Group;
    std::unique_ptr<GroupBox>      fMix1Group, fMix2Group, fOscModGroup, fLpfModGroup, fAmpGroup;
    std::unique_ptr<IFSControl>    fOscPad, fLfo1Pad, fLfo2Pad;
    std::unique_ptr<IFSEnvControl> fEnv1Pad, fEnv2Pad;
    std::unique_ptr<Knob>          fKnobs[kParamCount];
    std::unique_ptr<Dropdown>      fDropdowns[kParamCount];
    std::unique_ptr<Toggle>        fToggles[kParamCount];
    std::unique_ptr<Slider>        fSliders[kParamCount];
    std::unique_ptr<TabBar>        fEnvTab, fLfoTab, fMixTab;
    std::unique_ptr<PopupMenu>     fMenu;
#if IFSYNTH_PRESET_DUMP
    std::unique_ptr<DumpButton>    fDumpBtn;
#endif
    float                          fUiScale = 1.0f; // set each layout() pass; read by the dropH helpers below

    static bool isPadParam(uint32_t i)
    {
        switch (i)
        {
        case kParamOscBx: case kParamOscBy: case kParamOscStartEndY:
        case kParamOscSquash1: case kParamOscSquash2:
        case kParamEnvBlueX: case kParamEnvBlueY: case kParamEnvGreenX: case kParamEnvGreenY:
        case kParamEnvSquash1: case kParamEnvSquash2: case kParamEnvSquash3:
        case kParamLfoBx: case kParamLfoBy: case kParamLfoStartEndY:
        case kParamLfoSquash1: case kParamLfoSquash2:
        case kParamEnv2BlueX: case kParamEnv2BlueY: case kParamEnv2GreenX: case kParamEnv2GreenY:
        case kParamEnv2Squash1: case kParamEnv2Squash2: case kParamEnv2Squash3:
        case kParamLfo2Bx: case kParamLfo2By: case kParamLfo2StartEndY:
        case kParamLfo2Squash1: case kParamLfo2Squash2:
            return true;
        default: return false;
        }
    }
    static bool isDropdownParam(uint32_t i)
    {
        switch (i)
        {
        case kParamOscIterations:
        case kParamEnvIterations: case kParamEnvHoldMode:
        case kParamLfoIterations:
        case kParamEnv2Iterations: case kParamEnv2HoldMode:
        case kParamLfo2Iterations:
        case kParamMix1Mode: case kParamMix2Mode:
        case kParamVoiceMode:
            return true;
        default: return isModSrcParam(i); // mod source + VCA dropdowns
        }
    }
    static bool isToggleParam(uint32_t i)
    {
        switch (i)
        {
        case kParamMix1EnableLFO: case kParamMix1EnableEnv: case kParamMix1EnableCC:
        case kParamMix2EnableLFO: case kParamMix2EnableEnv: case kParamMix2EnableCC:
            return true;
        default: return false;
        }
    }
    static bool isSliderParam(uint32_t i) // the mod depths
    {
        switch (i)
        {
        case kParamOscPitchModDepth: case kParamOscBxModDepth: case kParamOscByModDepth:
        case kParamOscSq1ModDepth: case kParamOscSq2ModDepth:
        case kParamLpfCutoffModDepth:
            return true;
        default: return false;
        }
    }
    static bool isModSrcParam(uint32_t i) // source dropdowns (osc mods + LPF cutoff + VCA)
    {
        switch (i)
        {
        case kParamOscPitchModSrc: case kParamOscBxModSrc: case kParamOscByModSrc:
        case kParamOscSq1ModSrc: case kParamOscSq2ModSrc:
        case kParamLpfCutoffModSrc: case kParamVCASource:
            return true;
        default: return false;
        }
    }

    // Module-category accent colours (PM-inspired, but Env/LFO/Mixer kept clearly distinct).
    static Color colSound()   { return Color(60, 200, 90); }   // oscillator (sound) — green
    static Color colFilter()  { return Color(170, 112, 226); } // filter — purple
    static Color colEnv()     { return Color(220, 206, 72); }  // envelopes — muted yellow
    static Color colLfo()     { return Color(233, 112, 158); } // LFOs — rose
    static Color colComb()    { return Color(242, 150, 46); }  // combiner (mixers, VCA/amp) — orange
    static Color colNeutral() { return Color(120, 190, 205); } // modulation routing — teal

    // Same dispatch as groupFor, but returns the module's accent colour (for knobs).
    Color accentFor(uint32_t i) const
    {
        if (i == kParamGain || i == kParamVoiceMode || i == kParamSpread) return colComb();
        if (i == kParamLpfCutoff || i == kParamLpfResonance || i == kParamLpfKeyTrack)
            return colFilter();
        if (i == kParamLpfCutoffModSrc || i == kParamLpfCutoffModDepth) return colNeutral();
        if (i <= kParamOscIterations)  return colSound();
        if (i <= kParamEnvHoldMode)    return colEnv();
        if (i <= kParamLfoIterations)  return colLfo();
        if (i <= kParamEnv2HoldMode)   return colEnv();
        if (i <= kParamLfo2Iterations) return colLfo();
        if (i <= kParamMix2Mode)       return colComb();
        if (i == kParamVCASource)      return colComb();
        return colNeutral(); // osc mod
    }

    GroupBox* groupFor(uint32_t i) const
    {
        if (i == kParamGain || i == kParamVoiceMode || i == kParamSpread) return fAmpGroup.get();
        if (i == kParamLpfCutoff || i == kParamLpfResonance || i == kParamLpfKeyTrack)
            return fLpfGroup.get();
        if (i == kParamLpfCutoffModSrc || i == kParamLpfCutoffModDepth)
            return fLpfModGroup.get();
        if (i <= kParamOscIterations)    return fOscGroup.get();
        if (i <= kParamEnvHoldMode)      return fEnv1Group.get();
        if (i <= kParamLfoIterations)    return fLfo1Group.get();
        if (i <= kParamEnv2HoldMode)     return fEnv2Group.get();
        if (i <= kParamLfo2Iterations)   return fLfo2Group.get();
        if (i <= kParamMix1Mode)         return fMix1Group.get();
        if (i <= kParamMix2Mode)         return fMix2Group.get();
        if (i == kParamVCASource)        return fAmpGroup.get();
        return fOscModGroup.get(); // the IFS Osc mod inputs
    }

    static std::vector<std::string> choicesFor(uint32_t i)
    {
        if (i == kParamOscIterations) return {"3", "4"};
        if (i == kParamEnvHoldMode || i == kParamEnv2HoldMode) return {"None", "Blue", "Green", "Loop"};
        if (i == kParamMix1Mode || i == kParamMix2Mode) return {"Add", "Multiply"};
        if (i == kParamVoiceMode) return {"Mono", "10 Voices"};
        if (i == kParamVCASource) return {"Env 1", "Env 2"};
        if (isModSrcParam(i)) return {"None", "LFO 1", "LFO 2", "Env 1", "Env 2", "CV Mix 1", "CV Mix 2"};
        return {"1", "2", "3", "4", "5", "6", "7", "8"}; // env/lfo iterations
    }

    void routeToPad(uint32_t index, float v)
    {
        switch (index)
        {
        case kParamOscBx: fOscPad->setBx(v); break;
        case kParamOscBy: fOscPad->setBy(v); break;
        case kParamOscStartEndY: fOscPad->setStartEndY(v); break;
        case kParamOscSquash1: fOscPad->setSquash1(v); break;
        case kParamOscSquash2: fOscPad->setSquash2(v); break;
        case kParamOscSmoothing: fOscPad->setSmoothing(v); break;
        case kParamOscIterations: fOscPad->setIterations(v); break;
        case kParamEnvBlueX: fEnv1Pad->setBlueX(v); break;
        case kParamEnvBlueY: fEnv1Pad->setBlueY(v); break;
        case kParamEnvGreenX: fEnv1Pad->setGreenX(v); break;
        case kParamEnvGreenY: fEnv1Pad->setGreenY(v); break;
        case kParamEnvSquash1: fEnv1Pad->setSquash1(v); break;
        case kParamEnvSquash2: fEnv1Pad->setSquash2(v); break;
        case kParamEnvSquash3: fEnv1Pad->setSquash3(v); break;
        case kParamEnvSmoothing: fEnv1Pad->setSmoothing(v); break;
        case kParamEnvIterations: fEnv1Pad->setIterations(v); break;
        case kParamLfoBx: fLfo1Pad->setBx(v); break;
        case kParamLfoBy: fLfo1Pad->setBy(v); break;
        case kParamLfoStartEndY: fLfo1Pad->setStartEndY(v); break;
        case kParamLfoSquash1: fLfo1Pad->setSquash1(v); break;
        case kParamLfoSquash2: fLfo1Pad->setSquash2(v); break;
        case kParamLfoSmoothing: fLfo1Pad->setSmoothing(v); break;
        case kParamLfoIterations: fLfo1Pad->setIterations(v); break;
        case kParamEnv2BlueX: fEnv2Pad->setBlueX(v); break;
        case kParamEnv2BlueY: fEnv2Pad->setBlueY(v); break;
        case kParamEnv2GreenX: fEnv2Pad->setGreenX(v); break;
        case kParamEnv2GreenY: fEnv2Pad->setGreenY(v); break;
        case kParamEnv2Squash1: fEnv2Pad->setSquash1(v); break;
        case kParamEnv2Squash2: fEnv2Pad->setSquash2(v); break;
        case kParamEnv2Squash3: fEnv2Pad->setSquash3(v); break;
        case kParamEnv2Smoothing: fEnv2Pad->setSmoothing(v); break;
        case kParamEnv2Iterations: fEnv2Pad->setIterations(v); break;
        case kParamLfo2Bx: fLfo2Pad->setBx(v); break;
        case kParamLfo2By: fLfo2Pad->setBy(v); break;
        case kParamLfo2StartEndY: fLfo2Pad->setStartEndY(v); break;
        case kParamLfo2Squash1: fLfo2Pad->setSquash1(v); break;
        case kParamLfo2Squash2: fLfo2Pad->setSquash2(v); break;
        case kParamLfo2Smoothing: fLfo2Pad->setSmoothing(v); break;
        case kParamLfo2Iterations: fLfo2Pad->setIterations(v); break;
        default: break;
        }
    }

    // Patch-value mirror for the dev-only JSON dumper: no-op (and no member) in release builds.
#if IFSYNTH_PRESET_DUMP
    float fMirror[kParamCount];
    void recordMirror(uint32_t id, float v) { if (id < kParamCount) fMirror[id] = v; }
#else
    void recordMirror(uint32_t, float) {}
#endif

    // Glide only does anything in mono (index 0 of the Voices dropdown); grey it out otherwise.
    void updateGlideEnabled()
    {
        const bool mono = fDropdowns[kParamVoiceMode] && fDropdowns[kParamVoiceMode]->index() == 0;
        if (fKnobs[kParamSlewRate]) fKnobs[kParamSlewRate]->setEnabled(mono);
    }

    std::unique_ptr<GroupBox> makeGroup(const char* title, Color accent)
    {
        auto g = std::make_unique<GroupBox>(this);
        g->setTitle(title);
        g->setAccentColor(accent);
        return g;
    }

    enum TabId { kTabEnv = 0, kTabLfo = 1, kTabMix = 2 };

    std::unique_ptr<TabBar> makeTab(uint32_t id, Color accent)
    {
        auto t = std::make_unique<TabBar>(this, this);
        t->setId(id);
        t->setTabs({"1", "2"});
        t->setAccentColor(accent);
        t->setActive(0);
        return t;
    }

    // TabBar::Callback — show the picked instance, hide the other. Hiding a GroupBox hides its
    // whole child subtree (pad + knobs + dropdowns), so only the active tab is drawn/interactive.
    void tabSelected(TabBar* bar, int index) override
    {
        switch (bar->getId())
        {
        case kTabEnv: showTab(fEnv1Group.get(), fEnv2Group.get(), index); break;
        case kTabLfo: showTab(fLfo1Group.get(), fLfo2Group.get(), index); break;
        case kTabMix: showTab(fMix1Group.get(), fMix2Group.get(), index); break;
        default: break;
        }
        repaint();
    }
    static void showTab(GroupBox* a, GroupBox* b, int index)
    {
        if (index == 0) { a->show(); b->hide(); }
        else            { a->hide(); b->show(); }
    }

    void pushPadDefaults()
    {
        pushOscPad(fOscPad.get(), kParamOscBx);
        pushLfoPad(fLfo1Pad.get(), kParamLfoBx);
        pushLfoPad(fLfo2Pad.get(), kParamLfo2Bx);
        pushEnvPad(fEnv1Pad.get(), kParamEnvDuration);
        pushEnvPad(fEnv2Pad.get(), kParamEnv2Duration);
    }
    // The osc/lfo pads share IFSControl; params are laid out identically from a base index.
    void pushOscPad(IFSControl* p, uint32_t bx)
    {
        p->setBx(kParams[bx + 0].def); p->setBy(kParams[bx + 1].def);
        p->setStartEndY(kParams[bx + 2].def); p->setSquash1(kParams[bx + 3].def);
        p->setSquash2(kParams[bx + 4].def);
        p->setSmoothing(kParams[bx + 5].def); p->setIterations(kParams[bx + 6].def);
    }
    void pushLfoPad(IFSControl* p, uint32_t bx) { pushOscPad(p, bx); }
    void pushEnvPad(IFSEnvControl* p, uint32_t dur)
    {
        // env order: Duration, BlueX, BlueY, GreenX, GreenY, Sq1, Sq2, Sq3, Smooth, Iter, Hold
        // (no StartEndY — envelopes are pinned to start/end at 0).
        p->setBlueX(kParams[dur + 1].def);  p->setBlueY(kParams[dur + 2].def);
        p->setGreenX(kParams[dur + 3].def); p->setGreenY(kParams[dur + 4].def);
        p->setSquash1(kParams[dur + 5].def); p->setSquash2(kParams[dur + 6].def);
        p->setSquash3(kParams[dur + 7].def); p->setSmoothing(kParams[dur + 8].def);
        p->setIterations(kParams[dur + 9].def);
    }

    void layout()
    {
        const int W = static_cast<int>(getWidth());
        const int H = static_cast<int>(getHeight());
        const int m = 8, gap = 8;
        // Compute the window scale first so the (dev) DUMP strip + button scale with the canvas —
        // on Retina the host hands us a ~2x physical-pixel canvas, so fixed sizes render tiny.
        const float scale = std::fmin(static_cast<float>(W) / DISTRHO_UI_DEFAULT_WIDTH,
                                      static_cast<float>(H) / DISTRHO_UI_DEFAULT_HEIGHT);
        const int colW = (W - 2 * m - gap) / 2;
        // All five rows (3 module rows + mixer row + modulation row) share one height and are
        // derived from the ACTUAL canvas height so the stack fills the window exactly on any
        // host. This self-corrects for the Retina-vs-logical pixel difference: macOS hands us a
        // physical-pixel canvas (~1800 tall → rowH ~350), Linux a logical one (900 → rowH ~170),
        // and the shared `scale` below sizes knobs/text by the same factor. A previously
        // hard-coded 349px row assumed the macOS height and overflowed the default window on
        // hosts that report logical pixels (Linux), pushing the bottom rows off-screen. The
        // aspect ratio is locked (see the constructor), so rows never stretch disproportionately.
#if IFSYNTH_PRESET_DUMP
        const int topPad = static_cast<int>(24 * scale); // dev: top strip for the DUMP button
#else
        const int topPad = 0;
#endif
        // Four rows now that Env/LFO/Mixer are tab groups: Osc|LPF, Env-tabs|LFO-tabs,
        // Mixer-tabs|(free), and the OscMod|LpfMod|Amp strip. Derived from the canvas height so it
        // fills any size (Retina-safe); the aspect ratio is locked in the constructor.
        const int rowH = (H - 2 * m - 3 * gap - topPad) / 4;
        const int colL = m, colR = m + colW + gap;
        const int top0 = m + topPad;
        const int row0 = top0, row1 = top0 + rowH + gap;
        const int mixY = top0 + 2 * (rowH + gap);
        const int modY = top0 + 3 * (rowH + gap);

        // Uniform knob size + text scale (scale computed above). No tight upper clamp on text
        // scale: a host can hand us a canvas much larger than the design size, and text needs to
        // keep pace with that or it reads as "tiny" next to knobs/pads that already scale freely.
        const float knobD = std::fmax(30.0f, 54.0f * scale);
        const float txtScale = std::fmax(0.72f, std::fmin(2.0f, scale));
        fUiScale = txtScale; // stashed for layoutModPanel/layoutLpfModPanel/layoutAmp's dropH
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (fKnobs[i]) fKnobs[i]->setDiameter(knobD, txtScale);

        // Group titles / dropdown labels / popup menu text aren't part of the knob widget, so
        // they need the same scale applied explicitly. setUiScale scales each widget's own
        // internal box/label geometry along with its font, so text can't overflow its box.
        GroupBox* allGroups[] = { fOscGroup.get(), fLpfGroup.get(), fEnv1Group.get(), fEnv2Group.get(),
                                   fLfo1Group.get(), fLfo2Group.get(), fMix1Group.get(), fMix2Group.get(),
                                   fOscModGroup.get(), fLpfModGroup.get(), fAmpGroup.get() };
        for (GroupBox* g : allGroups)
            g->setUiScale(txtScale);
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (fDropdowns[i]) fDropdowns[i]->setUiScale(txtScale);
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (fSliders[i]) fSliders[i]->setUiScale(txtScale);
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (fToggles[i]) fToggles[i]->setUiScale(txtScale);
        fMenu->setUiScale(txtScale);

        // Top row is asymmetric: the Oscillator (pad + 5 controls) is wider than the LPF.
        const int topAvail = W - 2 * m - gap;
        const int oscW = static_cast<int>(topAvail * 0.64f);
        const int lpfW = topAvail - oscW;

        // Row 0: Osc (wide) | LPF (narrow).
        placeGroup(fOscGroup.get(), colL, row0, oscW, rowH);
        placeGroup(fLpfGroup.get(), colL + oscW + gap, row0, lpfW, rowH);

        // Row 1: LFO tab group (left) + Env tab group (right). Each pair overlays a single rect;
        // the TabBar shows one and hides the other.
        placeGroup(fLfo1Group.get(), colL, row1, colW, rowH);
        placeGroup(fLfo2Group.get(), colL, row1, colW, rowH);
        placeGroup(fEnv1Group.get(), colR, row1, colW, rowH);
        placeGroup(fEnv2Group.get(), colR, row1, colW, rowH);

        // Row 2: CV Mixer tab group (narrow — its content is sparse, so it needn't match the
        // column width above it) + the wide IFS Osc modulation panel + the LPF modulation panel.
        const int mixW    = static_cast<int>(300 * fUiScale);
        const int lpfModW = static_cast<int>(150 * fUiScale);
        const int lpfModX = W - m - lpfModW;
        const int oscModX = colL + mixW + gap;
        const int oscModW = lpfModX - gap - oscModX;
        placeGroup(fMix1Group.get(),   colL,    mixY, mixW,    rowH);
        placeGroup(fMix2Group.get(),   colL,    mixY, mixW,    rowH);
        placeGroup(fOscModGroup.get(), oscModX, mixY, oscModW, rowH);
        placeGroup(fLpfModGroup.get(), lpfModX, mixY, lpfModW, rowH);

        // Row 3: Amp box (right); the left of this row is free for now.
        const int ampW = static_cast<int>(224 * fUiScale);
        placeGroup(fAmpGroup.get(), W - m - ampW, modY, ampW, rowH);

        // Tab bars in each tab group's title strip (top-right).
        placeTab(fLfoTab.get(), colL, row1, colW, scale);
        placeTab(fEnvTab.get(), colR, row1, colW, scale);
        placeTab(fMixTab.get(), colL, mixY, mixW, scale);

        SubWidget* oscG[6]  = { fKnobs[kParamOscPitch].get(), fKnobs[kParamOscFine].get(),
                                fKnobs[kParamOscDetune].get(), fKnobs[kParamSlewRate].get(),
                                fKnobs[kParamOscSmoothing].get(), fDropdowns[kParamOscIterations].get() };
        layoutPadPanel(fOscGroup.get(), fOscPad.get(), oscG, 6);

        SubWidget* env1G[4] = { fKnobs[kParamEnvDuration].get(), fKnobs[kParamEnvSmoothing].get(),
                                fDropdowns[kParamEnvHoldMode].get(), fDropdowns[kParamEnvIterations].get() };
        layoutPadPanel(fEnv1Group.get(), fEnv1Pad.get(), env1G, 4);

        SubWidget* env2G[4] = { fKnobs[kParamEnv2Duration].get(), fKnobs[kParamEnv2Smoothing].get(),
                                fDropdowns[kParamEnv2HoldMode].get(), fDropdowns[kParamEnv2Iterations].get() };
        layoutPadPanel(fEnv2Group.get(), fEnv2Pad.get(), env2G, 4);

        SubWidget* lfo1G[3] = { fKnobs[kParamLfoRate].get(), fKnobs[kParamLfoSmoothing].get(),
                                fDropdowns[kParamLfoIterations].get() };
        layoutPadPanel(fLfo1Group.get(), fLfo1Pad.get(), lfo1G, 3);

        SubWidget* lfo2G[3] = { fKnobs[kParamLfo2Rate].get(), fKnobs[kParamLfo2Smoothing].get(),
                                fDropdowns[kParamLfo2Iterations].get() };
        layoutPadPanel(fLfo2Group.get(), fLfo2Pad.get(), lfo2G, 3);

        // Low-pass filter: cutoff, resonance, key-track knobs across the panel.
        SubWidget* lpf[3] = { fKnobs[kParamLpfCutoff].get(), fKnobs[kParamLpfResonance].get(),
                              fKnobs[kParamLpfKeyTrack].get() };
        layoutRow(fLpfGroup.get(), lpf, 3);

        SubWidget* mix1[5] = { fToggles[kParamMix1EnableLFO].get(), fToggles[kParamMix1EnableEnv].get(),
                               fToggles[kParamMix1EnableCC].get(), fKnobs[kParamMix1CCNumber].get(),
                               fDropdowns[kParamMix1Mode].get() };
        layoutRow(fMix1Group.get(), mix1, 5);
        SubWidget* mix2[5] = { fToggles[kParamMix2EnableLFO].get(), fToggles[kParamMix2EnableEnv].get(),
                               fToggles[kParamMix2EnableCC].get(), fKnobs[kParamMix2CCNumber].get(),
                               fDropdowns[kParamMix2Mode].get() };
        layoutRow(fMix2Group.get(), mix2, 5);

        layoutModPanel();

        layoutLpfModPanel();

        layoutAmp();

#if IFSYNTH_PRESET_DUMP
        if (fDumpBtn)
        {
            const int bw = static_cast<int>(58 * scale), bh = static_cast<int>(18 * scale);
            fDumpBtn->setSize(static_cast<uint>(bw), static_cast<uint>(bh));
            fDumpBtn->setAbsolutePos(W - bw - m, (topPad - bh) / 2 + m / 2);
        }
#endif
    }

    // LPF cutoff modulation: one cell — a source dropdown over a depth fader.
    void layoutLpfModPanel()
    {
        int cx, cy, cw, ch;
        fLpfModGroup->contentBounds(cx, cy, cw, ch);
        const int dropH = static_cast<int>(46 * fUiScale);
        Dropdown* d = fDropdowns[kParamLpfCutoffModSrc].get();
        d->setSize(static_cast<uint>(cw), static_cast<uint>(dropH));
        d->setAbsolutePos(cx, cy);
        Slider* s = fSliders[kParamLpfCutoffModDepth].get();
        s->setSize(static_cast<uint>(cw), static_cast<uint>(ch - dropH - 4));
        s->setAbsolutePos(cx, cy + dropH + 4);
    }

    // IFS Osc modulation: 5 cells, each a source dropdown over a depth fader.
    void layoutModPanel()
    {
        struct Cell { uint32_t src; uint32_t depth; };
        const Cell cells[5] = {
            {kParamOscPitchModSrc, kParamOscPitchModDepth},
            {kParamOscBxModSrc,    kParamOscBxModDepth},
            {kParamOscByModSrc,    kParamOscByModDepth},
            {kParamOscSq1ModSrc,   kParamOscSq1ModDepth},
            {kParamOscSq2ModSrc,   kParamOscSq2ModDepth},
        };
        int cx, cy, cw, ch;
        fOscModGroup->contentBounds(cx, cy, cw, ch);
        const int n = 5, cellW = cw / n, dropH = static_cast<int>(46 * fUiScale);
        for (int i = 0; i < n; ++i)
        {
            const int x = cx + i * cellW;
            Dropdown* d = fDropdowns[cells[i].src].get();
            d->setSize(static_cast<uint>(cellW), static_cast<uint>(dropH));
            d->setAbsolutePos(x, cy);
            Slider* s = fSliders[cells[i].depth].get();
            s->setSize(static_cast<uint>(cellW), static_cast<uint>(ch - dropH - 4));
            s->setAbsolutePos(x, cy + dropH + 4);
        }
    }

    // Amp box (lower right): VCA-envelope source + Voices dropdowns side by side on top, then the
    // master Gain and Spread knobs side by side below.
    void layoutAmp()
    {
        int cx, cy, cw, ch;
        fAmpGroup->contentBounds(cx, cy, cw, ch);
        const int dropH = static_cast<int>(46 * fUiScale);
        const int half = cw / 2;
        fDropdowns[kParamVCASource]->setSize(static_cast<uint>(half), static_cast<uint>(dropH));
        fDropdowns[kParamVCASource]->setAbsolutePos(cx, cy);
        fDropdowns[kParamVoiceMode]->setSize(static_cast<uint>(cw - half), static_cast<uint>(dropH));
        fDropdowns[kParamVoiceMode]->setAbsolutePos(cx + half, cy);

        const int ky = cy + dropH + 6;
        const int kh = ch - dropH - 6;
        fKnobs[kParamGain]->setSize(static_cast<uint>(half), static_cast<uint>(kh));
        fKnobs[kParamGain]->setAbsolutePos(cx, ky);
        fKnobs[kParamSpread]->setSize(static_cast<uint>(cw - half), static_cast<uint>(kh));
        fKnobs[kParamSpread]->setAbsolutePos(cx + half, ky);
    }

    // A pad-less panel: n controls evenly across the content area (used by the CV mixers).
    void layoutRow(GroupBox* group, SubWidget* grid[], int n)
    {
        int cx, cy, cw, ch;
        group->contentBounds(cx, cy, cw, ch);
        const int cellW = n > 0 ? cw / n : cw;
        for (int i = 0; i < n; ++i)
        {
            grid[i]->setSize(static_cast<uint>(cellW), static_cast<uint>(ch));
            grid[i]->setAbsolutePos(cx + i * cellW, cy);
        }
    }

    void placeGroup(GroupBox* g, int x, int y, int w, int h)
    {
        g->setAbsolutePos(x, y);
        g->setSize(static_cast<uint>(w), static_cast<uint>(h));
    }

    // Place a tab group's TabBar in the top-right of its panel title strip.
    void placeTab(TabBar* t, int gx, int gy, int gw, float scale)
    {
        const int tabH  = static_cast<int>(20 * scale);
        const int tabW  = static_cast<int>(56 * scale);
        const int inset = static_cast<int>(8 * scale);
        t->setSize(static_cast<uint>(tabW), static_cast<uint>(tabH));
        t->setAbsolutePos(gx + gw - tabW - inset, gy + static_cast<int>(4 * scale));
    }

    // Landscape panel: shrunk pad on the left, a single row of n controls on the right.
    void layoutPadPanel(GroupBox* group, NanoSubWidget* pad, SubWidget* grid[], int n)
    {
        int cx, cy, cw, ch;
        group->contentBounds(cx, cy, cw, ch);
        const int gap = 8;
        const int padW = static_cast<int>(cw * 0.42);
        // Fixed 2:1 landscape, independent of row height — hosts vary in whether they honour
        // setGeometryConstraints' keepAspectRatio, so this can't rely on a well-shaped `ch`.
        const int padH = std::min(ch, static_cast<int>(padW * 0.5f));
        pad->setSize(static_cast<uint>(padW), static_cast<uint>(padH));
        pad->setAbsolutePos(cx, cy + (ch - padH) / 2);

        const int kx = cx + padW + gap, kw = cw - padW - gap;
        const int cellW = n > 0 ? kw / n : kw;
        for (int i = 0; i < n; ++i)
        {
            grid[i]->setSize(static_cast<uint>(cellW), static_cast<uint>(ch));
            grid[i]->setAbsolutePos(kx + i * cellW, cy);
        }
    }

    static std::function<std::string(float)> formatterFor(uint32_t index)
    {
        if (index == kParamLfoRate || index == kParamLfo2Rate) // octaves → Hz (0 = 1 Hz)
            return [](float v) { char b[16]; std::snprintf(b, sizeof(b), "%.2f Hz", std::exp2(v)); return std::string(b); };
        if (index == kParamLpfCutoff) // octaves above A440 → cutoff frequency
            return [](float v) {
                const double f = 440.0 * std::exp2(v);
                char b[16];
                f >= 1000.0 ? std::snprintf(b, sizeof(b), "%.2f kHz", f / 1000.0)
                            : std::snprintf(b, sizeof(b), "%.0f Hz", f);
                return std::string(b);
            };
        if (index == kParamMix1CCNumber || index == kParamMix2CCNumber)
            return [](float v) { char b[16]; std::snprintf(b, sizeof(b), "CC %d", static_cast<int>(v + 0.5f)); return std::string(b); };
        if (index == kParamSpread) // stereo width as a percentage
            return [](float v) { char b[8]; std::snprintf(b, sizeof(b), "%.0f%%", v * 100.0f); return std::string(b); };
        const std::string u = kParams[index].unit;
        const bool isTime = u == "s";
        return [isTime, u](float v) {
            char b[24];
            if (isTime)
                v < 1.0f ? std::snprintf(b, sizeof(b), "%.0f ms", v * 1000.0f)
                         : std::snprintf(b, sizeof(b), "%.2f s", v);
            else if (!u.empty())
                std::snprintf(b, sizeof(b), "%.1f %s", v, u.c_str());
            else
                std::snprintf(b, sizeof(b), "%.2f", v);
            return std::string(b);
        };
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IFSynthUI)
};

UI* createUI()
{
    return new IFSynthUI();
}

END_NAMESPACE_DISTRHO
