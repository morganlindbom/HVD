// FootprintPreview.h
#pragma once
#include "storage/FootprintGeometry.h"
#include <QSet>
#include <QWidget>
namespace hvd {
class FootprintPreview : public QWidget {
    Q_OBJECT
  public:
    /** Construct an independent millimetre viewport.
     *
     * QPainter renders CPU paths without an OpenGL context or model dependency.
     */
    explicit FootprintPreview(QWidget *parent = nullptr);
    /** Apply one shared footprint and prepared drawing.
     *
     * Selection changes reset the camera and clear previous pad identities.
     */
    void setFootprint(const KiCadFootprint &footprint, const FootprintDrawing &drawing);
    /** Clear all geometry with an explanatory state.
     *
     * Direct models, missing files and pending selection never retain old paths.
     */
    void clear(const QString &message);
    /** Return prepared geometry for inspection.
     *
     * Tests verify millimetre bounds without reverse-engineering rendered pixels.
     */
    const FootprintDrawing &drawing() const;
    /** Map a source coordinate into current screen coordinates.
     *
     * Default view preserves native +X right and +Y down; back view mirrors X.
     */
    QPointF screenPosition(QPointF source) const;
    /** Return the current selected record index.
     *
     * Repeated numbers remain distinct because the number is never a key.
     */
    int selectedPad() const;
    /** Return zoom in pixels per millimetre.
     *
     * Observable camera state is independent of the 3D orbit and distance.
     */
    double zoom() const;
    /** Report one layer's visibility.
     *
     * Hidden layers are excluded from both drawing and pad hit testing.
     */
    bool layerVisible(const QString &layer) const;
  public slots:
    /** Highlight a shared pad record without emitting a feedback loop.
     *
     * Linked views receive the same source-array index, never a pad number.
     */
    void selectPad(int index);
    /** Change a named layer's visibility.
     *
     * This changes presentation only, preserving source geometry and framing.
     */
    void setLayerVisible(const QString &layer, bool visible);
    /** Toggle the adaptive millimetre grid.
     *
     * Grid density is bounded at extreme zoom levels.
     */
    void showGrid(bool visible);
    /** Toggle an explicitly labelled back-side view.
     *
     * All paths and hit testing mirror together, without rewriting coordinates.
     */
    void showBack(bool back);
    /** Fit visible prepared geometry with space for annotations.
     *
     * The narrower viewport dimension determines a conservative scale.
     */
    void fitToView();
    /** Reset pan and zoom to source bounds.
     *
     * Layer visibility and the explicitly selected side are retained.
     */
    void resetView();
  signals:
    /** Notify the browser of a selected source record.
     *
     * The browser synchronizes this identity with 3D connection markers.
     */
    void padSelected(int index);

  protected:
    /** Paint actual paths, pad numbers, axes and scale.
     *
     * Unsupported shapes have marked origins and visible diagnostics, not false outlines.
     */
    void paintEvent(QPaintEvent *event) override;
    /** Refit while fitted, retaining manual navigation after resize.
     *
     * Both behaviours use the same screen transform and conservative bounds.
     */
    void resizeEvent(QResizeEvent *event) override;
    /** Select a pad or begin middle/right-button panning.
     *
     * Repeated overlapping records cycle deterministically when clicked again.
     */
    void mousePressEvent(QMouseEvent *event) override;
    /** Pan using screen deltas independent of 3D input.
     *
     * Mirrored view converts horizontal motion through its own transform.
     */
    void mouseMoveEvent(QMouseEvent *event) override;
    /** Zoom around the pointer.
     *
     * Source position under the pointer stays fixed across scale changes.
     */
    void wheelEvent(QWheelEvent *event) override;

  private:
    /** Build the current source-to-screen mapping.
     *
     * Rendering and input share exactly one invertible millimetre transform.
     */
    QTransform transform() const;
    KiCadFootprint footprint_;
    FootprintDrawing drawing_;
    QSet<QString> hidden_;
    QPointF centre_, lastMouse_;
    double zoom_ = 20;
    bool grid_ = true, back_ = false, fitted_ = true;
    int selected_ = -1;
    QString empty_ = "Select a footprint to view its actual 2D geometry.";
};
/** Return a stable front/back-aware layer color.
 *
 * The legend and canvas share this presentation mapping.
 */
QColor footprintLayerColor(const QString &layer);
} // namespace hvd
