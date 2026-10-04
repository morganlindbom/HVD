// MainWindow.h
#pragma once
#include "core/Catalogue.h"
#include <QMainWindow>
#include <QPointer>
class QComboBox;
class QLineEdit;
class QListWidget;
class QTextBrowser;
class QLabel;
class QPushButton;
namespace hvd {
class ComponentEditor;
class ModelPreview;
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    /** Construct the standalone catalogue browser.
     *
     * Explicit roots make startup configuration and isolated UI tests use the same application workflow.
     */
    MainWindow(QString bundledRoot, QString userRoot, QWidget *parent = nullptr);
    /** Reload packages while preserving the previous valid catalogue on error.
     *
     * Diagnostics remain visible in the main window instead of crashing or silently dropping packages.
     */
    bool reloadCatalogue();

  protected:
    /** Check the active editor before closing the application.
     *
     * A cancelled or failed save keeps the application open.
     */
    void closeEvent(QCloseEvent *event) override;

  private:
    /** Populate filter choices from current catalogue metadata.
     *
     * Keeps still-valid selections and refreshes the visible component list.
     */
    void refreshFilters();
    /** Apply search and filter criteria.
     *
     * Stable component IDs are kept in list item data, never inferred from displayed labels.
     */
    void refreshList();
    /** Render a selected definition and its package diagnostics.
     *
     * Escapes text for HTML and explicitly labels missing assets and unknown specifications.
     */
    void inspect();
    /** Open a detached user editor or read-only copy workflow.
     *
     * Copying bundled entries generates a new ID and does not change the original package.
     */
    void edit(bool create, bool copy);
    /** Import a directory package selected by the user.
     *
     * The catalogue validates and stages the import before registering the ID.
     */
    void importComponent();
    /** Export the selected package to a new directory.
     *
     * Existing output directories are never overwritten.
     */
    void exportComponent();
    /** Show operation diagnostics in a persistent status panel.
     *
     * Both errors and nonblocking asset warnings remain inspectable.
     */
    void report(const Diagnostics &diagnostics, const QString &success = {});
    QString bundledRoot_, userRoot_;
    Catalogue catalogue_;
    QLineEdit *search_;
    QComboBox *category_, *kind_;
    QListWidget *list_;
    QTextBrowser *detail_;
    QLabel *diagnostics_;
    QPushButton *editButton_, *copyButton_, *exportButton_;
    ModelPreview *preview_;
    QPointer<ComponentEditor> editor_;
};
} // namespace hvd
