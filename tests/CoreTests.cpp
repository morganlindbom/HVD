// CoreTests.cpp
#include "core/Catalogue.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>
#include <filesystem>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
/** Create an explicitly synthetic valid definition.
 *
 * Test data makes no manufacturer or electrical compatibility claims.
 */
hvd::ComponentDefinition example(const QString &id = "test.component") {
    hvd::ComponentDefinition d;
    d.id = id;
    d.name = "Test sensor";
    d.category = "Testing";
    d.verificationStatus = "demonstration";
    d.manufacturer = "Synthetic test maker";
    d.partNumber = "DEMO-001";
    d.tags = {"alpha", "synthetic"};
    d.pins = {{"out", "Output", "2", "GPIO_TEST", "SIGNAL", {"digital-output"}, {}}};
    d.electricalLimits = {{"Voltage", "V", std::nullopt, std::nullopt}};
    d.properties = {{"count", "Count", "integer", 3, {{"minimum", 1}, {"maximum", 10}}}};
    d.extensions = {{"future", QJsonObject{{"note", "retained"}}}};
    return d;
}
/** Read file bytes for preservation assertions.
 *
 * Byte comparisons detect unintended replacement even when JSON values appear equivalent.
 */
QByteArray bytes(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}
/** Write test input without using production serialization.
 *
 * Allows tests to supply malformed JSON and unsupported schema versions independently.
 */
