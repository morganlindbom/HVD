// ModelPreview.cpp
#include "ModelPreview.h"
#include <QJsonObject>
#include <QLineF>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QPainter>
#include <QSurfaceFormat>
#include <QVector3D>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace hvd {
/** Construct a native OpenGL model viewport.
 *
 * Requests the installed Qt OpenGLWidgets backend and a desktop OpenGL 3.3 core
 * context.
 */
ModelPreview::ModelPreview(QWidget *parent) : QOpenGLWidget(parent) {
    setObjectName("modelPreview");
    setMinimumSize(300, 250);
    setFocusPolicy(Qt::StrongFocus);
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setSamples(4);
    setFormat(format);
}

/** Release resources with the owning OpenGL context current.
 *
 * Stops context-destruction callbacks before releasing GPU objects during
 * widget teardown.
 */
ModelPreview::~ModelPreview() {
    if (context())
        disconnect(context(), nullptr, this, nullptr);
    cleanup();
}

/** Apply an explicit model descriptor for the selected component.
 *
 * Model support is deliberately bounded; no filename or component name is
 * interpreted as a supported format.
 */
void ModelPreview::setModel(const QJsonValue &descriptor) {
    vertices_.clear();
    pads_.clear();
    imported_ = false;
    centre_ = {};
    radius_ = 30.0f;
    const auto object = descriptor.toObject();
    if (descriptor.isUndefined() || descriptor.isNull())
        status_ = "No supported 3D model for this component.";
    else if (!descriptor.isObject())
        status_ = "Invalid model descriptor: expected an object.";
    else if (object.value("schemaVersion") != QJsonValue(1))
        status_ = "Unsupported model descriptor schema (supported: 1).";
    else if (object.value("type") != "procedural" || object.value("generator") != "demo-board-v1")
        status_ = "Unsupported catalogue model: STEP/F3D are not parsed here. Use KiCad Libraries for "
                  "supported VRML sources.";
    else if (object.value("units") != "mm")
        status_ = "Invalid demo model units: millimetres (mm) required.";
    else {
        vertices_ = demoBoardGeometry();
        status_ = "Demo Board — synthetic model";
    }
    uploadPending_ = true;
    resetView();
    update();
}

/** Apply real imported geometry and calculate its millimetre bounds.
 *
 * Empty or failed imports clear previous vertices and keep the actual source
 * error visible.
 */
void ModelPreview::setGeometry(const ImportedGeometry &geometry) {
    vertices_ = geometry.vertices;
    pads_ = geometry.pads;
    imported_ = true;
    selectedPad_ = -1;
    centre_ = {};
    radius_ = 1;
    status_ = geometry.error.isEmpty() ? geometry.title : geometry.error;
    if (vertices_.isEmpty() && status_.isEmpty())
        status_ = "No supported 3D model selected.";
    if (!vertices_.isEmpty()) {
        QVector3D minimum(vertices_[0].x, vertices_[0].y, vertices_[0].z), maximum = minimum;
        for (const auto &v : vertices_) {
            minimum = {std::min(minimum.x(), v.x), std::min(minimum.y(), v.y), std::min(minimum.z(), v.z)};
            maximum = {std::max(maximum.x(), v.x), std::max(maximum.y(), v.y), std::max(maximum.z(), v.z)};
        }
        for (const auto &p : pads_) {
            minimum = {std::min(minimum.x(), float(p.position.x() - p.size.x() / 2)),
                       std::min(minimum.y(), float(-p.position.y() - p.size.y() / 2)),
                       std::min(minimum.z(), 0.0f)};
            maximum = {std::max(maximum.x(), float(p.position.x() + p.size.x() / 2)),
                       std::max(maximum.y(), float(-p.position.y() + p.size.y() / 2)),
                       std::max(maximum.z(), 0.0f)};
        }
        centre_ = (minimum + maximum) / 2;
        radius_ = std::max(1.0f, (maximum - minimum).length() / 2);
    }
    uploadPending_ = true;
    resetView();
}
/** Return physical pad records independently of mesh identity.
 *
 * Browser details retain the original footprint coordinates and repeated
 * numbers.
 */
const QVector<KiCadPad> &ModelPreview::pads() const { return pads_; }
/** Project a connection marker through the live geometry view.
 *
 * The KiCad downward-positive Y axis is inverted in the right-handed +Z-up
 * scene.
 */
