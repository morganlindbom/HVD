// KiCadLibrary.cpp
#include "KiCadLibrary.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hvd {
namespace {
/** Reject input with a local diagnostic.
 *
 * Exceptions are contained by asynchronous service boundaries and displayed by
 * the browser.
 */
[[noreturn]] void fail(const QString &message) { throw std::runtime_error(message.toStdString()); }
struct Expression {
    QString atom;
    QVector<Expression> children;
    bool list = false;
};
class Reader {
    QString text_;
    qsizetype at_ = 0;
    int count_ = 0;
    /** Skip whitespace and line comments.
     *
     * Comment characters inside quoted strings remain ordinary text.
     */
    void skip() {
        while (at_ < text_.size()) {
            if (text_[at_].isSpace())
                ++at_;
            else if (text_[at_] == ';' || text_[at_] == '#') {
                while (at_ < text_.size() && text_[at_] != '\n')
                    ++at_;
            } else
                break;
        }
    }

  public:
    /** Hold a bounded expression document.
     *
     * Parsing retains no references to the caller's temporary input.
     */
    explicit Reader(QString text) : text_(std::move(text)) {}
    /** Parse a structural expression with resource limits.
     *
     * Strings decode escaped quotes and backslashes; depth and atom count bound
     * malformed input.
     */
    Expression read(int depth = 0) {
        skip();
        if (depth > 128 || ++count_ > 1000000)
            fail("Footprint nesting or expression limit exceeded.");
        if (at_ >= text_.size())
            fail("Unexpected end of footprint.");
        Expression n;
        if (text_[at_] == '(') {
            n.list = true;
            ++at_;
            skip();
            while (at_ < text_.size() && text_[at_] != ')') {
                n.children.append(read(depth + 1));
                skip();
            }
            if (at_ == text_.size())
                fail("Unclosed footprint list.");
            ++at_;
        } else if (text_[at_] == '"') {
            ++at_;
            bool closed = false;
            while (at_ < text_.size()) {
                QChar c = text_[at_++];
                if (c == '"') {
                    closed = true;
                    break;
                }
                if (c == '\\') {
                    if (at_ == text_.size())
                        fail("Unclosed string escape.");
                    c = text_[at_++];
                    if (c == 'n')
                        c = '\n';
                    else if (c == 't')
                        c = '\t';
                    else if (c != '"' && c != '\\')
                        fail("Unsupported footprint string escape.");
                }
                n.atom += c;
            }
            if (!closed)
                fail("Unclosed footprint string.");
        } else {
            while (at_ < text_.size() && !text_[at_].isSpace() && text_[at_] != '(' && text_[at_] != ')')
                n.atom += text_[at_++];
            if (n.atom.isEmpty())
                fail("Unexpected closing parenthesis.");
        }
        return n;
    }
    /** Check that the entire document was consumed.
     *
     * Trailing expressions are rejected instead of ambiguously selecting one
     * footprint.
     */
    bool ended() {
        skip();
        return at_ == text_.size();
    }
};
/** Return an atomic child with bounds checking.
 *
 * Missing or nested scalar fields cannot silently become default numeric
 * zeroes.
 */
QString atom(const Expression &n, int index) {
    if (index >= n.children.size() || n.children[index].list)
        fail("Missing or invalid footprint atom.");
    return n.children[index].atom;
}
/** Find an optional named child expression.
 *
 * The tree is inspected at its immediate level, preserving nested grammar
 * ownership.
 */
const Expression *child(const Expression &n, const QString &name) {
    for (const auto &c : n.children)
        if (c.list && !c.children.isEmpty() && c.children[0].atom == name)
            return &c;
    return nullptr;
}
/** Read a finite bounded scalar.
 *
 * Invalid coordinates and nonnumeric model transforms are diagnosed before
 * rendering.
 */
double number(const Expression &n, int index) {
    bool ok = false;
    double v = atom(n, index).toDouble(&ok);
    if (!ok || !std::isfinite(v) || std::abs(v) > 1e6)
        fail("Invalid finite footprint number: " + atom(n, index));
    return v;
}
/** Read a required XY field.
 *
 * Footprint positions and dimensions are retained in millimetres without GPIO
 * interpretation.
 */
QPointF xy(const Expression &n, const QString &name) {
    auto c = child(n, name);
    if (!c || c->children.size() < 3)
        fail("Missing " + name + " XY field.");
    return {number(*c, 1), number(*c, 2)};
}
/** Read a model XYZ field or its documented default.
 *
 * Explicit malformed transforms are never treated as omitted values.
 */
QVector3D xyz(const Expression &n, const QString &name, QVector3D fallback) {
    auto c = child(n, name);
    if (!c)
        return fallback;
    auto v = child(*c, "xyz");
    if (!v || v->children.size() != 4)
        fail("Invalid model " + name + "; expected (xyz x y z).");
    return {float(number(*v, 1)), float(number(*v, 2)), float(number(*v, 3))};
}
/** Read text within the parser size limit.
 *
 * Source files stay read-only, including when decoding or validation fails.
 */
QString readText(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        fail("Cannot read " + path + ": " + f.errorString());
    if (f.size() > 32 * 1024 * 1024)
        fail("File exceeds 32 MiB parser limit: " + path);
    return QString::fromUtf8(f.readAll());
}
/** Read supported graphic fields from their structural expression.
 *
 * Positions remain native KiCad millimetres; omitted or unsupported geometry is
 * diagnosed explicitly instead of replaced by a plausible outline.
 */
KiCadGraphic graphic(const Expression &n, const QString &kind, QStringList &diagnostics) {
    KiCadGraphic g;
    g.kind = kind;
    if (auto layer = child(n, "layer"))
        g.layer = atom(*layer, 1);
    else
        fail("Missing layer for " + kind);
    if (auto stroke = child(n, "stroke")) {
        if (auto width = child(*stroke, "width"))
            g.width = number(*width, 1);
        if (auto type = child(*stroke, "type"))
            g.stroke = atom(*type, 1);
    } else if (auto width = child(n, "width"))
        g.width = number(*width, 1);
    if (g.width < 0)
        fail("Negative graphic stroke width.");
    if (auto fill = child(n, "fill")) {
        const auto value = atom(*fill, 1);
        g.filled = value == "solid" || value == "yes";
        if (!QStringList{"solid", "yes", "none", "no"}.contains(value))
            diagnostics.append("Incomplete 2D preview: unsupported fill " + value);
    }
    if (kind == "fp_line" || kind == "fp_rect")
        g.points = {xy(n, "start"), xy(n, "end")};
    else if (kind == "fp_circle")
        g.points = {xy(n, "center"), xy(n, "end")};
    else if (kind == "fp_arc") {
        if (!child(n, "mid")) {
            diagnostics.append("Incomplete 2D preview: legacy centre/angle arc is unsupported.");
            g.kind.clear();
        } else
            g.points = {xy(n, "start"), xy(n, "mid"), xy(n, "end")};
    } else if (kind == "fp_poly") {
        auto pts = child(n, "pts");
        if (!pts)
            fail("Polygon missing pts list.");
        for (const auto &pt : pts->children)
            if (pt.list && atom(pt, 0) == "xy")
                g.points.append({number(pt, 1), number(pt, 2)});
            else if (pt.list) {
                diagnostics.append("Incomplete 2D preview: polygon curves are unsupported.");
                g.kind.clear();
            }
        if (g.points.size() < 3)
            fail("Polygon requires at least three XY points.");
    } else {
        g.kind = "text";
        g.text = atom(n, 2);
        diagnostics.append("Text uses a Qt font approximation, not KiCad stroke-font glyphs.");
        if (g.text.contains("${"))
            diagnostics.append(
                "Text variables are displayed literally; no project substitutions are inferred.");
        g.points = {xy(n, "at")};
        if (auto at = child(n, "at"); at->children.size() > 3)
            g.rotation = number(*at, 3);
        if (auto effects = child(n, "effects")) {
            if (auto font = child(*effects, "font")) {
                g.textSize = xy(*font, "size");
                if (g.textSize.x() <= 0 || g.textSize.y() <= 0)
                    fail("Text dimensions must be positive.");
                diagnostics.append("Text uses a Qt font approximation, not KiCad stroke-font glyphs.");
            }
            if (auto justify = child(*effects, "justify"))
                for (int i = 1; i < justify->children.size(); ++i)
                    g.justification.append(atom(*justify, i));
            if (auto hide = child(*effects, "hide"))
                g.hidden = hide->children.size() == 1 || atom(*hide, 1) == "yes";
        }
        if (auto hide = child(n, "hide"))
            g.hidden = hide->children.size() == 1 || atom(*hide, 1) == "yes";
        for (const auto &a : n.children)
            if (!a.list && a.atom == "hide")
                g.hidden = true;
        g.mirrored = g.justification.contains("mirror");
    }
    return g;
}
} // namespace
/** Parse a standalone footprint tree.
 *
 * Pad instances are kept by array index, so repeated and mechanical pad numbers
 * do not collide.
 */
