// ModelPreviewTests.cpp
#include "ui/MainWindow.h"
#include "ui/ModelPreview.h"
#include <QApplication>
#include <QJsonObject>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QWheelEvent>
#include <QtTest>
#include <cmath>

namespace {
/** Construct the supported explicit demonstration descriptor.
 *
 * Tests select a procedural model by schema and generator, never by a filename or catalogue display name.
 */
QJsonObject descriptor() {
    return {{"schemaVersion", 1}, {"type", "procedural"}, {"generator", "demo-board-v1"}, {"units", "mm"}};
}

/** Select an entry through the actual component list.
 *
 * The list's normal selection signal routes the descriptor into the preview.
 */
bool select(hvd::MainWindow &window, const QString &id) {
    auto *list = window.findChild<QListWidget *>("componentList");
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->data(Qt::UserRole).toString() == id) {
            list->setCurrentRow(row);
            return true;
        }
    }
    return false;
}
/** Count rendered PCB-coloured pixels in a captured framebuffer.
 *
 * Sampling actual output detects a blank scene and stale geometry independently of camera state getters.
 */
int greenPixelCount(const QImage &image) {
    int count = 0;
    for (int y = 0; y < image.height(); y += 3)
        for (int x = 0; x < image.width(); x += 3) {
            const auto colour = image.pixelColor(x, y);
            if (colour.green() > colour.red() * 1.4 && colour.green() > colour.blue() * 1.2)
                ++count;
        }
    return count;
}
} // namespace

