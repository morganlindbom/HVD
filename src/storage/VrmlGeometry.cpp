// VrmlGeometry.cpp
#include "KiCadLibrary.h"
#include <QFile>
#include <QHash>
#include <QSet>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace hvd {
namespace {
/** Fail unsupported or malformed scene input.
 *
 * The browser catches these exceptions and clears geometry rather than showing
 * a substitute.
 */
[[noreturn]] void reject(const QString &text) { throw std::runtime_error(text.toStdString()); }
struct Node {
    QString type;
    QHash<QString, QVector<double>> values;
    QHash<QString, std::shared_ptr<Node>> nodes;
    QVector<std::shared_ptr<Node>> children;
};
class VrmlReader {
    QStringList tokens_;
    qsizetype at_ = 0;
    QHash<QString, std::shared_ptr<Node>> definitions_;
    Cancellation cancel_;
    /** Consume one token within bounds.
     *
     * Cancellation is checked throughout parsing rather than only between files.
     */
    QString take() {
        if (cancel_ && cancel_->load())
            reject("Model loading cancelled.");
        if (at_ >= tokens_.size())
            reject("Unexpected end of VRML scene.");
        return tokens_[at_++];
    }
    /** Require grammar punctuation.
     *
     * Mismatched braces and arrays fail with the actual token in the diagnostic.
     */
    void expect(const QString &s) {
        auto got = take();
        if (got != s)
            reject("VRML expected " + s + ", found " + got);
    }
    /** Decode finite numeric or boolean values.
     *
     * Large or invalid coordinates cannot overflow GPU geometry buffers
     * unnoticed.
     */
    double number() {
        auto t = take();
        if (t == "TRUE")
            return 1;
        if (t == "FALSE")
            return 0;
        bool ok;
        double n = t.toDouble(&ok);
        if (!ok || !std::isfinite(n) || std::abs(n) > 1e8)
            reject("Invalid VRML number: " + t);
        return n;
    }

