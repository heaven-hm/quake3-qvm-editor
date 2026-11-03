#pragma once

#include "qvm/decompiler/FunctionAnalyzer.hpp"
#include "qvm/decompiler/QvmParser.hpp"
#include "source/gui/models/FunctionsTreeModel.hpp"
#include "source/gui/models/InstructionTableModel.hpp"
#include "source/gui/models/StringsTableModel.hpp"
#include "source/gui/widgets/CodeEditor.hpp"
#include "source/gui/widgets/HexViewWidget.hpp"
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QTableView>
#include <QTabWidget>
#include <QWidget>

namespace qvm::gui {

class DecompilerPage : public QWidget {
    Q_OBJECT

public:
    explicit DecompilerPage(QWidget* parent = nullptr);

    void loadQvmFile(const QString& filePath);
    void setTheme(EditorTheme theme);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onOpenQvm();
    void onExportAssembly();
    void onExportPseudocode();
    void onFunctionClicked(const QModelIndex& index);
    void onClearFunctionFocus();
    void onFunctionSearchChanged(const QString& text);

private:
    void setupUi();
    void updateUiWithParsedQvm();
    void displayFunction(const FunctionInfo& func);
    void displayFullModule();

    // Top action bar
    QLabel* fileInfoLabel_ = nullptr;
    QPushButton* openButton_ = nullptr;
    QPushButton* exportAsmButton_ = nullptr;
    QPushButton* exportPseudoButton_ = nullptr;

    // Binary info cards
    QLabel* magicLabel_ = nullptr;
    QLabel* opCountLabel_ = nullptr;
    QLabel* codeSegLabel_ = nullptr;
    QLabel* dataSegLabel_ = nullptr;
    QLabel* litSegLabel_ = nullptr;
    QLabel* bssSegLabel_ = nullptr;

    // Functions list & search
    QLineEdit* functionSearchEdit_ = nullptr;
    QTableView* functionsTable_ = nullptr;
    FunctionsTreeModel* functionsModel_ = nullptr;
    QSortFilterProxyModel* functionsProxyModel_ = nullptr;

    // Single Function Focus Banner
    QWidget* focusBannerWidget_ = nullptr;
    QLabel* focusLabel_ = nullptr;
    QPushButton* clearFocusButton_ = nullptr;

    // Main detail tabs
    QTabWidget* detailTabs_ = nullptr;
    QTableView* instructionsTable_ = nullptr;
    InstructionTableModel* instructionsModel_ = nullptr;
    CodeEditor* pseudocodeEditor_ = nullptr;
    CodeEditor* disassemblyEditor_ = nullptr;
    QTableView* stringsTable_ = nullptr;
    StringsTableModel* stringsModel_ = nullptr;
    HexViewWidget* hexView_ = nullptr;

    // Data cache
    ParsedQvm currentParsedQvm_;
    std::vector<FunctionInfo> currentFunctions_;
    QString currentFilePath_;
    std::string fullPseudocode_;
    std::string fullDisassembly_;
    const FunctionInfo* currentSelectedFunc_ = nullptr;
    EditorTheme currentTheme_{EditorTheme::Dark};
};

} // namespace qvm::gui
