// ComponentPackage.h
#pragma once
#include "core/ComponentDefinition.h"

namespace hvd {
struct ComponentPackage {
    ComponentDefinition definition;
    QString directory;
    bool bundled = false;
};
class PackageStore {
  public:
    /** Resolve an asset inside its owning package.
     *
     * Rejects absolute, traversal, alias and symlink escape paths even when the final file is missing.
     */
    static QString resolveAsset(const QString &directory, const QString &relative, Diagnostics &out);
    /** Load a validated component directory.
     *
     * Invalid JSON or references preserve the caller's output; missing assets produce warnings.
     */
    static bool load(const QString &directory, bool bundled, ComponentPackage &output, Diagnostics &out);
    /** Save metadata atomically inside a user package.
     *
     * Existing IDs are immutable and changed content requires a newer revision; bundled data is read-only.
     */
    static bool save(const ComponentPackage &package, Diagnostics &out);
    /** Publish a package copy at a new destination.
     *
     * Stages metadata and referenced assets before an atomic directory rename; never overwrites a
     * destination.
     */
    static bool copy(const ComponentPackage &source, const QString &destination, ComponentPackage &output,
                     Diagnostics &out);
};
} // namespace hvd
