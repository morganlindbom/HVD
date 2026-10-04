// FootprintGeometry.h
#pragma once
#include "KiCadLibrary.h"
#include <QPainterPath>
#include <QRectF>
namespace hvd {
struct FootprintShape {
    QPainterPath path;
    QString layer, stroke = "solid";
    double width = .15;
    bool filled = false, drill = false;
    int pad = -1;
};
struct FootprintDrawing {
    QVector<FootprintShape> shapes;
    QStringList layers, diagnostics;
    QRectF bounds;
};
/** Prepare millimetre paths without presentation or OpenGL dependencies.
 *
 * Native KiCad +Y points down; positive pad rotation is counterclockwise on
 * screen. Pad indices are source-record identities, including repeated numbers.
 */
FootprintDrawing footprintDrawing(const KiCadFootprint &footprint);
/** Build a pad outline in local millimetres.
 *
 * Unsupported pad shapes return an empty path, never an invented rectangle.
 */
QPainterPath padOutline(const KiCadPad &pad);
} // namespace hvd
