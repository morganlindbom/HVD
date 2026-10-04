// ComponentDefinition.h
#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QVector>
#include <optional>

namespace hvd {
struct Diagnostic {
    enum class Severity { Warning, Error };
    Severity severity = Severity::Error;
    QString path;
    QString message;
};
using Diagnostics = QVector<Diagnostic>;

struct ElectricalLimit {
    QString name;
    QString unit;
    std::optional<double> minimum;
    std::optional<double> maximum;
};
struct Pin {
    QString id;
    QString name;
    QString physicalNumber;
    QString gpioIdentifier;
    QString logicalSignal;
    QStringList capabilities;
    QVector<ElectricalLimit> electricalLimits;
};
struct Property {
    QString id;
    QString name;
    QString type;
    QJsonValue defaultValue = QJsonValue::Null;
    QJsonObject constraints;
};
struct SourceReference {
    QString title;
    QString uri;
    bool inspected = false;
};
struct ComponentDefinition {
    int schemaVersion = 1;
    QString id;
    int revision = 1;
    QString name;
    QString manufacturer;
    QString partNumber;
    QString description;
    QString category;
    QString kind = "sensor";
    QStringList tags;
    QVector<Pin> pins;
    QVector<ElectricalLimit> electricalLimits;
    QVector<Property> properties;
    QJsonObject assets;
    QVector<SourceReference> sources;
    QString verificationStatus = "unverified";
    QJsonObject extensions;
};

/** List the supported component kinds.
 *
 * These stable strings form part of package schema version 1.
 */
QStringList componentKinds();
/** List the supported pin capabilities.
 *
 * Capabilities describe support and never allocate hardware resources.
 */
QStringList pinCapabilities();
/** Detect blocking diagnostics.
 *
 * Warnings such as missing assets do not invalidate a definition.
 */
bool hasErrors(const Diagnostics &diagnostics);
/** Format diagnostic messages for presentation.
 *
 * Paths identify the offending field or package so users can correct it.
 */
QString diagnosticText(const Diagnostics &diagnostics);
/** Validate a definition without mutating it.
 *
 * Enforces identifiers, supported vocabulary, property constraints and finite electrical values.
 */
Diagnostics validateDefinition(const ComponentDefinition &definition);
/** Serialize a definition with explicit unknown values.
 *
 * Null represents an unknown electrical bound or property default; extensions are preserved.
 */
QJsonObject definitionToJson(const ComponentDefinition &definition);
/** Decode and validate schema version 1.
 *
 * The output is assigned only when structural and semantic validation succeed.
 */
bool definitionFromJson(const QJsonObject &json, ComponentDefinition &output, Diagnostics &diagnostics);
} // namespace hvd
