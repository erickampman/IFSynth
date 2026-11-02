// HelpButton.hpp — a "?" box with a caption above it, styled like Dropdown and HeaderButton so they
// sit together in the header (the same button as the other Unlikelyware plugins). Used to toggle
// the Help panel; the box's glyph swaps between "?" and "x" so it reads as open/close rather than
// a single fixed action.

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

    void setOpen(bool open) { fGlyph = open ? "x" : "?"; repaint(); }
    void setLabel(const std::string& label) { fLabel = label; }
    void setUiScale(float s)
    {
        fUiScale = s;
        fBoxH   = static_cast<int>(26 * s);
        fLabelH = static_cast<int>(16 * s);
    }

protected:
    void onNanoDisplay() override
    {
        const float w = getWidth();
        const float by = boxTop(), bh = fBoxH, bw = bh, bx = (w - bw) * 0.5f;

        if (fHasFont && !fLabel.empty())
        {
            fontFace(NANOVG_DEJAVU_SANS_TTF);
            textAlign(ALIGN_CENTER | ALIGN_TOP);
            fontSize(13.0f * fUiScale);
            fillColor(Color(176, 182, 190));
            text(w * 0.5f, by - fLabelH + 1.0f, fLabel.c_str(), nullptr);
        }

        beginPath(); roundedRect(bx, by, bw, bh, 4.0f);
        fillColor(fPressed ? Color(48, 52, 60) : Color(34, 37, 43)); fill();
        strokeColor(Color(70, 76, 86)); strokeWidth(1.0f); stroke();

        if (fHasFont)
        {
            fontFace(NANOVG_DEJAVU_SANS_TTF);
            textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
            fontSize(15.0f * fUiScale);
            fillColor(Color(226, 230, 236));
            text(bx + bw * 0.5f, by + bh * 0.5f, fGlyph.c_str(), nullptr);
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        // DPF offers every sibling this event regardless of position — only claim our own area.
        if (ev.button != 1)
            return false;
        if (!ev.press)
        {
            if (!fPressed)
                return false;
            fPressed = false;
            repaint();
            return true;
        }
        if (ev.pos.getX() < 0 || ev.pos.getX() >= getWidth()
            || ev.pos.getY() < 0 || ev.pos.getY() >= getHeight())
            return false;
        fPressed = true;
        repaint();
        fOnClick();
        return true;
    }

private:
    float boxTop() const
    {
        const float rem = getHeight() - (fLabelH + fBoxH);
        return (rem > 0.0f ? rem * 0.5f : 0.0f) + fLabelH;
    }

    std::function<void()> fOnClick;
    std::string           fLabel;
    std::string           fGlyph = "?";
    bool                  fHasFont = false;
    bool                  fPressed = false;
    float                 fUiScale = 1.0f;

    int fBoxH = 26, fLabelH = 16;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HelpButton)
};

END_NAMESPACE_DGL

#endif // IFSYNTH_HELPBUTTON_HPP
