// ComponentEditor.h
#pragma once
#include "core/ComponentDefinition.h"
#include <QDialog>
#include <functional>

class QLineEdit;
class QComboBox;
class QPlainTextEdit;
class QTableWidget;
class QLabel;
namespace hvd {
class ComponentEditor : public QDialog {
    Q_OBJECT
  public:
    using SaveHandler = std::function<bool(const ComponentDefinition &, Diagnostics &)>;
    /** Open a metadata and pin editor on a detached definition value.
     *
     * The save callback owns persistence; cancelled edits never modify the catalogue.
     */
    ComponentEditor(ComponentDefinition definition, bool existing, SaveHandler save,
                    QWidget *parent = nullptr);
    /** Handle cancellation with an unsaved-changes prompt.
     *
     * Save, discard and cancel are offered for Escape, the Cancel button and window closure.
     */
    void reject() override;
    /** Report whether the editor contains unsaved modifications.
     *
     * The main window uses this state when closing the application.
     */
    bool dirty() const;

  private:
    /** Collect metadata and pin edits into an independent definition.
     *
     * Retains advanced properties, source references, assets and existing pin electrical limits.
     */
    ComponentDefinition collect() const;
    /** Validate and persist the edited definition.
     *
     * Failed saves leave the editor open and display actionable diagnostics.
     */
    void save();
    /** Mark the form modified.
     *
     * Changes the window marker without writing catalogue or package data.
     */
    void markDirty();
    ComponentDefinition original_;
    bool existing_;
    bool dirty_ = false;
    SaveHandler saveHandler_;
    QLineEdit *id_, *name_, *manufacturer_, *partNumber_, *category_, *tags_;
    QComboBox *kind_, *verification_;
    QPlainTextEdit *description_;
    QTableWidget *pins_;
    QLabel *diagnostics_;
};
} // namespace hvd