class ModelPreviewTests : public QObject {
    Q_OBJECT
  private slots:
    /** Isolate selection settings from the real application's configuration.
     *
     * Only this test executable's settings are cleared to exercise first-launch behaviour deterministically.
     */
    void initTestCase() {
        QCoreApplication::setOrganizationName("HVDTests");
        QCoreApplication::setApplicationName("ModelPreviewTests");
        QSettings().clear();
    }
    /** Verify actual procedural 3D vertices and expected millimetre bounds.
     *
     * Nonzero thickness and varied elevations ensure that the demo is geometry rather than a flat image.
     */
    void geometry() {
        const auto mesh = hvd::demoBoardGeometry();
        QVERIFY(mesh.size() > 2000);
        QCOMPARE(mesh.size() % 3, 0);
        float minimumZ = 0, maximumZ = 0;
        for (const auto &vertex : mesh) {
            QVERIFY(std::isfinite(vertex.x) && std::isfinite(vertex.y) && std::isfinite(vertex.z));
            QVERIFY(std::abs(vertex.x) <= 25.01f && std::abs(vertex.y) <= 12.01f);
            minimumZ = std::min(minimumZ, vertex.z);
            maximumZ = std::max(maximumZ, vertex.z);
        }
        QVERIFY(minimumZ < -4);
        QVERIFY(maximumZ > 4);
    }
    /** Verify automatic demo selection, native rendering and geometry clearing.
     *
     * Unsupported formats, schemas and units show explanatory empty states instead of retaining stale
     * geometry.
     */
    void selectionAndEmptyStates() {
        QTemporaryDir user;
        hvd::MainWindow window(BUNDLED_ROOT, user.path());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *preview = window.findChild<hvd::ModelPreview *>("modelPreview");
        QVERIFY(preview);
        QTRY_VERIFY(preview->renderingReady());
        QVERIFY(preview->hasModel());
        QVERIFY(preview->statusText().contains("synthetic"));
        qInfo().noquote() << "Renderer:" << preview->property("graphicsRenderer").toString()
                          << "OpenGL:" << preview->property("graphicsVersion").toString();
        const auto frame = preview->grabFramebuffer();
        QVERIFY(!frame.isNull());
        QVERIFY2(greenPixelCount(frame) > 80, "No green PCB geometry appeared in the OpenGL framebuffer.");
        QVERIFY(select(window, "example.synthetic-sensor"));
        QVERIFY(!preview->hasModel());
        QVERIFY(preview->statusText().contains("No supported"));
        QCOMPARE(greenPixelCount(preview->grabFramebuffer()), 0);
        auto unsupported = descriptor();
        unsupported["type"] = "step";
        preview->setModel(unsupported);
        QVERIFY(!preview->hasModel());
        QVERIFY(preview->statusText().contains("not parsed"));
        unsupported = descriptor();
        unsupported["schemaVersion"] = 99;
        preview->setModel(unsupported);
        QVERIFY(!preview->hasModel());
        unsupported = descriptor();
        unsupported["units"] = "in";
        preview->setModel(unsupported);
        QVERIFY(!preview->hasModel());
        for (int i = 0; i < 30; ++i) {
            QVERIFY(select(window, "example.synthetic-demo-board"));
            QVERIFY(preview->hasModel());
            QVERIFY(select(window, "example.synthetic-carrier"));
            QVERIFY(!preview->hasModel());
        }
        QVERIFY(select(window, "example.synthetic-demo-board"));
        window.close();
    }
    /** Exercise mouse orbit, wheel zoom, reset, fit and resizing.
     *
     * Sends normal Qt input events and verifies camera changes plus a usable framebuffer after resize.
     */
    void interactionAndResizing() {
        QTemporaryDir user;
        hvd::MainWindow window(BUNDLED_ROOT, user.path());
        window.resize(1280, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(select(window, "example.synthetic-demo-board"));
        auto *preview = window.findChild<hvd::ModelPreview *>("modelPreview");
        QTRY_VERIFY(preview->renderingReady());
        const QImage initialFrame = preview->grabFramebuffer();
        const float initialYaw = preview->yaw(), initialPitch = preview->pitch();
        const QPoint centre = preview->rect().center();
        QTest::mousePress(preview, Qt::LeftButton, Qt::NoModifier, centre);
        QMouseEvent drag(QEvent::MouseMove, QPointF(centre + QPoint(70, 40)),
                         QPointF(preview->mapToGlobal(centre + QPoint(70, 40))), Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(preview, &drag);
        QTest::mouseRelease(preview, Qt::LeftButton, Qt::NoModifier, centre + QPoint(70, 40));
        QVERIFY(preview->yaw() != initialYaw);
        QVERIFY(preview->pitch() != initialPitch);
        QVERIFY(preview->grabFramebuffer() != initialFrame);
        const float initialDistance = preview->distance();
        QWheelEvent wheel(QPointF(centre), QPointF(preview->mapToGlobal(centre)), QPoint(), QPoint(0, 120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(preview, &wheel);
        QVERIFY(preview->distance() < initialDistance);
        const float rotatedYaw = preview->yaw();
        QTest::mouseClick(window.findChild<QPushButton *>("fitViewButton"), Qt::LeftButton);
        QCOMPARE(preview->yaw(), rotatedYaw);
        QVERIFY(preview->distance() > 36);
        QTest::mouseClick(window.findChild<QPushButton *>("resetViewButton"), Qt::LeftButton);
        QCOMPARE(preview->yaw(), initialYaw);
        QCOMPARE(preview->pitch(), initialPitch);
        window.resize(1000, 650);
        QTest::qWait(80);
        QVERIFY(preview->renderingReady());
        QVERIFY(!preview->grabFramebuffer().isNull());
        QVERIFY(greenPixelCount(preview->grabFramebuffer()) > 80);
        QVERIFY(preview->distance() > 36 && std::isfinite(preview->distance()));
        window.resize(1450, 850);
        QTest::qWait(80);
        QVERIFY(!preview->grabFramebuffer().isNull());
        window.close();
        for (int i = 0; i < 4; ++i) {
            hvd::MainWindow reopened(BUNDLED_ROOT, user.path());
            reopened.show();
            QVERIFY(select(reopened, "example.synthetic-demo-board"));
            QTest::qWait(40);
            reopened.close();
        }
    }
};
QTEST_MAIN(ModelPreviewTests)
#include "ModelPreviewTests.moc"
