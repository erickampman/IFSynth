// GroupBox.hpp — a reusable titled panel that groups related controls for DPF NanoVG UIs.
//
// It draws a rounded panel with a caption (e.g. "ADSR Envelope") and acts as a container:
// child controls are parented to it, and it lays them out in a row inside its content area
// (below the title strip, within padding). As the synth grows, each module becomes one
// GroupBox — make a box, parent its knobs to it, addControl() each, position + size the box,
// call relayout().
//
// Positions are window-absolute in DGL (a subwidget's coords are not offset by its parent),
// so relayout() places children relative to the box's own absolute position. Mouse events
// still reach the nested controls: the default Widget::onMouse forwards to subwidgets, and
// this box does not override it.

#ifndef IFSYNTH_GROUPBOX_HPP
#define IFSYNTH_GROUPBOX_HPP

#include "NanoVG.hpp"

#include <string>
#include <vector>

START_NAMESPACE_DGL

class GroupBox : public NanoSubWidget
{
public:
    explicit GroupBox(Widget* parent)
        : NanoSubWidget(parent)
    {
        fHasFont = loadSharedResources(); // DPF's bundled DejaVu Sans
    }

    void setTitle(const std::string& title) { fTitle = title; }
    void setAccentColor(Color color)        { fAccent = color; }
    // Scales font size AND the title-strip/padding geometry together — scaling font alone
    // (without the strip it sits in) is what causes titles to overflow into the content area.
    void setUiScale(float s)
    {
        fUiScale     = s;
        fPadding     = static_cast<int>(10 * s);
        fTitleHeight = static_cast<int>(28 * s);
    }

    // Frame a child widget (an IFS pad) set into the panel with a flat accent-tinted rim drawn
    // just outside its rect. Window-absolute, like contentBounds().
    void setWell(int x, int y, int w, int h)
    {
        fWellX = x - getAbsoluteX(); fWellY = y - getAbsoluteY();
        fWellW = w; fWellH = h;
    }

    // Register a control (already parented to this box) for row layout.
    void addControl(SubWidget* control) { fControls.push_back(control); }

    // The inner content rectangle (window-absolute), below the title strip and inside padding.
    // For panels that lay their children out themselves rather than via relayout().
    void contentBounds(int& x, int& y, int& w, int& h) const
    {
        x = getAbsoluteX() + fPadding;
        y = getAbsoluteY() + fTitleHeight;
        w = static_cast<int>(getWidth())  - 2 * fPadding;
        h = static_cast<int>(getHeight()) - fTitleHeight - fPadding;
    }

    // Lay the registered controls out evenly across the content area.
    void relayout()
    {
        const int n = static_cast<int>(fControls.size());
        if (n == 0)
            return;

        const int contentX = getAbsoluteX() + fPadding;
        const int contentY = getAbsoluteY() + fTitleHeight;
        const int contentW = static_cast<int>(getWidth())  - 2 * fPadding;
        const int contentH = static_cast<int>(getHeight()) - fTitleHeight - fPadding;
        if (contentW <= 0 || contentH <= 0)
            return;

        const int cellW = contentW / n;
        for (int i = 0; i < n; ++i)
        {
            fControls[i]->setSize(static_cast<uint>(cellW), static_cast<uint>(contentH));
            fControls[i]->setAbsolutePos(contentX + i * cellW, contentY);
        }
    }

protected:
    void onNanoDisplay() override
    {
        const float w = getWidth();
        const float h = getHeight();

        // panel fill (slightly raised from the window background)
        beginPath();
        roundedRect(1.5f, 1.5f, w - 3.0f, h - 3.0f, 8.0f);
        fillColor(Color(21, 40, 40));
        fill();

        // border
        beginPath();
        roundedRect(1.5f, 1.5f, w - 3.0f, h - 3.0f, 8.0f);
        strokeColor(Color(44, 72, 72));
        strokeWidth(1.5f);
        stroke();

        // well: an accent-tinted rim just outside the pad (the pad paints its own interior)
        if (fWellW > 0 && fWellH > 0)
        {
            Color rim = fAccent; rim.alpha = 0.6f;
            beginPath();
            roundedRect(fWellX - 1.5f, fWellY - 1.5f, fWellW + 3.0f, fWellH + 3.0f, 6.5f);
            strokeColor(rim);
            strokeWidth(1.5f);
            stroke();
        }

        // title
        if (fHasFont)
        {
            fontFace(NANOVG_DEJAVU_SANS_TTF);
            textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
            fontSize(13.0f * fUiScale);
            fillColor(fAccent);
            text(12.0f * fUiScale, fTitleHeight * 0.5f + 3.0f, fTitle.c_str(), nullptr);
        }
    }

private:
    std::vector<SubWidget*> fControls;
    std::string             fTitle;
    Color                   fAccent { 150, 200, 210 };
    bool                    fHasFont = false;
    float                   fUiScale = 1.0f;

    int fWellX = 0, fWellY = 0, fWellW = 0, fWellH = 0;
    int fPadding     = 10;
    int fTitleHeight = 28;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GroupBox)
};

END_NAMESPACE_DGL

#endif // IFSYNTH_GROUPBOX_HPP
