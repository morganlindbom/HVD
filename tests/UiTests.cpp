// UiTests.cpp
#include "ui/ComponentEditor.h"
#include "ui/MainWindow.h"
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTimer>
#include <QtTest>

class UiTests : public QObject {
    Q_OBJECT
  private slots:
    /** Exercise the actual widget workflow with isolated user data.
     *
     * Search, filters, inspection, creation, pins, editing, saving and reopening use normal UI controls.
     */
    void catalogueWorkflow() {
        QTemporaryDir user;
        hvd::MainWindow window(BUNDLED_ROOT, user.path());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *list = window.findChild<QListWidget *>("componentList");
        auto *search = window.findChild<QLineEdit *>("searchField");
        auto *kind = window.findChild<QComboBox *>("kindFilter");
        auto *category = window.findChild<QComboBox *>("categoryFilter");
        auto *detail = window.findChild<QTextBrowser *>("detailPanel");
        QCOMPARE(list->count(), 3);
        QTest::keyClicks(search, "digital");
        QCOMPARE(list->count(), 1);
        QVERIFY(detail->toPlainText().contains("Missing asset"));
        QVERIFY(detail->toPlainText().contains("Physical number"));
        QVERIFY(!window.findChild<QPushButton *>("editComponentButton")->isEnabled());
        search->clear();
        kind->setCurrentIndex(kind->findData("board"));
        QCOMPARE(list->count(), 2);
        category->setCurrentIndex(category->findData("Examples / Sensors"));
        QCOMPARE(list->count(), 0);
        category->setCurrentIndex(0);
        kind->setCurrentIndex(0);
        QTest::mouseClick(window.findChild<QPushButton *>("newComponentButton"), Qt::LeftButton);
        auto *editor = window.findChild<hvd::ComponentEditor *>("componentEditor");
        QVERIFY(editor);
        auto *id = editor->findChild<QLineEdit *>("componentId");
        id->setText("ui.synthetic");
        QTest::keyClicks(editor->findChild<QLineEdit *>("componentName"), "UI demonstration");
        QTest::mouseClick(editor->findChild<QPushButton *>("addPinButton"), Qt::LeftButton);
        auto *pins = editor->findChild<QTableWidget *>("pinsTable");
        QCOMPARE(pins->rowCount(), 1);
        pins->item(0, 0)->setText("output");
        pins->item(0, 1)->setText("Output");
        pins->item(0, 2)->setText("7");
        pins->item(0, 3)->setText("GPIO_EXAMPLE");
        pins->item(0, 4)->setText("OUT");
        pins->item(0, 5)->setText("digital-output");
        QTest::mouseClick(editor->findChild<QPushButton *>("saveComponentButton"), Qt::LeftButton);
        QTRY_VERIFY(!window.findChild<hvd::ComponentEditor *>("componentEditor"));
        QCOMPARE(list->count(), 4);
        QVERIFY(detail->toPlainText().contains("UI demonstration"));
        QTest::mouseClick(window.findChild<QPushButton *>("editComponentButton"), Qt::LeftButton);
        editor = window.findChild<hvd::ComponentEditor *>("componentEditor");
        QVERIFY(editor);
        QVERIFY(editor->findChild<QLineEdit *>("componentId")->isReadOnly());
        editor->findChild<QLineEdit *>("componentName")->setText("UI edited demonstration");
        QTest::mouseClick(editor->findChild<QPushButton *>("saveComponentButton"), Qt::LeftButton);
        QTRY_VERIFY(!window.findChild<hvd::ComponentEditor *>("componentEditor"));
        QVERIFY(detail->toPlainText().contains("Revision: 2"));
        QVERIFY(window.reloadCatalogue());
        search->setText("UI edited");
        QCOMPARE(list->count(), 1);
        QVERIFY(detail->toPlainText().contains("GPIO_EXAMPLE"));
        window.close();
        hvd::MainWindow reopened(BUNDLED_ROOT, user.path());
        reopened.show();
        reopened.findChild<QLineEdit *>("searchField")->setText("UI edited");
        QCOMPARE(reopened.findChild<QListWidget *>("componentList")->count(), 1);
        QVERIFY(reopened.findChild<QTextBrowser *>("detailPanel")->toPlainText().contains("Revision: 2"));
        const QString screenshot = qEnvironmentVariable("HVD_UI_SCREENSHOT_PATH");
        if (!screenshot.isEmpty())
            QVERIFY(reopened.grab().save(screenshot));
        reopened.close();
    }
    /** Verify cancelled dirty closure and discarded edits.
     *
     * The normal application close path must leave the editor and main window open when the user cancels.
     */
    void unsavedChanges() {
        QTemporaryDir user;
        hvd::MainWindow window(BUNDLED_ROOT, user.path());
        window.show();
        QTest::mouseClick(window.findChild<QPushButton *>("newComponentButton"), Qt::LeftButton);
        auto *editor = window.findChild<hvd::ComponentEditor *>("componentEditor");
        QVERIFY(editor);
        editor->findChild<QLineEdit *>("componentName")->setText("Unsaved");
        QVERIFY(editor->dirty());
        // Cancel the standard unsaved-changes prompt.
        //
        // The timer acts only on the visible dialog's normal button.
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box)
                box->button(QMessageBox::Cancel)->click();
        });
        window.close();
        QVERIFY(window.isVisible());
        QVERIFY(editor->isVisible());
        // Discard the detached edit through the standard prompt.
        //
        // No package should be created by cancellation.
        QTimer::singleShot(0, [] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (box)
                box->button(QMessageBox::Discard)->click();
        });
        editor->reject();
        QTRY_VERIFY(!window.findChild<hvd::ComponentEditor *>("componentEditor"));
        QCOMPARE(QDir(user.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 0);
        window.close();
    }
    /** Verify a bundled component can become an independent editable user copy.
     *
     * Copying retains the original definition and reports unavailable assets without failing.
     */
    void bundledCopy() {
        QTemporaryDir user;
        hvd::MainWindow window(BUNDLED_ROOT, user.path());
        window.show();
        window.findChild<QLineEdit *>("searchField")->setText("digital");
        QTest::mouseClick(window.findChild<QPushButton *>("copyComponentButton"), Qt::LeftButton);
        auto *editor = window.findChild<hvd::ComponentEditor *>("componentEditor");
        QVERIFY(editor);
        const QString copiedId = editor->findChild<QLineEdit *>("componentId")->text();
        QVERIFY(copiedId != "example.synthetic-sensor");
        editor->findChild<QLineEdit *>("componentName")->setText("Independent copy");
        QTest::mouseClick(editor->findChild<QPushButton *>("saveComponentButton"), Qt::LeftButton);
        QTRY_VERIFY(!window.findChild<hvd::ComponentEditor *>("componentEditor"));
        QCOMPARE(window.findChild<QListWidget *>("componentList")->count(), 4);
        QVERIFY(window.reloadCatalogue());
        window.findChild<QLineEdit *>("searchField")->setText("Independent copy");
        QCOMPARE(window.findChild<QListWidget *>("componentList")->count(), 1);
        window.findChild<QLineEdit *>("searchField")->setText("Demonstration digital sensor");
        QCOMPARE(window.findChild<QListWidget *>("componentList")->count(), 1);
        window.close();
    }
};
QTEST_MAIN(UiTests)
#include "UiTests.moc"