KiCadFootprint parseFootprint(const QString &text, const QString &path) {
    Reader reader(text);
    auto tree = reader.read();
    if (!reader.ended() || !tree.list || (atom(tree, 0) != "footprint" && atom(tree, 0) != "module"))
        fail("Expected one complete (footprint ...) expression.");
    KiCadFootprint f;
    f.name = atom(tree, 1);
    f.path = path;
    if (auto d = child(tree, "descr"))
        f.description = atom(*d, 1);
    for (const auto &n : tree.children) {
        if (!n.list || n.children.isEmpty())
            continue;
        const auto tag = n.children[0].atom;
        if (tag == "pad") {
            KiCadPad p;
            p.number = atom(n, 1);
            p.type = atom(n, 2);
            p.shape = atom(n, 3);
            p.position = xy(n, "at");
            p.size = xy(n, "size");
            if (p.size.x() <= 0 || p.size.y() <= 0)
                fail("Pad dimensions must be positive.");
            auto at = child(n, "at");
            if (at->children.size() > 3)
                p.rotation = number(*at, 3);
            if (auto layers = child(n, "layers"))
                for (int i = 1; i < layers->children.size(); ++i)
                    p.layers.append(atom(*layers, i));
            else
                f.diagnostics.append("Pad " + p.number +
                                     ": missing layer information; 2D uses an Unknown layer.");
            if (auto drill = child(n, "drill")) {
                p.hasDrill = true;
                bool oval = atom(*drill, 1) == "oval";
                double d = number(*drill, oval ? 2 : 1);
                p.drillSize = {d, oval ? number(*drill, 3) : d};
                if (p.drillSize.x() <= 0 || p.drillSize.y() <= 0)
                    fail("Drill dimensions must be positive.");
                if (child(*drill, "offset"))
                    p.drillOffset = xy(*drill, "offset");
            }
            p.supportedShape = QStringList{"rect", "circle", "oval", "roundrect"}.contains(p.shape);
            if (p.shape == "roundrect") {
                auto ratio = child(n, "roundrect_rratio");
                if (!ratio)
                    p.supportedShape = false;
                else {
                    p.roundRatio = number(*ratio, 1);
                    if (p.roundRatio < 0 || p.roundRatio > .5)
                        fail("Roundrect ratio must be between 0 and 0.5.");
                }
            }
            if ((p.shape == "circle" && std::abs(p.size.x() - p.size.y()) > 1e-6) || child(n, "chamfer") ||
                child(n, "rect_delta"))
                p.supportedShape = false;
            if (!p.supportedShape)
                f.diagnostics.append("Incomplete 2D preview: pad record " + QString::number(f.pads.size()) +
                                     " (" + p.number + ") unsupported shape/outline " + p.shape +
                                     "; only origin and drill are shown.");
            f.pads.append(p);
        } else if (QStringList{"fp_line", "fp_rect", "fp_circle", "fp_arc", "fp_poly", "fp_text"}.contains(
                       tag) ||
                   (tag == "property" && QStringList{"Reference", "Value"}.contains(atom(n, 1)))) {
            auto g = graphic(n, tag, f.diagnostics);
            if (!g.kind.isEmpty())
                f.graphics.append(g);
        } else if (tag.startsWith("fp_") || tag == "zone" || tag == "image") {
            f.diagnostics.append("Incomplete 2D preview: unsupported footprint construct " + tag);
        } else if (tag == "model") {
            KiCadModel m;
            m.reference = atom(n, 1);
            m.offset = xyz(n, "offset", {});
            m.rotation = xyz(n, "rotate", {});
            m.scale = xyz(n, "scale", {1, 1, 1});
            if (m.scale.x() <= 0 || m.scale.y() <= 0 || m.scale.z() <= 0)
                fail("Model scale must be positive.");
            m.hidden = false;
            if (const auto *hide = child(n, "hide")) {
                if (hide->children.size() == 1)
                    m.hidden = true;
                else if (atom(*hide, 1) == "yes")
                    m.hidden = true;
                else if (atom(*hide, 1) != "no")
                    fail("Invalid model hide field; expected yes or no.");
            }
            for (const auto &c : n.children)
                if (c.list &&
                    !QStringList{"offset", "rotate", "scale", "hide"}.contains(c.children.value(0).atom))
                    f.diagnostics.append("Unsupported model field: " + c.children.value(0).atom);
            f.models.append(m);
        }
    }
    return f;
}
/** Load a footprint source without modifying it.
 *
 * File errors and parser errors retain the source path in the caller's
 * diagnostic.
 */
