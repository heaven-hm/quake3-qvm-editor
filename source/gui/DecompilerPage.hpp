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
#include <QPushButton>
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

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onOpenQvm();
    void onExportAssembly();
    void onExportPseudocode();
    void onFunctionDoubleClicked(const QModelIndex& index);

private:
    void setupUi();
    void updateUiWithParsedQvm();

    // UI elements
    QLabel* fileInfoLabel_ = nullptr;
    QLabel* magicLabel_ = nullptr;
    QLabel* opCountLabel_ = nullptr;
    QLabel* codeSegLabel_ = nullptr;
    QLabel* dataSegLabel_ = nullptr;
    QLabel* litSegLabel_ = nullptr;
    QLabel* bssSegLabel_ = nullptr;

    QPushButton* openButton_ = nullptr;
    QPushButton* exportAsmButton_ = nullptr;
    QPushButton* exportPseudoButton_ = nullptr;

    QTableView* functionsTable_ = nullptr;
    FunctionsTreeModel* functionsModel_ = nullptr;

    QTabWidget* detailTabs_ = nullptr;

    QTableView* instructionsTable_ = nullptr;
    InstructionTableModel* instructionsModel_ = nullptr;

    CodeEditor* pseudocodeEditor_ = nullptr;
    CodeEditor* disassemblyEditor_ = nullptr;

    QTableView* stringsTable_ = nullptr;
    StringsTableModel* stringsModel_ = nullptr;

    HexViewWidget* hexView_ = nullptr;

    ParsedQvm currentParsedQvm_;
    std::vector<FunctionInfo> currentFunctions_;
    QString currentFilePath_;
};

} // namespace qvm::gui