QPointF ModelPreview::padScreenPosition(int index) const {
    if (index < 0 || index >= pads_.size())
        return {-1000, -1000};
    const auto &p = pads_[index];
    auto clip = viewProjection_ * QVector4D(float(p.position.x()), float(-p.position.y()), 0, 1);
    if (clip.w() <= 0)
        return {-1000, -1000};
    return {(clip.x() / clip.w() + 1) * width() / 2, (1 - clip.y() / clip.w()) * height() / 2};
}
/** Toggle visible pad annotations.
 *
 * Overlay circles represent connection centres, not exact copper outlines or
 * electrical functions.
 */
void ModelPreview::showPads(bool visible) {
    padsVisible_ = visible;
    update();
}

/** Report whether a supported model is selected.
 *
 * Geometry selection does not imply that a graphics context is available.
 */
bool ModelPreview::hasModel() const { return !vertices_.isEmpty(); }

/** Report whether the renderer can draw geometry.
 *
 * A context and successfully linked shaders are required for a ready viewport.
 */
bool ModelPreview::renderingReady() const { return ready_ && isValid(); }

/** Return the explanatory model or error state.
 *
 * Context errors take precedence over a selected model's normal caption.
 */
QString ModelPreview::statusText() const {
    if (!rendererError_.isEmpty())
        return rendererError_;
    if (hasModel() && !isValid())
        return "OpenGL context unavailable; model could not be rendered.";
    return status_;
}

/** Return camera azimuth in degrees.
 *
 * The value is bounded to a single turn after each drag.
 */
float ModelPreview::yaw() const { return yaw_; }

/** Return camera elevation in degrees.
 *
 * Elevation is bounded to avoid a degenerate up vector.
 */
float ModelPreview::pitch() const { return pitch_; }

/** Return camera distance in millimetres.
 *
 * This is a view parameter rather than an electrical or component measurement.
 */
float ModelPreview::distance() const { return distance_; }

/** Restore the useful initial perspective and framing.
 *
 * The initial view looks from the connector end and shows the top surface plus
 * board thickness.
 */
void ModelPreview::resetView() {
    yaw_ = -125.0f;
    pitch_ = 43.0f;
    fitToView();
}

/** Fit the entire model into the current viewport.
 *
 * Preserves orientation and removes manual zoom while leaving a margin around
 * the scene.
 */
void ModelPreview::fitToView() {
    zoomRatio_ = 1.0f;
    distance_ = fittedDistance(width(), height());
    update();
}

/** Calculate the fitted camera distance for a viewport.
 *
 * The smaller horizontal/vertical perspective angle controls the fit of the 30
 * mm bounding sphere.
 */
float ModelPreview::fittedDistance(int width, int height) const {
    const float aspect = static_cast<float>(std::max(1, width)) / std::max(1, height);
    const float halfVertical = 21.0f * std::numbers::pi_v<float> / 180;
    const float halfHorizontal = std::atan(std::tan(halfVertical) * aspect);
    return radius_ / std::sin(std::min(halfVertical, halfHorizontal)) * 1.12f;
}

/** Initialize shaders and GPU buffers.
 *
 * The renderer uses explicit attributes, perspective projection and a
 * directional light with specular highlights.
 */
void ModelPreview::initializeGL() {
    initializeOpenGLFunctions();
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &ModelPreview::cleanup,
            Qt::DirectConnection);
    program_ = std::make_unique<QOpenGLShaderProgram>();
    constexpr auto vertexShader = R"(
        #version 330 core
        layout(location=0) in vec3 position;
        layout(location=1) in vec3 normal;
        layout(location=2) in vec3 colour;
        uniform mat4 mvp;
        out vec3 worldPosition;
        out vec3 worldNormal;
        out vec3 material;
        void main() {
            gl_Position = mvp * vec4(position, 1.0);
            worldPosition = position;
            worldNormal = normal;
            material = colour;
        })";
    constexpr auto fragmentShader = R"(
        #version 330 core
        in vec3 worldPosition;
        in vec3 worldNormal;
        in vec3 material;
        uniform vec3 eye;
        out vec4 fragment;
        void main() {
            vec3 n = normalize(worldNormal);
            vec3 light = normalize(vec3(-0.35, -0.5, 1.0));
            float diffuse = max(dot(n, light), 0.0);
            vec3 view = normalize(eye - worldPosition);
            float specular = pow(max(dot(reflect(-light, n), view), 0.0), 36.0);
            fragment = vec4(material * (0.36 + 0.64 * diffuse) + vec3(0.22 * specular), 1.0);
        })";
    if (!program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader) ||
        !program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader) || !program_->link() ||
        !vao_.create() || !buffer_.create()) {
        rendererError_ = "OpenGL initialization failed: " + program_->log();
        ready_ = false;
        return;
    }
    setProperty("graphicsRenderer",
                QString::fromLatin1(reinterpret_cast<const char *>(glGetString(GL_RENDERER))));
    setProperty("graphicsVersion",
                QString::fromLatin1(reinterpret_cast<const char *>(glGetString(GL_VERSION))));
    ready_ = true;
    rendererError_.clear();
    uploadPending_ = true;
}

