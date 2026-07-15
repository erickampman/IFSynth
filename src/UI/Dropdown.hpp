// Dropdown.hpp — a reusable labelled dropdown/combo control for DPF NanoVG UIs. It draws a
// caption + a box showing the current choice + a chevron; clicking asks the host UI to open a
// PopupMenu (the UI owns the menu so it can draw over every other panel). Identify the bound
// parameter with Widget::setId(); the UI writes the chosen index back via setIndex().

#ifndef IFSYNTH_DROPDOWN_HPP
#define IFSYNTH_DROPDOWN_HPP

#include "NanoVG.hpp"

#include <string>
#include <vector>

START_NAMESPACE_DGL

class Dropdown : public NanoSubWidget
{
public:
    class Callback
    {
    public:
        virtual ~Callback() {}
        virtual void dropdownClicked(Dropdown* dropdown) = 0;
    };

    explicit Dropdown(Widget* parent, Callback* callback)
        : NanoSubWidget(parent), fCallback(callback)
    {
        fHasFont = loadSharedResources();
    }

    void setLabel(const std::string& label)          { fLabel = label; }
    void setChoices(const std::vector<std::string>& c){ fChoices = c; }
    void setIndex(int i)                              { fIndex = i; repaint(); }

    int index() const { return fIndex; }
    const std::vector<std::string>& choices() const { return fChoices; }

    // The box rectangle in window-absolute coords, so the UI can position the popup below it.
    void boxRect(int& x, int& y, int& w, int& h) const
    {
        x = getAbsoluteX() + kInset;
        y = getAbsoluteY() + boxTop();
        w = static_cast<int>(getWidth()) - 2 * kInset;
        h = kBoxH;
    }

protected:
    void onNanoDisplay() override
    {
        const float w  = getWidth();
        const float bx = kInset, bw = w - 2 * kInset, by = boxTop(), bh = kBoxH;

        if (fHasFont)
        {
            fontFace(NANOVG_DEJAVU_SANS_TTF);
            textAlign(ALIGN_CENTER | ALIGN_BOTTOM);
            fontSize(13.0f);
            fillColor(Color(176, 182, 190));
            text(w * 0.5f, by - 6.0f, fLabel.c_str(), nullptr);
        }

        beginPath(); roundedRect(bx, by, bw, bh, 4.0f);
        fillColor(Color(34, 37, 43)); fill();
        strokeColor(Color(70, 76, 86)); strokeWidth(1.0f); stroke();

        if (fHasFont && fIndex >= 0 && fIndex < static_cast<int>(fChoices.size()))
        {
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fontSize(13.0f);
            fillColor(Color(226, 230, 236));
            text(bx + 10.0f, by + bh * 0.5f, fChoices[fIndex].c_str(), nullptr);
        }

        // chevron
        const float cx = bx + bw - 14.0f, cy = by + bh * 0.5f;
        beginPath(); moveTo(cx - 4, cy - 2); lineTo(cx + 4, cy - 2); lineTo(cx, cy + 3); closePath();
        fillColor(Color(150, 160, 172)); fill();
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button == 1 && ev.press) { fCallback->dropdownClicked(this); return true; }
        return false;
    }

private:
    float boxTop() const { return (getHeight() - kBoxH) * 0.5f; }

    Callback*                fCallback;
    std::string              fLabel;
    std::vector<std::string> fChoices;
    int                      fIndex = 0;
    bool                     fHasFont = false;

    static constexpr int kBoxH = 26, kInset = 12;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Dropdown)
};

END_NAMESPACE_DGL

#endif // IFSYNTH_DROPDOWN_HPP
