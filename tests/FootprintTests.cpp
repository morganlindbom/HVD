// FootprintTests.cpp
#include "storage/FootprintGeometry.h"
#include "ui/FootprintPreview.h"
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
#include <algorithm>
namespace {
/** Build asymmetric geometry with distinct repeated and mechanical records.
 *
 * Positive rotation, unequal axes, layers and off-centre drills detect mirrored
 * or dimension-only approximations independently of a 3D model.
 */
QString fixture() {
    return R"((footprint "asymmetric"
      (pad "1" thru_hole rect (at 0 0)(size 2 1)(drill oval .8 .4 (offset .2 0))(layers "*.Cu" "*.Mask"))
      (pad "1" smd roundrect (at 4 2 90)(size 3 1)(roundrect_rratio .25)(layers "F.Cu"))
      (pad "" np_thru_hole circle (at -2 3)(size 1 1)(drill .8)(layers "*.Cu"))
      (pad "3" smd oval (at 7 -1 30)(size 2 1)(layers "B.Cu"))
      (fp_line (start -3 -2)(end 8 -2)(stroke(width .2)(type solid))(layer "F.SilkS"))
      (fp_rect(start -3 -3)(end 9 4)(stroke(width .1)(type dash))(fill none)(layer "F.CrtYd"))
      (fp_circle(center 0 0)(end 1 0)(stroke(width .1)(type solid))(fill none)(layer "B.Fab"))
      (fp_arc(start 1 0)(mid 0 1)(end -1 0)(stroke(width .1)(type solid))(layer "F.Fab"))
      (fp_poly(pts(xy 5 0)(xy 6 0)(xy 5 1))(stroke(width .1)(type solid))(fill solid)(layer "F.SilkS"))
      (property "Reference" "REF**" (at 2 -4)(layer "F.SilkS")(effects(font(size 1 1))))
    ))";
}
/** Write isolated test sources without touching copied libraries.
 *
 * Fixtures own their temporary package and model paths.
 */
