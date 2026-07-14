// IFSynthUI.cpp — UI side: three titled panels built from the reusable widget kit.
//   "IFS Oscillator" — the IFSControl fractal pad + tune/smoothing/iterations knobs
//   "ADSR Envelope"  — A/D/S/R knobs
//   "Output"         — Gain knob
// The pad edits the five fractal-shape params directly; everything else is a Knob. The UI is
// the callback for both, shuttling values to/from the host by param index.

#include "DistrhoUI.hpp"

#include <cstdio>
#include <functional>
#include <memory>
#include <string>

#include "GroupBox.hpp"
#include "IFSControl.hpp"
#include "IFSynthParams.h"
#include "Knob.hpp"

START_NAMESPACE_DISTRHO

using namespace ifsynth;
using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::GroupBox;
using DGL_NAMESPACE::IFSControl;
using DGL_NAMESPACE::Knob;

class IFSynthUI : public UI,
                  public Knob::Callback,
                  public IFSControl::Callback
{
public:
    IFSynthUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        setGeometryConstraints(560, 460, false, false);

        const Color accent(120, 190, 205);
        fIfsGroup = makeGroup("IFS Oscillator", accent);
        fEnvGroup = makeGroup("ADSR Envelope", accent);
        fOutGroup = makeGroup("Output", accent);

        // The fractal pad (edits the five shape params).
        fPad = std::make_unique<IFSControl>(fIfsGroup.get(), this);
        fPad->setParamIds(kParamBx, kParamBy, kParamStartEndY, kParamIFS1Squash, kParamIFS2Squash);

        // A Knob for every non-shape param, parented to its panel.
        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            if (isPadParam(i))
                continue;
            const ParamSpec& s = kParams[i];
            GroupBox* group = groupFor(i);
            auto knob = std::make_unique<Knob>(group, this);
            knob->setId(i);
            knob->setLabel(s.name);
            knob->configure(s.min, s.max, s.logarithmic, s.def);
            knob->setRealValue(s.def, false);
            knob->setFormatter(formatterFor(i));
            if (group == fEnvGroup.get() || group == fOutGroup.get())
                group->addControl(knob.get());   // even-row panels use relayout()
            fKnobs[i] = std::move(knob);
        }

        pushPadDefaults();
        layout();
    }

protected:
    /* -- host -> UI ------------------------------------------------------- */
    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= kParamCount)
            return;
        if (fKnobs[index])
        {
            fKnobs[index]->setRealValue(value, false);
            fKnobs[index]->repaint();
        }
        switch (index) // params the pad draws from
        {
        case kParamBx:         fPad->setBx(value);         break;
        case kParamBy:         fPad->setBy(value);         break;
        case kParamStartEndY:  fPad->setStartEndY(value);  break;
        case kParamIFS1Squash: fPad->setSquash1(value);    break;
        case kParamIFS2Squash: fPad->setSquash2(value);    break;
        case kParamSmoothing:  fPad->setSmoothing(value);  break;
        case kParamIterations: fPad->setIterations(value); break;
        default: break;
        }
    }

    /* -- knob -> host ----------------------------------------------------- */
    void knobDragStarted(Knob* k) override  { editParameter(k->getId(), true); }
    void knobDragFinished(Knob* k) override { editParameter(k->getId(), false); }
    void knobValueChanged(Knob* k, float v) override { setParameterValue(k->getId(), v); }

    /* -- pad -> host ------------------------------------------------------ */
    void ifsDragStarted(uint32_t id) override  { editParameter(id, true); }
    void ifsDragFinished(uint32_t id) override { editParameter(id, false); }
    void ifsValueChanged(uint32_t id, float v) override { setParameterValue(id, v); }

    /* -- drawing / layout ------------------------------------------------- */
    void onNanoDisplay() override
    {
        beginPath();
        rect(0, 0, getWidth(), getHeight());
        fillColor(24, 26, 31);
        fill();
    }

    void onResize(const ResizeEvent& ev) override
    {
        UI::onResize(ev);
        layout();
    }

