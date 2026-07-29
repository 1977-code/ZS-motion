#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
    The editor is laid out once, at this fixed logical size, and then scaled to
    whatever the user drags the window to. Everything is vector-drawn, so it stays
    sharp at any size and the proportions never drift — the same approach as the
    rest of the ZS line.
*/
namespace zs::layout
{
    inline constexpr int width  = 960;
    inline constexpr int height = 700;

    inline constexpr int margin       = 30;
    inline constexpr int headerHeight = 64;

    // Preset / mode bar.
    inline constexpr int barY = 78;
    inline constexpr int barH = 40;

    // The rotor and the selector panel beside it.
    inline constexpr int stageY = 132;
    inline constexpr int stageH = 280;

    inline constexpr int panelW = 320;
    inline constexpr int stageGap = 20;

    // Two rows of seven knob cells, each under its own caption and rule.
    inline constexpr int knobColumns = 7;
    inline constexpr int captionOneY = 418;
    inline constexpr int rowOneY     = 434;
    inline constexpr int captionTwoY = 538;
    inline constexpr int rowTwoY     = 554;
    inline constexpr int knobRowH    = 96;

    inline constexpr int footerLine = 666;

    inline juce::Rectangle<int> contentBounds()
    {
        return { margin, 0, width - 2 * margin, height };
    }

    inline juce::Rectangle<int> rotorBounds()
    {
        return { margin, stageY, width - 2 * margin - panelW - stageGap, stageH };
    }

    inline juce::Rectangle<int> panelBounds()
    {
        return { width - margin - panelW, stageY, panelW, stageH };
    }

    inline juce::Rectangle<int> presetBarBounds()
    {
        return { margin, barY, 330, barH };
    }

    inline juce::Rectangle<int> modeBarBounds()
    {
        const int x = margin + 350;
        return { x, barY, width - margin - x, barH };
    }

    /** Row `index` of the four control rows inside the selector panel. */
    inline juce::Rectangle<int> panelRow (int index)
    {
        const auto inner = panelBounds().reduced (16);
        return { inner.getX(), inner.getY() + index * 60, inner.getWidth(), 46 };
    }

    /** The label column at the head of each panel row. */
    inline constexpr int panelLabelWidth = 58;

    /** Cell `index` of a seven-wide knob row starting at `y`. */
    inline juce::Rectangle<int> knobCell (int index, int y)
    {
        const int usable = width - 2 * margin;
        const int x0 = margin + juce::roundToInt ((double) usable * index / knobColumns);
        const int x1 = margin + juce::roundToInt ((double) usable * (index + 1) / knobColumns);
        return { x0, y, x1 - x0, knobRowH };
    }
}
