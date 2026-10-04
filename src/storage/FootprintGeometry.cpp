// FootprintGeometry.cpp
#include "FootprintGeometry.h"
#include <QFont>
#include <QLineF>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <numbers>
namespace hvd {
namespace {
/** Construct an obround with its exact radius and dimensions.
 *
 * Both pads and slotted drills share this shape; circles are its equal-axis case.
 */
QPainterPath obround(const QPointF &size) {
    QPainterPath path;
    QRectF rect(-size.x() / 2, -size.y() / 2, size.x(), size.y());
    double radius = std::min(size.x(), size.y()) / 2;
    path.addRoundedRect(rect, radius, radius, Qt::AbsoluteSize);
    return path;
}
/** Expand copper wildcards into explicit front and back preview layers.
 *
 * Mask/paste extents are deliberately excluded because expansion rules require
 * board context. Mechanical holes remain visible independently of copper.
 */
QStringList copperLayers(const QStringList &layers) {
    QStringList result;
    for (const auto &layer : layers) {
        if (layer == "*.Cu" || layer == "F&B.Cu")
            result << "F.Cu" << "B.Cu";
        else if (layer.endsWith(".Cu"))
            result << layer;
    }
    if (result.isEmpty())
        result << (layers.isEmpty() ? "Unknown" : "Mechanical");
    result.removeDuplicates();
    return result;
}
/** Form the actual circular arc through three source points.
 *
 * Qt arc angles use upward-positive trigonometry, so native downward-positive
 * angles are negated. The midpoint selects the intended minor or major sweep.
 */
QPainterPath arc(const QVector<QPointF> &p) {
    const auto a = p[0], b = p[1], c = p[2];
    double d = 2 * (a.x() * (b.y() - c.y()) + b.x() * (c.y() - a.y()) + c.x() * (a.y() - b.y()));
    if (std::abs(d) < 1e-12)
        return {};
    double aa = a.x() * a.x() + a.y() * a.y(), bb = b.x() * b.x() + b.y() * b.y(),
           cc = c.x() * c.x() + c.y() * c.y();
    QPointF centre((aa * (b.y() - c.y()) + bb * (c.y() - a.y()) + cc * (a.y() - b.y())) / d,
                   (aa * (c.x() - b.x()) + bb * (a.x() - c.x()) + cc * (b.x() - a.x())) / d);
    double radius = QLineF(centre, a).length();
    const auto angle = [centre](QPointF q) { return std::atan2(q.y() - centre.y(), q.x() - centre.x()); };
    const auto positive = [](double v) { return std::fmod(v + 2 * std::numbers::pi, 2 * std::numbers::pi); };
    double start = angle(a), sweep = positive(angle(c) - start);
    if (positive(angle(b) - start) > sweep)
        sweep -= 2 * std::numbers::pi;
    QPainterPath path;
    path.moveTo(a);
    path.arcTo(QRectF(centre.x() - radius, centre.y() - radius, 2 * radius, 2 * radius),
               -start * 180 / std::numbers::pi, -sweep * 180 / std::numbers::pi);
    return path;
}
} // namespace
/** Build one supported local pad outline.
 *
 * Rotation and translation are applied separately, preserving local drill offset.
 */
QPainterPath padOutline(const KiCadPad &pad) {
    QPainterPath path;
    if (!pad.supportedShape)
        return path;
    QRectF r(-pad.size.x() / 2, -pad.size.y() / 2, pad.size.x(), pad.size.y());
    if (pad.shape == "circle")
        path.addEllipse(r);
    else if (pad.shape == "rect")
        path.addRect(r);
    else if (pad.shape == "oval")
        path = obround(pad.size);
    else if (pad.shape == "roundrect") {
        double radius = std::min(pad.size.x(), pad.size.y()) * pad.roundRatio;
        path.addRoundedRect(r, radius, radius, Qt::AbsoluteSize);
    }
    return path;
}
/** Prepare actual footprint paths and their conservative bounds.
 *
 * This CPU geometry is independent of widgets and reusable for testing. Text
 * uses explicitly diagnosed font approximation; back layers are not silently
 * mirrored because all paths share the stored front-view coordinate system.
 */
FootprintDrawing footprintDrawing(const KiCadFootprint &footprint) {
    FootprintDrawing drawing;
    drawing.diagnostics = footprint.diagnostics;
    // Track stroked extents in millimetres, including text and holes.
    //
    // Zero-size initial rectangles are not treated as a physical origin bound.
    const auto add = [&drawing](FootprintShape shape) {
        if (shape.path.isEmpty())
            return;
        auto bounds = shape.path.boundingRect().adjusted(-shape.width / 2, -shape.width / 2, shape.width / 2,
                                                         shape.width / 2);
        drawing.bounds = drawing.shapes.isEmpty() ? bounds : drawing.bounds.united(bounds);
        drawing.layers.append(shape.layer);
        drawing.shapes.append(std::move(shape));
    };
    for (const auto &g : footprint.graphics) {
        if (g.hidden)
            continue;
        QPainterPath path;
        if (g.kind == "fp_line") {
            path.moveTo(g.points[0]);
            path.lineTo(g.points[1]);
        } else if (g.kind == "fp_rect")
            path.addRect(QRectF(g.points[0], g.points[1]).normalized());
        else if (g.kind == "fp_circle")
            path.addEllipse(g.points[0], QLineF(g.points[0], g.points[1]).length(),
                            QLineF(g.points[0], g.points[1]).length());
        else if (g.kind == "fp_arc") {
            path = arc(g.points);
            if (path.isEmpty())
                drawing.diagnostics.append("Incomplete 2D preview: degenerate three-point arc.");
        } else if (g.kind == "fp_poly") {
            path.moveTo(g.points[0]);
            for (int i = 1; i < g.points.size(); ++i)
                path.lineTo(g.points[i]);
            path.closeSubpath();
        } else if (g.kind == "text") {
            QFont font("Sans Serif");
            font.setPixelSize(1000);
            path.addText(0, 0, font, g.text);
            auto bounds = path.boundingRect();
            if (!bounds.isEmpty()) {
                QTransform local;
                local.scale(g.textSize.y() * .65 * g.text.size() / bounds.width(),
                            g.textSize.x() / bounds.height());
                path = local.map(path);
                bounds = path.boundingRect();
                double x = g.justification.contains("left")
                               ? bounds.left()
                               : (g.justification.contains("right") ? bounds.right() : bounds.center().x());
                double y = g.justification.contains("top")
                               ? bounds.top()
                               : (g.justification.contains("bottom") ? bounds.bottom() : bounds.center().y());
                QTransform t;
                t.translate(g.points[0].x(), g.points[0].y());
                t.rotate(-g.rotation);
                t.scale(g.mirrored ? -1 : 1, 1);
                t.translate(-x, -y);
                path = t.map(path);
            }
        }
        if (!QStringList{"solid", "default", "dash", "dot", "dash_dot", "dash_dot_dot"}.contains(g.stroke))
            drawing.diagnostics.append("Incomplete 2D preview: unsupported stroke " + g.stroke);
        add({path, g.layer, g.stroke, g.kind == "text" ? 0 : g.width, g.filled || g.kind == "text", false,
             -1});
    }
    for (int i = 0; i < footprint.pads.size(); ++i) {
        const auto &p = footprint.pads[i];
        QTransform t;
        t.translate(p.position.x(), p.position.y());
        t.rotate(-p.rotation);
        auto path = t.map(padOutline(p));
        for (const auto &layer : copperLayers(p.layers))
            add({path, layer, "solid", 0, true, false, i});
        if (path.isEmpty()) {
            QPainterPath marker;
            marker.moveTo(p.position + QPointF(-.25, -.25));
            marker.lineTo(p.position + QPointF(.25, .25));
            marker.moveTo(p.position + QPointF(-.25, .25));
            marker.lineTo(p.position + QPointF(.25, -.25));
            add({marker, "Unsupported", "solid", .08, false, false, i});
        }
        if (p.hasDrill) {
            QTransform offset;
            offset.translate(p.drillOffset.x(), p.drillOffset.y());
            add({t.map(offset.map(obround(p.drillSize))), "Drills", "solid", .03, true, true, i});
        }
    }
    drawing.layers.removeDuplicates();
    drawing.layers.sort();
    drawing.diagnostics.removeDuplicates();
    return drawing;
}
} // namespace hvd
