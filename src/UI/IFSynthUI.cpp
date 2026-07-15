// IFSynthUI.cpp — UI side. Two columns of module panels (wide layout), each panel a compact
// landscape tile: a shrunk fractal pad on the left + a single row of knobs/dropdowns on the
// right. Left column: Oscillator, Envelope 1, Envelope 2. Right column: LFO 1, LFO 2, Output.
// The pads edit fractal-shape params directly; scalars are knobs; enums are dropdowns. The UI
// is the callback for all of them, routing by param index. (CV mixers come later.)

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
using DGL_NAMESPACE::SubWidget;
using DGL_NAMESPACE::Toggle;

class IFSynthUI : public UI,
                  public Knob::Callback,
                  public IFSControl::Callback,
                  public Dropdown::Callback,
                  public PopupMenu::Callback,
                  public Toggle::Callback
{
public:
    IFSynthUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        setGeometryConstraints(780, 480, false, false);

        const Color accent(120, 190, 205);
        fOscGroup  = makeGroup("IFS Oscillator", accent);
        fEnv1Group = makeGroup("IFS Envelope 1", accent);
        fEnv2Group = makeGroup("IFS Envelope 2", accent);
        fLfo1Group = makeGroup("IFS LFO 1", accent);
        fLfo2Group = makeGroup("IFS LFO 2", accent);
        fMix1Group = makeGroup("CV Mixer 1  (LFO 1 + Env 1)", accent);
        fMix2Group = makeGroup("CV Mixer 2  (LFO 2 + Env 2)", accent);
        fOutGroup  = makeGroup("Output", accent);

        fOscPad = std::make_unique<IFSControl>(fOscGroup.get(), this);
        fOscPad->setParamIds(kParamOscBx, kParamOscBy, kParamOscStartEndY, kParamOscSquash1, kParamOscSquash2);

        fEnv1Pad = std::make_unique<IFSEnvControl>(fEnv1Group.get(), this);
        fEnv1Pad->setParamIds(kParamEnvBlueX, kParamEnvBlueY, kParamEnvGreenX, kParamEnvGreenY,
                              kParamEnvStartEndY, kParamEnvSquash1, kParamEnvSquash2, kParamEnvSquash3);
        fEnv2Pad = std::make_unique<IFSEnvControl>(fEnv2Group.get(), this);
        fEnv2Pad->setParamIds(kParamEnv2BlueX, kParamEnv2BlueY, kParamEnv2GreenX, kParamEnv2GreenY,
                              kParamEnv2StartEndY, kParamEnv2Squash1, kParamEnv2Squash2, kParamEnv2Squash3);

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
            const ParamSpec& s = kParams[i];
            auto knob = std::make_unique<Knob>(groupFor(i), this);
            knob->setId(i);
            knob->setLabel(s.name);
            knob->configure(s.min, s.max, s.logarithmic, s.def);
            knob->setRealValue(s.def, false);
            knob->setFormatter(formatterFor(i));
            fKnobs[i] = std::move(knob);
        }

        fMenu = std::make_unique<PopupMenu>(this, this); // created last → draws on top

        pushPadDefaults();
        layout();
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
        routeToPad(index, value);
    }

    /* -- toggle -> host --------------------------------------------------- */
    void toggleChanged(Toggle* t, bool on) override
    {
        const uint32_t id = t->getId();
        editParameter(id, true);
        setParameterValue(id, on ? 1.0f : 0.0f);
        editParameter(id, false);
    }

    /* -- knob -> host ----------------------------------------------------- */
    void knobDragStarted(Knob* k) override  { editParameter(k->getId(), true); }
    void knobDragFinished(Knob* k) override { editParameter(k->getId(), false); }
    void knobValueChanged(Knob* k, float v) override { setParameterValue(k->getId(), v); }

    /* -- pad -> host (all pads share IFSControl::Callback) ---------------- */
    void ifsDragStarted(uint32_t id) override  { editParameter(id, true); }
    void ifsDragFinished(uint32_t id) override { editParameter(id, false); }
    void ifsValueChanged(uint32_t id, float v) override { setParameterValue(id, v); }

    /* -- dropdown + menu -------------------------------------------------- */
    void dropdownClicked(Dropdown* d) override
    {
        int x, y, w, h;
        d->boxRect(x, y, w, h);
        fMenu->openFor(d->getId(), d->choices(), d->index(), x, y + h, w);
    }
    void menuSelected(PopupMenu* menu, int index) override
    {
        const uint32_t id = menu->tag();
        editParameter(id, true);
        setParameterValue(id, static_cast<float>(index));
        editParameter(id, false);
        if (fDropdowns[id]) fDropdowns[id]->setIndex(index);
        routeToPad(id, static_cast<float>(index));
    }

    /* -- drawing / layout ------------------------------------------------- */
    void onNanoDisplay() override
    {
        beginPath(); rect(0, 0, getWidth(), getHeight()); fillColor(24, 26, 31); fill();
    }
    void onResize(const ResizeEvent& ev) override { UI::onResize(ev); layout(); }

