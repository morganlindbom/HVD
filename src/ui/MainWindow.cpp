// MainWindow.cpp
#include "MainWindow.h"
#include "ComponentEditor.h"
#include "KiCadBrowser.h"
#include "ModelPreview.h"
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

namespace hvd {
namespace {
/** Escape catalogue text for HTML presentation.
 *
 * Empty strings are shown as unknown rather than as invented identity or
 * electrical data.
 */
QString display(const QString &value) { return value.isEmpty() ? "<i>Unknown</i>" : value.toHtmlEscaped(); }
/** Render electrical limits with their units.
 *
 * Unknown endpoints stay explicit; support metadata is never described as
 * compatibility certification.
 */
QString limitsHtml(const QVector<ElectricalLimit> &limits) {
    if (limits.isEmpty())
        return "<p>Electrical limits: unknown.</p>";
    QString html = "<ul>";
    for (const auto &limit : limits) {
        const QString minimum = limit.minimum ? QString::number(*limit.minimum) : "unknown";
        const QString maximum = limit.maximum ? QString::number(*limit.maximum) : "unknown";
        html += "<li>" + display(limit.name) + ": " + minimum + " to " + maximum + " " + display(limit.unit) +
                "</li>";
    }
    return html + "</ul>";
}
} // namespace
/** Construct the standalone catalogue browser.
 *
 * Explicit roots make startup configuration and isolated UI tests use the same
 * application workflow.
 */
MainWindow::MainWindow(QString bundledRoot, QString userRoot, QWidget *parent)
    : QMainWindow(parent), bundledRoot_(std::move(bundledRoot)), userRoot_(std::move(userRoot)) {
    setObjectName("componentManager");
    setWindowTitle("HVD Component Manager");
    resize(1240, 820);
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    setCentralWidget(central);
    auto *title = new QLabel("<h1>Component Manager</h1><p>Reusable hardware "
                             "definitions · Catalogue support "
                             "is not electrical certification</p>",
                             this);
    layout->addWidget(title);
    auto *filters = new QHBoxLayout;
    search_ = new QLineEdit(this);
    search_->setObjectName("searchField");
    search_->setPlaceholderText("Search name, manufacturer, part number or tags");
    search_->setClearButtonEnabled(true);
    category_ = new QComboBox(this);
    category_->setObjectName("categoryFilter");
    kind_ = new QComboBox(this);
    kind_->setObjectName("kindFilter");
    kind_->addItem("All kinds", "");
    for (const auto &kind : componentKinds())
        kind_->addItem(kind, kind);
    filters->addWidget(search_, 3);
    filters->addWidget(category_, 1);
    filters->addWidget(kind_, 1);
    layout->addLayout(filters);
    auto *splitter = new QSplitter(this);
    list_ = new QListWidget(this);
    list_->setObjectName("componentList");
    detail_ = new QTextBrowser(this);
    detail_->setObjectName("detailPanel");
    detail_->setOpenLinks(false);
    detail_->setOpenExternalLinks(false);
    splitter->addWidget(list_);
    splitter->addWidget(detail_);
    auto *modelPanel = new QWidget(this);
    auto *modelLayout = new QVBoxLayout(modelPanel);
    modelLayout->setContentsMargins(4, 0, 0, 0);
    auto *modelTitle = new QLabel("<h2>Model Preview</h2>", modelPanel);
    modelLayout->addWidget(modelTitle);
    preview_ = new ModelPreview(modelPanel);
    modelLayout->addWidget(preview_, 1);
    auto *viewControls = new QHBoxLayout;
    auto *resetView = new QPushButton("Reset view", modelPanel);
    resetView->setObjectName("resetViewButton");
    auto *fitView = new QPushButton("Fit to view", modelPanel);
    fitView->setObjectName("fitViewButton");
    viewControls->addWidget(resetView);
    viewControls->addWidget(fitView);
    viewControls->addStretch();
    modelLayout->addLayout(viewControls);
    connect(resetView, &QPushButton::clicked, preview_, &ModelPreview::resetView);
    connect(fitView, &QPushButton::clicked, preview_, &ModelPreview::fitToView);
    splitter->addWidget(modelPanel);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 3);
    splitter->setSizes({230, 410, 600});
    layout->addWidget(splitter, 1);
    auto *actions = new QHBoxLayout;
    auto *create = new QPushButton("New component", this);
    create->setObjectName("newComponentButton");
    editButton_ = new QPushButton("Edit user component", this);
    editButton_->setObjectName("editComponentButton");
    copyButton_ = new QPushButton("Create editable copy", this);
    copyButton_->setObjectName("copyComponentButton");
    auto *import = new QPushButton("Import package…", this);
    import->setObjectName("importComponentButton");
    exportButton_ = new QPushButton("Export package…", this);
    exportButton_->setObjectName("exportComponentButton");
    auto *reload = new QPushButton("Reload", this);
    for (auto *button : {create, editButton_, copyButton_, import, exportButton_, reload})
        actions->addWidget(button);
    actions->addStretch();
    layout->addLayout(actions);
    diagnostics_ = new QLabel(this);
    diagnostics_->setObjectName("catalogueDiagnostics");
    diagnostics_->setWordWrap(true);
    diagnostics_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(diagnostics_);
    connect(search_, &QLineEdit::textChanged, this, &MainWindow::refreshList);
    connect(category_, &QComboBox::currentIndexChanged, this, &MainWindow::refreshList);
    connect(kind_, &QComboBox::currentIndexChanged, this, &MainWindow::refreshList);
    connect(list_, &QListWidget::currentRowChanged, this, &MainWindow::inspect);
    // Start a new detached definition.
    //
    // Catalogue registration occurs only after the editor successfully saves.
    connect(create, &QPushButton::clicked, this, [this] { edit(true, false); });
    // Edit an existing user package.
    //
    // Bundled selections disable this action and remain immutable.
    connect(editButton_, &QPushButton::clicked, this, [this] { edit(false, false); });
    // Copy a definition under a new identity.
    //
    // Referenced assets are staged from the owning source package.
    connect(copyButton_, &QPushButton::clicked, this, [this] { edit(false, true); });
    connect(import, &QPushButton::clicked, this, &MainWindow::importComponent);
    connect(exportButton_, &QPushButton::clicked, this, &MainWindow::exportComponent);
    connect(reload, &QPushButton::clicked, this, &MainWindow::reloadCatalogue);
    // Open only supported document assets after rechecking containment.
    //
    // Executables and arbitrary external links are never dispatched from package
    // metadata.
    connect(detail_, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        if (!list_->currentItem() || url.scheme() != "asset")
            return;
        const auto *package = catalogue_.find(list_->currentItem()->data(Qt::UserRole).toString());
        if (!package)
            return;
        const QString role = url.path();
        Diagnostics diagnostics;
        const QString path = PackageStore::resolveAsset(
            package->directory, package->definition.assets.value(role).toString(), diagnostics);
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (!hasErrors(diagnostics) && QFileInfo(path).isFile() &&
            QStringList{"pdf", "txt", "png", "jpg", "jpeg", "bmp"}.contains(suffix)) {
            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
                diagnostics.append(
                    {Diagnostic::Severity::Error, path, "No document application could open this asset."});
        } else if (!hasErrors(diagnostics))
            diagnostics.append({Diagnostic::Severity::Warning, role,
                                "Missing asset or document type not supported for opening."});
        report(diagnostics);
    });
    auto *sources = new QTabWidget(this);
    sources->setObjectName("browserSources");
    auto *cataloguePanel = takeCentralWidget();
    sources->addTab(cataloguePanel, "Component Catalogue");
    auto *kiCad = new KiCadBrowser(sources);
    sources->addTab(kiCad, "KiCad Libraries");
    setCentralWidget(sources);
    // Activate read-only external metadata discovery on demand.
    //
    // The component catalogue and its editing actions retain their existing
    // authority.
    connect(sources, &QTabWidget::currentChanged, kiCad, [kiCad](int index) {
        if (index == 1)
            kiCad->activate();
    });
    reloadCatalogue();
    const QString remembered = QSettings().value("catalogue/selectedId").toString();
    const QString preferred = catalogue_.find(remembered) ? remembered : "example.synthetic-demo-board";
    for (int row = 0; row < list_->count(); ++row)
        if (list_->item(row)->data(Qt::UserRole).toString() == preferred)
            list_->setCurrentRow(row);
}
/** Reload packages while preserving the previous valid catalogue on error.
 *
 * Diagnostics remain visible in the main window instead of crashing or silently
 * dropping packages.
 */