KiCadFootprint loadFootprint(const QString &path) { return parseFootprint(readText(path), path); }
/** Index separate library identities with cooperative cancellation.
 *
 * Only filenames and directory metadata are read; 3D meshes are loaded on
 * selection.
 */
KiCadIndex scanKiCad(const QString &root, const Cancellation &cancel, bool includeAdditional) {
    KiCadIndex result;
    QDir dir(root);
    if (!dir.exists()) {
        result.diagnostics.append("Library root does not exist: " + root);
        return result;
    }
    result.modelRoot = QDir(root).filePath("3dmodels");
    if (!QDir(result.modelRoot).exists()) {
        result.modelRoot = root;
        result.diagnostics.append("No 3dmodels directory; using selected root to "
                                  "discover .3dshapes libraries.");
    }
    QStringList scanRoots;
    for (const auto &name : QStringList{"footprints", "symbols", "3dmodels"}) {
        auto source = QDir(root).filePath(name);
        if (QDir(source).exists())
            scanRoots.append(source);
        else
            result.diagnostics.append("Optional source folder unavailable: " + name);
    }
    if (includeAdditional)
        scanRoots = {root};
    else if (scanRoots.isEmpty())
        scanRoots.append(root);
    else
        result.diagnostics.append("Scan scope: main footprints/symbols/3dmodels folders only. Enable "
                                  "Include additional folders to browse demos, templates and loose files.");
    for (const auto &source : scanRoots) {
        QDirIterator it(source, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (cancel && cancel->load()) {
                result.cancelled = true;
                return result;
            }
            QString path = it.next();
            QFileInfo info(path);
            auto ext = info.suffix().toLower();
            auto parent = QFileInfo(info.absolutePath()).fileName();
            auto library = QDir(source).relativeFilePath(info.absolutePath());
            if (library == ".")
                library = parent;
            if (includeAdditional)
                for (const auto &prefix : QStringList{"footprints/", "symbols/", "3dmodels/"})
                    if (library.startsWith(prefix)) {
                        library.remove(0, prefix.size());
                        break;
                    }
            if (ext == "kicad_mod" && (includeAdditional || parent.endsWith(".pretty")))
                result.entries.append({"footprint", parent.endsWith(".pretty") ? library.chopped(7) : library,
                                       info.completeBaseName(), path});
            else if (ext == "kicad_sym") {
                auto identity = QDir(source).relativeFilePath(path);
                if (includeAdditional && identity.startsWith("symbols/"))
                    identity.remove(0, 8);
                identity.chop(10);
                result.entries.append({"symbol-library", identity, info.completeBaseName(), path});
            } else if ((ext == "wrl" || ext == "step") && (includeAdditional || parent.endsWith(".3dshapes")))
                result.entries.append({"model", parent.endsWith(".3dshapes") ? library.chopped(9) : library,
                                       info.fileName(), path});
            else if (info.fileName().contains("license", Qt::CaseInsensitive) ||
                     info.fileName().startsWith("COPYING", Qt::CaseInsensitive))
                result.licenses.append(path);
        }
    }
    for (const auto &info : QDir(root).entryInfoList(QDir::Files))
        if (info.fileName().contains("license", Qt::CaseInsensitive) ||
            info.fileName().startsWith("COPYING", Qt::CaseInsensitive))
            result.licenses.append(info.absoluteFilePath());
    result.licenses.removeDuplicates();
    std::sort(result.entries.begin(), result.entries.end(),
              [](const auto &a, const auto &b) { return a.id() < b.id(); });
    return result;
}
/** Resolve a reference under the configured source root.
 *
 * Canonical containment is separate from component-package asset resolution and
 * rejects external symlinks.
 */