/** Render actual shaded geometry and orientation overlays.
 *
 * OpenGL depth testing handles solid geometry; QPainter adds captions, mounting
 * labels and orientation axes.
 */
void ModelPreview::paintGL() {
    glClearColor(0.91f, 0.925f, 0.945f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    QMatrix4x4 view, projection;
    const float yawRadians = yaw_ * std::numbers::pi_v<float> / 180;
    const float pitchRadians = pitch_ * std::numbers::pi_v<float> / 180;
    const QVector3D eye = centre_ + QVector3D(distance_ * std::cos(pitchRadians) * std::cos(yawRadians),
                                              distance_ * std::cos(pitchRadians) * std::sin(yawRadians),
                                              distance_ * std::sin(pitchRadians));
    view.lookAt(eye, centre_, {0, 0, 1});
    projection.perspective(42.0f, static_cast<float>(width()) / std::max(1, height()),
                           std::max(0.001f, radius_ * 0.001f), distance_ + radius_ * 4.0f);
    viewProjection_ = projection * view;
    if (ready_ && hasModel()) {
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        program_->bind();
        vao_.bind();
        buffer_.bind();
        if (uploadPending_) {
            buffer_.allocate(vertices_.constData(), static_cast<int>(vertices_.size() * sizeof(ModelVertex)));
            uploadPending_ = false;
        }
        program_->enableAttributeArray(0);
        program_->enableAttributeArray(1);
        program_->enableAttributeArray(2);
        program_->setAttributeBuffer(0, GL_FLOAT, offsetof(ModelVertex, x), 3, sizeof(ModelVertex));
        program_->setAttributeBuffer(1, GL_FLOAT, offsetof(ModelVertex, nx), 3, sizeof(ModelVertex));
        program_->setAttributeBuffer(2, GL_FLOAT, offsetof(ModelVertex, r), 3, sizeof(ModelVertex));
        program_->setUniformValue("mvp", projection * view);
        program_->setUniformValue("eye", eye);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<int>(vertices_.size()));
        buffer_.release();
        vao_.release();
        program_->release();
        glDisable(GL_DEPTH_TEST);
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor("#283445"));
    if (!hasModel() || !ready_) {
        painter.drawText(rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap, statusText());
        return;
    }
    painter.drawText(QRect(14, 12, width() - 28, 64), Qt::AlignLeft | Qt::TextWordWrap,
                     imported_ ? "VRML97 · " + status_
                               : "Demo Board — synthetic model · 50 × 24 × 1.6 mm PCB");
    painter.drawText(QRect(14, height() - 47, width() - 28, 38), Qt::AlignLeft | Qt::TextWordWrap,
                     imported_ ? "Drag to rotate · wheel to zoom · units: mm\nNumbered "
                                 "circles: footprint connection centres"
                               : "M1–M4: mounting-hole markers · yellow triangle: pin "
                                 "1\nDrag to rotate · wheel to zoom · units: mm");
    const QMatrix4x4 mvp = projection * view;
    // Project mounting-marker labels into the live perspective view.
    //
    // These labels distinguish visual mounting markers from physically drilled
    // holes.
    const auto label = [&painter, &mvp, this](const QVector3D &point, const QString &text) {
        const QVector4D clip = mvp * QVector4D(point, 1);
        if (clip.w() <= 0)
            return;
        const QPointF pixel((clip.x() / clip.w() + 1) * width() / 2,
                            (1 - clip.y() / clip.w()) * height() / 2);
        painter.setPen(QColor("#fff6d6"));
        painter.drawText(QRectF(pixel.x() - 10, pixel.y() - 7, 28, 18), Qt::AlignCenter, text);
    };
    if (!imported_) {
        label({-21, -7, 1.1f}, "M1");
        label({-21, 7, 1.1f}, "M2");
        label({21, -7, 1.1f}, "M3");
        label({21, 7, 1.1f}, "M4");
    } else if (padsVisible_) {
        for (int i = 0; i < pads_.size(); ++i) {
            auto pixel = padScreenPosition(i);
            painter.setPen(QPen(i == selectedPad_ ? QColor("#dc4931") : QColor("#2465ae"), 2));
            painter.setBrush(i == selectedPad_ ? QColor("#ffddaa") : QColor("#eff6ff"));
            painter.drawEllipse(pixel, 11, 11);
            painter.drawText(QRectF(pixel.x() - 25, pixel.y() - 10, 50, 20), Qt::AlignCenter,
                             pads_[i].number.isEmpty() ? "M" : pads_[i].number);
        }
    }
    const QPointF origin(width() - 64, height() - 82);
    const QVector3D directions[]{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    const QColor colours[]{QColor("#c63a40"), QColor("#228349"), QColor("#3367cf")};
    const QString names[]{"X", "Y", "Z"};
    for (int i = 0; i < 3; ++i) {
        const auto direction = view.mapVector(directions[i]);
        const QPointF end = origin + QPointF(direction.x() * 36, -direction.y() * 36);
        painter.setPen(QPen(colours[i], 3));
        painter.drawLine(origin, end);
        painter.drawText(end + QPointF(3, -3), names[i]);
    }
}

/** Refit a previously fitted model after viewport changes.
 *
 * Maintains the user's relative zoom while updating the perspective fit for the
 * new aspect ratio.
 */
void ModelPreview::resizeGL(int width, int height) {
    distance_ = std::max(radius_ * 1.2f, fittedDistance(width, height) * zoomRatio_);
}

/** Begin a left-button orbit drag.
 *
 * Other buttons are passed to the base widget rather than changing the view.
 */
void ModelPreview::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (padsVisible_ && imported_ && hasModel()) {
            int nearest = -1;
            double best = 18;
            for (int i = 0; i < pads_.size(); ++i) {
                double d = QLineF(event->position(), padScreenPosition(i)).length();
                if (d < best) {
                    best = d;
                    nearest = i;
                }
            }
            if (nearest >= 0) {
                selectedPad_ = nearest;
                emit padSelected(nearest);
                update();
            }
        }
        lastMouse_ = event->position();
        event->accept();
    } else
        QOpenGLWidget::mousePressEvent(event);
}