private:
    // Panels declared before their child controls so the children are destroyed first.
    std::unique_ptr<GroupBox>   fIfsGroup, fEnvGroup, fOutGroup;
    std::unique_ptr<IFSControl> fPad;
    std::unique_ptr<Knob>       fKnobs[kParamCount]; // null for the five pad-edited params

    static bool isPadParam(uint32_t i)
    {
        return i == kParamBx || i == kParamBy || i == kParamStartEndY
            || i == kParamIFS1Squash || i == kParamIFS2Squash;
    }

    GroupBox* groupFor(uint32_t i) const
    {
        if (i == kParamGain) return fOutGroup.get();
        if (i == kParamAttack || i == kParamDecay || i == kParamSustain || i == kParamRelease)
            return fEnvGroup.get();
        return fIfsGroup.get(); // OscPitch / OscFine / Smoothing / Iterations
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
        fPad->setBx(kParams[kParamBx].def);
        fPad->setBy(kParams[kParamBy].def);
        fPad->setStartEndY(kParams[kParamStartEndY].def);
        fPad->setSquash1(kParams[kParamIFS1Squash].def);
        fPad->setSquash2(kParams[kParamIFS2Squash].def);
        fPad->setSmoothing(kParams[kParamSmoothing].def);
        fPad->setIterations(kParams[kParamIterations].def);
    }

    void layout()
    {
        const int W = static_cast<int>(getWidth());
        const int H = static_cast<int>(getHeight());
        const int m = 8, gap = 8;
        const int ifsH = static_cast<int>(H * 0.56);

        fIfsGroup->setAbsolutePos(m, m);
        fIfsGroup->setSize(static_cast<uint>(W - 2 * m), static_cast<uint>(ifsH));
        layoutIfsPanel();

        const int by = m + ifsH + gap;
        const int gh = H - by - m;
        const int usableW = W - 2 * m - gap;
        const int envW = usableW * 4 / 5;
        const int outW = usableW - envW;
        fEnvGroup->setAbsolutePos(m, by);
        fEnvGroup->setSize(static_cast<uint>(envW), static_cast<uint>(gh));
        fOutGroup->setAbsolutePos(m + envW + gap, by);
        fOutGroup->setSize(static_cast<uint>(outW), static_cast<uint>(gh));
        fEnvGroup->relayout();
        fOutGroup->relayout();
    }

    // The IFS panel lays out itself: pad on the left, a 2x2 knob block on the right.
    void layoutIfsPanel()
    {
        int cx, cy, cw, ch;
        fIfsGroup->contentBounds(cx, cy, cw, ch);
        const int gap = 10;
        const int padW = static_cast<int>(cw * 0.62);

        fPad->setAbsolutePos(cx, cy);
        fPad->setSize(static_cast<uint>(padW), static_cast<uint>(ch));

        const int kx = cx + padW + gap;
        const int kw = cw - padW - gap;
        const int cellW = kw / 2, cellH = ch / 2;
        Knob* grid[4] = {
            fKnobs[kParamOscPitch].get(), fKnobs[kParamOscFine].get(),
            fKnobs[kParamSmoothing].get(), fKnobs[kParamIterations].get()
        };
        for (int i = 0; i < 4; ++i)
        {
            const int r = i / 2, c = i % 2;
            grid[i]->setSize(static_cast<uint>(cellW), static_cast<uint>(cellH));
            grid[i]->setAbsolutePos(kx + c * cellW, cy + r * cellH);
        }
    }

    static std::function<std::string(float)> formatterFor(uint32_t index)
    {
        if (index == kParamIterations)
            return [](float v) { char b[16]; std::snprintf(b, sizeof(b), "%d", static_cast<int>(v) + 3); return std::string(b); };
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
