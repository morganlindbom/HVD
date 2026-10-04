// DemoBoardGeometry.cpp
#include "DemoBoardGeometry.h"
#include <QVector3D>
#include <array>
#include <cmath>
#include <numbers>

namespace hvd {
namespace {
/** Append a flat-shaded triangle.
 *
 * Vertices carry world-space normals and diffuse material colours for the OpenGL renderer.
 */
void triangle(QVector<ModelVertex> &out, const QVector3D &a, const QVector3D &b, const QVector3D &c,
              const QVector3D &colour) {
    const auto normal = QVector3D::crossProduct(b - a, c - a).normalized();
    for (const auto &point : {a, b, c})
        out.append({point.x(), point.y(), point.z(), normal.x(), normal.y(), normal.z(), colour.x(),
                    colour.y(), colour.z()});
}

/** Append a rectangular face as two triangles.
 *
 * Counterclockwise corner order determines the outward-facing normal.
 */
void face(QVector<ModelVertex> &out, const QVector3D &a, const QVector3D &b, const QVector3D &c,
          const QVector3D &d, const QVector3D &colour) {
    triangle(out, a, b, c, colour);
    triangle(out, a, c, d, colour);
}

/** Append a solid rectangular prism.
 *
 * Centre and dimensions are specified in millimetres, including the visible PCB thickness.
 */
void box(QVector<ModelVertex> &out, const QVector3D &centre, const QVector3D &size, const QVector3D &colour) {
    const auto low = centre - size / 2, high = centre + size / 2;
    const QVector3D a(low.x(), low.y(), low.z()), b(high.x(), low.y(), low.z());
    const QVector3D c(high.x(), high.y(), low.z()), d(low.x(), high.y(), low.z());
    const QVector3D e(low.x(), low.y(), high.z()), f(high.x(), low.y(), high.z());
    const QVector3D g(high.x(), high.y(), high.z()), h(low.x(), high.y(), high.z());
    face(out, e, f, g, h, colour);
    face(out, d, c, b, a, colour);
    face(out, a, b, f, e, colour);
    face(out, b, c, g, f, colour);
    face(out, c, d, h, g, colour);
    face(out, d, a, e, h, colour);
}

/** Append an annular mounting-hole marker above the PCB.
 *
 * Four gold rings with dark centres are visual markers, not physically drilled holes.
 */
void mountingMarker(QVector<ModelVertex> &out, float x, float y) {
    constexpr int segments = 32;
    const QVector3D gold(0.78f, 0.60f, 0.25f), dark(0.035f, 0.055f, 0.045f);
    for (int i = 0; i < segments; ++i) {
        const float a = 2 * std::numbers::pi_v<float> * i / segments;
        const float b = 2 * std::numbers::pi_v<float> * (i + 1) / segments;
        const QVector3D innerA(x + 1.05f * std::cos(a), y + 1.05f * std::sin(a), 0.83f);
        const QVector3D innerB(x + 1.05f * std::cos(b), y + 1.05f * std::sin(b), 0.83f);
        const QVector3D outerA(x + 1.85f * std::cos(a), y + 1.85f * std::sin(a), 0.84f);
        const QVector3D outerB(x + 1.85f * std::cos(b), y + 1.85f * std::sin(b), 0.84f);
        face(out, innerA, outerA, outerB, innerB, gold);
        triangle(out, QVector3D(x, y, 0.83f), innerA, innerB, dark);
    }
}
} // namespace

/** Generate the synthetic demonstration board in millimetres.
 *
 * A 50 x 24 x 1.6 mm PCB carries two 16-pin rows, a chip, a connector, four mounting markers and pin 1.
 */
QVector<ModelVertex> demoBoardGeometry() {
    QVector<ModelVertex> mesh;
    const QVector3D pcb(0.035f, 0.39f, 0.19f), dark(0.075f, 0.085f, 0.10f);
    const QVector3D metal(0.75f, 0.79f, 0.85f), gold(0.86f, 0.67f, 0.28f);
    box(mesh, {0, 0, 0}, {50, 24, 1.6f}, pcb);
    for (float x : {-21.0f, 21.0f})
        for (float y : {-7.0f, 7.0f})
            mountingMarker(mesh, x, y);
    for (float y : {-10.0f, 10.0f}) {
        box(mesh, {0, y, 1.6f}, {40, 2.1f, 1.6f}, dark);
        for (int pin = 0; pin < 16; ++pin) {
            const float x = -19.05f + pin * 2.54f;
            box(mesh, {x, y, -0.3f}, {0.68f, 0.68f, 8.0f}, metal);
            box(mesh, {x, y, 2.44f}, {1.1f, 1.1f, 0.13f}, gold);
        }
    }
    box(mesh, {2, 0, 2.05f}, {13, 9, 2.5f}, dark);
    for (int i = 0; i < 7; ++i)
        for (float y : {-5.0f, 5.0f})
            box(mesh, {-3.1f + i * 1.7f, y, 1.05f}, {0.65f, 1.2f, 0.35f}, metal);
    box(mesh, {-20.3f, 0, 2.7f}, {8.2f, 8.6f, 3.8f}, metal);
    box(mesh, {-24.45f, 0, 2.7f}, {0.15f, 6.6f, 2.3f}, dark);
    box(mesh, {-24.55f, 0, 2.3f}, {0.15f, 4.6f, 0.5f}, gold);
    // A bright triangular silkscreen marker identifies the first header pin.
    //
    // The scene label explains its meaning rather than assigning a real board pinout.
    triangle(mesh, {-19.7f, -7.8f, 0.86f}, {-17.3f, -7.8f, 0.86f}, {-18.5f, -5.8f, 0.86f},
             {1.0f, 0.89f, 0.15f});
    for (float x : {12.0f, 15.0f}) {
        box(mesh, {x, -2, 1.2f}, {1.8f, 3.0f, 0.8f}, {0.73f, 0.55f, 0.34f});
        box(mesh, {x, 2, 1.2f}, {1.8f, 2.4f, 0.8f}, dark);
    }
    return mesh;
}
} // namespace hvd
