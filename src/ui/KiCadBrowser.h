// KiCadBrowser.h
#pragma once
#include "storage/KiCadLibrary.h"
#include <QWidget>
class QComboBox;
class QLineEdit;
class QListWidget;
class QTextBrowser;
class QLabel;
class QPushButton;
class QCheckBox;
namespace hvd {
class ModelPreview;
class KiCadBrowser : public QWidget {
    Q_OBJECT
  public:
    /** Construct the read-only external-library browser.
     *
     * Indexing begins on activation and model loading is deferred until a source
     * is selected.
     */
    explicit KiCadBrowser(QWidget *parent = nullptr);
    /** Cancel outstanding work before widget destruction.
     *
     * Worker functions own value inputs and never capture the widget pointer.
     */
    ~KiCadBrowser() override;
    /** Set an explicit library root and begin indexing.
     *
     * UI folder choices persist through the application's existing QSettings
     * configuration.
     */
    void configureRoot(const QString &root, bool persist = false);
    /** Begin deferred discovery when the source tab becomes visible.
     *
     * Catalogue-only launches do not scan thousands of external library files.
     */
    void activate();
    /** Request a qualified footprint selection after indexing.
     *
     * The normal search, filtering and list-selection controls perform the
     * demonstration flow.
     */
    void demonstrate(const QString &qualified);

  private:
    /** Start cancellable metadata indexing outside the GUI thread.
     *
     * A cancelled or superseded result cannot replace the active root's current
     * index.
     */
    void scan();
    /** Apply library, kind and text filters to indexed filenames.
     *
     * Reset pagination after filter changes, retaining a bounded visible page
     * without hiding matches from navigation.
     */
    void refresh();
    /** Display one page of the complete filtered index.
     *
     * Next/previous navigation reaches every match without reading model files
     * during filtering or populating more than 1000 list rows.
     */
    void populatePage();
    /** Clear stale geometry and asynchronously load the selected source.
     *
     * A generation counter prevents old selections from publishing late meshes.
     */
    void inspect();
    QString root_, desired_;
    KiCadIndex index_;
    bool started_ = false;
    bool scanPending_ = false, indexComplete_ = false;
    int page_ = 0;
    QString scanState_ = "Not scanned";
    quint64 scanGeneration_ = 0, selectionGeneration_ = 0;
    Cancellation scanCancel_, loadCancel_;
    QComboBox *kind_, *library_;
    QLineEdit *search_;
    QListWidget *list_;
    QTextBrowser *detail_;
    QLabel *status_, *padInfo_;
    QLabel *pageInfo_;
    QPushButton *previous_, *next_;
    QCheckBox *additional_;
    ModelPreview *preview_;
};
} // namespace hvd
