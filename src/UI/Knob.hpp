// Knob.hpp — a reusable labelled rotary knob for DPF NanoVG UIs.
//
// It is a self-contained NanoSubWidget: give it a bounding box, a range, a caption and a value
// formatter, and it draws itself (arc value-ring + bevelled body + indicator + label + live
// readout) and handles its own interaction.
//
// Interaction comes from DPF's KnobEventHandler (vertical drag, scroll-wheel, double-click-to-
// default), but the knob always drives that handler in a *linear normalized* [0,1] space and
// applies any log law itself. That is deliberate: DPF's KnobEventHandler log-scale drag is
// broken (it re-applies logscale() to its linear accumulator every motion event, pinning wide
// ranges near the minimum), and a normalized handler also gives every knob — linear or log —
// the same predictable "one drag = full range" feel.
//
// Usage:
//   auto k = std::make_unique<Knob>(this, this);  // parent widget, Knob::Callback*
//   k->setId(paramIndex);                          // so the callback knows which control fired
//   k->setLabel("Attack");
//   k->configure(0.001f, 10.0f, /*logarithmic=*/true, /*def=*/0.01f);
//   k->setFormatter([](float v){ ... return std::string; });
//   k->setRealValue(0.01f);
//   k->setSize(w, h); k->setAbsolutePos(x, y);

#ifndef IFSYNTH_KNOB_HPP
#define IFSYNTH_KNOB_HPP

#include "NanoVG.hpp"
#include "EventHandlers.hpp"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

START_NAMESPACE_DGL

