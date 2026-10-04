// ComponentPackage.cpp
#include "ComponentPackage.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>

namespace hvd {
namespace {
/** Append a package operation failure.
 *
 * Includes the filesystem path to make permission and malformed-package failures actionable.
 */
void failure(Diagnostics &out, const QString &path, const QString &message) {
    out.append({Diagnostic::Severity::Error, path, message});
}
/** Check canonical containment for an existing ancestor.
 *
 * Windows path comparisons ignore case; canonical paths expose symlink targets.
 */
bool contained(const QString &root, const QString &candidate) {
#ifdef Q_OS_WIN
    constexpr auto sensitivity = Qt::CaseInsensitive;
#else
    constexpr auto sensitivity = Qt::CaseSensitive;
#endif
    return candidate.compare(root, sensitivity) == 0 || candidate.startsWith(root + '/', sensitivity);
}
/** Validate all references without requiring asset availability.
 *
 * Missing files remain visible warnings while unsafe paths prevent registration and saving.
 */
void checkAssets(const ComponentPackage &package, Diagnostics &out) {
    for (auto it = package.definition.assets.begin(); it != package.definition.assets.end(); ++it) {
        if (it.value().isNull())
            continue;
        const QString path = PackageStore::resolveAsset(package.directory, it.value().toString(), out);
        if (!path.isEmpty() && !QFileInfo(path).isFile())
            out.append({Diagnostic::Severity::Warning, "assets/" + it.key(),
                        "Missing asset: " + it.value().toString()});
    }
}
} // namespace

/** Resolve an asset inside its owning package.
 *
 * Rejects absolute, traversal, alias and symlink escape paths even when the final file is missing.
 */
QString PackageStore::resolveAsset(const QString &directory, const QString &relative, Diagnostics &out) {
    const QString root = QFileInfo(directory).canonicalFilePath();
    const QStringList parts = relative.split('/');
    bool safe = !root.isEmpty() && !relative.isEmpty() && !QDir::isAbsolutePath(relative) &&
                !relative.contains('\\') && !relative.contains(':');
    static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])($|\\.)",
                                             QRegularExpression::CaseInsensitiveOption);
    for (const auto &part : parts)
        safe &= !part.isEmpty() && part != "." && part != ".." && !part.endsWith('.') &&
                !part.endsWith(' ') && !part.contains(QRegularExpression("[<>\"|?*\\x00-\\x1f]")) &&
                !reserved.match(part).hasMatch();
    if (!safe) {
        failure(out, "assets/" + relative,
                "Use a relative package path without traversal or platform aliases.");
        return {};
    }
    const QString result = QDir(root).filePath(relative);
    QFileInfo ancestor(result);
    while (!ancestor.exists() && !ancestor.isSymLink() &&
           ancestor.absoluteFilePath() != ancestor.absolutePath())
        ancestor.setFile(ancestor.absolutePath());
    if (ancestor.canonicalFilePath().isEmpty() || !contained(root, ancestor.canonicalFilePath())) {
        failure(out, "assets/" + relative, "Asset path escapes the package or follows a broken symlink.");
        return {};
    }
    if (relative == "component.json") {
        failure(out, "assets/" + relative, "Definition file cannot also be an asset.");
        return {};
    }
    return result;
}
/** Load a validated component directory.
 *
 * Invalid JSON or references preserve the caller's output; missing assets produce warnings.
 */
bool PackageStore::load(const QString &directory, bool bundled, ComponentPackage &output, Diagnostics &out) {
    const QString filePath = QDir(directory).filePath("component.json");
    QFileInfo info(filePath);
    if (info.isSymLink() || !contained(QFileInfo(directory).canonicalFilePath(), info.canonicalFilePath())) {
        failure(out, filePath, "Definition must be a regular file inside its package.");
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        failure(out, filePath, file.errorString());
        return false;
    }
    if (file.size() > 4 * 1024 * 1024) {
        failure(out, filePath, "Definition exceeds the 4 MiB limit.");
        return false;
    }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        failure(out, filePath,
                "Invalid JSON object at offset " + QString::number(parse.offset) + ": " +
                    parse.errorString());
        return false;
    }
    ComponentPackage candidate;
    candidate.directory = QFileInfo(directory).canonicalFilePath();
    candidate.bundled = bundled;
    if (!definitionFromJson(document.object(), candidate.definition, out))
        return false;
    checkAssets(candidate, out);
    if (hasErrors(out))
        return false;
    output = candidate;
    return true;
}
/** Save metadata atomically inside a user package.
 *
 * Existing IDs are immutable and changed content requires a newer revision; bundled data is read-only.
 */
