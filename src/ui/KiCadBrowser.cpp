// KiCadBrowser.cpp
#include "KiCadBrowser.h"
#include "ModelPreview.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>
namespace hvd {
namespace {
struct Selection {
    KiCadEntry entry;
    KiCadFootprint footprint;
    ImportedGeometry geometry;
};
/** Escape source text for a read-only detail panel.
 *
 * Paths and library descriptions must not introduce HTML or active document
 * links.
 */
QString safe(const QString &text) { return text.toHtmlEscaped(); }
} // namespace
/** Construct a separate KiCad source browser.
 *
 * Catalogue packages stay under their existing validation authority and are
 * never modified by this panel.
 */
KiCadBrowser::KiCadBrowser(QWidget *parent) : QWidget(parent) {
    setObjectName("kiCadBrowser");
    auto layout = new QVBoxLayout(this);
    auto roots = new QHBoxLayout;
    auto choose = new QPushButton("Choose library root…", this);
    choose->setObjectName("kiCadRootButton");
    auto rescan = new QPushButton("Rescan", this);
    auto cancel = new QPushButton("Cancel loading / scan", this);
    roots->addWidget(choose);
    roots->addWidget(rescan);
    roots->addWidget(cancel);
    additional_ = new QCheckBox("Include additional folders", this);
    additional_->setObjectName("kiCadAdditionalFolders");
    additional_->setToolTip(
        "Also index demos, templates and loose footprint/model files under the selected root.");
    roots->addWidget(additional_);
    roots->addStretch();
    layout->addLayout(roots);
    auto filters = new QHBoxLayout;
    kind_ = new QComboBox(this);
    kind_->setObjectName("kiCadSourceKind");
    kind_->addItem("Footprints", "footprint");
    kind_->addItem("Symbol libraries", "symbol-library");
    kind_->addItem("3D models", "model");
    library_ = new QComboBox(this);
    library_->setObjectName("kiCadLibraryFilter");
    library_->addItem("All libraries", "");
    search_ = new QLineEdit(this);
    search_->setObjectName("kiCadSearch");
    search_->setPlaceholderText("Search library and item name");
    search_->setClearButtonEnabled(true);
    filters->addWidget(kind_);
    filters->addWidget(library_);
    filters->addWidget(search_, 2);
    layout->addLayout(filters);
    auto formats =
        new QLabel("Separate sources: footprint files, symbol-library files and 3D model files. "
                   "Rendering: VRML97 subset; STEP is not parsed (an existing WRL companion may be used).",
                   this);
    formats->setWordWrap(true);
    layout->addWidget(formats);
    auto clear = new QPushButton("Clear library/search filters", this);
    clear->setObjectName("kiCadClearFilters");
    filters->addWidget(clear);
    // Reset only the narrowing filters, retaining the selected source type.
    //
    // An empty search still has complete pagination in that source type.
    connect(clear, &QPushButton::clicked, this, [this] {
        library_->setCurrentIndex(0);
        search_->clear();
        refresh();
    });
    auto split = new QSplitter(this);
    list_ = new QListWidget(split);
    list_->setObjectName("kiCadItems");
    detail_ = new QTextBrowser(split);
    detail_->setObjectName("kiCadDetails");
    detail_->setOpenLinks(false);
    auto panel = new QWidget(split);
    auto view = new QVBoxLayout(panel);
    view->setContentsMargins(4, 0, 0, 0);
    view->addWidget(new QLabel("<h2>Model Preview</h2>", panel));
    preview_ = new ModelPreview(panel);
    preview_->setObjectName("kiCadPreview");
    view->addWidget(preview_, 1);
    auto controls = new QHBoxLayout;
    auto reset = new QPushButton("Reset view", panel);
    reset->setObjectName("kiCadReset");
    auto fit = new QPushButton("Fit to view", panel);
    fit->setObjectName("kiCadFit");
    auto pads = new QCheckBox("Show numbered pads", panel);
    pads->setObjectName("kiCadPadsToggle");
    pads->setChecked(true);
    controls->addWidget(reset);
    controls->addWidget(fit);
    controls->addWidget(pads);
    view->addLayout(controls);
    padInfo_ = new QLabel("Click a numbered marker to inspect its footprint coordinates.", panel);
    padInfo_->setObjectName("kiCadPadInfo");
    padInfo_->setWordWrap(true);
    view->addWidget(padInfo_);
    split->setSizes({280, 390, 620});
    split->setStretchFactor(2, 3);
    layout->addWidget(split, 1);
    auto pages = new QHBoxLayout;
    previous_ = new QPushButton("Previous page", this);
    previous_->setObjectName("kiCadPreviousPage");
    next_ = new QPushButton("Next page", this);
    next_->setObjectName("kiCadNextPage");
    pageInfo_ = new QLabel("No results", this);
    pageInfo_->setObjectName("kiCadPageInfo");
    pages->addWidget(previous_);
    pages->addWidget(pageInfo_, 1);
    pages->addWidget(next_);
    layout->addLayout(pages);
    previous_->setEnabled(false);
    next_->setEnabled(false);
    // Navigate a bounded page without changing filters or the metadata index.
    //
    // Each page can be reached even when many items have identical search terms.
    connect(previous_, &QPushButton::clicked, this, [this] {
        --page_;
        populatePage();
    });
    connect(next_, &QPushButton::clicked, this, [this] {
        ++page_;
        populatePage();
    });
    status_ = new QLabel(this);
    status_->setObjectName("kiCadStatus");
    status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(status_);
    connect(reset, &QPushButton::clicked, preview_, &ModelPreview::resetView);
    connect(fit, &QPushButton::clicked, preview_, &ModelPreview::fitToView);
    connect(pads, &QCheckBox::toggled, preview_, &ModelPreview::showPads);
    // Inspect selected physical markers.
    //
    // Numbers are source identifiers and do not imply electrical functions.
    connect(preview_, &ModelPreview::padSelected, this, [this](int i) {
        const auto &p = preview_->pads().at(i);
        padInfo_->setText(QString("Pad %1 · footprint (%2, %3) mm · %4 / %5 · rotation %6°")
                              .arg(p.number.isEmpty() ? "(unnumbered)" : p.number)
                              .arg(p.position.x())
                              .arg(p.position.y())
                              .arg(p.type, p.shape)
                              .arg(p.rotation));
    });
    // Persist a user-selected external root.
    //
    // Source libraries remain read-only regardless of the chosen folder.
    connect(choose, &QPushButton::clicked, this, [this] {
        auto root = QFileDialog::getExistingDirectory(
            this, "Select KiCad root containing footprints, symbols and 3dmodels", root_);
        if (!root.isEmpty())
            configureRoot(root, true);
    });
    connect(rescan, &QPushButton::clicked, this, &KiCadBrowser::scan);
    // Cancel both indexing and model loading.
    //
    // Invalidating generations discards already-completed but not yet delivered
    // asynchronous results.
    connect(cancel, &QPushButton::clicked, this, [this] {
        if (scanCancel_)
            scanCancel_->store(true);
        if (loadCancel_)
            loadCancel_->store(true);
        ++scanGeneration_;
        ++selectionGeneration_;
        scanPending_ = false;
        started_ = indexComplete_;
        scanState_ = indexComplete_ ? "Cancelled; retaining last complete index (Rescan to refresh)"
                                    : "Cancelled; no completed index (Rescan or reopen this tab)";
        preview_->setGeometry({});
        setProperty("loadedSourceId", QString{});
        status_->setText(scanState_ + " · root: " + root_);
    });
    // Repopulate library choices for the distinct source kind.
    //
    // Symbol filenames are indexed without claiming symbol pin parsing.
    connect(kind_, &QComboBox::currentIndexChanged, this, [this] {
        QSignalBlocker block(library_);
        library_->clear();
        library_->addItem("All libraries", "");
        QStringList names;
        for (const auto &e : index_.entries)
            if (e.kind == kind_->currentData())
                names.append(e.library);
        names.removeDuplicates();
        names.sort();
        for (const auto &name : names)
            library_->addItem(name, name);
        refresh();
    });
    connect(library_, &QComboBox::currentIndexChanged, this, &KiCadBrowser::refresh);
    auto debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(160);
    // Debounce searches over the metadata index.
    //
    // Model files are never opened by a text-filter operation.
    connect(search_, &QLineEdit::textChanged, debounce, [debounce] { debounce->start(); });
    connect(debounce, &QTimer::timeout, this, &KiCadBrowser::refresh);
    connect(list_, &QListWidget::currentRowChanged, this, &KiCadBrowser::inspect);
    connect(additional_, &QCheckBox::toggled, this, &KiCadBrowser::scan);
    QString installation = QDir(QCoreApplication::applicationDirPath()).filePath("kicad");
#ifdef HVD_KICAD_DEVELOPMENT_ROOT
    if (!QDir(installation).exists())
        installation = QString::fromUtf8(HVD_KICAD_DEVELOPMENT_ROOT);
#endif
    root_ = QSettings().value("kicad/libraryRoot", installation).toString();
    status_->setText("Read-only KiCad Libraries · root: " + root_ + " · open this tab to index.");
}
/** Cancel asynchronous jobs at teardown.
 *
 * Worker closures contain copied inputs and cancellation flags, never widget
 * pointers.
 */
KiCadBrowser::~KiCadBrowser() {
    if (scanCancel_)
        scanCancel_->store(true);
    if (loadCancel_)
        loadCancel_->store(true);
}
/** Configure the root through the existing settings mechanism.
 *
 * Root changes invalidate selection work and clear geometry before a new index
 * is requested.
 */
void KiCadBrowser::configureRoot(const QString &root, bool persist) {
    root_ = QFileInfo(root).absoluteFilePath();
    if (persist)
        QSettings().setValue("kicad/libraryRoot", root_);
    index_ = {};
    indexComplete_ = false;
    page_ = 0;
    list_->clear();
    library_->setCurrentIndex(0);
    detail_->clear();
    setProperty("loadedSourceId", QString{});
    if (loadCancel_)
        loadCancel_->store(true);
    ++selectionGeneration_;
    preview_->setGeometry({});
    scan();
}
/** Activate lazy source discovery.
 *
 * Reopening the tab reuses metadata until a root change or explicit rescan
 * occurs.
 */
void KiCadBrowser::activate() {
    if (!started_)
        scan();
}
/** Request a reproducible demonstration through visible controls.
 *
 * Qualified library and item names avoid collisions with symbols and generic
 * model filenames.
 */
void KiCadBrowser::demonstrate(const QString &qualified) {
    desired_ = qualified;
    kind_->setCurrentIndex(0);
    search_->setText(qualified);
    activate();
    if (!index_.entries.isEmpty())
        refresh();
}
/** Scan file metadata outside the UI thread.
 *
 * Cancelled scans retain the previously completed index and late results cannot
 * cross root boundaries.
 */
void KiCadBrowser::scan() {
    started_ = true;
    scanPending_ = true;
    if (scanCancel_)
        scanCancel_->store(true);
    scanCancel_ = std::make_shared<std::atomic_bool>(false);
    const auto cancel = scanCancel_;
    const auto generation = ++scanGeneration_;
    const auto root = root_;
    const bool additional = additional_->isChecked();
    scanState_ = indexComplete_ ? "Scanning; displaying last complete index until completion"
                                : "Scanning; no completed index yet";
    status_->setText(scanState_ + " · " + root);
    ++selectionGeneration_;
    if (loadCancel_)
        loadCancel_->store(true);
    preview_->setGeometry({});
    setProperty("loadedSourceId", QString{});
    auto watcher = new QFutureWatcher<KiCadIndex>(this);
    // Publish only the current completed scan.
    //
    // Watcher destruction disconnects delivery without blocking the GUI on worker
    // execution.
    connect(watcher, &QFutureWatcher<KiCadIndex>::finished, this, [this, watcher, generation] {
        auto result = watcher->result();
        watcher->deleteLater();
        if (generation != scanGeneration_ || result.cancelled)
            return;
        index_ = std::move(result);
        scanPending_ = false;
        indexComplete_ = true;
        scanState_ = "Complete metadata index (Rescan after changing files)";
        QStringList names;
        for (const auto &e : index_.entries)
            if (e.kind == kind_->currentData())
                names.append(e.library);
        names.removeDuplicates();
        names.sort();
        {
            QSignalBlocker block(library_);
            library_->clear();
            library_->addItem("All libraries", "");
            for (const auto &n : names)
                library_->addItem(n, n);
        }
        refresh();
    });
    // Capture immutable worker inputs.
    //
    // No OpenGL or QWidget API is called during filesystem enumeration.
    watcher->setFuture(
        QtConcurrent::run([root, cancel, additional] { return scanKiCad(root, cancel, additional); }));
}
/** Filter qualified indexed source names.
 *
 * Filter changes return to the first page so old offsets cannot hide new matches.
 */
void KiCadBrowser::refresh() {
    page_ = 0;
    populatePage();
}
/** Populate a bounded page from all matching indexed filenames.
 *
 * The displayed range, total and active filters make excluded matches visible
 * without parsing any model to populate the list.
 */
void KiCadBrowser::populatePage() {
    QString selected =
        list_->currentItem() ? list_->currentItem()->data(Qt::UserRole + 1).toString() : QString{};
    QSignalBlocker block(list_);
    list_->clear();
    int matches = 0;
    int footprints = 0, symbols = 0, vrml = 0, step = 0;
    QString query = search_->text().trimmed();
    for (int i = 0; i < index_.entries.size(); ++i) {
        const auto &e = index_.entries[i];
        footprints += e.kind == "footprint";
        symbols += e.kind == "symbol-library";
        vrml += e.kind == "model" && e.name.endsWith(".wrl", Qt::CaseInsensitive);
        step += e.kind == "model" && e.name.endsWith(".step", Qt::CaseInsensitive);
        if (e.kind != kind_->currentData() ||
            (!library_->currentData().toString().isEmpty() && e.library != library_->currentData()) ||
            !e.id().contains(query, Qt::CaseInsensitive))
            continue;
        ++matches;
        if (matches > page_ * 1000 && list_->count() < 1000) {
            QString label = e.library + ":" + e.name;
            if (e.kind == "model" && e.name.endsWith(".step", Qt::CaseInsensitive))
                label += " [STEP not parsed]";
            auto item = new QListWidgetItem(label, list_);
            item->setData(Qt::UserRole, i);
            item->setData(Qt::UserRole + 1, e.id());
            item->setToolTip(e.path);
        }
    }
    int pages = (matches + 999) / 1000;
    previous_->setEnabled(page_ > 0);
    next_->setEnabled(page_ + 1 < pages);
    pageInfo_->setText(
        QString("Page %1 of %2 · showing %3–%4 of %5 matches · 1,000 results per page; all matches reachable")
            .arg(matches ? page_ + 1 : 0)
            .arg(pages)
            .arg(matches ? page_ * 1000 + 1 : 0)
            .arg(page_ * 1000 + list_->count())
            .arg(matches));
    status_->setText(QString("%1 matches · showing %2 · %3 indexed sources · root: %4\n%5\n%6 · Type: %7 · "
                             "Library: %8 · Search: %9")
                         .arg(matches)
                         .arg(list_->count())
                         .arg(index_.entries.size())
                         .arg(root_, index_.diagnostics.join("\n"))
                         .arg(scanState_, kind_->currentText(), library_->currentText(),
                              query.isEmpty() ? "(none)" : query));
    status_->setText(status_->text() +
                     QString("\nIndex: %1 footprints · %2 symbol libraries · %3 VRML · %4 STEP (not parsed)")
                         .arg(footprints)
                         .arg(symbols)
                         .arg(vrml)
                         .arg(step));
    int row = list_->count() ? 0 : -1;
    for (int i = 0; i < list_->count(); ++i) {
        auto item = list_->item(i);
        if (item->data(Qt::UserRole + 1) == selected || (!desired_.isEmpty() && item->text() == desired_)) {
            row = i;
            desired_.clear();
            break;
        }
    }
    list_->setCurrentRow(row);
    inspect();
}
/** Load a selected source with stale-result protection.
 *
 * Previous geometry clears immediately, and only the current selection may
 * upload its completed CPU mesh.
 */
void KiCadBrowser::inspect() {
    setProperty("loadedSourceId", QString{});
    if (loadCancel_)
        loadCancel_->store(true);
    loadCancel_ = std::make_shared<std::atomic_bool>(false);
    auto cancel = loadCancel_;
    auto generation = ++selectionGeneration_;
    preview_->setGeometry({});
    padInfo_->setText("Click a numbered marker to inspect its footprint coordinates.");
    if (!list_->currentItem()) {
        detail_->setHtml("<p>No matching source.</p>");
        return;
    }
    auto entry = index_.entries.at(list_->currentItem()->data(Qt::UserRole).toInt());
    auto root = root_;
    detail_->setHtml("<h2>" + safe(entry.name) + "</h2><p>Read-only " + safe(entry.kind) + " · " +
                     safe(entry.id()) + "</p><p>" + safe(entry.path) +
                     "</p><p>Loading metadata and supported geometry…</p>");
    auto watcher = new QFutureWatcher<Selection>(this);
    // Deliver only the latest source result.
    //
    // GPU upload is scheduled by ModelPreview and occurs with its OpenGL context
    // current.
    connect(watcher, &QFutureWatcher<Selection>::finished, this, [this, watcher, generation] {
        auto r = watcher->result();
        watcher->deleteLater();
        if (generation != selectionGeneration_)
            return;
        QString html = "<h2>" + safe(r.entry.name) + "</h2><p><b>Source identity:</b> " + safe(r.entry.id()) +
                       "<br><b>Path:</b> " + safe(r.entry.path) +
                       "</p><p>Generic library source; manufacturer and electrical "
                       "verification unknown. No symbol-to-pad mapping inferred.</p>";
        if (r.entry.kind == "footprint") {
            html += "<p>" + safe(r.footprint.description) +
                    "</p><h3>Pads · mm</h3><p>Repeated numbers and unnumbered "
                    "mechanical pads are retained.</p><ul>";
            for (const auto &p : r.footprint.pads)
                html += QString("<li>%1 · %2 / %3 · (%4, %5) · %6 × %7 · %8°</li>")
                            .arg(safe(p.number.isEmpty() ? "(unnumbered)" : p.number), safe(p.type),
                                 safe(p.shape))
                            .arg(p.position.x())
                            .arg(p.position.y())
                            .arg(p.size.x())
                            .arg(p.size.y())
                            .arg(p.rotation);
            html += "</ul><h3>Model references</h3>";
            for (const auto &m : r.footprint.models)
                html += "<p>" + safe(m.reference) +
                        QString("<br>Offset: (%1,%2,%3) mm; rotation: (%4,%5,%6)°; "
                                "scale: (%7,%8,%9)</p>")
                            .arg(m.offset.x())
                            .arg(m.offset.y())
                            .arg(m.offset.z())
                            .arg(m.rotation.x())
                            .arg(m.rotation.y())
                            .arg(m.rotation.z())
                            .arg(m.scale.x())
                            .arg(m.scale.y())
                            .arg(m.scale.z());
        }
        html += "<h3>Loaded model paths</h3>";
        for (const auto &p : r.geometry.modelPaths)
            html += "<p>" + safe(p) + "</p>";
        html += "<h3>Diagnostics / license evidence</h3>";
        for (const auto &d : r.geometry.diagnostics)
            html += "<p>" + safe(d) + "</p>";
        html += index_.licenses.isEmpty() ? "<p>No standalone license file discovered. Embedded "
                                            "model notices are shown above when present.</p>"
                                          : "<p>Discovered license files (association requires "
                                            "review):<br>" +
                                                safe(index_.licenses.join("\n")) + "</p>";
        if (!r.geometry.error.isEmpty())
            html += "<p><b>" + safe(r.geometry.error) + "</b></p>";
        detail_->setHtml(html);
        preview_->setGeometry(r.geometry);
        setProperty("loadedSourceId", r.entry.id());
    });
    // Parse metadata and geometry entirely outside presentation code.
    //
    // Missing files return explicit errors, not a procedural demo model.
    watcher->setFuture(QtConcurrent::run([root, entry, cancel] {
        Selection r;
        r.entry = entry;
        try {
            if (entry.kind == "footprint")
                r.footprint = loadFootprint(entry.path);
            r.geometry = loadKiCadGeometry(root, entry, cancel);
        } catch (const std::exception &e) {
            r.geometry.error = QString::fromUtf8(e.what());
        }
        return r;
    }));
}
} // namespace hvd
