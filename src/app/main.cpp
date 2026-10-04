// main.cpp
#include "ui/KiCadBrowser.h"
#include "ui/MainWindow.h"
#include "ui/ModelPreview.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QListWidget>
#include <QPainter>
#include <QScreen>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTimer>
#include <algorithm>

/** Start the standalone component manager.
 *
 * Package roots are configurable; default user packages are isolated in
 * per-user application data.
 */
int main(int argc, char **argv) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName("HVD");
    QCoreApplication::setApplicationName("ComponentManager");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Standalone HVD Component Manager");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"bundled-root", "Bundled read-only package directory.", "directory",
                      QDir(QCoreApplication::applicationDirPath()).filePath("components")});
    parser.addOption(
        {"user-root", "User package directory.", "directory",
         QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("components")});
    parser.addOption({"smoke-exit", "Exit automatically after startup for launch validation."});
    parser.addOption({"select", "Select a component ID for a demonstration.", "component-id"});
    parser.addOption({"screenshot", "Capture the actual running window after rendering.", "png-path"});
    parser.addOption({"kicad-root", "Explicit read-only KiCad library root.", "directory"});
    parser.addOption({"kicad-select", "Demonstrate a library:item footprint through the KiCad browser.",
                      "qualified-name"});
    parser.process(application);
    hvd::MainWindow window(parser.value("bundled-root"), parser.value("user-root"));
    if (const auto *screen = QGuiApplication::primaryScreen()) {
        const QSize available = screen->availableGeometry().size();
        window.resize(std::min(1440, static_cast<int>(available.width() * 0.94)),
                      std::min(860, static_cast<int>(available.height() * 0.92)));
        window.move(screen->availableGeometry().topLeft() + QPoint(20, 12));
    }
    window.show();
    if (parser.isSet("select")) {
        auto *list = window.findChild<QListWidget *>("componentList");
        for (int row = 0; row < list->count(); ++row)
            if (list->item(row)->data(Qt::UserRole).toString() == parser.value("select"))
                list->setCurrentRow(row);
    }
    auto *kiCad = window.findChild<hvd::KiCadBrowser *>();
    if (parser.isSet("kicad-root"))
        kiCad->configureRoot(parser.value("kicad-root"));
    if (parser.isSet("kicad-select")) {
        window.findChild<QTabWidget *>("browserSources")->setCurrentIndex(1);
        kiCad->demonstrate(parser.value("kicad-select"));
    }
    if (parser.isSet("screenshot")) {
        const QString path = parser.value("screenshot");
        // Capture the running application after OpenGL has painted.
        //
        // This is a real application image rather than a generated illustration or
        // widget-test mockup.
        QTimer::singleShot(parser.isSet("kicad-select") ? 10000 : 1200, &window, [&window, path] {
            QDir().mkpath(QFileInfo(path).absolutePath());
            window.raise();
            window.activateWindow();
            QPixmap image = window.grab();
            // Include the live OpenGL framebuffer with its QPainter annotations.
            //
            // QWidget capture alone may omit OpenGL overlay text on Windows; no geometry is recreated here.
            for (auto *preview : window.findChildren<hvd::ModelPreview *>()) {
                if (!preview->isVisible())
                    continue;
                const QImage frame = preview->grabFramebuffer();
                QPainter painter(&image);
                painter.drawImage(QRect(preview->mapTo(&window, QPoint{}), preview->size()), frame);
            }
            window.setProperty("screenshotSaved", !image.isNull() && image.save(path));
        });
    }
    if (parser.isSet("smoke-exit"))
        QTimer::singleShot(parser.isSet("kicad-select") ? 14000 : (parser.isSet("screenshot") ? 2500 : 1500),
                           &application, &QApplication::quit);
    return application.exec();
}