bool PackageStore::save(const ComponentPackage &package, Diagnostics &out) {
    out += validateDefinition(package.definition);
    if (package.bundled)
        failure(out, package.directory, "Bundled components are read-only; create a copy with a new ID.");
    if (!QFileInfo(package.directory).isDir())
        failure(out, package.directory, "Package directory must exist before saving.");
    if (hasErrors(out))
        return false;
    checkAssets(package, out);
    if (hasErrors(out))
        return false;
    const QString filePath = QDir(package.directory).filePath("component.json");
    if (QFileInfo::exists(filePath)) {
        ComponentPackage previous;
        Diagnostics previousDiagnostics;
        if (!load(package.directory, false, previous, previousDiagnostics)) {
            out += previousDiagnostics;
            return false;
        }
        if (previous.definition.id != package.definition.id)
            failure(out, "id", "An existing package ID cannot be changed; create a new package.");
        const auto oldJson = definitionToJson(previous.definition),
                   newJson = definitionToJson(package.definition);
        if (newJson != oldJson && package.definition.revision <= previous.definition.revision)
            failure(out, "revision",
                    "Changed definitions require a revision greater than the saved revision.");
    }
    if (hasErrors(out))
        return false;
    QSaveFile file(filePath);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        failure(out, filePath, file.errorString());
        return false;
    }
    const QByteArray data =
        QJsonDocument(definitionToJson(package.definition)).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        failure(out, filePath, file.errorString());
        return false;
    }
    return true;
}
/** Publish a package copy at a new destination.
 *
 * Stages metadata and referenced assets before an atomic directory rename; never overwrites a destination.
 */
bool PackageStore::copy(const ComponentPackage &source, const QString &destination, ComponentPackage &output,
                        Diagnostics &out) {
    out += validateDefinition(source.definition);
    checkAssets(source, out);
    if (hasErrors(out))
        return false;
    const QFileInfo target(destination);
    if (target.exists() || target.isSymLink()) {
        failure(out, destination, "Destination already exists; choose a new directory.");
        return false;
    }
    if (!QFileInfo(target.absolutePath()).isDir()) {
        failure(out, destination, "Destination parent directory must exist.");
        return false;
    }
    QTemporaryDir staging(QDir(target.absolutePath()).filePath(".hvd-stage-XXXXXX"));
    if (!staging.isValid()) {
        failure(out, destination, "Cannot create staging directory.");
        return false;
    }
    ComponentPackage staged{source.definition, staging.path(), false};
    for (auto it = source.definition.assets.begin(); it != source.definition.assets.end(); ++it) {
        if (it.value().isNull())
            continue;
        Diagnostics paths;
        const QString from = resolveAsset(source.directory, it.value().toString(), paths);
        const QString to = resolveAsset(staging.path(), it.value().toString(), paths);
        out += paths;
        if (hasErrors(out))
            return false;
        if (!QFileInfo(from).isFile())
            continue;
        if (QFileInfo::exists(to))
            continue; // Multiple roles may share one asset.
        if (!QDir().mkpath(QFileInfo(to).absolutePath()) || !QFile::copy(from, to)) {
            failure(out, it.value().toString(), "Cannot copy referenced asset.");
            return false;
        }
    }
    if (!save(staged, out))
        return false;
    ComponentPackage checked;
    Diagnostics validation;
    if (!load(staging.path(), false, checked, validation)) {
        out += validation;
        return false;
    }
    if (!QDir().rename(staging.path(), target.absoluteFilePath())) {
        failure(out, destination, "Cannot publish staged package; destination remains unchanged.");
        return false;
    }
    staging.setAutoRemove(false);
    output = {source.definition, target.absoluteFilePath(), false};
    return true;
}
} // namespace hvd