bool write(const QString &path, const QByteArray &data) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
} // namespace
class CoreTests : public QObject {
    Q_OBJECT
  private slots:
    /** Verify bundled examples and field preservation.
     *
     * Missing demonstration assets warn without blocking; unknown bounds never become zero.
     */
    void validDefinitions() {
        hvd::Catalogue catalogue;
        hvd::Diagnostics out;
        QTemporaryDir user;
        QVERIFY2(catalogue.load(BUNDLED_ROOT, user.path(), out), qPrintable(hvd::diagnosticText(out)));
        QCOMPARE(catalogue.search().size(), 3);
        QVERIFY(!out.isEmpty());
        QVERIFY(!hvd::hasErrors(out));
        auto d = example();
        hvd::ComponentDefinition decoded;
        out.clear();
        QVERIFY(hvd::definitionFromJson(hvd::definitionToJson(d), decoded, out));
        QVERIFY(!decoded.electricalLimits[0].minimum.has_value());
        QCOMPARE(decoded.pins[0].physicalNumber, QString("2"));
        QCOMPARE(decoded.pins[0].gpioIdentifier, QString("GPIO_TEST"));
        QCOMPARE(decoded.pins[0].logicalSignal, QString("SIGNAL"));
        QCOMPARE(hvd::definitionToJson(decoded), hvd::definitionToJson(d));
    }
    /** Verify duplicate IDs and catalogue value independence.
     *
     * Registration cannot replace an existing stable ID, and a copied search result cannot mutate it.
     */
    void duplicatesAndIndependence() {
        QTemporaryDir package;
        hvd::Catalogue catalogue;
        hvd::Diagnostics out;
        auto d = example();
        QVERIFY(catalogue.registerPackage({d, package.path(), false}, out));
        auto copy = catalogue.search().first();
        copy.definition.name = "Changed";
        QCOMPARE(catalogue.find(d.id)->definition.name, QString("Test sensor"));
        out.clear();
        auto newer = d;
        newer.revision = 2;
        QVERIFY(!catalogue.registerPackage({newer, package.path(), false}, out));
        QVERIFY(hvd::diagnosticText(out).contains("Duplicate component ID"));
        d.pins.append(d.pins.first());
        out = hvd::validateDefinition(d);
        QVERIFY(hvd::diagnosticText(out).contains("Duplicate pin ID"));
    }
    /** Reject duplicate definitions during disk catalogue reload.
     *
     * Bundled/user identity collisions retain the previously loaded catalogue and report the offending ID.
     */
    void duplicatePackagesOnDisk() {
        QTemporaryDir bundled, user;
        QVERIFY(QDir(bundled.path()).mkdir("first"));
        hvd::Diagnostics out;
        QVERIFY(hvd::PackageStore::save({example(), bundled.filePath("first"), false}, out));
        hvd::Catalogue catalogue;
        QVERIFY(catalogue.load(bundled.path(), user.path(), out));
        QVERIFY(QDir(user.path()).mkdir("second"));
        out.clear();
        QVERIFY(hvd::PackageStore::save({example(), user.filePath("second"), false}, out));
        out.clear();
        QVERIFY(!catalogue.load(bundled.path(), user.path(), out));
        QVERIFY(hvd::diagnosticText(out).contains("Duplicate component ID"));
        QCOMPARE(catalogue.search().size(), 1);
        QVERIFY(catalogue.find("test.component")->bundled);
    }
    /** Verify actionable structural and property diagnostics.
     *
     * Incorrect defaults, constraints and JSON collection types cannot silently coerce into valid
     * definitions.
     */
    void malformedDefinitions() {
        auto d = example();
        d.properties[0].defaultValue = "wrong";
        QVERIFY(hvd::hasErrors(hvd::validateDefinition(d)));
        d = example();
        d.properties[0].constraints = {{"minimum", 10}, {"maximum", 1}};
        QVERIFY(hvd::hasErrors(hvd::validateDefinition(d)));
        d = example();
        d.properties[0].constraints = {{"choices", QJsonArray{1, "bad"}}};
        QVERIFY(hvd::hasErrors(hvd::validateDefinition(d)));
        d = example();
        d.properties[0].constraints = {{"pattern", "["}};
        QVERIFY(hvd::hasErrors(hvd::validateDefinition(d)));
        d = example();
        d.verificationStatus = "source-reviewed";
        QVERIFY(hvd::hasErrors(hvd::validateDefinition(d)));
        auto json = hvd::definitionToJson(example());
        json["pins"] = "invalid";
        hvd::ComponentDefinition previous = example("retained");
        hvd::Diagnostics out;
        QVERIFY(!hvd::definitionFromJson(json, previous, out));
        QCOMPARE(previous.id, QString("retained"));
        QVERIFY(hvd::diagnosticText(out).contains("pins"));
        json = hvd::definitionToJson(example());
        json["revision"] = 1.5;
        out.clear();
        QVERIFY(!hvd::definitionFromJson(json, previous, out));
    }
    /** Verify search across all supported metadata and exact filters.
     *
     * Case-insensitive text and simultaneous category/kind criteria share the core service.
     */
    void searchAndFilters() {
        QTemporaryDir root;
        hvd::Catalogue catalogue;
        hvd::Diagnostics out;
        QVERIFY(catalogue.registerPackage({example(), root.path(), false}, out));
        for (const auto &query : {"TEST SENSOR", "test maker", "demo-001", "ALPHA"})
            QCOMPARE(catalogue.search(query).size(), 1);
        QCOMPARE(catalogue.search("", "Testing", "sensor").size(), 1);
        QCOMPARE(catalogue.search("", "Other", "sensor").size(), 0);
        QCOMPARE(catalogue.search("", "Testing", "board").size(), 0);
        QVERIFY(!catalogue.find("unresolved"));
    }
    /** Verify lossless metadata save/load and revision protection.
     *
     * Atomic updates preserve identity, optional values, pin identifiers and extension metadata.
     */
    void saveLoadRoundTrip() {
        QTemporaryDir root;
        hvd::Diagnostics out;
        hvd::ComponentPackage package{example(), root.path(), false};
        QVERIFY(hvd::PackageStore::save(package, out));
        hvd::ComponentPackage loaded;
        QVERIFY(hvd::PackageStore::load(root.path(), false, loaded, out));
        QCOMPARE(hvd::definitionToJson(loaded.definition), hvd::definitionToJson(package.definition));
        package.definition.name = "Edited";
        out.clear();
        QVERIFY(!hvd::PackageStore::save(package, out));
        package.definition.revision = 2;
        out.clear();
        QVERIFY(hvd::PackageStore::save(package, out));
        out.clear();
        QVERIFY(hvd::PackageStore::load(root.path(), false, loaded, out));
        QCOMPARE(loaded.definition.name, QString("Edited"));
        QCOMPARE(loaded.definition.revision, 2);
        package.bundled = true;
        out.clear();
        QVERIFY(!hvd::PackageStore::save(package, out));
    }
    /** Verify package import/export including assets and duplicate rejection.
     *
     * Exported directories reload in a separate catalogue with identical definition and asset bytes.
     */
    void importExportRoundTrip() {
        QTemporaryDir source, user, exported, bundled;
        QVERIFY(QDir(source.path()).mkpath("assets"));
        QVERIFY(write(source.filePath("assets/document.txt"), "Synthetic asset\n"));
        auto d = example();
        d.assets = {{"datasheet", "assets/document.txt"}, {"image", QJsonValue::Null}};
        hvd::Diagnostics out;
        QVERIFY(hvd::PackageStore::save({d, source.path(), false}, out));
        hvd::Catalogue catalogue;
        QVERIFY(catalogue.importPackage(source.path(), user.path(), out));
        const QString destination = exported.filePath("package");
        QVERIFY(catalogue.exportPackage(d.id, destination, out));
        QCOMPARE(bytes(QDir(destination).filePath("assets/document.txt")), QByteArray("Synthetic asset\n"));
        hvd::Catalogue reloaded;
        out.clear();
        QVERIFY(reloaded.load(bundled.path(), user.path(), out));
        QCOMPARE(hvd::definitionToJson(reloaded.find(d.id)->definition), hvd::definitionToJson(d));
        out.clear();
        QVERIFY(!catalogue.importPackage(source.path(), user.path(), out));
        out.clear();
        QVERIFY(!catalogue.exportPackage(d.id, destination, out));
        QCOMPARE(bytes(QDir(destination).filePath("assets/document.txt")), QByteArray("Synthetic asset\n"));
        out.clear();
        QVERIFY(catalogue.saveUser(example("copy.id"), user.path(), {}, out, d.id));
        QCOMPARE(catalogue.find(d.id)->definition.id, d.id);
    }
    /** Verify unsupported schemas and transactional reload failure.
     *
     * Invalid on-disk content never replaces an already loaded valid catalogue or output package.
     */
    void unsupportedSchemaAndReload() {
        QTemporaryDir root, user;
        QVERIFY(QDir(root.path()).mkdir("one"));
        const QString directory = root.filePath("one");
        hvd::Diagnostics out;
        QVERIFY(hvd::PackageStore::save({example(), directory, false}, out));
        hvd::Catalogue catalogue;
        QVERIFY(catalogue.load(root.path(), user.path(), out));
        auto json = hvd::definitionToJson(example());
        json["schemaVersion"] = 999;
        QVERIFY(write(QDir(directory).filePath("component.json"), QJsonDocument(json).toJson()));
        out.clear();
        QVERIFY(!catalogue.load(root.path(), user.path(), out));
        QVERIFY(hvd::diagnosticText(out).contains("Unsupported schema"));
        QCOMPARE(catalogue.search().size(), 1);
        hvd::ComponentPackage retained{example("retained"), directory, false};
        out.clear();
        QVERIFY(!hvd::PackageStore::load(directory, false, retained, out));
        QCOMPARE(retained.definition.id, QString("retained"));
        QVERIFY(write(QDir(directory).filePath("component.json"), "{ broken JSON"));
        out.clear();
        QVERIFY(!catalogue.load(root.path(), user.path(), out));
        QCOMPARE(catalogue.search().size(), 1);
    }
    /** Verify path traversal, platform aliases and missing assets.
     *
     * Safe missing references warn while escape paths reject a package before registration or publication.
     */
    void assetContainment() {
        QTemporaryDir root;
        for (const auto &path : {"../outside.txt", "/absolute", "C:/outside", "assets/../../outside",
                                 "assets\\outside", "assets/NUL.txt", "assets/a.", "assets//a"}) {
            hvd::Diagnostics out;
            QVERIFY(hvd::PackageStore::resolveAsset(root.path(), path, out).isEmpty());
            QVERIFY(hvd::hasErrors(out));
        }
        hvd::Diagnostics out;
        QVERIFY(!hvd::PackageStore::resolveAsset(root.path(), "assets/missing.txt", out).isEmpty());
        auto d = example();
        d.assets = {{"datasheet", "assets/missing.txt"}};
        QVERIFY(hvd::PackageStore::save({d, root.path(), false}, out));
        hvd::ComponentPackage loaded;
        out.clear();
        QVERIFY(hvd::PackageStore::load(root.path(), false, loaded, out));
        QVERIFY(!out.isEmpty());
        QVERIFY(!hvd::hasErrors(out));
        d.assets = {{"datasheet", "../escape"}};
        out.clear();
        QVERIFY(!hvd::PackageStore::save({d, root.path(), false}, out));
    }
    /** Verify symlink escape detection when the platform permits symlinks.
     *
     * Canonical ancestor checks also reject a missing leaf under a linked external directory.
     */
    void symlinkContainment() {
        QTemporaryDir root, outside;
        std::error_code error;
        std::filesystem::create_directory_symlink(std::filesystem::path(outside.path().toStdWString()),
                                                  std::filesystem::path(root.filePath("link").toStdWString()),
                                                  error);
        if (error)
            QSKIP("OS account does not permit directory symlinks.");
        hvd::Diagnostics out;
        QVERIFY(hvd::PackageStore::resolveAsset(root.path(), "link/missing.txt", out).isEmpty());
        QVERIFY(hvd::hasErrors(out));
    }
    /** Verify failed writes preserve previous valid bytes.
     *
     * Includes semantic rejection and a real OS-denied atomic replacement on Windows.
     */
    void failedWritesPreserveData() {
        QTemporaryDir root;
        hvd::Diagnostics out;
        auto d = example();
        QVERIFY(hvd::PackageStore::save({d, root.path(), false}, out));
        const QString path = root.filePath("component.json");
        const auto original = bytes(path);
        d.name.clear();
        out.clear();
        QVERIFY(!hvd::PackageStore::save({d, root.path(), false}, out));
        QCOMPARE(bytes(path), original);
#ifdef Q_OS_WIN
        const HANDLE lock =
            CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(lock != INVALID_HANDLE_VALUE);
        d = example();
        d.name = "Cannot replace locked file";
        d.revision = 2;
        out.clear();
        const bool saved = hvd::PackageStore::save({d, root.path(), false}, out);
        CloseHandle(lock);
        QVERIFY(!saved);
        QVERIFY(hvd::hasErrors(out));
        QCOMPARE(bytes(path), original);
#else
        QSKIP("OS-denied replacement test currently uses Windows file sharing.");
#endif
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "CoreTests.moc"