  public:
    /** Tokenize a bounded VRML document.
     *
     * Comments and strings are handled lexically, independently from nested node
     * parsing.
     */
    VrmlReader(const QString &text, Cancellation cancel) : cancel_(std::move(cancel)) {
        for (qsizetype i = 0; i < text.size();) {
            if (cancel_ && cancel_->load())
                reject("Model loading cancelled.");
            auto c = text[i];
            if (c.isSpace() || c == ',') {
                ++i;
                continue;
            }
            if (c == '#') {
                while (i < text.size() && text[i] != '\n')
                    ++i;
                continue;
            }
            if (QString("{}[]").contains(c)) {
                tokens_.append(QString(c));
                ++i;
                continue;
            }
            QString token;
            if (c == '"') {
                ++i;
                bool closed = false;
                while (i < text.size()) {
                    c = text[i++];
                    if (c == '"') {
                        closed = true;
                        break;
                    }
                    if (c == '\\' && i < text.size())
                        c = text[i++];
                    token += c;
                }
                if (!closed)
                    reject("Unclosed VRML string.");
            } else {
                while (i < text.size() && !text[i].isSpace() && !QString("{}[],#").contains(text[i]))
                    token += text[i++];
            }
            tokens_.append(token);
            if (tokens_.size() > 4000000)
                reject("VRML token limit exceeded.");
        }
    }
    /** Report whether top-level scene input remains.
     *
     * Multiple valid Shape and Transform nodes are retained as a complete scene.
     */
    bool ended() const { return at_ == tokens_.size(); }
    /** Parse an explicitly supported VRML97 node.
     *
     * Shared references are resolved without cycles; unknown node types and
     * fields are rejected.
     */
    std::shared_ptr<Node> node(int depth = 0) {
        if (depth > 128)
            reject("VRML nesting limit exceeded.");
        QString type = take(), id;
        if (type == "NULL")
            return {};
        if (type == "USE") {
            auto name = take();
            if (!definitions_.contains(name))
                reject("Unknown or cyclic VRML USE: " + name);
            return definitions_[name];
        }
        if (type == "DEF") {
            id = take();
            if (definitions_.contains(id))
                reject("Duplicate VRML DEF: " + id);
            type = take();
        }
        if (!QSet<QString>{"Shape", "Appearance", "Material", "IndexedFaceSet", "Coordinate", "Normal",
                           "Transform", "Group"}
                 .contains(type))
            reject("Unsupported VRML node: " + type);
        auto n = std::make_shared<Node>();
        n->type = type;
        expect("{");
        while (at_ < tokens_.size() && tokens_[at_] != "}") {
            auto field = take();
            const QHash<QString, QStringList> allowed{
                {"Shape", {"appearance", "geometry"}},
                {"Appearance", {"material", "texture", "textureTransform"}},
                {"Material",
                 {"ambientIntensity", "diffuseColor", "specularColor", "emissiveColor", "shininess",
                  "transparency"}},
                {"Coordinate", {"point"}},
                {"Normal", {"vector"}},
                {"IndexedFaceSet",
                 {"coord", "coordIndex", "normal", "normalIndex", "normalPerVertex", "ccw", "convex", "solid",
                  "creaseAngle"}},
                {"Transform",
                 {"translation", "rotation", "scale", "center", "scaleOrientation", "children", "bboxCenter",
                  "bboxSize"}},
                {"Group", {"children", "bboxCenter", "bboxSize"}}};
            if (!allowed.value(type).contains(field))
                reject("Unsupported VRML field: " + type + "." + field);
            if (QStringList{"appearance", "geometry", "material", "texture", "textureTransform", "coord",
                            "normal"}
                    .contains(field))
                n->nodes.insert(field, node(depth + 1));
            else if (field == "children") {
                bool array = at_ < tokens_.size() && tokens_[at_] == "[";
                if (array)
                    ++at_;
                do {
                    if (array && at_ < tokens_.size() && tokens_[at_] == "]")
                        break;
                    n->children.append(node(depth + 1));
                } while (array && at_ < tokens_.size() && tokens_[at_] != "]");
                if (array)
                    expect("]");
            } else if (QStringList{"point", "vector", "coordIndex", "normalIndex"}.contains(field)) {
                expect("[");
                QVector<double> values;
                while (at_ < tokens_.size() && tokens_[at_] != "]")
                    values.append(number());
                expect("]");
                n->values.insert(field, values);
            } else {
                int arity = QStringList{"translation", "scale",        "center",        "bboxCenter",
                                        "bboxSize",    "diffuseColor", "specularColor", "emissiveColor"}
                                    .contains(field)
                                ? 3
                                : (field == "rotation" || field == "scaleOrientation" ? 4 : 1);
                QVector<double> values;
                for (int i = 0; i < arity; ++i)
                    values.append(number());
                n->values.insert(field, values);
            }
        }
        expect("}");
        if (!id.isEmpty())
            definitions_.insert(id, n);
        return n;
    }
};
/** Read a scene vector with its specification default.
 *
 * Defaults apply only to absent fields, because present fields have already
 * passed strict arity validation.
 */
QVector3D vector(const std::shared_ptr<Node> &n, const QString &field, QVector3D fallback = {}) {
    auto v = n->values.value(field);
    return v.isEmpty() ? fallback : QVector3D(float(v[0]), float(v[1]), float(v[2]));
}
/** Apply an axis-angle scene rotation.
 *
 * VRML uses radians; Qt receives degrees and normalizes the nonzero axis.
 */
void rotate(QMatrix4x4 &m, const QVector<double> &v, double sign = 1) {
    if (v.isEmpty() || v[3] == 0)
        return;
    QVector3D axis{float(v[0]), float(v[1]), float(v[2])};
    if (axis.lengthSquared() < 1e-12)
        reject("VRML rotation has a zero axis.");
    m.rotate(float(sign * v[3] * 180 / std::numbers::pi), axis);
}
/** Convert a parsed scene into placed triangles.
 *
 * Normals are recomputed after transforms; non-triangle faces are explicitly
 * unsupported in this milestone.
 */
void tessellate(const std::shared_ptr<Node> &n, QMatrix4x4 matrix, QVector<ModelVertex> &out,
                QStringList &diagnostics, const Cancellation &cancel, int depth = 0) {
    if (!n)
        return;
    if (depth > 128)
        reject("VRML scene depth limit exceeded.");
    if (cancel && cancel->load())
        reject("Model loading cancelled.");
    if (n->type == "Transform" || n->type == "Group") {
        if (n->type == "Transform") {
            QMatrix4x4 local;
            auto center = vector(n, "center");
            local.translate(vector(n, "translation"));
            local.translate(center);
            rotate(local, n->values.value("rotation"));
            rotate(local, n->values.value("scaleOrientation"));
            auto s = vector(n, "scale", {1, 1, 1});
            if (s.x() <= 0 || s.y() <= 0 || s.z() <= 0)
                reject("Unsupported non-positive VRML scale.");
            local.scale(s);
            rotate(local, n->values.value("scaleOrientation"), -1);
            local.translate(-center);
            matrix *= local;
        }
        for (const auto &c : n->children)
            tessellate(c, matrix, out, diagnostics, cancel, depth + 1);
        return;
    }
    if (n->type != "Shape")
        return;
    auto mesh = n->nodes.value("geometry");
    if (!mesh)
        return;
    if (mesh->type != "IndexedFaceSet")
        reject("Only IndexedFaceSet geometry is supported.");
    QVector3D colour{0.8f, 0.8f, 0.8f};
    auto appearance = n->nodes.value("appearance");
    if (appearance) {
        if (appearance->nodes.value("texture") || appearance->nodes.value("textureTransform"))
            reject("VRML textures are unsupported.");
        auto material = appearance->nodes.value("material");
        if (material) {
            colour = vector(material, "diffuseColor", colour);
            if (material->values.value("transparency", {0})[0] != 0)
                reject("Transparent VRML materials are unsupported.");
            auto emissive = vector(material, "emissiveColor");
            if (!emissive.isNull())
                diagnostics.append("Emissive material is approximated by diffuse lighting.");
        }
    }
    auto coord = mesh->nodes.value("coord");
    if (!coord || coord->type != "Coordinate")
        reject("IndexedFaceSet is missing Coordinate data.");
    auto points = coord->values.value("point");
    auto indices = mesh->values.value("coordIndex");
    if (points.size() % 3)
        reject("VRML point array is not a sequence of XYZ triples.");
    if (mesh->nodes.value("normal"))
        diagnostics.append("Explicit VRML normals are replaced by geometric face normals.");
    QVector<int> face;
    for (double index : indices) {
        if (cancel && cancel->load())
            reject("Model loading cancelled.");
        if (index == -1) {
            if (face.size() != 3)
                reject("Only triangle IndexedFaceSet faces are supported; found " +
                       QString::number(face.size()) + " vertices.");
            QVector3D p[3];
            for (int i = 0; i < 3; ++i) {
                int j = face[i] * 3;
                p[i] = matrix.map({float(points[j]), float(points[j + 1]), float(points[j + 2])});
            }
            if (mesh->values.value("ccw", {1})[0] == 0)
                std::swap(p[1], p[2]);
            auto normal = QVector3D::crossProduct(p[1] - p[0], p[2] - p[0]).normalized();
            if (!normal.isNull())
                for (const auto &v : p)
                    out.append({v.x(), v.y(), v.z(), normal.x(), normal.y(), normal.z(), colour.x(),
                                colour.y(), colour.z()});
            face.clear();
            if (out.size() > 3000000)
                reject("VRML triangle limit exceeded.");
        } else {
            if (index < 0 || index != std::floor(index) || index >= points.size() / 3)
                reject("VRML coordIndex outside coordinate array.");
            face.append(int(index));
        }
    }
    if (!face.isEmpty())
        reject("Unterminated VRML face: missing -1.");
}
} // namespace
/** Load and tessellate supported VRML97 geometry on the CPU.
 *
 * Lighting uses preserved diffuse colours and geometric normals; specular and
 * crease-angle shading are approximate.
 */
