// FootprintPreview.cpp
#include "FootprintPreview.h"
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
namespace hvd {
/** Assign distinct layer colors for a dark neutral canvas.
 *
 * Front and back use different hues and are independently controlled.
 */
QColor footprintLayerColor(const QString &layer) {
    if (layer == "Drills")
        return QColor("#101820");
    if (layer == "Unsupported")
        return QColor("#ff6995");
    if (layer == "F.Cu")
        return QColor("#e78a63");
    if (layer == "B.Cu")
        return QColor("#6ba5ee");
    if (layer.endsWith(".Cu"))
        return QColor("#ddb75d");
    if (layer == "F.SilkS")
        return QColor("#f1eed6");
    if (layer == "B.SilkS")
        return QColor("#91bada");
    if (layer == "F.Fab")
        return QColor("#83bfa9");
    if (layer == "B.Fab")
        return QColor("#53a1b5");
    if (layer == "F.CrtYd")
        return QColor("#d094e6");
    if (layer == "B.CrtYd")
        return QColor("#9a9cf0");
    return QColor("#bbc4ca");
}
/** Initialize a focusable standalone painter viewport.
 *
 * Mouse events never modify the external source or the 3D camera.
 */
FootprintPreview::FootprintPreview(QWidget *parent) : QWidget(parent) {
    setObjectName("footprintPreview");
    setMinimumSize(260, 250);
    setFocusPolicy(Qt::StrongFocus);
}
/** Replace footprint geometry in one GUI operation.
 *
 * Default layer visibility excludes back layers, with explicit controls to show them.
 */
void FootprintPreview::setFootprint(const KiCadFootprint &footprint, const FootprintDrawing &drawing) {
    footprint_ = footprint;
    drawing_ = drawing;
    selected_ = -1;
    empty_.clear();
    hidden_.clear();
    for (const auto &layer : drawing_.layers)
        if (layer.startsWith("B."))
            hidden_.insert(layer);
    fitToView();
}
/** Remove all old drawing and selection state.
 *
 * The message explains why the current source has no footprint geometry.
 */
void FootprintPreview::clear(const QString &message) {
    footprint_ = {};
    drawing_ = {};
    hidden_.clear();
    selected_ = -1;
    empty_ = message;
    update();
}
/** Expose immutable prepared geometry.
 *
 * Bounds and source layers remain independent of visible-layer filtering.
 */
const FootprintDrawing &FootprintPreview::drawing() const { return drawing_; }
/** Map source millimetres through the current camera.
 *
 * Hit tests and visual checks use this same mapping.
 */
QPointF FootprintPreview::screenPosition(QPointF source) const { return transform().map(source); }
/** Return a source-record selection index.
 *
 * It is reset whenever the selected footprint changes.
 */
int FootprintPreview::selectedPad() const { return selected_; }
/** Return independent 2D zoom.
 *
 * Its unit is pixels per millimetre.
 */
double FootprintPreview::zoom() const { return zoom_; }
/** Check one named layer's presentation state.
 *
 * Layers not explicitly hidden remain visible.
 */
bool FootprintPreview::layerVisible(const QString &layer) const { return !hidden_.contains(layer); }
/** Apply a linked selection without re-emission.
 *
 * Bounds checking prevents stale indices from touching new source records.
 */
void FootprintPreview::selectPad(int index) {
    selected_ = index >= 0 && index < footprint_.pads.size() ? index : -1;
    update();
}
/** Hide or show one layer without modifying geometry.
 *
 * The next paint and hit test use the updated filter.
 */
void FootprintPreview::setLayerVisible(const QString &layer, bool visible) {
    if (visible)
        hidden_.remove(layer);
    else
        hidden_.insert(layer);
    update();
}
/** Toggle grid presentation.
 *
 * Grid spacing adapts to zoom while geometry remains in source millimetres.
 */
void FootprintPreview::showGrid(bool visible) {
    grid_ = visible;
    update();
}
/** Explicitly mirror the view around its horizontal centre.
 *
 * Stored front/back layer coordinates remain unchanged.
 */
void FootprintPreview::showBack(bool back) {
    back_ = back;
    update();
}
/** Fit conservative geometry bounds with annotation margins.
 *
 * Small or empty outlines have a finite fallback scale.
 */
void FootprintPreview::fitToView() {
    QRectF b;
    bool first = true;
    for (const auto &shape : drawing_.shapes) {
        if (!layerVisible(shape.layer))
            continue;
        auto bounds = shape.path.boundingRect().adjusted(-shape.width / 2, -shape.width / 2, shape.width / 2,
                                                         shape.width / 2);
        b = first ? bounds : b.united(bounds);
        first = false;
    }
    centre_ = b.isEmpty() ? QPointF{} : b.center();
    zoom_ = std::clamp(std::min(std::max(40, width() - 90) / std::max(1.0, b.width()),
                                std::max(40, height() - 130) / std::max(1.0, b.height())),
                       .02, 1000.0);
    fitted_ = true;
    update();
}
/** Restore fitted framing after manual navigation.
 *
 * This affects only the 2D camera.
 */
void FootprintPreview::resetView() { fitToView(); }
/** Construct the shared camera transform.
 *
 * Native downward-positive Y is preserved; only explicit back view mirrors X.
 */
QTransform FootprintPreview::transform() const {
    QTransform t;
    t.translate(width() / 2.0, height() / 2.0);
    t.scale(back_ ? -zoom_ : zoom_, zoom_);
    t.translate(-centre_.x(), -centre_.y());
    return t;
}
/** Render footprint paths with source-record annotations.
 *
 * Holes are painted after pads. Layer filters also suppress corresponding labels.
 */
void FootprintPreview::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#202d38"));
    if (!empty_.isEmpty()) {
        p.setPen(QColor("#d9e2e8"));
        p.drawText(rect().adjusted(24, 20, -24, -20), Qt::AlignCenter | Qt::TextWordWrap, empty_);
        return;
    }
    auto t = transform();
    double spacing = std::pow(10.0, std::floor(std::log10(35 / zoom_)));
    if (spacing * zoom_ < 15)
        spacing *= 5;
    auto visible = t.inverted().mapRect(QRectF(rect()));
    if (grid_) {
        p.setPen(QPen(QColor("#354653"), 1));
        for (double x = std::ceil(visible.left() / spacing) * spacing; x <= visible.right(); x += spacing)
            p.drawLine(screenPosition({x, visible.top()}), screenPosition({x, visible.bottom()}));
        for (double y = std::ceil(visible.top() / spacing) * spacing; y <= visible.bottom(); y += spacing)
            p.drawLine(screenPosition({visible.left(), y}), screenPosition({visible.right(), y}));
    }
    auto origin = screenPosition({});
    p.setPen(QPen(QColor("#b67272"), 1));
    p.drawLine(QPointF(0, origin.y()), QPointF(width(), origin.y()));
    p.setPen(QPen(QColor("#669f89"), 1));
    p.drawLine(QPointF(origin.x(), 0), QPointF(origin.x(), height()));
    p.save();
    p.setTransform(t);
    for (bool holes : {false, true})
        for (const auto &shape : drawing_.shapes) {
            if (shape.drill != holes || !layerVisible(shape.layer))
                continue;
            QColor color = footprintLayerColor(shape.layer);
            Qt::PenStyle style = Qt::SolidLine;
            if (shape.stroke == "dash")
                style = Qt::DashLine;
            else if (shape.stroke == "dot")
                style = Qt::DotLine;
            else if (shape.stroke == "dash_dot")
                style = Qt::DashDotLine;
            else if (shape.stroke == "dash_dot_dot")
                style = Qt::DashDotDotLine;
            p.setPen(QPen(color, shape.width > 0 ? shape.width : .03, style));
            p.setBrush(shape.filled ? QBrush(color) : Qt::NoBrush);
            p.drawPath(shape.path);
            if (selected_ >= 0 && shape.pad == selected_ && !shape.drill) {
                p.setPen(QPen(QColor("#fff168"), 3 / zoom_));
                p.setBrush(Qt::NoBrush);
                p.drawPath(shape.path);
            }
        }
    p.restore();
    p.setFont(QFont("Sans Serif", 9));
    for (int i = 0; i < footprint_.pads.size(); ++i) {
        bool shown = false;
        for (const auto &s : drawing_.shapes)
            if (s.pad == i && layerVisible(s.layer)) {
                shown = true;
                break;
            }
        if (!shown)
            continue;
        const auto &pad = footprint_.pads[i];
        auto at = screenPosition(pad.position);
        QString label = pad.number.isEmpty() ? QString("M%1").arg(i + 1) : pad.number;
        QRectF box(at.x() - 18, at.y() - 10, 36, 20);
        p.setPen(i == selected_ ? QColor("#fff168") : QColor("#fffefa"));
        p.drawText(box, Qt::AlignCenter, label);
    }
    p.setPen(QColor("#dce7ee"));
    p.drawText(12, 20, back_ ? "Back view (X mirrored)" : "Front view (+X right, +Y down)");
    double bar = std::pow(10.0, std::floor(std::log10(90 / zoom_)));
    if (bar * zoom_ < 30)
        bar *= 5;
    int y = height() - 25;
    p.drawLine(QPointF(16, y), QPointF(16 + bar * zoom_, y));
    p.drawLine(16, y - 4, 16, y + 4);
    p.drawLine(QPointF(16 + bar * zoom_, y - 4), QPointF(16 + bar * zoom_, y + 4));
    p.drawText(16, y - 8, QString::number(bar, 'g', 4) + " mm");
    p.drawText(width() - 125, height() - 12, grid_ ? QString("Grid %1 mm").arg(spacing) : "Grid off");
    if (!drawing_.diagnostics.isEmpty()) {
        bool partial = std::any_of(drawing_.diagnostics.begin(), drawing_.diagnostics.end(),
                                   [](const auto &d) { return d.startsWith("Incomplete"); });
        p.setPen(partial ? QColor("#ffbc8d") : QColor("#b6c9d7"));
        p.drawText(12, 40,
                   partial ? "INCOMPLETE: see 2D diagnostics below" : "Preview diagnostics: see details");
    }
}
/** Preserve fitted or manual camera intent on resize.
 *
 * Manual pan/zoom does not unexpectedly reset when splitters move.
 */
