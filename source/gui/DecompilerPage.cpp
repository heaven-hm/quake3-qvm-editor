#include "source/gui/DecompilerPage.hpp"
#include "qvm/decompiler/Exporter.hpp"
#include "qvm/decompiler/PseudocodeGenerator.hpp"
#include "qvm/decompiler/StringExtractor.hpp"
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMimeData>
#include <QMessageBox>
#include <QVBoxLayout>

namespace qvm::gui {

DecompilerPage::DecompilerPage(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);
    setupUi();
}

void DecompilerPage::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Top action bar
    auto topBar = new QHBoxLayout();
    openButton_ = new QPushButton(tr("Open QVM File..."), this);
    openButton_->setFixedHeight(32);
    openButton_->setStyleSheet(
        "QPushButton { background-color: #0284c7; color: white; font-weight: bold; border-radius: 4px; padding: 4px 16px; }"
        "QPushButton:hover { background-color: #0369a1; }"
    );
    connect(openButton_, &QPushButton::clicked, this, &DecompilerPage::onOpenQvm);

    exportAsmButton_ = new QPushButton(tr("Export Assembly..."), this);
    exportAsmButton_->setEnabled(false);
    connect(exportAsmButton_, &QPushButton::clicked, this, &DecompilerPage::onExportAssembly);

    exportPseudoButton_ = new QPushButton(tr("Export Pseudocode..."), this);
    exportPseudoButton_->setEnabled(false);
    connect(exportPseudoButton_, &QPushButton::clicked, this, &DecompilerPage::onExportPseudocode);

    fileInfoLabel_ = new QLabel(tr("Drag and drop a .qvm file here or click 'Open QVM File...'"), this);
    fileInfoLabel_->setStyleSheet("color: #94a3b8; font-style: italic;");

    topBar->addWidget(openButton_);
    topBar->addWidget(exportAsmButton_);
    topBar->addWidget(exportPseudoButton_);
    topBar->addSpacing(16);
    topBar->addWidget(fileInfoLabel_, 1);
    mainLayout->addLayout(topBar);

    // Header Info Card
    auto headerGroup = new QGroupBox(tr("QVM Binary Header"), this);
    auto headerLayout = new QHBoxLayout(headerGroup);
    headerLayout->setContentsMargins(12, 8, 12, 8);

    magicLabel_ = new QLabel(tr("Magic: -"), this);
    opCountLabel_ = new QLabel(tr("Instructions: -"), this);
    codeSegLabel_ = new QLabel(tr("CODE: -"), this);
    dataSegLabel_ = new QLabel(tr("DATA: -"), this);
    litSegLabel_ = new QLabel(tr("LIT: -"), this);
    bssSegLabel_ = new QLabel(tr("BSS: -"), this);

    for (auto* lbl : {magicLabel_, opCountLabel_, codeSegLabel_, dataSegLabel_, litSegLabel_, bssSegLabel_}) {
        lbl->setStyleSheet("font-family: Menlo, monospace; font-size: 11px;");
        headerLayout->addWidget(lbl);
        headerLayout->addSpacing(8);
    }
    headerLayout->addStretch();
    mainLayout->addWidget(headerGroup);

    // Main splitter: Left (Functions) vs Right (Tabs: Instructions, Pseudocode, Strings, Hex, Disassembly)
    auto splitter = new QSplitter(Qt::Horizontal, this);

    // Left Panel: Function Navigation
    auto funcGroup = new QGroupBox(tr("Functions"), this);
    auto funcLayout = new QVBoxLayout(funcGroup);
    funcLayout->setContentsMargins(4, 4, 4, 4);

    functionsTable_ = new QTableView(this);
    functionsModel_ = new FunctionsTreeModel(this);
    functionsTable_->setModel(functionsModel_);
    functionsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    functionsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    functionsTable_->horizontalHeader()->setStretchLastSection(true);
    functionsTable_->verticalHeader()->setVisible(false);
    connect(functionsTable_, &QTableView::doubleClicked, this, &DecompilerPage::onFunctionDoubleClicked);
    funcLayout->addWidget(functionsTable_);
    splitter->addWidget(funcGroup);

    // Right Panel: Detail Tabs
    detailTabs_ = new QTabWidget(this);

    // Tab 1: Instruction Table
    instructionsTable_ = new QTableView(this);
    instructionsModel_ = new InstructionTableModel(this);
    instructionsTable_->setModel(instructionsModel_);
    instructionsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    instructionsTable_->horizontalHeader()->setStretchLastSection(true);
    instructionsTable_->verticalHeader()->setDefaultSectionSize(22);
    detailTabs_->addTab(instructionsTable_, tr("Decoded Instructions"));

    // Tab 2: Reconstructed Pseudocode
    pseudocodeEditor_ = new CodeEditor(this);
    pseudocodeEditor_->setReadOnly(true);
    detailTabs_->addTab(pseudocodeEditor_, tr("C Pseudocode"));

    // Tab 3: Disassembly
    disassemblyEditor_ = new CodeEditor(this);
    disassemblyEditor_->setReadOnly(true);
    detailTabs_->addTab(disassemblyEditor_, tr("Assembly View"));

    // Tab 4: Strings Table
    stringsTable_ = new QTableView(this);
    stringsModel_ = new StringsTableModel(this);
    stringsTable_->setModel(stringsModel_);
    stringsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    stringsTable_->horizontalHeader()->setStretchLastSection(true);
    detailTabs_->addTab(stringsTable_, tr("Extracted Strings"));

    // Tab 5: Hex View
    hexView_ = new HexViewWidget(this);
    detailTabs_->addTab(hexView_, tr("Hex View"));

    splitter->addWidget(detailTabs_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    mainLayout->addWidget(splitter, 1);
}

