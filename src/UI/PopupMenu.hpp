// PopupMenu.hpp — a reusable overlay menu for DPF NanoVG UIs. It is meant to be a UI-level
// subwidget created LAST (so it draws on top of every panel and, while visible, is offered
// mouse events first — modal capture). The UI opens it for a given control, and gets the chosen
// row back via the Callback. A `tag` carries the caller's identity (e.g. a host param index).

#ifndef IFSYNTH_POPUPMENU_HPP
#define IFSYNTH_POPUPMENU_HPP

#include "NanoVG.hpp"

#include <cstdint>
#include <string>
#include <vector>

START_NAMESPACE_DGL

class PopupMenu : public NanoSubWidget
{
public:
    class Callback
    {
    public:
        virtual ~Callback() {}
        virtual void menuSelected(PopupMenu* menu, int index) = 0;
    };

    explicit PopupMenu(Widget* parent, Callback* callback)
        : NanoSubWidget(parent), fCallback(callback)
    {
        fHasFont = loadSharedResources();
        hide();
    }

    // Open at window-absolute (x, y) with the given width.
    void openFor(uint32_t tag, const std::vector<std::string>& items, int current, int x, int y, int width)
    {
        fTag = tag; fItems = items; fCurrent = current; fHover = -1;
        const int h = static_cast<int>(items.size()) * kRowH + 2 * kPad;
        setAbsolutePos(x, y);
        setSize(static_cast<uint>(width), static_cast<uint>(h));
        toFront();
        show();
        repaint();
    }

    uint32_t tag() const { return fTag; }

protected:
    void onNanoDisplay() override
    {
        const float w = getWidth(), h = getHeight();
        beginPath(); roundedRect(0, 0, w, h, 5.0f);
        fillColor(Color(38, 42, 49)); fill();
        strokeColor(Color(80, 86, 96)); strokeWidth(1.0f); stroke();

        for (int i = 0; i < static_cast<int>(fItems.size()); ++i)
        {
            const float ry = kPad + i * kRowH;
            if (i == fHover)
            {
                beginPath(); rect(2, ry, w - 4, kRowH);
                fillColor(Color(63, 185, 207, 60)); fill();
            }
            if (fHasFont)
            {
                fontFace(NANOVG_DEJAVU_SANS_TTF);
                textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
                fontSize(13.0f);
                fillColor(i == fCurrent ? Color(120, 210, 225) : Color(226, 230, 236));
                text(12.0f, ry + kRowH * 0.5f, fItems[i].c_str(), nullptr);
            }
        }
    }

    bool onMouse(const MouseEvent& ev) override
    {
        if (!isVisible())
            return false;
        if (ev.press) // any press while open resolves (select a row, or dismiss)
        {
            const int idx = rowAt(ev.pos.getX(), ev.pos.getY());
            hide();
            repaint();
            if (idx >= 0)
                fCallback->menuSelected(this, idx);
        }
        return true; // modal: consume all events while visible
    }

    bool onMotion(const MotionEvent& ev) override
    {
        if (!isVisible())
            return false;
        const int h = rowAt(ev.pos.getX(), ev.pos.getY());
        if (h != fHover) { fHover = h; repaint(); }
        return true;
    }

private:
    int rowAt(double mx, double my) const
    {
        if (mx < 0 || mx > getWidth()) return -1;
        const double ry = my - kPad;
        if (ry < 0) return -1;
        const int idx = static_cast<int>(ry / kRowH);
        return (idx >= 0 && idx < static_cast<int>(fItems.size())) ? idx : -1;
    }

    Callback*                fCallback;
    std::vector<std::string> fItems;
    int                      fCurrent = 0;
    int                      fHover = -1;
    uint32_t                 fTag = 0;
    bool                     fHasFont = false;

    static constexpr int kRowH = 24, kPad = 4;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PopupMenu)
};

END_NAMESPACE_DGL

#endif // IFSYNTH_POPUPMENU_HPP
