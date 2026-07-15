// IFSynthUI.cpp — UI side: three titled panels built from the reusable widget kit.
//   "IFS Oscillator" — IFSControl pad + pitch/fine/smoothing knobs + iterations dropdown
//   "IFS Envelope"   — IFSEnvControl pad + duration/smoothing knobs + hold/iterations dropdowns
//   "Output"         — Gain knob
// The two pads edit the fractal-shape params directly; scalar params are knobs; enum params are
// dropdowns. The UI is the callback for all of them, shuttling values to/from the host by index.

#include "DistrhoUI.hpp"

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

class IFSynthUI : public UI,
                  public Knob::Callback,
                  public IFSControl::Callback,
                  public Dropdown::Callback,
                  public PopupMenu::Callback
{
public:
    IFSynthUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        setGeometryConstraints(560, 620, false, false);

        const Color accent(120, 190, 205);
        fOscGroup = makeGroup("IFS Oscillator", accent);
        fEnvGroup = makeGroup("IFS Envelope", accent);
        fOutGroup = makeGroup("Output", accent);

        fOscPad = std::make_unique<IFSControl>(fOscGroup.get(), this);
        fOscPad->setParamIds(kParamOscBx, kParamOscBy, kParamOscStartEndY, kParamOscSquash1, kParamOscSquash2);

        fEnvPad = std::make_unique<IFSEnvControl>(fEnvGroup.get(), this);
        fEnvPad->setParamIds(kParamEnvBlueX, kParamEnvBlueY, kParamEnvGreenX, kParamEnvGreenY,
                             kParamEnvStartEndY, kParamEnvSquash1, kParamEnvSquash2, kParamEnvSquash3);

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
            const ParamSpec& s = kParams[i];
            GroupBox* group = groupFor(i);
            auto knob = std::make_unique<Knob>(group, this);
            knob->setId(i);
            knob->setLabel(s.name);
            knob->configure(s.min, s.max, s.logarithmic, s.def);
            knob->setRealValue(s.def, false);
            knob->setFormatter(formatterFor(i));
            fKnobs[i] = std::move(knob);
        }

        // Popup menu: a UI-level child created LAST so it draws over everything and captures
        // events first while open.
        fMenu = std::make_unique<PopupMenu>(this, this);

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
        routeToPad(index, value);
    }

    /* -- knob -> host ----------------------------------------------------- */
    void knobDragStarted(Knob* k) override  { editParameter(k->getId(), true); }
    void knobDragFinished(Knob* k) override { editParameter(k->getId(), false); }
    void knobValueChanged(Knob* k, float v) override { setParameterValue(k->getId(), v); }

    /* -- pad -> host (both pads share IFSControl::Callback) --------------- */
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
    std::unique_ptr<GroupBox>      fOscGroup, fEnvGroup, fOutGroup;
    std::unique_ptr<IFSControl>    fOscPad;
    std::unique_ptr<IFSEnvControl> fEnvPad;
    std::unique_ptr<Knob>          fKnobs[kParamCount];
    std::unique_ptr<Dropdown>      fDropdowns[kParamCount];
    std::unique_ptr<PopupMenu>     fMenu;

    static bool isPadParam(uint32_t i)
    {
        switch (i)
        {
        case kParamOscBx: case kParamOscBy: case kParamOscStartEndY:
        case kParamOscSquash1: case kParamOscSquash2:
        case kParamEnvBlueX: case kParamEnvBlueY: case kParamEnvGreenX: case kParamEnvGreenY:
        case kParamEnvStartEndY: case kParamEnvSquash1: case kParamEnvSquash2: case kParamEnvSquash3:
            return true;
        default: return false;
        }
    }
    static bool isDropdownParam(uint32_t i)
    {
        return i == kParamOscIterations || i == kParamEnvIterations || i == kParamEnvHoldMode;
    }

    GroupBox* groupFor(uint32_t i) const
    {
        if (i == kParamGain) return fOutGroup.get();
        if (i >= kParamEnvDuration) return fEnvGroup.get();
        return fOscGroup.get();
    }

    static std::vector<std::string> choicesFor(uint32_t i)
    {
        if (i == kParamOscIterations) return {"3", "4"};
        if (i == kParamEnvIterations) return {"1", "2", "3", "4", "5", "6", "7", "8"};
        if (i == kParamEnvHoldMode)   return {"None", "Blue", "Green", "Loop"};
        return {};
    }

    // Params the pads draw from.
    void routeToPad(uint32_t index, float v)
    {
        switch (index)
        {
        case kParamOscBx:         fOscPad->setBx(v); break;
        case kParamOscBy:         fOscPad->setBy(v); break;
        case kParamOscStartEndY:  fOscPad->setStartEndY(v); break;
        case kParamOscSquash1:    fOscPad->setSquash1(v); break;
        case kParamOscSquash2:    fOscPad->setSquash2(v); break;
        case kParamOscSmoothing:  fOscPad->setSmoothing(v); break;
        case kParamOscIterations: fOscPad->setIterations(v); break;
        case kParamEnvBlueX:      fEnvPad->setBlueX(v); break;
        case kParamEnvBlueY:      fEnvPad->setBlueY(v); break;
        case kParamEnvGreenX:     fEnvPad->setGreenX(v); break;
        case kParamEnvGreenY:     fEnvPad->setGreenY(v); break;
        case kParamEnvStartEndY:  fEnvPad->setStartEndY(v); break;
        case kParamEnvSquash1:    fEnvPad->setSquash1(v); break;
        case kParamEnvSquash2:    fEnvPad->setSquash2(v); break;
        case kParamEnvSquash3:    fEnvPad->setSquash3(v); break;
        case kParamEnvSmoothing:  fEnvPad->setSmoothing(v); break;
        case kParamEnvIterations: fEnvPad->setIterations(v); break;
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
        fOscPad->setBx(kParams[kParamOscBx].def);
        fOscPad->setBy(kParams[kParamOscBy].def);
        fOscPad->setStartEndY(kParams[kParamOscStartEndY].def);
        fOscPad->setSquash1(kParams[kParamOscSquash1].def);
        fOscPad->setSquash2(kParams[kParamOscSquash2].def);
        fOscPad->setSmoothing(kParams[kParamOscSmoothing].def);
        fOscPad->setIterations(kParams[kParamOscIterations].def);

        fEnvPad->setBlueX(kParams[kParamEnvBlueX].def);
        fEnvPad->setBlueY(kParams[kParamEnvBlueY].def);
        fEnvPad->setGreenX(kParams[kParamEnvGreenX].def);
        fEnvPad->setGreenY(kParams[kParamEnvGreenY].def);
        fEnvPad->setStartEndY(kParams[kParamEnvStartEndY].def);
        fEnvPad->setSquash1(kParams[kParamEnvSquash1].def);
        fEnvPad->setSquash2(kParams[kParamEnvSquash2].def);
        fEnvPad->setSquash3(kParams[kParamEnvSquash3].def);
        fEnvPad->setSmoothing(kParams[kParamEnvSmoothing].def);
        fEnvPad->setIterations(kParams[kParamEnvIterations].def);
    }

    void layout()
    {
        const int W = static_cast<int>(getWidth());
        const int H = static_cast<int>(getHeight());
        const int m = 8, gap = 8, outH = 150;
        const int rowH = (H - 2 * m - 2 * gap - outH) / 2;

        fOscGroup->setAbsolutePos(m, m);
        fOscGroup->setSize(static_cast<uint>(W - 2 * m), static_cast<uint>(rowH));

        const int envY = m + rowH + gap;
        fEnvGroup->setAbsolutePos(m, envY);
        fEnvGroup->setSize(static_cast<uint>(W - 2 * m), static_cast<uint>(rowH));

        const int outY = envY + rowH + gap;
        fOutGroup->setAbsolutePos(m, outY);
        fOutGroup->setSize(static_cast<uint>(W - 2 * m), static_cast<uint>(outH));

        SubWidget* oscGrid[4] = { fKnobs[kParamOscPitch].get(), fKnobs[kParamOscFine].get(),
                                  fKnobs[kParamOscSmoothing].get(), fDropdowns[kParamOscIterations].get() };
        layoutPadPanel(fOscGroup.get(), fOscPad.get(), oscGrid);

        SubWidget* envGrid[4] = { fKnobs[kParamEnvDuration].get(), fKnobs[kParamEnvSmoothing].get(),
                                  fDropdowns[kParamEnvHoldMode].get(), fDropdowns[kParamEnvIterations].get() };
        layoutPadPanel(fEnvGroup.get(), fEnvPad.get(), envGrid);

        layoutOutput();
    }

    void layoutPadPanel(GroupBox* group, NanoSubWidget* pad, SubWidget* grid[4])
    {
        int cx, cy, cw, ch;
        group->contentBounds(cx, cy, cw, ch);
        const int gap = 10;
        const int padW = static_cast<int>(cw * 0.60);
        pad->setSize(static_cast<uint>(padW), static_cast<uint>(ch));
        pad->setAbsolutePos(cx, cy);

        const int kx = cx + padW + gap, kw = cw - padW - gap;
        const int cellW = kw / 2, cellH = ch / 2;
        for (int i = 0; i < 4; ++i)
        {
            const int r = i / 2, c = i % 2;
            grid[i]->setSize(static_cast<uint>(cellW), static_cast<uint>(cellH));
            grid[i]->setAbsolutePos(kx + c * cellW, cy + r * cellH);
        }
    }

    void layoutOutput()
    {
        int cx, cy, cw, ch;
        fOutGroup->contentBounds(cx, cy, cw, ch);
        const int gw = ch; // square-ish, centered
        fKnobs[kParamGain]->setSize(static_cast<uint>(gw), static_cast<uint>(ch));
        fKnobs[kParamGain]->setAbsolutePos(cx + (cw - gw) / 2, cy);
    }

    static std::function<std::string(float)> formatterFor(uint32_t index)
    {
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
