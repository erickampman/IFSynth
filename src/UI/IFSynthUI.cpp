// IFSynthUI.cpp — UI side. A grid of module panels (wide layout). Three module rows of two
// panels each: Oscillator | Low-Pass Filter, Envelope 1 | Envelope 2, LFO 1 | LFO 2. Then a CV
// mixer row (Mixer 1 | Mixer 2) and a bottom row: the wide IFS Osc modulation matrix + a narrow
// Amp box in the lower right (VCA-envelope source over the master gain). Pad panels put a shrunk
// fractal pad on the left + a single row of knobs/dropdowns on the right; the pads edit
// fractal-shape params directly, scalars are knobs, enums are dropdowns. The UI is the callback
// for all of them, routing by param index.

#include "DistrhoUI.hpp"

#include "src/DistrhoUIInternal.hpp" // DPF's UIExporter (DPF_ROOT_DIR/distrho is on the include path)

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Dropdown.hpp"
#include "EmbeddedResources.hpp" // build-time codegen from Resources/* (see dpf/utils/res2c.py)
#include "GeneratedPresets.h" // build-time codegen from Presets/*.json (see tools/gen_presets.py)
#include "GroupBox.hpp"
#include "HelpButton.hpp"
#include "IFSControl.hpp"
#include "IFSEnvControl.hpp"
#include "CPPTypes.hpp"         // DEFAULT_SAMPLE_RATE (the UIExporter's nominal rate below)
#include "DistrhoPluginInfo.h"
#include "IFSynthParams.h"
#include "Knob.hpp"
#include "Logo.hpp"
#include "PopupMenu.hpp"
#include "PresetDump.hpp" // dev-only JSON patch dumper (no-op unless IFSYNTH_PRESET_DUMP=1)
#include "PresetKeys.hpp"
#include "Slider.hpp"
#include "TabBar.hpp"
#include "TitleText.hpp"
#include "Toggle.hpp"
#include "plugin_gui.h"         // the C seam to src/plugin.cpp (the raw CLAP plugin)

#include <cstdlib>
#include <cstdio>

#include "extra/WebView.hpp" // native WebView overlay (WKWebView/WebKitGTK/WebView2), for Help.
                              // Must be included at global scope (see DPF's EmbedExternalUI
                              // example); WebView.hpp opens the DISTRHO namespace itself.

START_NAMESPACE_DISTRHO

using namespace ifsynth;
using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::Dropdown;
using DGL_NAMESPACE::GroupBox;
using DGL_NAMESPACE::HelpButton;
using DGL_NAMESPACE::IFSControl;
using DGL_NAMESPACE::IFSEnvControl;
using DGL_NAMESPACE::Knob;
using DGL_NAMESPACE::Logo;
using DGL_NAMESPACE::NanoSubWidget;
using DGL_NAMESPACE::PopupMenu;
using DGL_NAMESPACE::Slider;
using DGL_NAMESPACE::TitleText;
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
        setGeometryConstraints(784, 624, true, false); // matches the 980:780 design ratio

        // Per-module accent colours, following PM's category scheme (sound=green, filter=purple,
        // control signals=amber, combiner=orange, modulation=teal).
        fOscGroup  = makeGroup("IFS Oscillator", colSound());
        fSinGroup  = makeGroup("Osc 2", colSound());
        fLpfGroup  = makeGroup("Low-Pass Filter", colFilter());
        // Env 1/2 and LFO 1/2 share one rect and a four-tab bar that switches which is visible, so
        // each title is the module name (the tabs carry the number). The CV Mixers sit side by side.
        fEnv1Group = makeGroup("IFS Envelope", colEnv());
        fEnv2Group = makeGroup("IFS Envelope", colEnv());
        fLfo1Group = makeGroup("IFS LFO", colLfo());
        fLfo2Group = makeGroup("IFS LFO", colLfo());
        fMix1Group = makeGroup("CV Mixer 1", colComb());
        fMix2Group = makeGroup("CV Mixer 2", colComb());
        fOscModGroup = makeGroup("IFS Osc Modulation  (source + depth)", colNeutral());
        fOsc2ModGroup = makeGroup("Osc 2 Mod", colNeutral());
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
                d->setChoices(choicesForWidget(i));
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

        // Tab bar (created after the panels so it draws on top of the title strip). The two
        // envelopes and two LFOs share one rect and one four-tab bar; only Env 1 starts visible.
        fModTab = makeTab(kTabMod, colEnv(), {"Env 1", "Env 2", "LFO 1", "LFO 2"});
        fEnv2Group->hide();
        fLfo1Group->hide();
        fLfo2Group->hide();

#if IFSYNTH_PRESET_DUMP
        fDumpBtn = std::make_unique<DumpButton>(this, [this] {
            const std::string p = dumpPresetToFile(fMirror);
            std::fprintf(stderr, "[IFSynth] patch %s %s\n",
                         p.empty() ? "dump FAILED" : "dumped to", p.c_str());
        });
#endif
        // Logo: raw PNG bytes embedded at build time (see EmbeddedResources.hpp); decoded once
        // here via NanoVG. Sits left of the preset dropdown in the top strip.
        fLogo = std::make_unique<Logo>(this, EmbeddedResources::Unlikelyware_Final_ColorData,
                                        EmbeddedResources::Unlikelyware_Final_ColorDataSize);

        // Wordmark: "IFS" + "ynth" in two accent colors, Almendra (embedded, matches the
        // unlikelyware.com brand face). Sits between the logo and the Preset dropdown.
        // Colors sampled directly from the logo: "IFS" = rightmost (tan/gold) band,
        // "ynth" = middle (terracotta orange) band.
        fTitleText = std::make_unique<TitleText>(this, EmbeddedResources::Almendra_RegularData,
                                                  EmbeddedResources::Almendra_RegularDataSize,
                                                  Color(222, 184, 135), Color(200, 109, 84));

        // Preset selector: not tied to a GroupBox (it applies to the whole patch), parented to
        // the UI itself like fMenu/fDumpBtn. Shares the generic Dropdown::Callback +
        // PopupMenu::Callback plumbing every other dropdown uses; menuSelected() below tells it
        // apart from a real per-param dropdown via kPresetDropdownId.
        fPresetDropdown = std::make_unique<Dropdown>(this, this);
        fPresetDropdown->setId(kPresetDropdownId);
        fPresetDropdown->setLabel("Preset");
        {
            std::vector<std::string> names;
            for (int i = 0; i < kPresetCount; ++i) names.push_back(kPresets[i].name);
            fPresetDropdown->setChoices(names);
        }
        fPresetDropdown->setPlaceholder("--");
        fPresetDropdown->setIndex(-1); // no preset selected until the user picks one

        // Help: toggles a native WebView overlay showing the bundled help.html. Right-aligned in
        // the top strip (see the dev DUMP button's placement, further right when both exist).
        fHelpButton = std::make_unique<HelpButton>(this, [this] { toggleHelp(); });

        fMenu = std::make_unique<PopupMenu>(this, this); // created last → draws on top

        pushPadDefaults();

        // DPF is only the GUI layer here: UIExporter set uiData->dspPtr (our clap_plugin_t) before
        // calling createUI(), so the seam in plugin_gui.h is already usable. Replaying every value
        // through parameterChanged() syncs the knobs, dropdowns, toggles, the IFS pads and the dump
        // mirror, so an editor opened on a host-restored patch starts in sync instead of showing the
        // parameter defaults.
        fPlugin = static_cast<const clap_plugin_t*>(getPluginInstancePointer());
        if (fPlugin != nullptr) {
            for (uint32_t i = 0; i < kParamCount; ++i)
                parameterChanged(i, static_cast<float>(ifs_param_value(fPlugin, i)));
        }

        layout();
        updateGlideEnabled();
