// IFSynthUI.cpp — UI side: reusable Knob widgets grouped into titled GroupBox panels and
// bound to the plugin parameters. The knob visuals/interaction live in Knob.hpp and the panel
// framing in GroupBox.hpp; this file just builds the panels, drops a knob per parameter into
// the right one, lays them out, and shuttles values to/from the host. Each future IFS module
// becomes another GroupBox with its own knobs.

#include "DistrhoUI.hpp"

#include <cstdio>
#include <memory>

#include "GroupBox.hpp"
#include "Knob.hpp"
#include "IFSynthParams.h"

START_NAMESPACE_DISTRHO

using namespace ifsynth;
using DGL_NAMESPACE::Color;
using DGL_NAMESPACE::GroupBox;
using DGL_NAMESPACE::Knob;

class IFSynthUI : public UI,
                      public Knob::Callback
{
public:
    IFSynthUI()
        : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT)
    {
        setGeometryConstraints(380, 240, false, false);

        const Color accent(120, 190, 205);
        fEnvGroup = std::make_unique<GroupBox>(this);
        fEnvGroup->setTitle("ADSR Envelope");
        fEnvGroup->setAccentColor(accent);
        fOutGroup = std::make_unique<GroupBox>(this);
        fOutGroup->setTitle("Output");
        fOutGroup->setAccentColor(accent);

        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            const ParamSpec& s = kParams[i];
            GroupBox* group = (i == kParamGain) ? fOutGroup.get() : fEnvGroup.get();

            auto knob = std::make_unique<Knob>(group, this);
            knob->setId(i);
            knob->setLabel(s.name);
            knob->configure(s.min, s.max, s.logarithmic, s.def);
            knob->setRealValue(s.def, false);
            const bool isTime = s.unit[0] == 's';
            knob->setFormatter([isTime](float v) { return format(v, isTime); });

            group->addControl(knob.get());
            fKnobs[i] = std::move(knob);
        }
        layout();
    }

protected:
    /* -- host -> UI ------------------------------------------------------- */
    void parameterChanged(uint32_t index, float value) override
    {
        if (index < kParamCount)
        {
            fKnobs[index]->setRealValue(value, false);
            fKnobs[index]->repaint();
        }
    }

    /* -- knob -> host (Knob::Callback, real units) ------------------------ */
    void knobDragStarted(Knob* knob) override  { editParameter(knob->getId(), true); }
    void knobDragFinished(Knob* knob) override { editParameter(knob->getId(), false); }
    void knobValueChanged(Knob* knob, float realValue) override
    {
        setParameterValue(knob->getId(), realValue);
    }

    /* -- drawing / layout ------------------------------------------------- */
    void onNanoDisplay() override
    {
        // Window background; the group panels (and their knobs) draw themselves.
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
    // Declared before the knobs so the knobs (their children) are destroyed first.
    std::unique_ptr<GroupBox> fEnvGroup;
    std::unique_ptr<GroupBox> fOutGroup;
    std::unique_ptr<Knob>     fKnobs[kParamCount];

    // Envelope panel (4 knobs) beside a narrow Output panel (1 knob), widths ∝ knob counts.
    void layout()
    {
        const int W = static_cast<int>(getWidth());
        const int H = static_cast<int>(getHeight());
        const int margin = 8, gap = 8;
        const int usableW = W - 2 * margin - gap;
        const int envW = usableW * 4 / 5;
        const int outW = usableW - envW;
        const int y = margin, gh = H - 2 * margin;

        fEnvGroup->setAbsolutePos(margin, y);
        fEnvGroup->setSize(static_cast<uint>(envW), static_cast<uint>(gh));
        fOutGroup->setAbsolutePos(margin + envW + gap, y);
        fOutGroup->setSize(static_cast<uint>(outW), static_cast<uint>(gh));

        fEnvGroup->relayout();
        fOutGroup->relayout();
    }

    static std::string format(float v, bool isTime)
    {
        char buf[24];
        if (isTime)
        {
            if (v < 1.0f)
                std::snprintf(buf, sizeof(buf), "%.0f ms", v * 1000.0f);
            else
                std::snprintf(buf, sizeof(buf), "%.2f s", v);
        }
        else
        {
            std::snprintf(buf, sizeof(buf), "%.2f", v);
        }
        return buf;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IFSynthUI)
};

UI* createUI()
{
    return new IFSynthUI();
}

END_NAMESPACE_DISTRHO