bool MainWindow::reloadCatalogue() {
    Diagnostics diagnostics;
    const bool success = catalogue_.load(bundledRoot_, userRoot_, diagnostics);
    report(diagnostics,
           success ? "Catalogue loaded. User packages: " + userRoot_ : "Previous valid catalogue retained.");
    refreshFilters();
    return success;
}
/** Populate filter choices from current catalogue metadata.
 *
 * Keeps still-valid selections and refreshes the visible component list.
 */
void MainWindow::refreshFilters() {
    const QString selected = category_->currentData().toString();
    QSignalBlocker blocker(category_);
    category_->clear();
    category_->addItem("All categories", "");
    QStringList categories;
    for (const auto &package : catalogue_.search())
        categories.append(package.definition.category);
    categories.removeDuplicates();
    categories.sort(Qt::CaseInsensitive);
    for (const auto &category : categories)
        category_->addItem(category, category);
    const int index = category_->findData(selected);
    category_->setCurrentIndex(index < 0 ? 0 : index);
    refreshList();
}
/** Apply search and filter criteria.
 *
 * Stable component IDs are kept in list item data, never inferred from
 * displayed labels.
 */
void MainWindow::refreshList() {
    const QString selected =
        list_->currentItem() ? list_->currentItem()->data(Qt::UserRole).toString() : QString{};
    list_->clear();
    for (const auto &package : catalogue_.search(search_->text(), category_->currentData().toString(),
                                                 kind_->currentData().toString())) {
        auto *item = new QListWidgetItem(package.definition.name + "\n" + package.definition.id +
                                             (package.bundled ? " · bundled" : " · user"),
                                         list_);
        item->setData(Qt::UserRole, package.definition.id);
        if (package.definition.id == selected)
            list_->setCurrentItem(item);
    }
    if (!list_->currentItem() && list_->count())
        list_->setCurrentRow(0);
    inspect();
}
/** Render a selected definition and its package diagnostics.
 *
 * Escapes text for HTML and explicitly labels missing assets and unknown
 * specifications.
 */