#if IFSYNTH_PRESET_DUMP
        for (uint32_t i = 0; i < kParamCount; ++i) fMirror[i] = kParams[i].def;
#endif
    }

    ~IFSynthUI() override
    {
        if (fHelpWebView != nullptr)
            webViewDestroy(fHelpWebView);
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
    // A click outside the plugin window never reaches us, so treat losing focus to the host (or
    // another app) as "clicked elsewhere" and close any open menu. Clicks inside the window are
    // handled by the menu itself.
    void uiFocus(bool focus, DGL_NAMESPACE::CrossingMode mode) override
    {
        UI::uiFocus(focus, mode);
        if (!focus && fMenu != nullptr)
            fMenu->dismiss();
    }

    void menuSelected(PopupMenu* menu, int index) override
    {
        const uint32_t id = menu->tag();
        if (id == kPresetDropdownId) { applyPreset(index); return; }
        editParameter(id, true);
        setParameterValue(id, static_cast<float>(index));
        editParameter(id, false);
        recordMirror(id, static_cast<float>(index));
        if (fDropdowns[id]) fDropdowns[id]->setIndex(index);
        routeToPad(id, static_cast<float>(index));
        if (id == kParamVoiceMode) updateGlideEnabled();
    }

    // Apply every param the selected preset has a value for (looked up by "module.item" key via
    // kPresetKeys — the same mapping PresetDump.hpp uses to write these files). Params the
    // preset doesn't mention are left at their current value. parameterChanged() already knows
    // how to sync every widget kind (knob/dropdown/toggle/slider) + the pads + glide-enable, so
    // it's reused here instead of duplicating that dispatch.
    void applyPreset(int index)
    {
        if (index < 0 || index >= kPresetCount)
            return;
        const PresetEntry& preset = kPresets[index];
        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            const std::string key = std::string(kPresetKeys[i].module) + "." + kPresetKeys[i].item;
            for (int j = 0; j < preset.count; ++j)
            {
                if (key == preset.values[j].key)
                {
                    const float v = preset.values[j].value;
                    editParameter(i, true);
                    setParameterValue(i, v);
                    editParameter(i, false);
                    parameterChanged(i, v);
                    break;
                }
            }
        }
        if (fPresetDropdown) fPresetDropdown->setIndex(index);
    }

    /* -- drawing / layout ------------------------------------------------- */
    void onNanoDisplay() override
    {
        beginPath(); rect(0, 0, getWidth(), getHeight()); fillColor(Color(14, 28, 28)); fill();
    }
    void onResize(const ResizeEvent& ev) override
    {
        UI::onResize(ev);
        layout();
        if (fHelpWebView != nullptr)
            resizeHelpWebView();
    }

    // Two jobs on the idle tick:
    //  1. Some Linux hosts (notably REAPER via pugl) don't apply the UI's initial size until the
    //     event loop starts pumping, so the window loads mis-sized (top gap, bottom row clipped)
    //     until a manual resize. Re-assert the default size once on the first tick to force the
    //     host to apply correct geometry, which drives onResize() -> layout().
    //  2. WebViews need regular pumping to process their own event loop / message callbacks.
    void uiIdle() override
    {
#ifndef __APPLE__
        // Not on macOS: setSize() takes pixels, so on a Retina screen this would shrink the editor
        // to half its points inside the host's container. There the GUI shell's applySize() owns
        // the size, from the window's live backing scale.
        if (!fInitialSizeNudged)
        {
            fInitialSizeNudged = true;
            setSize(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT);
        }
#endif
        if (fHelpWebView != nullptr)
            webViewIdle(fHelpWebView);
    }

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
    std::unique_ptr<GroupBox>      fOscGroup, fSinGroup, fLpfGroup, fEnv1Group, fEnv2Group, fLfo1Group, fLfo2Group;
    std::unique_ptr<GroupBox>      fMix1Group, fMix2Group, fOscModGroup, fOsc2ModGroup, fLpfModGroup, fAmpGroup;
    std::unique_ptr<IFSControl>    fOscPad, fLfo1Pad, fLfo2Pad;
    std::unique_ptr<IFSEnvControl> fEnv1Pad, fEnv2Pad;
    std::unique_ptr<Knob>          fKnobs[kParamCount];
    std::unique_ptr<Dropdown>      fDropdowns[kParamCount];
    std::unique_ptr<Toggle>        fToggles[kParamCount];
    std::unique_ptr<Slider>        fSliders[kParamCount];
    std::unique_ptr<TabBar>        fModTab;
    std::unique_ptr<Logo>          fLogo;
    std::unique_ptr<TitleText>     fTitleText;
    std::unique_ptr<Dropdown>      fPresetDropdown;
    const clap_plugin_t*           fPlugin = nullptr;  // the raw CLAP plugin (see plugin_gui.h)
    std::unique_ptr<HelpButton>    fHelpButton;
    WebViewHandle                  fHelpWebView = nullptr;
    int                             fTopPad = 0; // set each layout() pass; read by the help-panel geometry below
    std::unique_ptr<PopupMenu>     fMenu;
    // Sentinel dropdown id, distinct from every real ParamIndex (which are all < kParamCount),
    // so dropdownClicked()/menuSelected() can tell the preset selector apart from a per-param
    // dropdown sharing the same generic PopupMenu.
    static constexpr uint32_t kPresetDropdownId = kParamCount;
#if IFSYNTH_PRESET_DUMP
    std::unique_ptr<DumpButton>    fDumpBtn;
