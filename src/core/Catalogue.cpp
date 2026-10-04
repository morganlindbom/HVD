// Catalogue.cpp
#include "Catalogue.h"
#include <QDir>
#include <QFileInfo>
#include <QUuid>

namespace hvd {
/** Look up an immutable catalogue entry by stable ID.
 *
 * Returns null for an unresolved reference; callers must not retain pointers across catalogue changes.
 */
const ComponentPackage *Catalogue::find(const QString &id) const {
    const auto it = entries_.constFind(id);
    return it == entries_.cend() ? nullptr : &it.value();
}
/** Search identity metadata with optional exact filters.
 *
 * Matches name, manufacturer, part number and tags case-insensitively without altering catalogue data.
 */
QVector<ComponentPackage> Catalogue::search(const QString &query, const QString &category,
                                            const QString &kind) const {
    QVector<ComponentPackage> result;
    for (const auto &entry : entries_) {
        const auto &d = entry.definition;
        const QString searchable =
            d.name + '\n' + d.manufacturer + '\n' + d.partNumber + '\n' + d.tags.join('\n');
        if (searchable.contains(query.trimmed(), Qt::CaseInsensitive) &&
            (category.isEmpty() || d.category == category) && (kind.isEmpty() || d.kind == kind))
            result.append(entry);
    }
    return result;
}
/** Register a validated entry without replacing an existing ID.
 *
 * User packages cannot shadow bundled entries even if their revisions differ.
 */
bool Catalogue::registerPackage(const ComponentPackage &package, Diagnostics &out) {
    out += validateDefinition(package.definition);
    for (auto it = package.definition.assets.begin(); it != package.definition.assets.end(); ++it)
        if (!it.value().isNull())
            PackageStore::resolveAsset(package.directory, it.value().toString(), out);
    if (entries_.contains(package.definition.id))
        out.append({Diagnostic::Severity::Error, "id", "Duplicate component ID: " + package.definition.id});
    if (hasErrors(out))
        return false;
    entries_.insert(package.definition.id, package);
    return true;
}
/** Reload bundled and user package roots transactionally.
 *
 * Any invalid package leaves the previous catalogue intact; absent user roots are initially empty.
 */
bool Catalogue::load(const QString &bundledRoot, const QString &userRoot, Diagnostics &out) {
    Catalogue candidate;
    const QString bundledPath = QDir::cleanPath(QFileInfo(bundledRoot).absoluteFilePath());
    const QString userPath = QDir::cleanPath(QFileInfo(userRoot).absoluteFilePath());
    if (bundledPath.compare(userPath, Qt::CaseInsensitive) == 0 ||
        userPath.startsWith(bundledPath + '/', Qt::CaseInsensitive) ||
        bundledPath.startsWith(userPath + '/', Qt::CaseInsensitive)) {
        out.append({Diagnostic::Severity::Error, userRoot,
                    "Bundled and user roots must be separate, nonoverlapping directories."});
        return false;
    }
    for (const auto &root : {bundledRoot, userRoot}) {
        const bool bundled = root == bundledRoot;
        if (!QFileInfo(root).isDir()) {
            if (bundled || QFileInfo::exists(root))
                out.append(
                    {Diagnostic::Severity::Error, root, "Catalogue root is missing or is not a directory."});
            continue;
        }
        for (const auto &directory :
             QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            ComponentPackage package;
            Diagnostics local;
            if (directory.isSymLink()) {
                local.append({Diagnostic::Severity::Error, directory.filePath(),
                              "Package directories must not be symlinks."});
            } else if (PackageStore::load(directory.filePath(), bundled, package, local)) {
                candidate.registerPackage(package, local);
            }
            for (auto diagnostic : local) {
                diagnostic.path = directory.fileName() + "/" + diagnostic.path;
                out.append(diagnostic);
            }
        }
    }
    if (hasErrors(out))
        return false;
    entries_ = candidate.entries_;
    return true;
}
/** Save a user definition and update the catalogue after persistence succeeds.
 *
 * A null source creates a new package; edits preserve identity, increment revision and use atomic writes.
 */
bool Catalogue::saveUser(const ComponentDefinition &definition, const QString &userRoot,
                         const QString &existingId, Diagnostics &out, const QString &assetSourceId) {
    out += validateDefinition(definition);
    if (hasErrors(out))
        return false;
    if (existingId.isEmpty() && entries_.contains(definition.id)) {
        out.append({Diagnostic::Severity::Error, "id", "Duplicate component ID: " + definition.id});
        return false;
    }
    ComponentPackage saved;
    if (!existingId.isEmpty()) {
        const auto *previous = find(existingId);
        if (!previous || previous->bundled || previous->definition.id != definition.id) {
            out.append({Diagnostic::Severity::Error, existingId,
                        "Only an existing user component with unchanged ID may be edited."});
            return false;
        }
        saved = *previous;
        saved.definition = definition;
        if (!PackageStore::save(saved, out))
            return false;
    } else {
        if (!QDir().mkpath(userRoot)) {
            out.append({Diagnostic::Severity::Error, userRoot, "Cannot create user catalogue directory."});
            return false;
        }
        // A valid empty source directory supports new definitions with unknown or missing assets.
        ComponentPackage source{definition, userRoot, false};
        if (!assetSourceId.isEmpty()) {
            const auto *assetSource = find(assetSourceId);
            if (!assetSource) {
                out.append({Diagnostic::Severity::Error, assetSourceId, "Copy source is unresolved."});
                return false;
            }
            source.directory = assetSource->directory;
        }
        const QString destination =
            QDir(userRoot).filePath("component-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
        if (!PackageStore::copy(source, destination, saved, out))
            return false;
    }
    entries_.insert(definition.id, saved);
    return true;
}
/** Import a package into the user root without overwriting an ID.
 *
 * Validates and stages all referenced files before exposing the new catalogue entry.
 */
bool Catalogue::importPackage(const QString &sourceDirectory, const QString &userRoot, Diagnostics &out) {
    ComponentPackage source, saved;
    if (!PackageStore::load(sourceDirectory, false, source, out))
        return false;
    if (entries_.contains(source.definition.id)) {
        out.append({Diagnostic::Severity::Error, "id", "Duplicate component ID: " + source.definition.id});
        return false;
    }
    if (!QDir().mkpath(userRoot)) {
        out.append({Diagnostic::Severity::Error, userRoot, "Cannot create user catalogue directory."});
        return false;
    }
    const QString destination =
        QDir(userRoot).filePath("component-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!PackageStore::copy(source, destination, saved, out))
        return false;
    entries_.insert(saved.definition.id, saved);
    return true;
}
/** Export a registered package to a new directory.
 *
 * Bundled and user packages use the same portable format and destination safety checks.
 */
bool Catalogue::exportPackage(const QString &id, const QString &destination, Diagnostics &out) const {
    const auto *entry = find(id);
    if (!entry) {
        out.append({Diagnostic::Severity::Error, id, "Unresolved component ID."});
        return false;
    }
    ComponentPackage exported;
    return PackageStore::copy(*entry, destination, exported, out);
}
} // namespace hvd