QString resolveKiCadModel(const QString &root, const QString &reference, const QString &footprintPath) {
    QString value = reference;
    QString modelRoot = QDir(root).filePath("3dmodels");
    if (!QDir(modelRoot).exists())
        modelRoot = root;
    value.replace("${KICAD9_3DMODEL_DIR}", modelRoot);
    if (value.contains("${") || value.contains('$'))
        fail("Unresolved KiCad model variable: " + reference);
    QString path =
        QDir::isAbsolutePath(value) ? value : QDir(QFileInfo(footprintPath).absolutePath()).filePath(value);
    QString base = QFileInfo(root).canonicalFilePath();
    QString canonical = QFileInfo(path).canonicalFilePath();
    if (base.isEmpty())
        fail("Invalid configured library root: " + root);
    if (canonical.isEmpty())
        fail("Missing model: " + QDir::cleanPath(path));
    if (!canonical.startsWith(base + "/", Qt::CaseInsensitive) || !QFileInfo(canonical).isFile())
        fail("Model escapes configured library root: " + reference);
    return canonical;
}
/** Apply documented KiCad model placement.
 *
 * In the renderer +Z is up, footprint pads map (x,y) to (x,-y), and model
 * offsets already use 3D axes.
 */
QMatrix4x4 kiCadPlacement(const KiCadModel &model) {
    QMatrix4x4 m;
    m.translate(model.offset);
    m.rotate(-model.rotation.z(), 0, 0, 1);
    m.rotate(-model.rotation.y(), 0, 1, 0);
    m.rotate(-model.rotation.x(), 1, 0, 0);
    m.scale(model.scale * 2.54f);
    return m;
}
/** Load real supported geometry for a selected source identity.
 *
 * Errors clear all returned vertices; no procedural substitute or component
 * registration occurs.
 */
