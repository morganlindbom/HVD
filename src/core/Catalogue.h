// Catalogue.h
#pragma once
#include "storage/ComponentPackage.h"
#include <QMap>

namespace hvd {
class Catalogue {
  public:
    /** Look up an immutable catalogue entry by stable ID.
     *
     * Returns null for an unresolved reference; callers must not retain pointers across catalogue changes.
     */
    const ComponentPackage *find(const QString &id) const;
    /** Search identity metadata with optional exact filters.
     *
     * Matches name, manufacturer, part number and tags case-insensitively without altering catalogue data.
     */
    QVector<ComponentPackage> search(const QString &query = {}, const QString &category = {},
                                     const QString &kind = {}) const;
    /** Register a validated entry without replacing an existing ID.
     *
     * User packages cannot shadow bundled entries even if their revisions differ.
     */
    bool registerPackage(const ComponentPackage &package, Diagnostics &out);
    /** Reload bundled and user package roots transactionally.
     *
     * Any invalid package leaves the previous catalogue intact; absent user roots are initially empty.
     */
    bool load(const QString &bundledRoot, const QString &userRoot, Diagnostics &out);
    /** Save a user definition and update the catalogue after persistence succeeds.
     *
     * A null source creates a new package; edits preserve identity, increment revision and use atomic writes.
     */
    bool saveUser(const ComponentDefinition &definition, const QString &userRoot, const QString &existingId,
                  Diagnostics &out, const QString &assetSourceId = {});
    /** Import a package into the user root without overwriting an ID.
     *
     * Validates and stages all referenced files before exposing the new catalogue entry.
     */
    bool importPackage(const QString &sourceDirectory, const QString &userRoot, Diagnostics &out);
    /** Export a registered package to a new directory.
     *
     * Bundled and user packages use the same portable format and destination safety checks.
     */
    bool exportPackage(const QString &id, const QString &destination, Diagnostics &out) const;

  private:
    QMap<QString, ComponentPackage> entries_;
};
} // namespace hvd
