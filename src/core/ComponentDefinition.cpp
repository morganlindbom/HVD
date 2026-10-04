// ComponentDefinition.cpp
#include "ComponentDefinition.h"
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <cmath>
#include <limits>

namespace hvd {
namespace {
/** Append a field-specific validation error.
 *
 * Centralizes actionable diagnostics without throwing away other failures.
 */
void error(Diagnostics &out, const QString &path, const QString &message) {
    out.append({Diagnostic::Severity::Error, path, message});
}
/** Check a stable portable identifier.
 *
 * Identifiers are restricted to ASCII characters suitable for directory names and cross-application
 * references.
 */
bool validId(const QString &id) {
    static const QRegularExpression pattern("^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$");
    return pattern.match(id).hasMatch() && !id.endsWith('.');
}
/** Validate finite ordered electrical bounds.
 *
 * Unknown bounds remain optional; a named quantity always carries an explicit unit.
 */
void validateLimits(const QVector<ElectricalLimit> &limits, const QString &path, Diagnostics &out) {
    QSet<QString> names;
    for (const auto &limit : limits) {
        if (limit.name.trimmed().isEmpty() || limit.unit.trimmed().isEmpty())
            error(out, path, "Electrical limits require a name and explicit unit.");
        if (names.contains(limit.name))
            error(out, path, "Duplicate electrical limit: " + limit.name);
        names.insert(limit.name);
        if ((limit.minimum && !std::isfinite(*limit.minimum)) ||
            (limit.maximum && !std::isfinite(*limit.maximum)))
            error(out, path, "Electrical bounds must be finite or null (unknown).");
        if (limit.minimum && limit.maximum && *limit.minimum > *limit.maximum)
            error(out, path, "Electrical minimum exceeds maximum.");
    }
}
/** Match a known default to its declared property type.
 *
 * Null is explicitly unknown and is accepted for all property types.
 */
bool matchesType(const QJsonValue &value, const QString &type) {
    if (value.isNull())
        return true;
    if (type == "string")
        return value.isString();
    if (type == "boolean")
        return value.isBool();
    if (type == "number")
        return value.isDouble() && std::isfinite(value.toDouble());
    if (type == "integer")
        return value.isDouble() && std::isfinite(value.toDouble()) &&
               std::floor(value.toDouble()) == value.toDouble();
    return false;
}
/** Encode electrical quantities.
 *
 * Missing numeric bounds are written as JSON null instead of zero.
 */
QJsonArray limitsToJson(const QVector<ElectricalLimit> &limits) {
    QJsonArray result;
    for (const auto &limit : limits)
        result.append(
            QJsonObject{{"name", limit.name},
                        {"unit", limit.unit},
                        {"minimum", limit.minimum ? QJsonValue(*limit.minimum) : QJsonValue::Null},
                        {"maximum", limit.maximum ? QJsonValue(*limit.maximum) : QJsonValue::Null}});
    return result;
}
/** Require an array when a collection field is supplied.
 *
 * Missing optional collections default to empty; incorrect JSON types are rejected.
 */
QJsonArray arrayField(const QJsonObject &object, const QString &key, Diagnostics &out,
                      const QString &prefix = {}) {
    if (object.contains(key) && !object.value(key).isArray())
        error(out, prefix + key, "Expected an array.");
    return object.value(key).toArray();
}
/** Read an optional or required string field.
 *
 * Optional null strings denote unknown metadata; required identity strings must be supplied.
 */
QString stringField(const QJsonObject &object, const QString &key, Diagnostics &out,
                    const QString &prefix = {}, bool required = false) {
    const auto value = object.value(key);
    if ((required || (!value.isUndefined() && !value.isNull())) && !value.isString())
        error(out, prefix + key, "Expected a string.");
    return value.toString();
}
/** Decode a collection of strings.
 *
 * Non-string entries are diagnosed instead of being silently coerced.
 */
QStringList stringsField(const QJsonObject &object, const QString &key, Diagnostics &out,
                         const QString &prefix = {}) {
    QStringList result;
    for (const auto &value : arrayField(object, key, out, prefix)) {
        if (!value.isString())
            error(out, prefix + key, "All entries must be strings.");
        else
            result.append(value.toString());
    }
    return result;
}
/** Decode optional electrical quantities.
 *
 * Bounds are numbers or null; an omitted bound remains unknown.
 */
QVector<ElectricalLimit> limitsFromJson(const QJsonObject &object, Diagnostics &out, const QString &prefix) {
    QVector<ElectricalLimit> result;
    for (const auto &value : arrayField(object, "electricalLimits", out, prefix)) {
        if (!value.isObject()) {
            error(out, prefix + "electricalLimits", "Expected a limit object.");
            continue;
        }
        const auto entry = value.toObject();
        ElectricalLimit limit;
        limit.name = stringField(entry, "name", out, prefix, true);
        limit.unit = stringField(entry, "unit", out, prefix, true);
        for (const auto &key : {QString("minimum"), QString("maximum")}) {
            const auto bound = entry.value(key);
            if (bound.isUndefined() || bound.isNull())
                continue;
            if (!bound.isDouble())
                error(out, prefix + key, "Expected a number or null (unknown).");
            else if (key == "minimum")
                limit.minimum = bound.toDouble();
            else
                limit.maximum = bound.toDouble();
        }
        result.append(limit);
    }
    return result;
}
/** Read a positive schema or revision integer.
 *
 * Fractional, negative and overflowing values cannot be used as version references.
 */
int integerField(const QJsonObject &json, const QString &key, Diagnostics &out) {
    const auto value = json.value(key);
    const double number = value.toDouble(-1);
    if (!value.isDouble() || number < 1 || number > std::numeric_limits<int>::max() ||
        std::floor(number) != number) {
        error(out, key, "Expected a positive integer.");
        return 0;
    }
    return static_cast<int>(number);
}
} // namespace

/** List the supported component kinds.
 *
 * These stable strings form part of package schema version 1.
 */
QStringList componentKinds() {
    return {"board", "microcontroller", "sensor", "actuator", "driver", "passive", "connector"};
}
/** List the supported pin capabilities.
 *
 * Capabilities describe support and never allocate hardware resources.
 */
QStringList pinCapabilities() {
    return {"digital-input", "digital-output", "analog-input", "pwm", "uart", "spi", "i2c",
            "pio",           "power",          "ground"};
}
/** Detect blocking diagnostics.
 *
 * Warnings such as missing assets do not invalidate a definition.
 */
bool hasErrors(const Diagnostics &diagnostics) {
    for (const auto &item : diagnostics)
        if (item.severity == Diagnostic::Severity::Error)
            return true;
    return false;
}
/** Format diagnostic messages for presentation.
 *
 * Paths identify the offending field or package so users can correct it.
 */
QString diagnosticText(const Diagnostics &diagnostics) {
    QStringList lines;
    for (const auto &item : diagnostics)
        lines.append((item.severity == Diagnostic::Severity::Error ? "Error: " : "Warning: ") + item.path +
                     ": " + item.message);
    lines.removeDuplicates();
    return lines.join('\n');
}
/** Validate a definition without mutating it.
 *
 * Enforces identifiers, supported vocabulary, property constraints and finite electrical values.
 */
Diagnostics validateDefinition(const ComponentDefinition &d) {
    Diagnostics out;
    if (d.schemaVersion != 1)
        error(out, "schemaVersion", "Unsupported schema; supported version is 1.");
    if (!validId(d.id))
        error(out, "id", "Use 1-128 ASCII letters, digits, dots, hyphens or underscores.");
    if (d.revision < 1)
        error(out, "revision", "Revision must be a positive integer.");
    if (d.name.trimmed().isEmpty())
        error(out, "name", "A component name is required.");
    if (d.category.trimmed().isEmpty())
        error(out, "category", "A category is required.");
    if (!componentKinds().contains(d.kind))
        error(out, "kind", "Unknown component kind: " + d.kind);
    if (!QStringList{"unverified", "demonstration", "source-reviewed"}.contains(d.verificationStatus))
        error(out, "verificationStatus", "Use unverified, demonstration or source-reviewed.");
    if (d.verificationStatus == "source-reviewed") {
        bool inspected = false;
        for (const auto &source : d.sources)
            inspected |= source.inspected;
        if (!inspected)
            error(out, "sources", "Source-reviewed requires an inspected source reference.");
    }
    for (const auto &source : d.sources) {
        const QUrl uri(source.uri);
        if (source.title.trimmed().isEmpty() || !uri.isValid() ||
            !QStringList{"https", "http"}.contains(uri.scheme()) || uri.host().isEmpty())
            error(out, "sources",
                  "Sources require a title and absolute HTTP(S) URI; local documents belong in assets.");
    }
    QSet<QString> pinIds;
    for (const auto &pin : d.pins) {
        const QString path = "pins/" + pin.id;
        if (!validId(pin.id) || pin.name.trimmed().isEmpty())
            error(out, path, "Pin requires a stable ID and name.");
        if (pinIds.contains(pin.id))
            error(out, path, "Duplicate pin ID: " + pin.id);
        pinIds.insert(pin.id);
        for (const auto &capability : pin.capabilities)
            if (!pinCapabilities().contains(capability))
                error(out, path, "Unknown capability: " + capability);
        validateLimits(pin.electricalLimits, path + "/electricalLimits", out);
    }
    validateLimits(d.electricalLimits, "electricalLimits", out);
    QSet<QString> propertyIds;
    for (const auto &p : d.properties) {
        const QString path = "properties/" + p.id;
        if (!validId(p.id) || p.name.trimmed().isEmpty())
            error(out, path, "Property requires a stable ID and name.");
        if (propertyIds.contains(p.id))
            error(out, path, "Duplicate property ID.");
        propertyIds.insert(p.id);
        if (!QStringList{"string", "boolean", "number", "integer"}.contains(p.type))
            error(out, path, "Unsupported property type.");
        if (!matchesType(p.defaultValue, p.type))
            error(out, path, "Default does not match property type.");
        for (auto it = p.constraints.begin(); it != p.constraints.end(); ++it) {
            if (!QStringList{"minimum", "maximum", "choices", "pattern"}.contains(it.key()))
                error(out, path, "Unsupported constraint: " + it.key());
            if (it.key() == "minimum" || it.key() == "maximum") {
                if ((p.type != "number" && p.type != "integer") || !it.value().isDouble() ||
                    !std::isfinite(it.value().toDouble()))
                    error(out, path, "Numeric bounds require a numeric property.");
            }
        }
        const auto min = p.constraints.value("minimum"), max = p.constraints.value("maximum");
        if (min.isDouble() && max.isDouble() && min.toDouble() > max.toDouble())
            error(out, path, "Minimum exceeds maximum.");
        if (!p.defaultValue.isNull() && p.defaultValue.isDouble() &&
            ((min.isDouble() && p.defaultValue.toDouble() < min.toDouble()) ||
             (max.isDouble() && p.defaultValue.toDouble() > max.toDouble())))
            error(out, path, "Default is outside numeric bounds.");
        if (p.constraints.contains("choices")) {
            const auto choices = p.constraints.value("choices");
            if (!choices.isArray() || choices.toArray().isEmpty())
                error(out, path, "Choices must be a nonempty array.");
            for (const auto &choice : choices.toArray())
                if (choice.isNull() || !matchesType(choice, p.type))
                    error(out, path, "Choice does not match type.");
            if (!p.defaultValue.isNull() && !choices.toArray().contains(p.defaultValue))
                error(out, path, "Default is not an allowed choice.");
        }
        if (p.constraints.contains("pattern")) {
            const auto pattern = p.constraints.value("pattern");
            QRegularExpression regex(pattern.toString());
            if (p.type != "string" || !pattern.isString() || !regex.isValid())
                error(out, path, "Pattern must be a valid regular expression for a string property.");
            else if (!p.defaultValue.isNull() && !regex.match(p.defaultValue.toString()).hasMatch())
                error(out, path, "Default does not match pattern.");
        }
    }
    for (auto it = d.assets.begin(); it != d.assets.end(); ++it) {
        if (!QStringList{"datasheet", "image", "symbol", "footprint", "model3d"}.contains(it.key()))
            error(out, "assets/" + it.key(), "Unsupported asset role; use extensions for future metadata.");
        if (!it.value().isNull() && (!it.value().isString() || it.value().toString().isEmpty()))
            error(out, "assets/" + it.key(), "Asset reference must be a relative path or null.");
        if (it.value().isString()) {
            const QString path = it.value().toString();
            bool safe = !QDir::isAbsolutePath(path) && !path.contains(':') && !path.contains('\\');
            for (const auto &part : path.split('/'))
                safe &= !part.isEmpty() && part != "." && part != ".." && !part.endsWith('.') &&
                        !part.endsWith(' ');
            if (!safe)
                error(out, "assets/" + it.key(),
                      "Asset paths must stay relative to their package without traversal.");
        }
    }
    return out;
}
/** Serialize a definition with explicit unknown values.
 *
 * Null represents an unknown electrical bound or property default; extensions are preserved.
 */
QJsonObject definitionToJson(const ComponentDefinition &d) {
    QJsonArray pins, properties, sources;
    for (const auto &pin : d.pins)
        pins.append(QJsonObject{{"id", pin.id},
                                {"name", pin.name},
                                {"physicalNumber", pin.physicalNumber},
                                {"gpioIdentifier", pin.gpioIdentifier},
                                {"logicalSignal", pin.logicalSignal},
                                {"capabilities", QJsonArray::fromStringList(pin.capabilities)},
                                {"electricalLimits", limitsToJson(pin.electricalLimits)}});
    for (const auto &p : d.properties)
        properties.append(QJsonObject{{"id", p.id},
                                      {"name", p.name},
                                      {"type", p.type},
                                      {"default", p.defaultValue},
                                      {"constraints", p.constraints}});
    for (const auto &s : d.sources)
        sources.append(QJsonObject{{"title", s.title}, {"uri", s.uri}, {"inspected", s.inspected}});
    return {{"schemaVersion", d.schemaVersion},
            {"id", d.id},
            {"revision", d.revision},
            {"name", d.name},
            {"manufacturer", d.manufacturer},
            {"partNumber", d.partNumber},
            {"description", d.description},
            {"category", d.category},
            {"kind", d.kind},
            {"tags", QJsonArray::fromStringList(d.tags)},
            {"pins", pins},
            {"properties", properties},
            {"electricalLimits", limitsToJson(d.electricalLimits)},
            {"assets", d.assets},
            {"sources", sources},
            {"verificationStatus", d.verificationStatus},
            {"extensions", d.extensions}};
}
/** Decode and validate schema version 1.
 *
 * The output is assigned only when structural and semantic validation succeed.
 */
bool definitionFromJson(const QJsonObject &json, ComponentDefinition &output, Diagnostics &out) {
    ComponentDefinition d;
    d.schemaVersion = integerField(json, "schemaVersion", out);
    d.revision = integerField(json, "revision", out);
    d.id = stringField(json, "id", out, {}, true);
    d.name = stringField(json, "name", out, {}, true);
    d.manufacturer = stringField(json, "manufacturer", out);
    d.partNumber = stringField(json, "partNumber", out);
    d.description = stringField(json, "description", out);
    d.category = stringField(json, "category", out, {}, true);
    d.kind = stringField(json, "kind", out, {}, true);
    d.verificationStatus = stringField(json, "verificationStatus", out, {}, true);
    d.tags = stringsField(json, "tags", out);
    d.electricalLimits = limitsFromJson(json, out, {});
    for (const auto &value : arrayField(json, "pins", out)) {
        if (!value.isObject()) {
            error(out, "pins", "Expected a pin object.");
            continue;
        }
        const auto o = value.toObject();
        Pin pin;
        pin.id = stringField(o, "id", out, "pins/", true);
        pin.name = stringField(o, "name", out, "pins/", true);
        pin.physicalNumber = stringField(o, "physicalNumber", out, "pins/");
        pin.gpioIdentifier = stringField(o, "gpioIdentifier", out, "pins/");
        pin.logicalSignal = stringField(o, "logicalSignal", out, "pins/");
        pin.capabilities = stringsField(o, "capabilities", out, "pins/");
        pin.electricalLimits = limitsFromJson(o, out, "pins/");
        d.pins.append(pin);
    }
    for (const auto &value : arrayField(json, "properties", out)) {
        if (!value.isObject()) {
            error(out, "properties", "Expected a property object.");
            continue;
        }
        const auto o = value.toObject();
        Property p;
        p.id = stringField(o, "id", out, "properties/", true);
        p.name = stringField(o, "name", out, "properties/", true);
        p.type = stringField(o, "type", out, "properties/", true);
        p.defaultValue = o.contains("default") ? o.value("default") : QJsonValue::Null;
        if (o.contains("constraints") && !o.value("constraints").isObject())
            error(out, "properties/constraints", "Expected an object.");
        p.constraints = o.value("constraints").toObject();
        d.properties.append(p);
    }
    for (const auto &value : arrayField(json, "sources", out)) {
        if (!value.isObject()) {
            error(out, "sources", "Expected a source object.");
            continue;
        }
        const auto o = value.toObject();
        if (!o.value("inspected").isBool())
            error(out, "sources/inspected", "Expected an explicit boolean.");
        d.sources.append({stringField(o, "title", out, "sources/", true),
                          stringField(o, "uri", out, "sources/", true), o.value("inspected").toBool()});
    }
    for (const auto &key : {QString("assets"), QString("extensions")}) {
        if (json.contains(key) && !json.value(key).isObject())
            error(out, key, "Expected an object.");
    }
    d.assets = json.value("assets").toObject();
    d.extensions = json.value("extensions").toObject();
    // Reject accidental fields instead of silently losing them during an edit.
    const auto supported = definitionToJson(d).keys();
    for (auto it = json.begin(); it != json.end(); ++it)
        if (!supported.contains(it.key()))
            error(out, it.key(), "Unknown field; place future metadata in extensions.");
    out += validateDefinition(d);
    if (hasErrors(out))
        return false;
    output = d;
    return true;
}
} // namespace hvd