/** Orbit the camera during a left-button drag.
 *
 * Clamps elevation while allowing a full azimuth rotation around the board.
 */
void ModelPreview::mouseMoveEvent(QMouseEvent *event) {
    if (hasModel() && event->buttons().testFlag(Qt::LeftButton)) {
        const QPointF delta = event->position() - lastMouse_;
        yaw_ = std::remainder(yaw_ - static_cast<float>(delta.x()) * 0.5f, 360.0f);
        pitch_ = std::clamp(pitch_ + static_cast<float>(delta.y()) * 0.4f, -85.0f, 85.0f);
        lastMouse_ = event->position();
        update();
        event->accept();
    } else
        QOpenGLWidget::mouseMoveEvent(event);
}

/** Zoom the perspective camera with wheel or touchpad input.
 *
 * Wheel steps change distance exponentially; bounded zoom remains stable after
 * resizing.
 */
void ModelPreview::wheelEvent(QWheelEvent *event) {
    if (!hasModel()) {
        event->ignore();
        return;
    }
    const float steps =
        event->angleDelta().y() != 0 ? event->angleDelta().y() / 120.0f : event->pixelDelta().y() / 40.0f;
    zoomRatio_ = std::clamp(zoomRatio_ * std::exp(-steps * 0.15f), 0.40f, 8.0f);
    distance_ = std::max(radius_ * 1.2f, fittedDistance(width(), height()) * zoomRatio_);
    update();
    event->accept();
}

/** Release GPU resources after context loss or destruction.
 *
 * No catalogue data is changed, and CPU vertices can be uploaded if a new
 * context is created.
 */
void ModelPreview::cleanup() {
    if (context() && context()->isValid()) {
        makeCurrent();
        buffer_.destroy();
        vao_.destroy();
        program_.reset();
        doneCurrent();
    }
    ready_ = false;
    uploadPending_ = true;
}
} // namespace hvd