#endif
    float                          fUiScale = 1.0f; // set each layout() pass; read by the dropH helpers below
    bool                           fInitialSizeNudged = false; // one-shot size re-assert on first uiIdle (see uiIdle)

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
    static bool isToggleParam(uint32_t i)
    {
        switch (i)
        {
        case kParamMix1EnableLFO: case kParamMix1EnableEnv: case kParamMix1EnableCC:
        case kParamMix2EnableLFO: case kParamMix2EnableEnv: case kParamMix2EnableCC:
        case kParamEnvScaleRelease: case kParamEnv2ScaleRelease:
            return true;
        default: return false;
        }
    }
    static bool isSliderParam(uint32_t /*i*/) // mod depths are now knobs; no sliders remain
    {
        return false;
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
        if (i == kParamVoiceMode || i == kParamMpe || i == kParamVelocity || i == kParamSpread
            || i == kParamSinLevel
            || i == kParamIfsLevel || i == kParamSlewRate)
            return colComb();
        if (i == kParamSinPitch || i == kParamSinFine || i == kParamSinDetune
            || i == kParamSinWaveform || i == kParamIfsSyncMode)
            return colSound();
        if (i == kParamLpfCutoff || i == kParamLpfResonance || i == kParamLpfKeyTrack)
            return colFilter();
        if (i == kParamLpfCutoffModSrc || i == kParamLpfCutoffModDepth) return colNeutral();
        if (i == kParamOsc2PitchModSrc || i == kParamOsc2PitchModDepth) return colNeutral();
        if (i == kParamEnvLegatoMode || i == kParamEnvScaleRelease
            || i == kParamEnv2LegatoMode || i == kParamEnv2ScaleRelease) return colEnv();
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
        if (i == kParamVoiceMode || i == kParamMpe || i == kParamVelocity || i == kParamSpread
            || i == kParamSinLevel
            || i == kParamIfsLevel || i == kParamSlewRate)   // Glide sits under Voices' column
            return fAmpGroup.get();
        if (i == kParamSinPitch || i == kParamSinFine || i == kParamSinDetune
            || i == kParamSinWaveform)
            return fSinGroup.get();
        if (i == kParamIfsSyncMode) return fOscGroup.get();
        if (i == kParamLpfCutoff || i == kParamLpfResonance || i == kParamLpfKeyTrack)
            return fLpfGroup.get();
        if (i == kParamLpfCutoffModSrc || i == kParamLpfCutoffModDepth)
            return fLpfModGroup.get();
        if (i == kParamOsc2PitchModSrc || i == kParamOsc2PitchModDepth)
            return fOsc2ModGroup.get();
        if (i == kParamEnvLegatoMode || i == kParamEnvScaleRelease)   return fEnv1Group.get();
        if (i == kParamEnv2LegatoMode || i == kParamEnv2ScaleRelease) return fEnv2Group.get();
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

    // The label lists live in IFSynthParams.h so the CLAP params extension reports exactly the same
    // enums; this just adapts them to the widget's std::vector<std::string> shape.
    static std::vector<std::string> choicesForWidget(uint32_t i)
    {
        std::vector<std::string> v;
        for (const char* const* c = ifsynth::choicesFor(i); c != nullptr && *c != nullptr; ++c)
            v.emplace_back(*c);
        return v;
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

    // Help panel: a native WebView overlay covering everything below the top strip, so the
    // always-visible Help button up there can toggle it closed again.
    void helpPanelGeometry(int& x, int& y, int& w, int& h) const
    {
        const int m = 8;
        x = m;
        y = fTopPad + m;
        w = static_cast<int>(getWidth()) - 2 * m;
        h = static_cast<int>(getHeight()) - fTopPad - 2 * m;
    }

    void resizeHelpWebView()
    {
        int x, y, w, h;
        helpPanelGeometry(x, y, w, h);
        webViewResize(fHelpWebView, static_cast<uint>(w), static_cast<uint>(h), fUiScale);
    }

    static std::string helpFilePath()
    {
        const char* tmp = std::getenv("TMPDIR");
        std::string dir = (tmp && *tmp) ? tmp : "/tmp";
        if (!dir.empty() && dir.back() != '/') dir += '/';
        return dir + "ifsynth-help.html";
    }

    // The WebView needs a real file:// URL, not an in-memory buffer, so the embedded HTML bytes
    // get written out here. Overwritten every time the panel opens, so it always matches
    // whatever's actually embedded in this build.
    static std::string writeHelpFile()
    {
        const std::string path = helpFilePath();
        FILE* const f = std::fopen(path.c_str(), "wb");
        if (f == nullptr)
            return "";
        std::fwrite(EmbeddedResources::helpData, 1, EmbeddedResources::helpDataSize, f);
        std::fclose(f);
        return path;
    }

    void toggleHelp()
    {
        if (fHelpWebView != nullptr)
        {
            webViewDestroy(fHelpWebView);
            fHelpWebView = nullptr;
            fHelpButton->setOpen(false);
            return;
        }

        const std::string path = writeHelpFile();
        if (path.empty())
            return;
        const std::string url = "file://" + path;

        int x, y, w, h;
        helpPanelGeometry(x, y, w, h);

        WebViewOptions options;
        options.offset.x = x;
        options.offset.y = y;
        fHelpWebView = webViewCreate(url.c_str(), getWindow().getNativeWindowHandle(),
                                      static_cast<uint>(w), static_cast<uint>(h), fUiScale, options);
        fHelpButton->setOpen(fHelpWebView != nullptr);
    }

    std::unique_ptr<GroupBox> makeGroup(const char* title, Color accent)
    {
        auto g = std::make_unique<GroupBox>(this);
        g->setTitle(title);
        g->setAccentColor(accent);
        return g;
    }

    enum TabId { kTabMod = 0 };

    std::unique_ptr<TabBar> makeTab(uint32_t id, Color accent, const std::vector<std::string>& tabs)
    {
        auto t = std::make_unique<TabBar>(this, this);
        t->setId(id);
        t->setTabs(tabs);
        t->setAccentColor(accent);
        t->setActive(0);
        return t;
    }

    // TabBar::Callback — show the picked panel, hide the rest. Hiding a GroupBox hides its whole
    // child subtree (pad + knobs + dropdowns), so only the active tab is drawn/interactive. The
    // modulator bar takes the accent of the panel it shows (yellow envelopes, pink LFOs).
    void tabSelected(TabBar* bar, int index) override
    {
        switch (bar->getId())
        {
        case kTabMod:
        {
            GroupBox* panels[4] = { fEnv1Group.get(), fEnv2Group.get(), fLfo1Group.get(), fLfo2Group.get() };
            for (int i = 0; i < 4; ++i)
                i == index ? panels[i]->show() : panels[i]->hide();
            bar->setAccentColor(index < 2 ? colEnv() : colLfo());
            break;
        }
        default: break;
        }
        repaint();
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
        // Row heights are derived from the ACTUAL canvas height so the stack fills the window exactly on any
        // host. This self-corrects for the Retina-vs-logical pixel difference: macOS hands us a
        // physical-pixel canvas (~1800 tall → rowH ~350), Linux a logical one (900 → rowH ~170),
        // and the shared `scale` below sizes knobs/text by the same factor. A previously
        // hard-coded 349px row assumed the macOS height and overflowed the default window on
        // hosts that report logical pixels (Linux), pushing the bottom rows off-screen. The
        // aspect ratio is locked (see the constructor), so rows never stretch disproportionately.
        // Always-on top strip: logo, Preset dropdown, and (dev builds) the DUMP button. Sized to
        // fit the logo's ~64 design-px height (the tallest thing in the strip) plus a little
        // breathing room above/below.
        const int logoH  = static_cast<int>(48 * scale);
        const int topPad = logoH + static_cast<int>(6 * scale);
        fTopPad = topPad;
        // Four rows: IFS Osc | Osc 2, the Env/LFO tab panel | LPF, CV Mixer | Amp, and the
        // OscMod | Osc2Mod | LpfMod strip. Derived from the canvas height so it fills any size
        // (Retina-safe); the aspect ratio is locked in the constructor. Row 1 is the tallest so the
        // envelope and LFO pads can grow; row 2 fits the CV Mixer's checkbox column and the Amp's
        // dropdowns over knobs; the bottom strip takes the rest (a dropdown over a knob per cell).
        const int totalH = H - 2 * m - 3 * gap - topPad;
        // Rows 0 and 1 are the same height so the oscillator and Env/LFO pads can be the same size.
        const int row0H  = totalH * 176 / 686;
        const int rowH   = row0H;                                        // row 1 (Env/LFO | LPF)
        const int row2H  = totalH * 167 / 686;
        const int rowHb  = totalH - row0H - rowH - row2H;                // bottom row ≈ 167/686
        const int colL = m;
        const int top0 = m + topPad;
        const int row0 = top0, row1 = top0 + row0H + gap;
        const int row2 = row1 + rowH + gap;
        const int modY = row2 + row2H + gap;

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
        GroupBox* allGroups[] = { fOscGroup.get(), fSinGroup.get(), fLpfGroup.get(),
                                   fEnv1Group.get(), fEnv2Group.get(), fLfo1Group.get(), fLfo2Group.get(),
                                   fMix1Group.get(), fMix2Group.get(),
                                   fOscModGroup.get(), fOsc2ModGroup.get(), fLpfModGroup.get(), fAmpGroup.get() };
        for (GroupBox* g : allGroups)
            g->setUiScale(txtScale);
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (fDropdowns[i]) fDropdowns[i]->setUiScale(txtScale);
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (fSliders[i]) fSliders[i]->setUiScale(txtScale);
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (fToggles[i]) fToggles[i]->setUiScale(txtScale);
        fMenu->setUiScale(txtScale);
        fPresetDropdown->setUiScale(txtScale);

        // Logo, then the "IFSynth" wordmark, then the Preset dropdown — all left-aligned in the
        // top strip (the dev DUMP button, if present, takes the right side — see below). The
        // logo's width comes from its own aspect ratio (never stretched), so the wordmark's x
        // position is read back from it rather than assumed. The wordmark itself gets a fixed
        // reserved slot (NanoVG text measurement isn't safe to call outside onNanoDisplay, so
        // this can't be measured-and-fitted the same way); tune titleW if it over/under-fills.
        fLogo->setHeightPreservingAspect(static_cast<float>(logoH));
        fLogo->setAbsolutePos(m, m + (topPad - logoH) / 2);

        const int titleX = m + static_cast<int>(fLogo->getWidth()) + gap;
        const int titleW = static_cast<int>(260 * scale);
        fTitleText->setFontSize(48.0f * scale);
        fTitleText->setSize(static_cast<uint>(titleW), static_cast<uint>(topPad));
        fTitleText->setAbsolutePos(titleX, m);

        {
            const int pw = static_cast<int>(220 * scale);
            const int px = titleX + titleW + gap;
            fPresetDropdown->setSize(static_cast<uint>(pw), static_cast<uint>(topPad));
            fPresetDropdown->setAbsolutePos(px, m / 2);
        }

        // Row 0: IFS Oscillator (fills) | Osc 2.
        const int sinW = static_cast<int>(300 * fUiScale);
        const int sinX = W - m - sinW;
        placeGroup(fOscGroup.get(), colL, row0, sinX - gap - colL, row0H);
        placeGroup(fSinGroup.get(), sinX, row0, sinW,              row0H);

        // Row 1: the Env 1/2 + LFO 1/2 panels overlay one rect (the tab bar switches which is
        // visible) beside a compact LPF.
        const int lpfW = static_cast<int>(270 * fUiScale);
        const int modW = W - 2 * m - gap - lpfW;
        const int lpfX = colL + modW + gap;
        placeGroup(fEnv1Group.get(), colL, row1, modW, rowH);
        placeGroup(fEnv2Group.get(), colL, row1, modW, rowH);
        placeGroup(fLfo1Group.get(), colL, row1, modW, rowH);
        placeGroup(fLfo2Group.get(), colL, row1, modW, rowH);
        placeGroup(fLpfGroup.get(),  lpfX, row1, lpfW, rowH);

        // Row 2: CV Mixer 1 | CV Mixer 2 | Amp (four dropdowns over four knobs, Glide included).
        const int ampW  = static_cast<int>(520 * fUiScale);
        const int ampX  = W - m - ampW;
        const int mixW  = (ampX - colL - 2 * gap) / 2;
        const int mix2X = colL + mixW + gap;
        placeGroup(fMix1Group.get(), colL,  row2, mixW, row2H);
        placeGroup(fMix2Group.get(), mix2X, row2, mixW, row2H);
        placeGroup(fAmpGroup.get(),  ampX, row2, ampW, row2H);

        // Row 3: IFS Osc modulation (wide, fills) + Osc 2 Mod + LPF modulation.
        const int lpfModW  = static_cast<int>(140 * fUiScale);
        const int osc2ModW = static_cast<int>(140 * fUiScale);
        const int lpfModX  = W - m - lpfModW;
        const int osc2ModX = lpfModX - gap - osc2ModW;
        placeGroup(fOscModGroup.get(),  colL,     modY, osc2ModX - gap - colL, rowHb);
        placeGroup(fOsc2ModGroup.get(), osc2ModX, modY, osc2ModW, rowHb);
        placeGroup(fLpfModGroup.get(),  lpfModX,  modY, lpfModW,  rowHb);

        // The Env/LFO tab bar, in its panel's title strip (top-right).
        placeTab(fModTab.get(), colL, row1, modW, 4, 46, scale);

        // One pad size for the oscillator and the Env/LFO panels: 2:1, as large as the shorter
        // content area and the narrower 42% column allow, less a little room for the well's rim.
        {
            int x, y, w0, h0, w1, h1;
            fOscGroup->contentBounds(x, y, w0, h0);
            fEnv1Group->contentBounds(x, y, w1, h1);
            const int rim = static_cast<int>(3 * scale);
            fPadH = std::min(std::min(h0, h1) - 2 * rim,
                             static_cast<int>(std::min(w0, w1) * kPadFrac * 0.5f));
            fPadW = 2 * fPadH;
        }

        layoutOscPanel();

        // Osc 2: Wave + Pitch dropdowns stacked in a left column; Fine, Detune knobs to the right.
        layoutOsc2Panel(fSinGroup.get(), fDropdowns[kParamSinWaveform].get(), fDropdowns[kParamSinPitch].get(),
                        fKnobs[kParamSinFine].get(), fKnobs[kParamSinDetune].get());

        // Env: Duration + Smoothing knobs, then a 2x2 grid of Hold / Legato over Iterations /
        // Scale Release. The LFOs share the rect and the pad size, so the pad stays put.
        layoutEnvPanel(fEnv1Group.get(), fEnv1Pad.get(), kParamEnvDuration, kParamEnvSmoothing,
                       kParamEnvHoldMode, kParamEnvLegatoMode, kParamEnvIterations, kParamEnvScaleRelease);
        layoutEnvPanel(fEnv2Group.get(), fEnv2Pad.get(), kParamEnv2Duration, kParamEnv2Smoothing,
                       kParamEnv2HoldMode, kParamEnv2LegatoMode, kParamEnv2Iterations, kParamEnv2ScaleRelease);

        // LFO: the pad, then Iterations above the Rate + Smoothing knobs.
        layoutLfoPanel(fLfo1Group.get(), fLfo1Pad.get(), kParamLfoRate, kParamLfoSmoothing, kParamLfoIterations);
        layoutLfoPanel(fLfo2Group.get(), fLfo2Pad.get(), kParamLfo2Rate, kParamLfo2Smoothing, kParamLfo2Iterations);

        // Low-pass filter: cutoff, resonance, key-track knobs across the panel.
        SubWidget* lpf[3] = { fKnobs[kParamLpfCutoff].get(), fKnobs[kParamLpfResonance].get(),
                              fKnobs[kParamLpfKeyTrack].get() };
        layoutRow(fLpfGroup.get(), lpf, 3);

        // CV Mixer: LFO/Env/CC checkboxes stacked in a column on the left; Mode dropdown over the
        // CC# knob on the right.
        SubWidget* mix1T[3] = { fToggles[kParamMix1EnableLFO].get(), fToggles[kParamMix1EnableEnv].get(),
                                fToggles[kParamMix1EnableCC].get() };
        layoutMixPanel(fMix1Group.get(), mix1T, fKnobs[kParamMix1CCNumber].get(),
                       fDropdowns[kParamMix1Mode].get());
        SubWidget* mix2T[3] = { fToggles[kParamMix2EnableLFO].get(), fToggles[kParamMix2EnableEnv].get(),
                                fToggles[kParamMix2EnableCC].get() };
        layoutMixPanel(fMix2Group.get(), mix2T, fKnobs[kParamMix2CCNumber].get(),
                       fDropdowns[kParamMix2Mode].get());

        layoutModPanel();

        // Osc 2 pitch mod + LPF cutoff mod: identical single-cell panels (source dropdown + depth knob).
        layoutModCell(fOsc2ModGroup.get(), fDropdowns[kParamOsc2PitchModSrc].get(),
                      fKnobs[kParamOsc2PitchModDepth].get());
        layoutLpfModPanel();

        layoutAmp();

        // Help button: far right of the top strip (the dev DUMP button, if present, sits just
        // left of it instead of also claiming the far-right corner).
        const int helpW = static_cast<int>(34 * scale), helpH = static_cast<int>(28 * scale);
        fHelpButton->setSize(static_cast<uint>(helpW), static_cast<uint>(helpH));
        fHelpButton->setAbsolutePos(W - helpW - m, (topPad - helpH) / 2 + m / 2);

#if IFSYNTH_PRESET_DUMP
        if (fDumpBtn)
        {
            const int bw = static_cast<int>(58 * scale), bh = static_cast<int>(18 * scale);
            fDumpBtn->setSize(static_cast<uint>(bw), static_cast<uint>(bh));
            fDumpBtn->setAbsolutePos(W - helpW - gap - bw - m, (topPad - bh) / 2 + m / 2);
        }
#endif
    }

    // Largest share of a panel's content width a pad may take (see the shared size in layout()).
    static constexpr float kPadFrac = 0.42f;
    int fPadW = 0, fPadH = 0;   // shared by every IFS pad (osc, envelopes, LFOs), set in layout()

    // Pad on the left (the shared fPadW x fPadH), framed as a well; returns the content area to its
    // right.
    void placePad(GroupBox* group, NanoSubWidget* pad, int& kx, int& cy, int& kw, int& ch)
    {
        int cx, cw;
        group->contentBounds(cx, cy, cw, ch);
        const int gap = 8;
        const int inset = static_cast<int>(3 * fUiScale);   // keeps the rim off the box edge
        const int px = cx + inset, py = cy + (ch - fPadH) / 2;
        pad->setSize(static_cast<uint>(fPadW), static_cast<uint>(fPadH));
        pad->setAbsolutePos(px, py);
        group->setWell(px, py, fPadW, fPadH);
        kx = px + fPadW + gap;
        kw = cx + cw - kx;
    }

    // IFS Osc panel: pad, then the four knobs in a row with the Iterations and Sync dropdowns
    // under them (each dropdown centred under a pair of knobs).
    void layoutOscPanel()
    {
        int kx, cy, kw, ch;
        placePad(fOscGroup.get(), fOscPad.get(), kx, cy, kw, ch);
        const int dropH = static_cast<int>(46 * fUiScale);
        const int knobH = ch - dropH;
        const int cellW = kw / 4;
        Knob* knobs[4] = { fKnobs[kParamOscPitch].get(), fKnobs[kParamOscFine].get(),
                           fKnobs[kParamOscDetune].get(), fKnobs[kParamOscSmoothing].get() };
        for (int i = 0; i < 4; ++i)
        {
            knobs[i]->setSize(static_cast<uint>(cellW), static_cast<uint>(knobH));
            knobs[i]->setAbsolutePos(kx + i * cellW, cy);
        }
        Dropdown* drops[2] = { fDropdowns[kParamOscIterations].get(), fDropdowns[kParamIfsSyncMode].get() };
        const int dropW = std::min(2 * cellW, static_cast<int>(130 * fUiScale));
        for (int i = 0; i < 2; ++i)
        {
            drops[i]->setSize(static_cast<uint>(dropW), static_cast<uint>(dropH));
            drops[i]->setAbsolutePos(kx + i * 2 * cellW + (2 * cellW - dropW) / 2, cy + knobH);
        }
    }

    // IFS Env panel: pad, two full-height knobs, then a 2x2 grid (top: Hold, Legato; bottom:
    // Iterations, Scale Release).
    void layoutEnvPanel(GroupBox* group, NanoSubWidget* pad, uint32_t durK, uint32_t smoothK,
                        uint32_t holdD, uint32_t legatoD, uint32_t itersD, uint32_t scaleRelT)
    {
        int kx, cy, kw, ch;
        placePad(group, pad, kx, cy, kw, ch);
        const int knobW = kw * 18 / 100;              // each knob; the grid gets the remaining ~64%
        Knob* knobs[2] = { fKnobs[durK].get(), fKnobs[smoothK].get() };
        for (int i = 0; i < 2; ++i)
        {
            knobs[i]->setSize(static_cast<uint>(knobW), static_cast<uint>(ch));
            knobs[i]->setAbsolutePos(kx + i * knobW, cy);
        }
        const int gx = kx + 2 * knobW, gw = kw - 2 * knobW;
        const int cellW = gw / 2, cellH = ch / 2;
        SubWidget* grid[4] = { fDropdowns[holdD].get(), fDropdowns[legatoD].get(),
                               fDropdowns[itersD].get(), fToggles[scaleRelT].get() };
        for (int i = 0; i < 4; ++i)
        {
            grid[i]->setSize(static_cast<uint>(cellW), static_cast<uint>(i < 2 ? cellH : ch - cellH));
            grid[i]->setAbsolutePos(gx + (i % 2) * cellW, cy + (i / 2) * cellH);
        }
    }

    // A single modulation cell: a source dropdown over a depth knob, filling the group.
    void layoutModCell(GroupBox* group, Dropdown* src, Knob* depth)
    {
        int cx, cy, cw, ch;
        group->contentBounds(cx, cy, cw, ch);
        const int dropH = static_cast<int>(46 * fUiScale);
        src->setSize(static_cast<uint>(cw), static_cast<uint>(dropH));
        src->setAbsolutePos(cx, cy);
        depth->setSize(static_cast<uint>(cw), static_cast<uint>(ch - dropH - 4));
        depth->setAbsolutePos(cx, cy + dropH + 4);
    }

    // LPF cutoff modulation: one cell — a source dropdown over a depth knob.
    void layoutLpfModPanel()
    {
        layoutModCell(fLpfModGroup.get(), fDropdowns[kParamLpfCutoffModSrc].get(),
                      fKnobs[kParamLpfCutoffModDepth].get());
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
            Knob* k = fKnobs[cells[i].depth].get();
            k->setSize(static_cast<uint>(cellW), static_cast<uint>(ch - dropH - 4));
            k->setAbsolutePos(x, cy + dropH + 4);
        }
    }

    // Amp box: VCA-envelope source, Voices, MPE and Velocity dropdowns side by side on top, then the
    // Osc 2 / IFS mix levels, Spread and Glide knobs across the bottom, one under each dropdown.
    void layoutAmp()
    {
        int cx, cy, cw, ch;
        fAmpGroup->contentBounds(cx, cy, cw, ch);
        const int dropH = static_cast<int>(46 * fUiScale);
        Dropdown* top[4] = { fDropdowns[kParamVCASource].get(), fDropdowns[kParamVoiceMode].get(),
                             fDropdowns[kParamMpe].get(), fDropdowns[kParamVelocity].get() };
        for (int i = 0; i < 4; ++i) {
            const int x0 = cx + cw * i / 4, x1 = cx + cw * (i + 1) / 4;
            top[i]->setSize(static_cast<uint>(x1 - x0), static_cast<uint>(dropH));
            top[i]->setAbsolutePos(x0, cy);
        }

        const int ky = cy + dropH + 6;
        const int kh = ch - dropH - 6;
        Knob* levels[4] = { fKnobs[kParamSinLevel].get(), fKnobs[kParamIfsLevel].get(),
                            fKnobs[kParamSpread].get(), fKnobs[kParamSlewRate].get() };
        for (int i = 0; i < 4; ++i)
        {
            const int x0 = cx + cw * i / 4, x1 = cx + cw * (i + 1) / 4;
            levels[i]->setSize(static_cast<uint>(x1 - x0), static_cast<uint>(kh));
            levels[i]->setAbsolutePos(x0, ky);
            levels[i]->setDiameter(0.0f, fUiScale);   // 0 = fill the cell (larger than the shared size)
        }
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

    // CV Mixer panel: a column of three checkboxes on the left, Mode dropdown over the CC# knob
    // on the right.
    void layoutMixPanel(GroupBox* group, SubWidget* toggles[3], SubWidget* ccKnob, SubWidget* mode)
    {
        int cx, cy, cw, ch;
        group->contentBounds(cx, cy, cw, ch);
        const int leftW  = cw * 42 / 100;   // checkbox column
        const int rightW = cw - leftW;
        const int rightX = cx + leftW;

        const int tcH = ch / 3;
        for (int i = 0; i < 3; ++i)
        {
            toggles[i]->setSize(static_cast<uint>(leftW), static_cast<uint>(tcH));
            toggles[i]->setAbsolutePos(cx, cy + i * tcH);
        }

        const int topH = ch * 45 / 100;     // Mode dropdown band
        const int botH = ch - topH;         // CC# knob band
        mode->setSize(static_cast<uint>(rightW), static_cast<uint>(topH));
        mode->setAbsolutePos(rightX, cy);
        ccKnob->setSize(static_cast<uint>(rightW), static_cast<uint>(botH));
        ccKnob->setAbsolutePos(rightX, cy + topH);
    }

    // Osc 2 panel: the two dropdowns (Wave, Pitch) stacked in a left column; two knobs to the right.
    void layoutOsc2Panel(GroupBox* group, SubWidget* dd0, SubWidget* dd1, SubWidget* knob0, SubWidget* knob1)
    {
        int cx, cy, cw, ch;
        group->contentBounds(cx, cy, cw, ch);
        const int leftW  = cw * 40 / 100;   // dropdown column
        const int rightW = cw - leftW;
        const int rightX = cx + leftW;

        const int dh = ch / 2;
        dd0->setSize(static_cast<uint>(leftW), static_cast<uint>(dh));
        dd0->setAbsolutePos(cx, cy);
        dd1->setSize(static_cast<uint>(leftW), static_cast<uint>(ch - dh));
        dd1->setAbsolutePos(cx, cy + dh);

        const int kw = rightW / 2;
        knob0->setSize(static_cast<uint>(kw), static_cast<uint>(ch));
        knob0->setAbsolutePos(rightX, cy);
        knob1->setSize(static_cast<uint>(rightW - kw), static_cast<uint>(ch));
        knob1->setAbsolutePos(rightX + kw, cy);
    }

    void placeGroup(GroupBox* g, int x, int y, int w, int h)
    {
        g->setAbsolutePos(x, y);
        g->setSize(static_cast<uint>(w), static_cast<uint>(h));
    }

    // Place a tab group's TabBar in the top-right of its panel title strip.
    void placeTab(TabBar* t, int gx, int gy, int gw, int nTabs, int tabEachW, float scale)
    {
        const int tabH  = static_cast<int>(20 * scale);
        const int tabW  = static_cast<int>(nTabs * tabEachW * scale);
        const int inset = static_cast<int>(8 * scale);
        t->setSize(static_cast<uint>(tabW), static_cast<uint>(tabH));
        t->setAbsolutePos(gx + gw - tabW - inset, gy + static_cast<int>(4 * scale));
    }

    // LFO panel: laid out on the Env panel's grid (same rect, one tab bar), so switching tabs
    // doesn't move the shared controls: Rate and Smoothing where the Env's Duration and Smoothing
    // knobs are, Iterations in the Env's Iterations cell. The Hold/Legato/Scale Rel cells stay empty.
    void layoutLfoPanel(GroupBox* group, NanoSubWidget* pad, uint32_t rateK, uint32_t smoothK,
                        uint32_t itersD)
    {
        int kx, cy, kw, ch;
        placePad(group, pad, kx, cy, kw, ch);
        const int knobW = kw * 18 / 100;              // matches layoutEnvPanel
        Knob* knobs[2] = { fKnobs[rateK].get(), fKnobs[smoothK].get() };
        for (int i = 0; i < 2; ++i)
        {
            knobs[i]->setSize(static_cast<uint>(knobW), static_cast<uint>(ch));
            knobs[i]->setAbsolutePos(kx + i * knobW, cy);
        }
        const int gx = kx + 2 * knobW, cellW = (kw - 2 * knobW) / 2, cellH = ch / 2;
        fDropdowns[itersD]->setSize(static_cast<uint>(cellW), static_cast<uint>(ch - cellH));
        fDropdowns[itersD]->setAbsolutePos(gx, cy + cellH);
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

// ================================================================================================
// CLAP `gui` extension
// ================================================================================================
// Modelled on DPF's own ClapUI (DistrhoPluginCLAP.cpp) and PlainOldSynth's UI: gui_create() only
// allocates a holder, because UIExporter cannot be constructed until the host hands us a parent
// window in set_parent(). DPF is used purely as the GUI layer — the plugin itself is the raw CLAP
// plugin in src/plugin.cpp, so parameter traffic crosses the C seam in plugin_gui.h rather than
// DPF's plugin side.

namespace {

struct GuiState {
    const clap_plugin_t* plugin = nullptr;
    std::unique_ptr<UIExporter> exporter;
    uintptr_t parentWindow = 0;
    // The editor size in the HOST's units — logical points on macOS (CLAP cocoa), pixels on X11.
    // This, not the UIExporter's pixel size, is the source of truth: the pixel size is derived from
    // it with the live scale (applySize), so it can be re-derived when the window changes screens.
    uint32_t width  = DISTRHO_UI_DEFAULT_WIDTH;
    uint32_t height = DISTRHO_UI_DEFAULT_HEIGHT;
    double appliedScale = 0.0;   // the scale the UIExporter's pixel size was last derived with
    bool timerRegistered = false;
    clap_id timerId = 0;
};

#ifdef __APPLE__
extern "C" double ifs_gui_backing_scale(uintptr_t nsView);   // BackingScale_mac.mm
#endif

// Host units → UIExporter pixels. On macOS this is the backing scale of the window the editor is in
// RIGHT NOW, deliberately not UIExporter::getScaleFactor(): DPF caches that once, possibly from the
// wrong screen, and never updates it, while pugl maps mouse positions with the live value — see
// BackingScale_mac.mm. On X11, CLAP sizes are already physical pixels, so there is nothing to convert
// (matching DPF's own CLAP wrapper, which scales on macOS only).
double liveScale(const GuiState* gui)
{
#ifdef __APPLE__
    return ifs_gui_backing_scale(gui->parentWindow);
#else
    (void)gui;
    return 1.0;
#endif
}

// Size the UIExporter to gui->width x gui->height host units at the live scale. pugl re-reads the
// screen's scale while doing this and re-sends the pixel size, so it also re-syncs DPF after a move
// between screens even when the size in points did not change.
void applySize(GuiState* gui)
{
    if (!gui->exporter) return;
    const double scale = liveScale(gui);
    gui->appliedScale = scale;
    gui->exporter->setWindowSizeFromHost(uint(std::lround(gui->width  * scale)),
                                         uint(std::lround(gui->height * scale)));
}

#ifdef __APPLE__
constexpr const char* kWindowApi = CLAP_WINDOW_API_COCOA;
#else
constexpr const char* kWindowApi = CLAP_WINDOW_API_X11;
#endif

bool gui_is_api_supported(const clap_plugin_t*, const char* api, bool isFloating)
{
    return !isFloating && std::strcmp(api, kWindowApi) == 0;
}

bool gui_get_preferred_api(const clap_plugin_t*, const char** api, bool* isFloating)
{
    *api = kWindowApi;
    *isFloating = false;
    return true;
}

bool gui_create(const clap_plugin_t* plugin, const char* api, bool isFloating)
{
    if (!gui_is_api_supported(plugin, api, isFloating)) return false;
    auto* gui = new GuiState();
    gui->plugin = plugin;
    ifs_plugin_set_gui(plugin, gui);   // plugin.cpp owns the pointer and hands it back to us
    return true;
}

void gui_destroy(const clap_plugin_t* plugin)
{
    auto* gui = static_cast<GuiState*>(ifs_plugin_get_gui(plugin));
    ifs_plugin_set_gui(plugin, nullptr);
    if (gui == nullptr) return;

    if (gui->timerRegistered) {
        const clap_host_t* host = ifs_plugin_host(plugin);
        const auto* timer = static_cast<const clap_host_timer_support_t*>(
            host->get_extension(host, CLAP_EXT_TIMER_SUPPORT));
        if (timer != nullptr) timer->unregister_timer(host, gui->timerId);
    }
    delete gui;   // the UIExporter is destroyed here, which tears the UI down
}

// The UIExporter's edit/set callbacks. `ptr` is whatever was passed as UIExporter's first argument —
// we pass the clap_plugin_t, so these forward straight into the seam.
void onEditParam(void* ptr, uint32_t index, bool started)
{
    const auto* plugin = static_cast<const clap_plugin_t*>(ptr);
    if (started) ifs_param_begin_gesture(plugin, index);
    else         ifs_param_end_gesture(plugin, index);
}

void onSetParam(void* ptr, uint32_t index, float value)
{
    ifs_param_set_from_gui(static_cast<const clap_plugin_t*>(ptr), index, double(value));
}

bool gui_set_scale(const clap_plugin_t*, double) { return false; }   // logical size API (cocoa): no-op

bool gui_get_size(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height)
{
    auto* gui = static_cast<GuiState*>(ifs_plugin_get_gui(plugin));
    if (gui == nullptr) return false;
    // Host units, straight from the source of truth. Converting the UIExporter's pixel size back
    // with DPF's cached scale is what made the host allocate a container of the wrong size.
    *width  = gui->width;
    *height = gui->height;
    return true;
}

bool gui_can_resize(const clap_plugin_t*) { return true; }

bool gui_get_resize_hints(const clap_plugin_t*, clap_gui_resize_hints_t* hints)
{
    // The UI locks the 980:780 design ratio via setGeometryConstraints(), so tell the host the same.
    hints->can_resize_horizontally = true;
    hints->can_resize_vertically   = true;
    hints->preserve_aspect_ratio   = true;
    hints->aspect_ratio_width      = DISTRHO_UI_DEFAULT_WIDTH;
    hints->aspect_ratio_height     = DISTRHO_UI_DEFAULT_HEIGHT;
    return true;
}

bool gui_adjust_size(const clap_plugin_t*, uint32_t*, uint32_t*) { return true; }   // accept as-is

bool gui_set_size(const clap_plugin_t* plugin, uint32_t width, uint32_t height)
{
    auto* gui = static_cast<GuiState*>(ifs_plugin_get_gui(plugin));
    if (gui == nullptr) return false;
    gui->width  = width;
    gui->height = height;
    applySize(gui);
    return true;
}

bool gui_set_parent(const clap_plugin_t* plugin, const clap_window_t* window)
{
    auto* gui = static_cast<GuiState*>(ifs_plugin_get_gui(plugin));
    if (gui == nullptr || std::strcmp(window->api, kWindowApi) != 0) return false;

    // cocoa is a void*, x11 is already an integer handle — hence the different casts.
#ifdef __APPLE__
    gui->parentWindow = reinterpret_cast<uintptr_t>(window->cocoa);
#else
    gui->parentWindow = static_cast<uintptr_t>(window->x11);
#endif

    if (!gui->exporter) {
        gui->exporter.reset(new UIExporter(
            const_cast<clap_plugin_t*>(plugin),   // callbacksPtr → the edit/set callbacks above
            gui->parentWindow,                    // winId: the host's embed target
            DEFAULT_SAMPLE_RATE,                  // the UI does not need the real rate
            onEditParam, onSetParam,
            nullptr, nullptr, nullptr, nullptr,   // state / note / size / file callbacks: unused
            nullptr,                              // bundlePath
            const_cast<clap_plugin_t*>(plugin),   // dspPtr → UI::getPluginInstancePointer()
            0.0,                                  // scaleFactor: 0 = auto
            // kTypeClassic, not kTypeAuto: matches DPF's own CLAP wrapper for this embedded case.
            DGL_NAMESPACE::Application::kTypeClassic));
        // The UIExporter starts at DISTRHO_UI_DEFAULT_WIDTH x HEIGHT *pixels*; put it at the host-unit
        // size instead, at the scale of the window it is actually in.
        applySize(gui);
    }
    return true;
}

bool gui_set_transient(const clap_plugin_t*, const clap_window_t*) { return false; }   // embedded only

void gui_suggest_title(const clap_plugin_t*, const char*) {}

bool gui_show(const clap_plugin_t* plugin)
{
    auto* gui = static_cast<GuiState*>(ifs_plugin_get_gui(plugin));
    if (gui == nullptr || !gui->exporter) return false;

    if (!gui->timerRegistered) {
        const clap_host_t* host = ifs_plugin_host(plugin);
        const auto* timer = static_cast<const clap_host_timer_support_t*>(
            host->get_extension(host, CLAP_EXT_TIMER_SUPPORT));
        if (timer != nullptr && timer->register_timer(host, 16 /* ~60fps */, &gui->timerId))
            gui->timerRegistered = true;
    }
    return true;
}

bool gui_hide(const clap_plugin_t* plugin)
{
    auto* gui = static_cast<GuiState*>(ifs_plugin_get_gui(plugin));
    if (gui == nullptr || !gui->exporter) return false;

    if (gui->timerRegistered) {
        const clap_host_t* host = ifs_plugin_host(plugin);
        const auto* timer = static_cast<const clap_host_timer_support_t*>(
            host->get_extension(host, CLAP_EXT_TIMER_SUPPORT));
        if (timer != nullptr) timer->unregister_timer(host, gui->timerId);
        gui->timerRegistered = false;
    }
    return true;
}

const clap_plugin_gui_t kGui = {
    .is_api_supported  = gui_is_api_supported,
    .get_preferred_api = gui_get_preferred_api,
    .create            = gui_create,
    .destroy           = gui_destroy,
    .set_scale         = gui_set_scale,
    .get_size          = gui_get_size,
    .can_resize        = gui_can_resize,
    .get_resize_hints  = gui_get_resize_hints,
    .adjust_size       = gui_adjust_size,
    .set_size          = gui_set_size,
    .set_parent        = gui_set_parent,
    .set_transient     = gui_set_transient,
    .suggest_title     = gui_suggest_title,
    .show              = gui_show,
    .hide              = gui_hide,
};

} // namespace

extern "C" const clap_plugin_gui_t* ifs_gui_extension(void) { return &kGui; }

// Host automation → an open editor. Called on the main thread by plugin.cpp.
extern "C" void ifs_gui_push_param(void* guiPtr, uint32_t index, double value)
{
    auto* gui = static_cast<GuiState*>(guiPtr);
    if (gui != nullptr && gui->exporter)
        gui->exporter->parameterChanged(index, float(value));
}

// CLAP timer-support → pump the DPF/pugl event loop (see this file's header comment).
extern "C" void ifs_gui_on_timer(const clap_plugin_t* plugin, clap_id)
{
    auto* gui = static_cast<GuiState*>(ifs_plugin_get_gui(plugin));
    if (gui == nullptr || !gui->exporter) return;
    // The host window moved to a screen with a different backing scale (or the size was set before
    // it was in a window): re-derive the pixel size so drawing and mouse agree again.
    if (liveScale(gui) != gui->appliedScale) applySize(gui);
    gui->exporter->plugin_idle();
}

END_NAMESPACE_DISTRHO
