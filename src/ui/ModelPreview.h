// ModelPreview.h
#pragma once
#include "DemoBoardGeometry.h"
#include <QJsonValue>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPointF>
#include <memory>

namespace hvd {
class ModelPreview : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
  public:
    /** Construct a native OpenGL model viewport.
     *
     * Rendering and camera interaction stay within the UI layer, independent of
     * package and catalogue services.
     */
    explicit ModelPreview(QWidget *parent = nullptr);
    /** Release resources with the owning OpenGL context current.
     *
     * Connections are detached before widget teardown to avoid callbacks into a
     * destroyed renderer.
     */
    ~ModelPreview() override;
    /** Apply an explicit model descriptor for the selected component.
     *
     * Only schema 1, procedural demo-board-v1 in millimetres is rendered; missing
     * and unsupported descriptors clear geometry.
     */
    void setModel(const QJsonValue &descriptor);
    /** Report whether a supported model is selected.
     *
     * Selection support is separate from successful OpenGL initialization.
     */
    bool hasModel() const;
    /** Report whether the renderer can draw geometry.
     *
     * Tests and startup capture use this to detect context or shader failures.
     */
    bool renderingReady() const;
    /** Return the current explanatory empty or error state.
     *
     * Unsupported model formats and graphics failures are displayed instead of a
     * silent blank viewport.
     */
    QString statusText() const;
    /** Return camera azimuth in degrees.
     *
     * Exposes observable interaction state without allocating resources or
     * changing catalogue data.
     */
    float yaw() const;
    /** Return camera elevation in degrees.
     *
     * Elevation stays away from the pole so the world-up vector remains well
     * defined.
     */
    float pitch() const;
    /** Return camera distance in millimetres.
     *
     * Zoom is bounded to prevent clipping through the model or losing it at
     * extreme distances.
     */
    float distance() const;
    /** Apply an asynchronously prepared CPU mesh.
     *
     * Bounds determine framing and vertices upload only inside the widget's
     * current OpenGL context.
     */
    void setGeometry(const ImportedGeometry &geometry);
    /** Return distinct physical pad records.
     *
     * Stable array indices preserve repeated and empty pad numbers for selection.
     */
    const QVector<KiCadPad> &pads() const;
    /** Project a physical pad into widget coordinates.
     *
     * Uses the same live view matrix as the imported geometry; KiCad XY maps to
     * X,-Y.
     */
    QPointF padScreenPosition(int index) const;
    /** Set connection-marker visibility.
     *
     * Markers annotate physical locations without inventing pad electrical
     * functions.
     */
    void showPads(bool visible);
    /** Highlight a physical record selected in the linked 2D viewport.
     *
     * Uses source-array identity without emitting a feedback signal.
     */
    void selectPad(int index);
    /** Return the selected physical record index.
     *
     * Exposes linked selection independently of number labels.
     */
    int selectedPad() const;
  signals:
    /** Notify presentation when a physical marker is selected.
     *
     * Emits its array index rather than treating potentially repeated numbers as
     * unique IDs.
     */
    void padSelected(int index);
  public slots:
    /** Restore the useful initial perspective and framing.
     *
     * Resets orientation as well as distance to reveal the board, chip, connector
     * and header pins.
     */
    void resetView();
    /** Fit the entire model into the current viewport.
     *
     * A bounding sphere and the narrower perspective angle provide margin at any
     * aspect ratio.
     */
    void fitToView();

  protected:
    /** Initialize shaders and GPU buffers.
     *
     * Uses OpenGL 3.3 core attributes with ambient, diffuse and specular
     * lighting.
     */
    void initializeGL() override;
    /** Render actual shaded geometry and orientation overlays.
     *
     * Depth testing keeps board, chip and pins correctly occluded while text
     * stays readable.
     */
    void paintGL() override;
    /** Refit a previously fitted model after viewport changes.
     *
     * Manual zoom is retained proportionally as aspect ratio changes.
     */
    void resizeGL(int width, int height) override;
    /** Begin a left-button orbit drag.
     *
     * The stored position supports smooth incremental camera changes.
     */
    void mousePressEvent(QMouseEvent *event) override;
    /** Orbit the camera during a left-button drag.
     *
     * Horizontal movement changes azimuth and vertical movement changes
     * elevation.
     */
    void mouseMoveEvent(QMouseEvent *event) override;
    /** Zoom the perspective camera with wheel or touchpad input.
     *
     * Exponential zoom gives consistent relative movement independent of current
     * distance.
     */
    void wheelEvent(QWheelEvent *event) override;

  private:
    /** Release GPU resources after context loss or destruction.
     *
     * The CPU geometry remains available for a subsequent context initialization.
     */
    void cleanup();
    /** Calculate the fitted camera distance for a viewport.
     *
     * Uses the selected scene radius and 42 degree vertical field of view with a
     * framing margin.
     */
    float fittedDistance(int width, int height) const;
    QVector<ModelVertex> vertices_;
    QVector<KiCadPad> pads_;
    QVector3D centre_;
    float radius_ = 30.0f;
    bool imported_ = false, padsVisible_ = true;
    int selectedPad_ = -1;
    QMatrix4x4 viewProjection_;
    std::unique_ptr<QOpenGLShaderProgram> program_;
    QOpenGLBuffer buffer_{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao_;
    QString status_ = "No supported 3D model for this component.";
    QString rendererError_;
    bool ready_ = false;
    bool uploadPending_ = true;
    QPointF lastMouse_;
    float yaw_ = -125.0f;
    float pitch_ = 43.0f;
    float distance_ = 100.0f;
    float zoomRatio_ = 1.0f;
};
} // namespace hvd