private:
    // Panels declared before their child controls so the children are destroyed first.
    std::unique_ptr<GroupBox>      fOscGroup, fEnv1Group, fEnv2Group, fLfo1Group, fLfo2Group;
    std::unique_ptr<GroupBox>      fMix1Group, fMix2Group, fOutGroup;
    std::unique_ptr<IFSControl>    fOscPad, fLfo1Pad, fLfo2Pad;
    std::unique_ptr<IFSEnvControl> fEnv1Pad, fEnv2Pad;
    std::unique_ptr<Knob>          fKnobs[kParamCount];
    std::unique_ptr<Dropdown>      fDropdowns[kParamCount];
    std::unique_ptr<Toggle>        fToggles[kParamCount];
    std::unique_ptr<PopupMenu>     fMenu;

    static bool isPadParam(uint32_t i)
    {
        switch (i)
        {
        case kParamOscBx: case kParamOscBy: case kParamOscStartEndY:
        case kParamOscSquash1: case kParamOscSquash2:
        case kParamEnvBlueX: case kParamEnvBlueY: case kParamEnvGreenX: case kParamEnvGreenY:
        case kParamEnvStartEndY: case kParamEnvSquash1: case kParamEnvSquash2: case kParamEnvSquash3:
        case kParamLfoBx: case kParamLfoBy: case kParamLfoStartEndY:
        case kParamLfoSquash1: case kParamLfoSquash2:
        case kParamEnv2BlueX: case kParamEnv2BlueY: case kParamEnv2GreenX: case kParamEnv2GreenY:
        case kParamEnv2StartEndY: case kParamEnv2Squash1: case kParamEnv2Squash2: case kParamEnv2Squash3:
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
            return true;
        default: return false;
        }
    }

    GroupBox* groupFor(uint32_t i) const
    {
        if (i == kParamGain)            return fOutGroup.get();
        if (i <= kParamOscIterations)   return fOscGroup.get();
        if (i <= kParamEnvHoldMode)     return fEnv1Group.get();
        if (i <= kParamLfoIterations)   return fLfo1Group.get();
        if (i <= kParamEnv2HoldMode)    return fEnv2Group.get();
        if (i <= kParamLfo2Iterations)  return fLfo2Group.get();
        if (i <= kParamMix1Mode)        return fMix1Group.get();
        return fMix2Group.get();
    }

    static std::vector<std::string> choicesFor(uint32_t i)
    {
        if (i == kParamOscIterations) return {"3", "4"};
        if (i == kParamEnvHoldMode || i == kParamEnv2HoldMode) return {"None", "Blue", "Green", "Loop"};
        if (i == kParamMix1Mode || i == kParamMix2Mode) return {"Add", "Multiply"};
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
        case kParamEnvStartEndY: fEnv1Pad->setStartEndY(v); break;
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
        case kParamEnv2StartEndY: fEnv2Pad->setStartEndY(v); break;
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

    std::unique_ptr<GroupBox> makeGroup(const char* title, Color accent)
    {
        auto g = std::make_unique<GroupBox>(this);
        g->setTitle(title);
        g->setAccentColor(accent);
        return g;
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
        // env order: Duration, StartEndY, BlueX, BlueY, GreenX, GreenY, Sq1, Sq2, Sq3, Smooth, Iter, Hold
        p->setStartEndY(kParams[dur + 1].def);
        p->setBlueX(kParams[dur + 2].def);  p->setBlueY(kParams[dur + 3].def);
        p->setGreenX(kParams[dur + 4].def); p->setGreenY(kParams[dur + 5].def);
        p->setSquash1(kParams[dur + 6].def); p->setSquash2(kParams[dur + 7].def);
        p->setSquash3(kParams[dur + 8].def); p->setSmoothing(kParams[dur + 9].def);
        p->setIterations(kParams[dur + 10].def);
    }

    void layout()
    {
        const int W = static_cast<int>(getWidth());
        const int H = static_cast<int>(getHeight());
        const int m = 8, gap = 8;
        const int mixerH = 128; // short pad-less mixer row at the bottom
        const int colW = (W - 2 * m - gap) / 2;
        const int rowH = (H - 2 * m - 3 * gap - mixerH) / 3;
        const int colL = m, colR = m + colW + gap;
        const int row0 = m, row1 = m + rowH + gap, row2 = m + 2 * (rowH + gap);
        const int mixY = m + 3 * (rowH + gap);

        placeGroup(fOscGroup.get(),  colL, row0, colW, rowH);
        placeGroup(fEnv1Group.get(), colL, row1, colW, rowH);
        placeGroup(fEnv2Group.get(), colL, row2, colW, rowH);
        placeGroup(fLfo1Group.get(), colR, row0, colW, rowH);
        placeGroup(fLfo2Group.get(), colR, row1, colW, rowH);
        placeGroup(fOutGroup.get(),  colR, row2, colW, rowH);
        placeGroup(fMix1Group.get(), colL, mixY, colW, mixerH);
        placeGroup(fMix2Group.get(), colR, mixY, colW, mixerH);

        SubWidget* oscG[4]  = { fKnobs[kParamOscPitch].get(), fKnobs[kParamOscFine].get(),
                                fKnobs[kParamOscSmoothing].get(), fDropdowns[kParamOscIterations].get() };
        layoutPadPanel(fOscGroup.get(), fOscPad.get(), oscG, 4);

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

        SubWidget* mix1[5] = { fToggles[kParamMix1EnableLFO].get(), fToggles[kParamMix1EnableEnv].get(),
                               fToggles[kParamMix1EnableCC].get(), fKnobs[kParamMix1CCNumber].get(),
                               fDropdowns[kParamMix1Mode].get() };
        layoutRow(fMix1Group.get(), mix1, 5);
        SubWidget* mix2[5] = { fToggles[kParamMix2EnableLFO].get(), fToggles[kParamMix2EnableEnv].get(),
                               fToggles[kParamMix2EnableCC].get(), fKnobs[kParamMix2CCNumber].get(),
                               fDropdowns[kParamMix2Mode].get() };
        layoutRow(fMix2Group.get(), mix2, 5);

        layoutOutput();
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

    // Landscape panel: shrunk pad on the left, a single row of n controls on the right.
    void layoutPadPanel(GroupBox* group, NanoSubWidget* pad, SubWidget* grid[], int n)
    {
        int cx, cy, cw, ch;
        group->contentBounds(cx, cy, cw, ch);
        const int gap = 8;
        const int padW = static_cast<int>(cw * 0.42);
        pad->setSize(static_cast<uint>(padW), static_cast<uint>(ch));
        pad->setAbsolutePos(cx, cy);

        const int kx = cx + padW + gap, kw = cw - padW - gap;
        const int cellW = n > 0 ? kw / n : kw;
        for (int i = 0; i < n; ++i)
        {
            grid[i]->setSize(static_cast<uint>(cellW), static_cast<uint>(ch));
            grid[i]->setAbsolutePos(kx + i * cellW, cy);
        }
    }

    void layoutOutput()
    {
        int cx, cy, cw, ch;
        fOutGroup->contentBounds(cx, cy, cw, ch);
        const int gw = std::min(cw, ch);
        fKnobs[kParamGain]->setSize(static_cast<uint>(gw), static_cast<uint>(ch));
        fKnobs[kParamGain]->setAbsolutePos(cx + (cw - gw) / 2, cy);
    }

    static std::function<std::string(float)> formatterFor(uint32_t index)
    {
        if (index == kParamLfoRate || index == kParamLfo2Rate) // octaves → Hz (0 = 1 Hz)
            return [](float v) { char b[16]; std::snprintf(b, sizeof(b), "%.2f Hz", std::exp2(v)); return std::string(b); };
        if (index == kParamMix1CCNumber || index == kParamMix2CCNumber)
            return [](float v) { char b[16]; std::snprintf(b, sizeof(b), "CC %d", static_cast<int>(v + 0.5f)); return std::string(b); };
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