void DecompilerPage::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        const auto urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString path = urls.first().toLocalFile();
            if (path.endsWith(".qvm", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void DecompilerPage::dropEvent(QDropEvent* event) {
    const auto urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString path = urls.first().toLocalFile();
        if (!path.isEmpty()) {
            loadQvmFile(path);
            event->acceptProposedAction();
        }
    }
}

void DecompilerPage::onOpenQvm() {
    QString file = QFileDialog::getOpenFileName(
        this, tr("Open Quake III QVM Binary"), QString(), tr("Quake III QVM Files (*.qvm);;All Files (*.*)"));
    if (!file.isEmpty()) {
        loadQvmFile(file);
    }
}

void DecompilerPage::loadQvmFile(const QString& filePath) {
    auto parsedRes = QvmParser::parseFile(filePath.toStdString());
    if (parsedRes.hasError()) {
        QMessageBox::critical(this, tr("Failed to Parse QVM"),
                              QString::fromStdString(parsedRes.error().message()));
        return;
    }

    currentParsedQvm_ = std::move(parsedRes.value());
    currentFilePath_ = filePath;

    updateUiWithParsedQvm();
}

void DecompilerPage::updateUiWithParsedQvm() {
    fileInfoLabel_->setText(currentFilePath_ + " (" + QString::number(currentParsedQvm_.fileSize) + " bytes)");
    fileInfoLabel_->setStyleSheet("color: #e2e8f0; font-weight: bold;");

    const auto& h = currentParsedQvm_.header;
    magicLabel_->setText(QString("Magic: 0x%1 (%2)")
        .arg(h.magic, 8, 16, QLatin1Char('0'))
        .arg(h.isVersion2() ? "Ver2" : "Ver1"));
    opCountLabel_->setText(QString("Instructions: %1").arg(h.instructionCount));
    codeSegLabel_->setText(QString("CODE: 0x%1 (%2 B)").arg(h.codeOffset, 0, 16).arg(h.codeLength));
    dataSegLabel_->setText(QString("DATA: 0x%1 (%2 B)").arg(h.dataOffset, 0, 16).arg(h.dataLength));
    litSegLabel_->setText(QString("LIT: %1 B").arg(h.litLength));
    bssSegLabel_->setText(QString("BSS: %1 B").arg(h.bssLength));

    // Update models
    instructionsModel_->setInstructions(currentParsedQvm_.instructions);

    currentFunctions_ = FunctionAnalyzer::analyzeFunctions(currentParsedQvm_.instructions);
    functionsModel_->setFunctions(currentFunctions_);

    auto strings = StringExtractor::extractAll(currentParsedQvm_.sections);
    stringsModel_->setStrings(strings);

    // Reconstruct pseudocode
    std::string pseudocode = PseudocodeGenerator::generate(currentParsedQvm_, currentFunctions_);
    pseudocodeEditor_->setPlainText(QString::fromStdString(pseudocode));

    // Generate disassembly
    std::string disasm = Exporter::generateDisassembly(currentParsedQvm_);
    disassemblyEditor_->setPlainText(QString::fromStdString(disasm));

    // Set hex view with code and data segments
    std::vector<uint8_t> allBytes = currentParsedQvm_.sections.code;
    allBytes.insert(allBytes.end(), currentParsedQvm_.sections.data.begin(), currentParsedQvm_.sections.data.end());
    allBytes.insert(allBytes.end(), currentParsedQvm_.sections.lit.begin(), currentParsedQvm_.sections.lit.end());
    hexView_->setData(allBytes);

    exportAsmButton_->setEnabled(true);
    exportPseudoButton_->setEnabled(true);
}

void DecompilerPage::onFunctionDoubleClicked(const QModelIndex& index) {
    const auto* func = functionsModel_->functionAt(index.row());
    if (func) {
        // Scroll to instruction in instructions table
        instructionsTable_->scrollTo(instructionsModel_->index(static_cast<int>(func->startIndex), 0));
        instructionsTable_->selectRow(static_cast<int>(func->startIndex));

        // Switch to instructions tab
        detailTabs_->setCurrentWidget(instructionsTable_);
    }
}

void DecompilerPage::onExportAssembly() {
    if (currentParsedQvm_.instructions.empty()) return;

    QString defaultPath = currentFilePath_;
    if (defaultPath.endsWith(".qvm", Qt::CaseInsensitive)) {
        defaultPath.chop(4);
    }
    defaultPath += ".asm";

    QString savePath = QFileDialog::getSaveFileName(
        this, tr("Export Disassembly"), defaultPath, tr("Assembly Files (*.asm);;All Files (*.*)"));
    if (!savePath.isEmpty()) {
        if (auto err = Exporter::exportDisassembly(currentParsedQvm_, savePath.toStdString()); err.isError()) {
            QMessageBox::critical(this, tr("Export Error"), QString::fromStdString(err.message()));
        } else {
            QMessageBox::information(this, tr("Export Succeeded"),
                                     tr("Disassembly successfully saved to:\n") + savePath);
        }
    }
}

void DecompilerPage::onExportPseudocode() {
    if (currentParsedQvm_.instructions.empty()) return;

    QString defaultPath = currentFilePath_;
    if (defaultPath.endsWith(".qvm", Qt::CaseInsensitive)) {
        defaultPath.chop(4);
    }
    defaultPath += ".c";

    QString savePath = QFileDialog::getSaveFileName(
        this, tr("Export C-Like Pseudocode"), defaultPath, tr("C Source Files (*.c);;All Files (*.*)"));
    if (!savePath.isEmpty()) {
        if (auto err = Exporter::exportPseudocode(currentParsedQvm_, savePath.toStdString()); err.isError()) {
            QMessageBox::critical(this, tr("Export Error"), QString::fromStdString(err.message()));
        } else {
            QMessageBox::information(this, tr("Export Succeeded"),
                                     tr("Pseudocode successfully saved to:\n") + savePath);
        }
    }
}

} // namespace qvm::gui