void write(const QString &path, const QByteArray &text) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(text), text.size());
}
} // namespace
class FootprintTests : public QObject {
    Q_OBJECT
  private slots:
    /** Isolate test settings from the user application.
     *
     * Root selection and remembered source preferences remain untouched.
     */
    void initTestCase() {
        QCoreApplication::setOrganizationName("HVDTests");
        QCoreApplication::setApplicationName("FootprintTests");
        QSettings().clear();
    }
    /** Parse pads, drills, layer identities and all supported primitives.
     *
     * Repeated numbers remain separate physical records.
     */
    void primitives() {
        auto f = hvd::parseFootprint(fixture());
        QCOMPARE(f.pads.size(), 4);
        QCOMPARE(f.graphics.size(), 6);
        QCOMPARE(f.pads[0].drillSize, QPointF(.8, .4));
        QCOMPARE(f.pads[0].drillOffset, QPointF(.2, 0));
        QCOMPARE(f.pads[0].layers, QStringList({"*.Cu", "*.Mask"}));
        QCOMPARE(f.pads[1].number, f.pads[0].number);
        QVERIFY(f.pads[2].number.isEmpty());
        QCOMPARE(f.pads[1].rotation, 90.0);
        auto drawing = hvd::footprintDrawing(f);
        QVERIFY(drawing.layers.contains("F.Cu"));
        QVERIFY(drawing.layers.contains("B.Cu"));
        QVERIFY(drawing.layers.contains("Drills"));
        QVERIFY(drawing.bounds.left() < -3);
        QVERIFY(drawing.bounds.right() > 9);
        QVERIFY(drawing.bounds.top() < -4);
        QVERIFY(drawing.bounds.bottom() > 4);
        auto rotated = std::find_if(drawing.shapes.begin(), drawing.shapes.end(),
                                    [](const auto &s) { return s.pad == 1; });
        QVERIFY(rotated != drawing.shapes.end());
        QVERIFY(qAbs(rotated->path.boundingRect().width() - 1) < 1e-6);
        QVERIFY(qAbs(rotated->path.boundingRect().height() - 3) < 1e-6);
        QCOMPARE(rotated->path.boundingRect().center(), QPointF(4, 2));
        auto arc = std::find_if(drawing.shapes.begin(), drawing.shapes.end(),
                                [](const auto &s) { return s.layer == "F.Fab"; });
        QVERIFY(arc != drawing.shapes.end());
        QVERIFY(arc->path.boundingRect().bottom() > .99);
        QVERIFY(arc->path.boundingRect().top() > -.01);
    }
    /** Preserve partial geometry with explicit unsupported diagnostics.
     *
     * Custom pads never acquire invented rectangular outlines.
     */
    void unsupportedAndMalformed() {
        auto f = hvd::parseFootprint(
            "(footprint x(pad 1 smd custom(at 0 0)(size 2 2)(layers F.Cu))(fp_curve(pts(xy 0 0))))");
        QVERIFY(!f.pads[0].supportedShape);
        QVERIFY(hvd::padOutline(f.pads[0]).isEmpty());
        QVERIFY(f.diagnostics.join(" ").contains("Incomplete"));
        for (const auto &s :
             QStringList{"(footprint x(pad 1 thru_hole circle(at 0 0)(size 1 1)(drill oval 1 -1)))",
                         "(footprint x(fp_line(start 0 0)(end nan 0)(layer F.SilkS)))",
                         "(footprint x(fp_poly(pts(xy 0 0)(xy 1 1))(layer F.Fab)))"})
            QVERIFY_EXCEPTION_THROWN(hvd::parseFootprint(s), std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(hvd::loadFootprint("missing-2d-fixture.kicad_mod"), std::runtime_error);
    }
    /** Verify orientation, hit testing, zoom, pan, resize and layer visibility.
     *
     * An asymmetric fixture detects implicit Y or back-side mirroring.
     */
    void cameraAndSelection() {
        hvd::FootprintPreview view;
        view.resize(700, 520);
        view.show();
        auto f = hvd::parseFootprint(fixture());
        view.setFootprint(f, hvd::footprintDrawing(f));
        QVERIFY(view.screenPosition({4, 2}).x() > view.screenPosition({0, 0}).x());
        QVERIFY(view.screenPosition({4, 2}).y() > view.screenPosition({0, 0}).y());
        QSignalSpy spy(&view, &hvd::FootprintPreview::padSelected);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.screenPosition({4, 2}).toPoint());
        QCOMPARE(view.selectedPad(), 1);
        QCOMPARE(spy.count(), 1);
        view.setLayerVisible("F.Cu", false);
        QVERIFY(!view.layerVisible("F.Cu"));
        view.selectPad(-1);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.screenPosition({4, 2}).toPoint());
        QCOMPARE(view.selectedPad(), -1);
        view.setLayerVisible("F.Cu", true);
        view.showBack(true);
        QVERIFY(view.screenPosition({4, 2}).x() < view.screenPosition({0, 0}).x());
        view.showBack(false);
        auto before = view.zoom();
        auto point = view.rect().center();
        QWheelEvent wheel(point, view.mapToGlobal(point), QPoint(), QPoint(0, 120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&view, &wheel);
        QVERIFY(view.zoom() > before);
        auto origin = view.screenPosition({});
        QTest::mousePress(&view, Qt::MiddleButton, Qt::NoModifier, point);
        QMouseEvent move(QEvent::MouseMove, point + QPoint(35, 20), view.mapToGlobal(point + QPoint(35, 20)),
                         Qt::NoButton, Qt::MiddleButton, Qt::NoModifier);
        QApplication::sendEvent(&view, &move);
        QTest::mouseRelease(&view, Qt::MiddleButton, Qt::NoModifier, point + QPoint(35, 20));
        QVERIFY(QLineF(origin, view.screenPosition({})).length() > 30);
        view.resetView();
        view.resize(400, 400);
        QApplication::processEvents();
        QVERIFY(view.rect().adjusted(20, 20, -20, -20).contains(view.screenPosition({4, 2}).toPoint()));
        view.clear("No footprint association");
        QVERIFY(view.drawing().shapes.isEmpty());
        QCOMPARE(view.selectedPad(), -1);
    }
    /** Verify unselected artwork colours and coincident record cycling.
     *
     * Unselected graphic records use -1 and must never be mistaken for selected pads.
     */
    void layerPixelsAndCoincidentRecords() {
        auto f = hvd::parseFootprint("(footprint test(fp_rect(start -2 -2)(end 2 2)(stroke(width .2)(type "
                                     "solid))(fill solid)(layer F.SilkS))(pad 1 smd rect(at 0 0)(size 1 "
                                     "1)(layers F.Cu))(pad 1 smd rect(at 0 0)(size 1 1)(layers F.Cu)))");
        hvd::FootprintPreview view;
        view.resize(500, 500);
        view.show();
        view.setFootprint(f, hvd::footprintDrawing(f));
        auto pixel = view.screenPosition({1, 1}).toPoint();
        auto image = view.grab().toImage();
        QCOMPARE(image.pixelColor((QPointF(pixel) * image.devicePixelRatio()).toPoint()),
                 hvd::footprintLayerColor("F.SilkS"));
        auto initialZoom = view.zoom();
        view.setLayerVisible("F.SilkS", false);
        image = view.grab().toImage();
        QVERIFY(image.pixelColor((QPointF(pixel) * image.devicePixelRatio()).toPoint()) !=
                hvd::footprintLayerColor("F.SilkS"));
        view.fitToView();
        QVERIFY(view.zoom() > initialZoom);
        const auto at = view.screenPosition({}).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, at);
        QCOMPARE(view.selectedPad(), 0);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, at);
        QCOMPARE(view.selectedPad(), 1);
        auto rotated = hvd::parseFootprint("(footprint slot(pad 1 thru_hole oval(at 4 2 90)(size 3 2)(drill "
                                           "oval 1.2 .6(offset .5 0))(layers *.Cu)))");
        auto d = hvd::footprintDrawing(rotated);
        auto drill =
            std::find_if(d.shapes.begin(), d.shapes.end(), [](const auto &shape) { return shape.drill; });
        QVERIFY(drill != d.shapes.end());
        QCOMPARE(drill->path.boundingRect().center(), QPointF(4, 1.5));
        QVERIFY(qAbs(drill->path.boundingRect().width() - .6) < 1e-6);
        QVERIFY(qAbs(drill->path.boundingRect().height() - 1.2) < 1e-6);
    }
    /** Verify dimensions from representative copied footprints.
     *
     * Real source coordinates independently anchor screenshot spacing and orientation checks.
     */
    void actualFixtures() {
        QString root = QString::fromUtf8(KICAD_ROOT);
        if (!QDir(root + "/footprints").exists())
            root += "/9.0/share/kicad";
        if (!QDir(root + "/footprints").exists())
            QSKIP("Copied KiCad footprint fixtures are unavailable.");
        auto connector = hvd::loadFootprint(
            root + "/footprints/Connector_PinHeader_2.54mm.pretty/PinHeader_1x06_P2.54mm_Vertical.kicad_mod");
        QCOMPARE(connector.pads.size(), 6);
        for (int i = 0; i < 6; ++i) {
            QCOMPARE(connector.pads[i].number, QString::number(i + 1));
            QVERIFY(qAbs(connector.pads[i].position.y() - i * 2.54) < 1e-6);
            QCOMPARE(connector.pads[i].position.x(), 0.0);
            QCOMPARE(connector.pads[i].drillSize, QPointF(1, 1));
        }
        auto smd = hvd::loadFootprint(root + "/footprints/Resistor_SMD.pretty/R_0805_2012Metric.kicad_mod");
        QCOMPARE(smd.pads.size(), 2);
        QCOMPARE(smd.pads[0].position, QPointF(-.9125, 0));
        QCOMPARE(smd.pads[1].position, QPointF(.9125, 0));
        QCOMPARE(smd.pads[0].size, QPointF(1.025, 1.4));
        QCOMPARE(smd.pads[0].shape, QString("roundrect"));
        QVERIFY(!smd.pads[0].hasDrill);
        for (const auto &f : {connector, smd}) {
            auto d = hvd::footprintDrawing(f);
            QVERIFY(!d.shapes.isEmpty());
            QVERIFY(!d.diagnostics.join(" ").contains("Incomplete"));
        }
    }
    /** Verify shared source-record selection and model-independent geometry.
     *
     * Rapid changes, empty searches and direct models clear old footprint paths.
     */
    void browserSynchronization() {
        QTemporaryDir temp;
        write(temp.filePath("footprints/Test.pretty/Asymmetric.kicad_mod"), fixture().toUtf8());
        write(temp.filePath("3dmodels/Test.3dshapes/direct.wrl"),
              "#VRML V2.0 utf8\nShape{geometry IndexedFaceSet{coord Coordinate{point[0 0 0,1 0 0,0 1 "
              "0]}coordIndex[0,1,2,-1]}} ");
        write(temp.filePath("footprints/Test.pretty/Malformed.kicad_mod"), "(footprint broken");
        hvd::KiCadBrowser browser;
        browser.resize(1450, 850);
        browser.show();
        browser.configureRoot(temp.path());
        auto search = browser.findChild<QLineEdit *>("kiCadSearch");
        auto flat = browser.findChild<hvd::FootprintPreview *>();
        auto model = browser.findChild<hvd::ModelPreview *>();
        search->setText("Asymmetric");
        QTRY_COMPARE_WITH_TIMEOUT(flat->drawing().layers.size(), 7, 10000);
        QVERIFY(!model->hasModel());
        QVERIFY(flat->drawing().shapes.size() > 10);
        QCOMPARE(model->pads().size(), 4);
        QTest::mouseClick(flat, Qt::LeftButton, Qt::NoModifier, flat->screenPosition({4, 2}).toPoint());
        QCOMPARE(flat->selectedPad(), 1);
        QCOMPARE(model->selectedPad(), 1);
        QVERIFY(browser.findChild<QLabel *>("kiCadPadInfo")->text().contains("Record 1"));
        model->padSelected(0);
        QCOMPARE(flat->selectedPad(), 0);
        QCOMPARE(model->selectedPad(), 0);
        auto supported = fixture();
        supported.insert(supported.lastIndexOf(')'),
                         "(model \"${KICAD9_3DMODEL_DIR}/Test.3dshapes/direct.wrl\")");
        write(temp.filePath("footprints/Test.pretty/Supported.kicad_mod"), supported.toUtf8());
        browser.configureRoot(temp.path());
        search->setText("Supported");
        QTRY_VERIFY_WITH_TIMEOUT(model->hasModel(), 10000);
        QTRY_VERIFY(model->renderingReady());
        model->grabFramebuffer();
        const auto modelDistance = model->distance();
        const auto modelYaw = model->yaw();
        auto point = flat->rect().center();
        QWheelEvent wheel(point, flat->mapToGlobal(point), QPoint(), QPoint(0, 120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(flat, &wheel);
        QCOMPARE(model->distance(), modelDistance);
        QCOMPARE(model->yaw(), modelYaw);
        QTest::mouseClick(model, Qt::LeftButton, Qt::NoModifier, model->padScreenPosition(1).toPoint());
        QCOMPARE(flat->selectedPad(), 1);
        QCOMPARE(model->selectedPad(), 1);
        search->setText("Malformed");
        QTRY_VERIFY(flat->drawing().shapes.isEmpty());
        QTRY_VERIFY(!browser.property("loadedSourceId").toString().isEmpty());
        search->setText("Asymmetric");
        QTRY_VERIFY(!flat->drawing().shapes.isEmpty());
        browser.findChild<QComboBox *>("kiCadSourceKind")->setCurrentIndex(2);
        search->clear();
        QTRY_VERIFY_WITH_TIMEOUT(model->hasModel(), 10000);
        QVERIFY(flat->drawing().shapes.isEmpty());
        search->setText("absent");
        QTRY_VERIFY(!model->hasModel());
        QVERIFY(flat->drawing().shapes.isEmpty());
    }
};
QTEST_MAIN(FootprintTests)
#include "FootprintTests.moc"
