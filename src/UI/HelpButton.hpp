// HelpButton.hpp — a small labeled button (same visual pattern as the dev-only DUMP button, but
// always compiled in). Used to toggle the Help panel; the label swaps between "?" and "x" so it
// reads as open/close rather than a single fixed action.

#ifndef IFSYNTH_HELPBUTTON_HPP
#define IFSYNTH_HELPBUTTON_HPP

#include "NanoVG.hpp"

#include <functional>
#include <string>

START_NAMESPACE_DGL

class HelpButton : public NanoSubWidget
{
public:
    HelpButton(Widget* parent, std::function<void()> onClick)
        : NanoSubWidget(parent), fOnClick(std::move(onClick))
    {
        fHasFont = loadSharedResources();
    }

    void setOpen(bool open) { fLabel = open ? "x" : "?"; }

protected:
    void onNanoDisplay() override
    {
        const float w = getWidth(), h = getHeight();
        beginPath(); roundedRect(0, 0, w, h, h * 0.22f);
        fillColor(Color(40, 46, 56)); fill();
        strokeColor(Color(120, 190, 205)); strokeWidth(1.0f); stroke();
        if (fHasFont)
        {
            fontFace(NANOVG_DEJAVU_SANS_TTF);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fontSize(h * 0.6f);
            fillColor(Color(190, 226, 232));
            text(w * 0.5f, h * 0.5f, fLabel.c_str(), nullptr);
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
    std::string fLabel = "?";
    bool fHasFont = false;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HelpButton)
};

END_NAMESPACE_DGL

#endif // IFSYNTH_HELPBUTTON_HPP