class Knob : public NanoSubWidget,
             public KnobEventHandler,
             private KnobEventHandler::Callback
{
public:
    // A knob reports in real (mapped) units and identifies itself by Widget id.
    class Callback
    {
    public:
        virtual ~Callback() {}
        virtual void knobDragStarted(Knob* knob) = 0;
        virtual void knobDragFinished(Knob* knob) = 0;
        virtual void knobValueChanged(Knob* knob, float realValue) = 0;
    };

    explicit Knob(Widget* parent, Callback* callback)
        : NanoSubWidget(parent),
          KnobEventHandler(this),
          fCallback(callback)
    {
        fHasFont = loadSharedResources(); // DPF's bundled DejaVu Sans for this NanoVG context
        KnobEventHandler::setRange(0.0f, 1.0f);   // the handler works in normalized space
        KnobEventHandler::setDefault(0.0f);
        setOrientation(KnobEventHandler::Vertical); // drag up = increase
        KnobEventHandler::setCallback(this);        // we intercept + remap to real units
    }

    // Real-unit range + optional log law + default (for double-click reset).
    void configure(float min, float max, bool logarithmic, float def)
    {
        fMin = min;
        fMax = max;
        fLog = logarithmic;
        KnobEventHandler::setDefault(toNormalized(def));
    }

    void setRealValue(float v, bool sendCallback = false)
    {
        KnobEventHandler::setValue(toNormalized(v), sendCallback);
    }
    float getRealValue() const { return toReal(KnobEventHandler::getValue()); }

    void setLabel(const std::string& label) { fLabel = label; }
    void setAccentColor(Color color)         { fAccent = color; }
    void setFormatter(std::function<std::string(float)> fn) { fFormat = std::move(fn); }

    // A disabled knob is greyed out and ignores all interaction (the value is still shown).
    void setEnabled(bool e) { if (fEnabled != e) { fEnabled = e; repaint(); } }
    bool isEnabled() const  { return fEnabled; }

protected:
    void onNanoDisplay() override
    {
        const float w  = getWidth();
        const float h  = getHeight();
        // Reserve a fixed band at the bottom for the label + readout so text is never clipped;
        // the knob scales to the space above it.
        const float kTextBand = 30.0f;
        const float knobH = h - kTextBand;
        const float cx = w * 0.5f;
        const float cy = knobH * 0.5f;
        const float r  = std::fmin(w, knobH) * 0.32f;

        const float frac     = getNormalizedValue();      // 0..1 (= control position)
        const float valAngle = kArcStart + frac * kArcSweep;

        // value-ring track (dim, full sweep)
        beginPath();
        arc(cx, cy, r + 6, kArcStart, kArcStart + kArcSweep, CW);
        strokeColor(Color(52, 56, 63));
        strokeWidth(4.0f);
        stroke();

        // value-ring fill (accent, up to current value; greyed when disabled)
        beginPath();
        arc(cx, cy, r + 6, kArcStart, valAngle, CW);
        strokeColor(fEnabled ? fAccent : Color(74, 78, 86));
        strokeWidth(4.0f);
        stroke();

        // knob body (bevelled radial gradient)
        Paint body = radialGradient(cx, cy - r * 0.3f, r * 0.2f, r, Color(64, 68, 76), Color(34, 37, 43));
        beginPath();
        circle(cx, cy, r);
        fillPaint(body);
        fill();

        // rim
        beginPath();
        circle(cx, cy, r);
        strokeColor(Color(18, 20, 24));
        strokeWidth(1.5f);
        stroke();

        // indicator line (center → value angle)
        const float ix = cx + std::cos(valAngle) * (r - 3);
        const float iy = cy + std::sin(valAngle) * (r - 3);
        beginPath();
        moveTo(cx + std::cos(valAngle) * (r * 0.35f), cy + std::sin(valAngle) * (r * 0.35f));
        lineTo(ix, iy);
        strokeColor(fEnabled ? Color(233, 236, 240) : Color(120, 125, 132));
        strokeWidth(2.5f);
        stroke();

        // label + value readout (skip if the shared font failed to load)
        if (fHasFont)
        {
            const float real = getRealValue();
            fontFace(NANOVG_DEJAVU_SANS_TTF);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            fontSize(13.0f);
            fillColor(fEnabled ? Color(176, 182, 190) : Color(108, 113, 120));
            text(cx, knobH + 1.0f, fLabel.c_str(), nullptr);

            const std::string readout = fFormat ? fFormat(real) : defaultFormat(real);
            fontSize(12.0f);
            fillColor(fEnabled ? Color(226, 230, 236) : Color(118, 123, 130));
            text(cx, knobH + 15.0f, readout.c_str(), nullptr);
        }
    }

    // Hand interaction to the KnobEventHandler (a disabled knob ignores all of it).
    bool onMouse(const MouseEvent& ev)   override { return fEnabled && mouseEvent(ev); }
    bool onMotion(const MotionEvent& ev) override { return fEnabled && motionEvent(ev); }
    bool onScroll(const ScrollEvent& ev) override { return fEnabled && scrollEvent(ev); }

private:
    // KnobEventHandler::Callback — remap normalized → real and forward to the user callback.
    void knobDragStarted(SubWidget*) override  { fCallback->knobDragStarted(this); }
    void knobDragFinished(SubWidget*) override { fCallback->knobDragFinished(this); }
    void knobValueChanged(SubWidget*, float norm) override
    {
        fCallback->knobValueChanged(this, toReal(norm));
        repaint();
    }

    float toReal(float norm) const
    {
        norm = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);
        return fLog ? fMin * std::pow(fMax / fMin, norm) : fMin + norm * (fMax - fMin);
    }
    float toNormalized(float v) const
    {
        v = v < fMin ? fMin : (v > fMax ? fMax : v);
        return fLog ? std::log(v / fMin) / std::log(fMax / fMin) : (v - fMin) / (fMax - fMin);
    }

    Callback*                         fCallback;
    std::string                       fLabel;
    std::function<std::string(float)> fFormat;
    Color                             fAccent { 79, 208, 224 };
    bool                              fHasFont = false;
    bool                              fEnabled = true;
    float                             fMin = 0.0f, fMax = 1.0f;
    bool                              fLog = false;

    static constexpr float kArcStart = 0.75f * 3.14159265358979f; // 135°
    static constexpr float kArcSweep = 1.50f * 3.14159265358979f; // 270° sweep, gap at bottom

    static std::string defaultFormat(float v)
    {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.2f", v);
        return buf;
    }

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Knob)
};

END_NAMESPACE_DGL

#endif // IFSYNTH_KNOB_HPP