void MainWindow::inspect() {
    const auto *package =
        list_->currentItem() ? catalogue_.find(list_->currentItem()->data(Qt::UserRole).toString()) : nullptr;
    editButton_->setEnabled(package && !package->bundled);
    copyButton_->setEnabled(package);
    exportButton_->setEnabled(package);
    if (!package) {
        preview_->setModel(QJsonValue::Null);
        detail_->setHtml("<h2>No component selected</h2><p>Choose a component or "
                         "adjust the filters.</p>");
        return;
    }
    const auto &d = package->definition;
    preview_->setModel(d.extensions.value("model"));
    QString html = "<h2>" + display(d.name) + "</h2><p><b>ID:</b> " + display(d.id) + " · <b>Revision:</b> " +
                   QString::number(d.revision) + " · <b>Schema:</b> " + QString::number(d.schemaVersion) +
                   "</p>";
    html += "<p><b>Manufacturer:</b> " + display(d.manufacturer) + "<br><b>Part number:</b> " +
            display(d.partNumber) + "<br><b>Category:</b> " + display(d.category) + " · " + display(d.kind) +
            "<br><b>Tags:</b> " + display(d.tags.join(", ")) + "</p>";
    html += "<p>" + d.description.toHtmlEscaped().replace('\n', "<br>") + "</p>";
    html += "<p><b>Verification:</b> " + display(d.verificationStatus) +
            (package->bundled ? " · Bundled / read-only" : " · User package") +
            "<br>No hardware or electrical compatibility is certified.</p>";
    html += limitsHtml(d.electricalLimits) + "<h3>Pins</h3>";
    if (d.pins.isEmpty())
        html += "<p>No pins specified.</p>";
    for (const auto &pin : d.pins) {
        html += "<p><b>" + display(pin.name) + "</b> [" + display(pin.id) +
                "]<br>Physical number: " + display(pin.physicalNumber) +
                " · GPIO: " + display(pin.gpioIdentifier) +
                " · Logical signal: " + display(pin.logicalSignal) +
                "<br>Capabilities: " + display(pin.capabilities.join(", ")) + "</p>" +
                limitsHtml(pin.electricalLimits);
    }
    html += "<h3>Configurable properties</h3>";
    if (d.properties.isEmpty())
        html += "<p>No properties specified.</p>";
    for (const auto &property : d.properties) {
        const QString defaultText =
            property.defaultValue.isNull()
                ? "Unknown"
                : QString::fromUtf8(
                      QJsonDocument(QJsonArray{property.defaultValue}).toJson(QJsonDocument::Compact));
        html += "<p><b>" + display(property.name) + "</b> [" + display(property.id) + "] · " +
                display(property.type) + "<br>Default: " + defaultText.toHtmlEscaped() + "<br>Constraints: " +
                QString::fromUtf8(QJsonDocument(property.constraints).toJson(QJsonDocument::Compact))
                    .toHtmlEscaped() +
                "</p>";
    }
    html += "<h3>Assets</h3>";
    for (const auto &role : {QString("datasheet"), QString("image"), QString("symbol"), QString("footprint"),
                             QString("model3d")}) {
        const auto reference = d.assets.value(role);
        if (reference.isUndefined() || reference.isNull()) {
            html += "<p>" + role + ": Unknown / not supplied</p>";
            continue;
        }
        Diagnostics diagnostics;
        const QString path =
            PackageStore::resolveAsset(package->directory, reference.toString(), diagnostics);
        html += "<p>" + role + ": " + display(reference.toString());
        if (hasErrors(diagnostics))
            html += " · Invalid path";
        else if (!QFileInfo(path).isFile())
            html += " · <b>Missing asset</b>";
        else if (QStringList{"pdf", "txt", "png", "jpg", "jpeg", "bmp"}.contains(
                     QFileInfo(path).suffix().toLower()))
            html += " · <a href=\"asset:" + role + "\">Open document</a>";
        else
            html += " · Available (reference only)";
        html += "</p>";
    }
    html += "<h3>Source references</h3>";
    if (d.sources.isEmpty())
        html += "<p>No manufacturer documents supplied or inspected.</p>";
    for (const auto &source : d.sources)
        html += "<p>" + display(source.title) + "<br>" + display(source.uri) +
                (source.inspected ? " · Marked inspected by author" : " · Not inspected") + "</p>";
    html += "<p><b>Package directory:</b> " + display(package->directory) + "</p>";
    detail_->setHtml(html);
}
/** Open a detached user editor or read-only copy workflow.
 *
 * Copying bundled entries generates a new ID and does not change the original
 * package.
 */
