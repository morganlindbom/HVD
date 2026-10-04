// KiCadTests.cpp
#include "storage/KiCadLibrary.h"
#include "ui/KiCadBrowser.h"
#include "ui/ModelPreview.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <QtTest>
#include <cmath>
namespace {
/** Write a fixture in an isolated temporary source tree.
 *
 * Tests never alter the user's copied KiCad libraries or license notices.
 */
void write(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size())
        qFatal("Cannot write isolated fixture");
}
/** Create a supported shared-material triangle fixture.
 *
 * Points remain VRML units until the KiCad placement matrix converts them to
 * millimetres.
 */
QByteArray triangle() {
    return "#VRML V2.0 utf8\nDEF mat Material { diffuseColor 0.7 0.2 0.1 "
           "}\nTransform { translation 1 0 0 children [ Shape { appearance "
           "Appearance { material USE mat } geometry IndexedFaceSet { coord "
           "Coordinate { point [0 0 0, 1 0 0, 0 1 0] } coordIndex [0,1,2,-1] } } "
           "] }";
}
} // namespace
class KiCadTests : public QObject {
    Q_OBJECT
  private slots:
    /** Isolate persistent application configuration.
     *
     * The real application library root and selection settings remain untouched.
     */
    void initTestCase() {
        QCoreApplication::setOrganizationName("HVDTests");
        QCoreApplication::setApplicationName("KiCadTests");
        QSettings().clear();
    }
    /** Parse nested expressions, escaped strings and valid repeated pads.
     *
     * Mechanical pads and repeated electrical numbers are not
     * component-definition pin IDs.
     */
    void footprintGrammar() {
        auto f = hvd::parseFootprint(
            "(footprint \"test \\\"quoted\\\"\" (descr \"Nested (text)\") (property \"x\" \"y\" (effects "
            "(font (size 1 1)))) (pad \"1\" thru_hole circle (at 0 2 90)(size 1 2)(drill .5)) (pad \"1\" smd "
            "rect (at 3 4)(size 2 2)) (pad \"\" np_thru_hole circle(at 5 6)(size 1 1)) (model "
            "\"${KICAD9_3DMODEL_DIR}/x.wrl\" (offset(xyz 1 2 3))(rotate(xyz 10 20 30))(scale(xyz 2 3 4))))");
        QCOMPARE(f.name, QString("test \"quoted\""));
        QCOMPARE(f.description, QString("Nested (text)"));
        QCOMPARE(f.pads.size(), 3);
        QCOMPARE(f.pads[0].number, f.pads[1].number);
        QVERIFY(f.pads[2].number.isEmpty());
        QCOMPARE(f.pads[0].rotation, 90.0);
        QCOMPARE(f.models[0].offset, QVector3D(1, 2, 3));
        QCOMPARE(f.models[0].scale, QVector3D(2, 3, 4));
    }
    /** Reject malformed expressions and nonfinite fields.
     *
     * Invalid transforms produce errors rather than silently loading at the
     * origin.
     */
    void malformed() {
        for (const QString &s : QStringList{"(footprint \"x\"", "(footprint \"x\") trailing",
                                            "(footprint \"x\" (pad \"1\" smd rect (at nan 0)(size 1 1)))",
                                            "(footprint \"x\" (model \"x.wrl\" (scale(xyz 0 1 1))))",
                                            "(footprint \"unterminated)"})
            QVERIFY_EXCEPTION_THROWN(hvd::parseFootprint(s), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(hvd::loadFootprint("missing-fixture.kicad_mod"), std::runtime_error);
    }
    /** Resolve known variables while enforcing external source containment.
     *
     * Package validation is not involved and unknown variables never fall back to
     * current-directory paths.
     */
    void resolver() {
        QTemporaryDir temp;
        auto model = temp.filePath("3dmodels/Test.3dshapes/model.wrl");
        write(model, triangle());
        auto foot = temp.filePath("footprints/Test.pretty/part.kicad_mod");
        QCOMPARE(hvd::resolveKiCadModel(temp.path(), "${KICAD9_3DMODEL_DIR}/Test.3dshapes/model.wrl", foot),
                 QFileInfo(model).canonicalFilePath());
        QVERIFY_EXCEPTION_THROWN(hvd::resolveKiCadModel(temp.path(), "${UNKNOWN}/model.wrl", foot),
                                 std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(
            hvd::resolveKiCadModel(temp.path(), "${KICAD9_3DMODEL_DIR}/missing.wrl", foot),
            std::runtime_error);
        QTemporaryDir other;
        write(other.filePath("out.wrl"), triangle());
        QVERIFY_EXCEPTION_THROWN(hvd::resolveKiCadModel(temp.path(), other.filePath("out.wrl"), foot),
                                 std::runtime_error);
    }
    /** Index actual source folders without unrelated demo copies.
     *
     * Nested library namespaces remain unique and cancellation preserves read-only source bytes.
     */
    void indexing() {
        QTemporaryDir temp;
        write(temp.filePath("footprints/Test.pretty/Same.kicad_mod"), "(footprint \"Same\")");
        write(temp.filePath("footprints/nested/Test.pretty/Same.kicad_mod"), "(footprint \"Same\")");
        write(temp.filePath("symbols/Test.kicad_sym"), "(kicad_symbol_lib)");
        write(temp.filePath("3dmodels/Test.3dshapes/model.wrl"), triangle());
        write(temp.filePath("demos/Test.pretty/Same.kicad_mod"), "(footprint \"Same\")");
        auto cancel = std::make_shared<std::atomic_bool>(false);
        auto index = hvd::scanKiCad(temp.path(), cancel);
        QCOMPARE(index.entries.size(), 4);
        QSet<QString> ids;
        for (const auto &e : index.entries)
            ids.insert(e.id());
        QCOMPARE(ids.size(), 4);
        QVERIFY(ids.contains("footprint:Test:Same"));
        QVERIFY(ids.contains("footprint:nested/Test:Same"));
        QCOMPARE(index.modelRoot, temp.filePath("3dmodels"));
        QVERIFY(!hvd::scanKiCad(temp.filePath("absent"), cancel).diagnostics.isEmpty());
    }
    /** Verify KiCad units and placement against analytic coordinates.
     *
     * A known point exercises scale, signed Euler rotation and millimetre offset
     * independently from framing.
     */
    void placement() {
        hvd::KiCadModel m;
        m.offset = {10, 20, 30};
        m.rotation = {0, 0, 90};
        m.scale = {2, 3, 4};
        auto p = hvd::kiCadPlacement(m).map(QVector3D(1, 2, 3));
        QVERIFY((p - QVector3D(25.24f, 14.92f, 60.48f)).length() < .0001f);
    }
    /** Load shared materials and nested scene transforms.
     *
     * Unsupported geometry, malformed indices and cancellation are explicit
     * errors.
     */
    void vrml() {
        QTemporaryDir temp;
        auto path = temp.filePath("scene.wrl");
        write(path, triangle());
        QStringList diagnostics;
        auto cancel = std::make_shared<std::atomic_bool>(false);
        auto v = hvd::loadVrml(path, hvd::kiCadPlacement({}), diagnostics, cancel);
        QCOMPARE(v.size(), 3);
        QVERIFY(std::abs(v[0].x - 2.54f) < .0001f);
        QVERIFY(std::abs(v[1].x - 5.08f) < .0001f);
        QVERIFY(std::abs(v[0].r - .7f) < .0001f);
        write(path, "#VRML V2.0 utf8\nShape { geometry Box {} }");
        QVERIFY_EXCEPTION_THROWN(hvd::loadVrml(path, {}, diagnostics, cancel), std::runtime_error);
        write(path, triangle().replace("0,1,2,-1", "0,1,9,-1"));
        QVERIFY_EXCEPTION_THROWN(hvd::loadVrml(path, {}, diagnostics, cancel), std::runtime_error);
        cancel->store(true);
        write(path, triangle());
        QVERIFY_EXCEPTION_THROWN(hvd::loadVrml(path, {}, diagnostics, cancel), std::runtime_error);
    }
    /** Verify the actual copied resistor model against its pad spacing.
     *
     * Mesh bounds and vertices near both lead centres independently detect
     * incorrect model scale or placement.
     */
    void actualLibrary() {
        QString root = QString::fromUtf8(KICAD_ROOT);
        QString name = "R_Axial_DIN0207_L6.3mm_D2.5mm_P7.62mm_Horizontal";
        auto path = QDir(root).filePath("footprints/Resistor_THT.pretty/" + name + ".kicad_mod");
        if (!QFileInfo::exists(path))
            QSKIP("Copied resistor fixture is unavailable.");
        auto f = hvd::loadFootprint(path);
        QCOMPARE(f.pads.size(), 2);
        QCOMPARE(f.pads[1].position, QPointF(7.62, 0));
        auto cancel = std::make_shared<std::atomic_bool>(false);
        auto mesh = hvd::loadKiCadGeometry(root, {"footprint", "Resistor_THT", name, path}, cancel);
        QVERIFY2(mesh.error.isEmpty(), qPrintable(mesh.error));
        QVERIFY(mesh.vertices.size() > 100);
        QVERIFY(mesh.modelPaths.first().endsWith(".wrl"));
        float min = 1e8f, max = -1e8f;
        bool lead1 = false, lead2 = false;
        for (const auto &v : mesh.vertices) {
            min = std::min(min, v.x);
            max = std::max(max, v.x);
            if (v.z < -.5f && std::abs(v.y) < .31f) {
                lead1 |= std::abs(v.x) < .31f;
                lead2 |= std::abs(v.x - 7.62f) < .31f;
            }
        }
        QVERIFY(std::abs(min + .29718f) < .01f);
        QVERIFY(std::abs(max - 7.91718f) < .01f);
        QVERIFY(lead1 && lead2);
    }
    /** Exercise visible search, selection, markers and stale asynchronous
     * results.
     *
     * Native rendering uses an isolated library fixture, including a source
     * without a model and repeated teardown.
     */
    void browserFlow() {
        QTemporaryDir temp;
        auto model = temp.filePath("3dmodels/Test.3dshapes/model.wrl");
        write(model, triangle());
        auto first = temp.filePath("footprints/Test.pretty/Visible.kicad_mod");
        auto empty = temp.filePath("footprints/Test.pretty/Empty.kicad_mod");
        write(first, "(footprint \"Visible\" (pad \"1\" thru_hole circle(at 0 0)(size 1 "
                     "1))(pad \"2\" thru_hole circle(at 2.54 -2.54)(size 1 1))(model "
                     "\"${KICAD9_3DMODEL_DIR}/Test.3dshapes/model.wrl\"))");
        write(empty, "(footprint \"Empty\" (pad \"\" np_thru_hole circle(at 0 "
                     "0)(size 1 1)))");
        hvd::KiCadBrowser browser;
        browser.resize(1200, 760);
        browser.show();
        browser.configureRoot(temp.path());
        auto search = browser.findChild<QLineEdit *>("kiCadSearch");
        auto list = browser.findChild<QListWidget *>("kiCadItems");
        auto preview = browser.findChild<hvd::ModelPreview *>("kiCadPreview");
        search->setText("Test:Visible");
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(preview->hasModel(), 10000);
        QTRY_VERIFY(preview->renderingReady());
        preview->grabFramebuffer();
        QSignalSpy spy(preview, &hvd::ModelPreview::padSelected);
        QTest::mouseClick(preview, Qt::LeftButton, Qt::NoModifier, preview->padScreenPosition(0).toPoint());
        QCOMPARE(spy.count(), 1);
        QVERIFY(browser.findChild<QLabel *>("kiCadPadInfo")->text().contains("(0, 0)"));
        auto yaw = preview->yaw();
        QPoint from = preview->rect().center();
        QMouseEvent press(QEvent::MouseButtonPress, from, preview->mapToGlobal(from), Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(preview, &press);
        QMouseEvent move(QEvent::MouseMove, from + QPoint(45, 20),
                         preview->mapToGlobal(from + QPoint(45, 20)), Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(preview, &move);
        QVERIFY(preview->yaw() != yaw);
        float distance = preview->distance();
        QWheelEvent wheel(from, preview->mapToGlobal(from), {}, QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(preview, &wheel);
        QVERIFY(preview->distance() < distance);
        preview->resetView();
        QCOMPARE(preview->yaw(), yaw);
        preview->fitToView();
        browser.resize(1000, 600);
        QTest::qWait(50);
        QVERIFY(preview->distance() > 0);
        auto toggle = browser.findChild<QCheckBox *>("kiCadPadsToggle");
        toggle->setChecked(false);
        toggle->setChecked(true);
        search->clear();
        QTRY_COMPARE(list->count(), 2);
        for (int i = 0; i < 20; ++i)
            list->setCurrentRow(i % 2);
        search->setText("Empty");
        QTRY_COMPARE(list->count(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(
            !preview->hasModel() && preview->statusText().contains("No visible supported"), 10000);
        QTest::qWait(100);
        QVERIFY(!preview->hasModel());
        search->setText("Visible");
        QTRY_VERIFY_WITH_TIMEOUT(preview->hasModel(), 10000);
        browser.close();
        auto cancelled = std::make_shared<std::atomic_bool>(true);
        QVERIFY(hvd::scanKiCad(temp.path(), cancelled).cancelled);
        for (int i = 0; i < 3; ++i) {
            hvd::KiCadBrowser b;
            b.configureRoot(temp.path());
            b.close();
        }
    }
};
QTEST_MAIN(KiCadTests)
#include "KiCadTests.moc"
