// ComponentEditor.cpp
#include "ComponentEditor.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <limits>

namespace hvd {
/** Open a metadata and pin editor on a detached definition value.
 *
 * The save callback owns persistence; cancelled edits never modify the catalogue.
 */
ComponentEditor::ComponentEditor(ComponentDefinition definition, bool existing, SaveHandler saveHandler,
                                 QWidget *parent)
    : QDialog(parent), original_(std::move(definition)), existing_(existing),
      saveHandler_(std::move(saveHandler)) {
    setObjectName("componentEditor");
    setWindowTitle(existing ? "Edit component[*]" : "Create component[*]");
    resize(940, 740);
    setAttribute(Qt::WA_DeleteOnClose);
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    // Construct consistently named metadata controls for keyboard access and UI tests.
    //
    // Each control starts with the detached definition's value.
    const auto field = [this, form](const QString &label, const QString &object, const QString &value) {
        auto *edit = new QLineEdit(value, this);
        edit->setObjectName(object);
        form->addRow(label, edit);
        return edit;
    };
    id_ = field("Stable ID", "componentId", original_.id);
    id_->setReadOnly(existing);
    name_ = field("Name", "componentName", original_.name);
    manufacturer_ = field("Manufacturer (blank = unknown)", "manufacturer", original_.manufacturer);
    partNumber_ = field("Part number (blank = unknown)", "partNumber", original_.partNumber);
    category_ = field("Category", "componentCategory", original_.category);
    tags_ = field("Tags (comma-separated)", "componentTags", original_.tags.join(", "));
    kind_ = new QComboBox(this);
    kind_->setObjectName("componentKind");
    kind_->addItems(componentKinds());
    kind_->setCurrentText(original_.kind);
    form->addRow("Kind", kind_);
    verification_ = new QComboBox(this);
    verification_->addItems({"unverified", "demonstration", "source-reviewed"});
    verification_->setCurrentText(original_.verificationStatus);
    form->addRow("Source status (not electrical certification)", verification_);
    description_ = new QPlainTextEdit(original_.description, this);
    description_->setObjectName("componentDescription");
    description_->setMaximumHeight(95);
    form->addRow("Description", description_);
    layout->addLayout(form);
    auto *hint =
        new QLabel("Pins: keep physical number, GPIO identifier and logical signal distinct. "
                   "Capabilities use comma-separated schema names. Electrical limits remain unchanged.",
                   this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    pins_ = new QTableWidget(0, 6, this);
    pins_->setObjectName("pinsTable");
    pins_->setHorizontalHeaderLabels(
        {"Stable pin ID", "Name", "Physical number", "GPIO identifier", "Logical signal", "Capabilities"});
    pins_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    pins_->setSelectionBehavior(QAbstractItemView::SelectRows);
    for (const auto &pin : original_.pins) {
        const int row = pins_->rowCount();
        pins_->insertRow(row);
        const QStringList values{pin.id,
                                 pin.name,
                                 pin.physicalNumber,
                                 pin.gpioIdentifier,
                                 pin.logicalSignal,
                                 pin.capabilities.join(", ")};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values[column]);
            if (column == 0)
                item->setData(Qt::UserRole, pin.id);
            pins_->setItem(row, column, item);
        }
    }
    layout->addWidget(pins_, 1);
    auto *pinButtons = new QHBoxLayout;
    auto *add = new QPushButton("Add pin", this);
    add->setObjectName("addPinButton");
    auto *remove = new QPushButton("Remove selected pins", this);
    pinButtons->addWidget(add);
    pinButtons->addWidget(remove);
    pinButtons->addStretch();
    layout->addLayout(pinButtons);
    diagnostics_ = new QLabel(this);
    diagnostics_->setWordWrap(true);
    diagnostics_->setObjectName("editorDiagnostics");
    diagnostics_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(diagnostics_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setObjectName("saveComponentButton");
    layout->addWidget(buttons);
    for (auto *edit : {id_, name_, manufacturer_, partNumber_, category_, tags_})
        connect(edit, &QLineEdit::textChanged, this, &ComponentEditor::markDirty);
    connect(kind_, &QComboBox::currentTextChanged, this, &ComponentEditor::markDirty);
    connect(verification_, &QComboBox::currentTextChanged, this, &ComponentEditor::markDirty);
    connect(description_, &QPlainTextEdit::textChanged, this, &ComponentEditor::markDirty);
    connect(pins_, &QTableWidget::itemChanged, this, &ComponentEditor::markDirty);
    // Add an independent pin row with a stable generated ID.
    //
    // Users can fill the identifiers and capability list before validation.
    connect(add, &QPushButton::clicked, this, [this] {
        const int row = pins_->rowCount();
        pins_->insertRow(row);
        for (int column = 0; column < 6; ++column)
            pins_->setItem(
                row, column,
                new QTableWidgetItem(column == 0 ? "pin-" + QUuid::createUuid().toString(QUuid::WithoutBraces)
                                                 : QString{}));
        markDirty();
    });
    // Remove selected rows in reverse order.
    //
    // Reverse iteration prevents shifted indices from deleting unrelated pins.
    connect(remove, &QPushButton::clicked, this, [this] {
        for (int row = pins_->rowCount() - 1; row >= 0; --row)
            if (pins_->selectionModel()->isRowSelected(row, QModelIndex()))
                pins_->removeRow(row);
        markDirty();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &ComponentEditor::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &ComponentEditor::reject);
}
/** Collect metadata and pin edits into an independent definition.
 *
 * Retains advanced properties, source references, assets and existing pin electrical limits.
 */
ComponentDefinition ComponentEditor::collect() const {
    auto definition = original_;
    definition.id = id_->text().trimmed();
    definition.name = name_->text().trimmed();
    definition.manufacturer = manufacturer_->text().trimmed();
    definition.partNumber = partNumber_->text().trimmed();
    definition.category = category_->text().trimmed();
    definition.kind = kind_->currentText();
    definition.verificationStatus = verification_->currentText();
    definition.description = description_->toPlainText();
    definition.tags.clear();
    for (const auto &tag : tags_->text().split(',', Qt::SkipEmptyParts))
        if (!tag.trimmed().isEmpty())
            definition.tags.append(tag.trimmed());
    definition.pins.clear();
    for (int row = 0; row < pins_->rowCount(); ++row) {
        QStringList values;
        for (int column = 0; column < 6; ++column)
            values.append(pins_->item(row, column) ? pins_->item(row, column)->text().trimmed() : QString{});
        Pin pin;
        if (pins_->item(row, 0)) {
            const QString originalId = pins_->item(row, 0)->data(Qt::UserRole).toString();
            for (const auto &originalPin : original_.pins)
                if (originalPin.id == originalId)
                    pin = originalPin;
        }
        pin.id = values[0];
        pin.name = values[1];
        pin.physicalNumber = values[2];
        pin.gpioIdentifier = values[3];
        pin.logicalSignal = values[4];
        pin.capabilities.clear();
        for (const auto &capability : values[5].split(',', Qt::SkipEmptyParts))
            if (!capability.trimmed().isEmpty())
                pin.capabilities.append(capability.trimmed());
        definition.pins.append(pin);
    }
    if (existing_ && definitionToJson(definition) != definitionToJson(original_) &&
        definition.revision < std::numeric_limits<int>::max())
        ++definition.revision;
    return definition;
}
/** Validate and persist the edited definition.
 *
 * Failed saves leave the editor open and display actionable diagnostics.
 */
void ComponentEditor::save() {
    const auto definition = collect();
    Diagnostics diagnostics = validateDefinition(definition);
    if (hasErrors(diagnostics) || !saveHandler_(definition, diagnostics)) {
        diagnostics_->setText(diagnosticText(diagnostics));
        return;
    }
    dirty_ = false;
    setWindowModified(false);
    accept();
}
/** Mark the form modified.
 *
 * Changes the window marker without writing catalogue or package data.
 */
void ComponentEditor::markDirty() {
    dirty_ = true;
    setWindowModified(true);
}
/** Report whether the editor contains unsaved modifications.
 *
 * The main window uses this state when closing the application.
 */
bool ComponentEditor::dirty() const { return dirty_; }
/** Handle cancellation with an unsaved-changes prompt.
 *
 * Save, discard and cancel are offered for Escape, the Cancel button and window closure.
 */
void ComponentEditor::reject() {
    if (dirty_) {
        const auto response = QMessageBox::question(
            this, "Unsaved component", "Save changes before closing?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
        if (response == QMessageBox::Cancel)
            return;
        if (response == QMessageBox::Save) {
            save();
            return;
        }
    }
    QDialog::reject();
}
} // namespace hvd