QVector<ModelVertex> loadVrml(const QString &path, const QMatrix4x4 &placement, QStringList &diagnostics,
                              const Cancellation &cancel) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        reject("Cannot read VRML: " + path + ": " + file.errorString());
    if (file.size() > 32 * 1024 * 1024)
        reject("VRML exceeds 32 MiB limit: " + path);
    auto text = QString::fromUtf8(file.readAll());
    if (!text.startsWith("#VRML V2.0 utf8"))
        reject("Unsupported VRML header; VRML97 V2.0 utf8 required: " + path);
    VrmlReader reader(text, cancel);
    QVector<ModelVertex> vertices;
    while (!reader.ended())
        tessellate(reader.node(), placement, vertices, diagnostics, cancel);
    if (vertices.isEmpty())
        reject("VRML contains no supported visible triangles: " + path);
    diagnostics.append("VRML diffuse colours retained; specular, ambient and "
                       "crease-angle shading use preview lighting.");
    QStringList notice;
    for (const auto &line : text.split('\n')) {
        const auto comment = line.trimmed();
        if (comment.isEmpty())
            continue;
        if (!comment.startsWith('#'))
            break;
        if (comment.contains("license", Qt::CaseInsensitive) ||
            comment.contains("Copyright", Qt::CaseInsensitive))
            notice.append(comment.mid(1).trimmed());
    }
    diagnostics.append(notice.isEmpty() ? "No embedded license statement found in " + path
                                        : "Embedded source/license notice: " + notice.join(" "));
    diagnostics.removeDuplicates();
    return vertices;
}
} // namespace hvd