void MainWindow::edit(bool create, bool copy) {
    if (editor_) {
        editor_->raise();
        return;
    }
    ComponentDefinition definition;
    QString existingId, sourceId;
    if (create) {
        definition.id = "user-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        definition.category = "User components";
    } else {
        if (!list_->currentItem())
            return;
        const auto *package = catalogue_.find(list_->currentItem()->data(Qt::UserRole).toString());
        if (!package || (package->bundled && !copy))
            return;
        definition = package->definition;
        if (copy) {
            sourceId = definition.id;
            definition.id = "user-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
            definition.name += " (copy)";
            definition.revision = 1;
            if (definition.verificationStatus == "source-reviewed")
                definition.verificationStatus = "unverified";
        } else
            existingId = definition.id;
    }
    // Save through the catalogue's user-package boundary.
    //
    // Only a successful disk commit updates the list and selected identity.
    auto save = [this, existingId, sourceId](const ComponentDefinition &edited, Diagnostics &diagnostics) {
        if (!catalogue_.saveUser(edited, userRoot_, existingId, diagnostics, sourceId))
            return false;
        report(diagnostics, "Saved revision " + QString::number(edited.revision) + ".");
        search_->clear();
        kind_->setCurrentIndex(0);
        category_->setCurrentIndex(0);
        refreshFilters();
        for (int row = 0; row < list_->count(); ++row)
            if (list_->item(row)->data(Qt::UserRole).toString() == edited.id)
                list_->setCurrentRow(row);
        return true;
    };
    editor_ = new ComponentEditor(definition, !existingId.isEmpty(), save, this);
    editor_->open();
}
/** Import a directory package selected by the user.
 *
 * The catalogue validates and stages the import before registering the ID.
 */
void MainWindow::importComponent() {
    const QString directory =
        QFileDialog::getExistingDirectory(this, "Select package directory containing component.json");
    if (directory.isEmpty())
        return;
    Diagnostics diagnostics;
    if (catalogue_.importPackage(directory, userRoot_, diagnostics))
        refreshFilters();
    report(diagnostics, "Package imported.");
}
/** Export the selected package to a new directory.
 *
 * Existing output directories are never overwritten.
 */
void MainWindow::exportComponent() {
    if (!list_->currentItem())
        return;
    const QString id = list_->currentItem()->data(Qt::UserRole).toString();
    const QString destination = QFileDialog::getSaveFileName(this, "Choose a NEW package directory",
                                                             id + "-package", "Directory package (*)");
    if (destination.isEmpty())
        return;
    Diagnostics diagnostics;
    catalogue_.exportPackage(id, destination, diagnostics);
    report(diagnostics, "Package exported to " + destination);
}
/** Show operation diagnostics in a persistent status panel.
 *
 * Both errors and nonblocking asset warnings remain inspectable.
 */
void MainWindow::report(const Diagnostics &diagnostics, const QString &success) {
    const QString text = diagnosticText(diagnostics);
    diagnostics_->setText(hasErrors(diagnostics) ? text
                                                 : success + (text.isEmpty() ? QString{} : "\n" + text));
}
/** Check the active editor before closing the application.
 *
 * A cancelled or failed save keeps the application open.
 */
void MainWindow::closeEvent(QCloseEvent *event) {
    if (editor_) {
        editor_->reject();
        if (editor_ && editor_->isVisible()) {
            event->ignore();
            return;
        }
    }
    if (list_->currentItem())
        QSettings().setValue("catalogue/selectedId", list_->currentItem()->data(Qt::UserRole).toString());
    event->accept();
}
} // namespace hvd
