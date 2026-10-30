#pragma once

#include "logicbench/Signal.h"

#include <QColor>

namespace lbgui {

/// One place for the colours, so a wire, a pin and an LED showing the same
/// value always look the same.
namespace theme {

constexpr int kGrid = 10;                ///< canvas grid and snap step
constexpr qreal kPinLength = 12.0;
constexpr qreal kBodyWidth = 56.0;
constexpr qreal kPinSpacing = 18.0;

inline QColor signalColor(lb::Signal signal)
{
    switch (signal) {
    case lb::Signal::High:      return QColor(0x22, 0xA6, 0x55);   // green, driven high
    case lb::Signal::Low:       return QColor(0x50, 0x5A, 0x6B);   // slate, driven low
    default:                    return QColor(0x7B, 0x61, 0xFF);   // violet, undefined
    }
}

inline QColor bodyColor()        { return QColor(0xFA, 0xFB, 0xFC); }
inline QColor outlineColor()     { return QColor(0x2D, 0x37, 0x48); }
inline QColor selectionColor()   { return QColor(0x1E, 0x88, 0xE5); }
inline QColor oscillationColor() { return QColor(0xE5, 0x3E, 0x3E); }
inline QColor gridColor()        { return QColor(0xE3, 0xE8, 0xEE); }
inline QColor canvasColor()      { return QColor(0xFF, 0xFF, 0xFF); }
inline QColor labelColor()       { return QColor(0x2D, 0x37, 0x48); }

} // namespace theme
} // namespace lbgui