void FootprintPreview::resizeEvent(QResizeEvent *) {
    if (fitted_)
        fitToView();
}
/** Hit-test actual visible paths and select their record.
 *
 * Coincident repeated records cycle in source order; right/middle input pans.
 */
void FootprintPreview::mousePressEvent(QMouseEvent *e) {
    lastMouse_ = e->position();
    if (e->button() != Qt::LeftButton)
        return;
    auto source = transform().inverted().map(e->position());
    QVector<int> hits;
    for (const auto &s : drawing_.shapes)
        if (s.pad >= 0 && layerVisible(s.layer) &&
            (s.path.contains(source) || QLineF(source, footprint_.pads[s.pad].position).length() < 5 / zoom_))
            if (!hits.contains(s.pad))
                hits.append(s.pad);
    if (hits.isEmpty())
        return;
    std::sort(hits.begin(), hits.end());
    int found = hits.indexOf(selected_);
    selectPad(hits[(found + 1) % hits.size()]);
    emit padSelected(selected_);
}
/** Pan with middle/right drag through the source transform.
 *
 * Horizontal direction accounts for explicit back-view mirroring.
 */
void FootprintPreview::mouseMoveEvent(QMouseEvent *e) {
    if (e->buttons() & (Qt::MiddleButton | Qt::RightButton)) {
        QPointF d = e->position() - lastMouse_;
        centre_ -= QPointF(d.x() / (back_ ? -zoom_ : zoom_), d.y() / zoom_);
        fitted_ = false;
        update();
    }
    lastMouse_ = e->position();
}
/** Zoom exponentially around the pointer's source coordinate.
 *
 * A bounded zoom prevents numerical overflow and excessive grid work.
 */
void FootprintPreview::wheelEvent(QWheelEvent *e) {
    auto before = transform().inverted().map(e->position());
    zoom_ = std::clamp(zoom_ * std::exp(e->angleDelta().y() / 600.0), .02, 1000.0);
    auto after = transform().inverted().map(e->position());
    centre_ += before - after;
    fitted_ = false;
    update();
    e->accept();
}
} // namespace hvd
