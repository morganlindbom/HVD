// KiCadLibrary.h
#pragma once
#include <QMatrix4x4>
#include <QPointF>
#include <QStringList>
#include <QVector3D>
#include <QVector>
#include <atomic>
#include <memory>

namespace hvd {
struct ModelVertex {
    float x, y, z, nx, ny, nz, r, g, b;
};
struct KiCadPad {
    QString number, type, shape;
    QPointF position, size;
    double rotation = 0;
};
struct KiCadModel {
    QString reference;
    QVector3D offset, rotation, scale{1, 1, 1};
    bool hidden = false;
};
struct KiCadFootprint {
    QString name, description, path;
    QVector<KiCadPad> pads;
    QVector<KiCadModel> models;
    QStringList diagnostics;
};
struct KiCadEntry {
    QString kind, library, name, path;
    /** Return a qualified source identity.
     *
     * Source kinds remain distinct even when library and item names coincide.
     */
    QString id() const { return kind + ":" + library + ":" + name; }
};
using Cancellation = std::shared_ptr<std::atomic_bool>;
struct KiCadIndex {
    QVector<KiCadEntry> entries;
    QStringList licenses, diagnostics;
    QString modelRoot;
    bool cancelled = false;
};
struct ImportedGeometry {
    QVector<ModelVertex> vertices;
    QVector<KiCadPad> pads;
    QString title;
    QStringList modelPaths, diagnostics;
    QString error;
};
/** Parse a complete standalone footprint expression.
 *
 * Quoted atoms and nested lists are parsed structurally; repeated and empty pad
 * numbers stay valid.
 */
KiCadFootprint parseFootprint(const QString &text, const QString &path = {});
/** Read a bounded UTF-8 footprint file.
 *
 * Missing, oversized and malformed files return actionable exceptions without
 * modifying their sources.
 */
KiCadFootprint loadFootprint(const QString &path);
/** Discover source metadata without reading model geometry.
 *
 * Enumeration checks cancellation and does not write, copy or register
 * manufacturer components.
 */
KiCadIndex scanKiCad(const QString &root, const Cancellation &cancel);
/** Resolve an external model through the configured read-only library root.
 *
 * Only KICAD9_3DMODEL_DIR is mapped; unresolved variables and canonical path
 * escapes are rejected.
 */
QString resolveKiCadModel(const QString &root, const QString &reference, const QString &footprintPath);
/** Construct KiCad model placement in a right-handed millimetre scene.
 *
 * Applies VRML's 2.54 mm unit, local scale, negative XYZ Euler rotations, then
 * millimetre offset.
 */
QMatrix4x4 kiCadPlacement(const KiCadModel &model);
/** Parse and tessellate supported VRML97 triangle scenes on the CPU.
 *
 * DEF/USE materials and Transform nodes are supported; unsupported geometry
 * fails rather than substituting.
 */
QVector<ModelVertex> loadVrml(const QString &path, const QMatrix4x4 &placement, QStringList &diagnostics,
                              const Cancellation &cancel);
/** Load a footprint's supported models without an OpenGL context.
 *
 * STEP references may explicitly use an existing same-name WRL companion; STEP
 * parsing is never claimed.
 */
ImportedGeometry loadKiCadGeometry(const QString &root, const KiCadEntry &entry, const Cancellation &cancel);
} // namespace hvd