/** Load one source with optional already-parsed footprint records.
 *
 * UI callers avoid re-reading footprint files; the legacy public entry point
 * retains independent loading for direct models and callers without metadata.
 */
static ImportedGeometry loadSource(const QString &root, const KiCadEntry &entry, const Cancellation &cancel,
                                   const KiCadFootprint *parsed) {
    ImportedGeometry result;
    result.title = entry.library + ":" + entry.name;
    try {
        QVector<KiCadModel> models;
        if (entry.kind == "footprint") {
            auto f = parsed ? *parsed : loadFootprint(entry.path);
            result.pads = f.pads;
            result.diagnostics = f.diagnostics;
            models = f.models;
        } else if (entry.kind == "model")
            models.append({entry.path, {}, {}, {1, 1, 1}, false});
        else {
            result.error = "Symbol parsing and pin-to-pad mapping are not implemented.";
            return result;
        }
        for (const auto &m : models) {
            if (cancel && cancel->load()) {
                result.vertices.clear();
                result.error = "Loading cancelled.";
                return result;
            }
            if (m.hidden) {
                result.diagnostics.append("Hidden model skipped: " + m.reference);
                continue;
            }
            QString path;
            if (QFileInfo(m.reference).suffix().compare("step", Qt::CaseInsensitive) == 0) {
                QString companion = m.reference.left(m.reference.size() - 5) + ".wrl";
                result.diagnostics.append("STEP is not parsed. Looking for VRML companion for " +
                                          m.reference);
                try {
                    path = resolveKiCadModel(root, companion, entry.path);
                } catch (const std::exception &) {
                    // Resolve the original source before diagnosing format support.
                    //
                    // A present STEP file is different from a missing model reference.
                    const auto original = resolveKiCadModel(root, m.reference, entry.path);
                    fail("STEP model is present but cannot be rendered: " + original +
                         ". STEP parsing is unsupported and no existing same-name WRL companion is "
                         "available.");
                }
                result.diagnostics.append("Using existing VRML companion: " + path);
            } else
                path = resolveKiCadModel(root, m.reference, entry.path);
            if (QFileInfo(path).suffix().toLower() != "wrl")
                fail("Unsupported model format: " + path + " (VRML97 triangle scenes only).");
            result.vertices += loadVrml(path, kiCadPlacement(m), result.diagnostics, cancel);
            result.modelPaths.append(path);
        }
        if (result.vertices.isEmpty())
            result.error =
                models.isEmpty()
                    ? "No associated model: this footprint has no model references."
                    : "No visible supported model is available for this source (models may be hidden).";
    } catch (const std::exception &e) {
        result.vertices.clear();
        result.error = QString::fromUtf8(e.what());
    }
    return result;
}
/** Load a source through the normal parser and model resolver.
 *
 * Direct model selections do not imply footprint associations.
 */
ImportedGeometry loadKiCadGeometry(const QString &root, const KiCadEntry &entry, const Cancellation &cancel) {
    return loadSource(root, entry, cancel, nullptr);
}
/** Load real models using the footprint shared by both previews.
 *
 * Independent 2D records survive missing assets and unsupported model formats.
 */
ImportedGeometry loadFootprintModels(const QString &root, const KiCadFootprint &footprint,
                                     const Cancellation &cancel) {
    return loadSource(root, {"footprint", "", footprint.name, footprint.path}, cancel, &footprint);
}
} // namespace hvd
