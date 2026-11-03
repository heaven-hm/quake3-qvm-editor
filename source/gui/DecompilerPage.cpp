#include "source/gui/DecompilerPage.hpp"
#include "qvm/decompiler/Exporter.hpp"
#include "qvm/decompiler/PseudocodeGenerator.hpp"
#include "qvm/decompiler/StringExtractor.hpp"
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
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
    mainLayout->setContentsMargins(16, 12, 16, 16);
    mainLayout->setSpacing(10);

    // --- Top Action Bar ---
    auto topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    openButton_ = new QPushButton(QIcon(":/icons/open.svg"), tr("Open QVM File..."), this);
    openButton_->setProperty("primary", true);
    openButton_->setFixedHeight(34);
    connect(openButton_, &QPushButton::clicked, this, &DecompilerPage::onOpenQvm);

    exportAsmButton_ = new QPushButton(QIcon(":/icons/export.svg"), tr("Export ASM..."), this);
    exportAsmButton_->setFixedHeight(34);
    exportAsmButton_->setEnabled(false);
    connect(exportAsmButton_, &QPushButton::clicked, this, &DecompilerPage::onExportAssembly);

    exportPseudoButton_ = new QPushButton(QIcon(":/icons/code.svg"), tr("Export C..."), this);
    exportPseudoButton_->setFixedHeight(34);
    exportPseudoButton_->setEnabled(false);
    connect(exportPseudoButton_, &QPushButton::clicked, this, &DecompilerPage::onExportPseudocode);

    fileInfoLabel_ = new QLabel(tr("Drag and drop a .qvm file here or click 'Open QVM File...'"), this);
    fileInfoLabel_->setStyleSheet("color: #9cad9f; font-style: italic;");

    topBar->addWidget(openButton_);
    topBar->addWidget(exportAsmButton_);
    topBar->addWidget(exportPseudoButton_);
    topBar->addSpacing(12);
    topBar->addWidget(fileInfoLabel_, 1);
    mainLayout->addLayout(topBar);

    // --- Header Information Card ---
    auto headerGroup = new QGroupBox(tr("QVM Binary Header & Segments"), this);
    auto headerLayout = new QHBoxLayout(headerGroup);
    headerLayout->setContentsMargins(14, 8, 14, 8);
    headerLayout->setSpacing(12);

    magicLabel_ = new QLabel(tr("Magic: -"), this);
    opCountLabel_ = new QLabel(tr("Instructions: -"), this);
    codeSegLabel_ = new QLabel(tr("CODE: -"), this);
    dataSegLabel_ = new QLabel(tr("DATA: -"), this);
    litSegLabel_ = new QLabel(tr("LIT: -"), this);
    bssSegLabel_ = new QLabel(tr("BSS: -"), this);

    for (auto* lbl : {magicLabel_, opCountLabel_, codeSegLabel_, dataSegLabel_, litSegLabel_, bssSegLabel_}) {
        lbl->setStyleSheet("font-family: Menlo, Consolas, monospace; font-size: 11px;");
        headerLayout->addWidget(lbl);
    }
    headerLayout->addStretch();
    mainLayout->addWidget(headerGroup);

    // --- Main Splitter (Left: Functions, Right: Tabs & Single Function Focus) ---
    auto splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // --- Left Panel: Functions Browser & Search ---
    auto funcWidget = new QWidget(this);
    auto funcLayout = new QVBoxLayout(funcWidget);
    funcLayout->setContentsMargins(0, 0, 0, 0);
    funcLayout->setSpacing(6);

    auto funcHeaderLayout = new QHBoxLayout();
    auto funcTitleLabel = new QLabel(tr("FUNCTIONS"), this);
    funcTitleLabel->setStyleSheet("font-weight: 700; font-size: 11px; letter-spacing: 1px; color: #758679;");
    funcHeaderLayout->addWidget(funcTitleLabel);
    funcLayout->addLayout(funcHeaderLayout);

    // Search filter input
    functionSearchEdit_ = new QLineEdit(this);
    functionSearchEdit_->setPlaceholderText(tr("Search functions (e.g. vmMain)..."));
    functionSearchEdit_->addAction(QIcon(":/icons/search.svg"), QLineEdit::LeadingPosition);
    functionSearchEdit_->setClearButtonEnabled(true);
    connect(functionSearchEdit_, &QLineEdit::textChanged, this, &DecompilerPage::onFunctionSearchChanged);
    funcLayout->addWidget(functionSearchEdit_);

    // Functions Table with Proxy Model
    functionsTable_ = new QTableView(this);
    functionsModel_ = new FunctionsTreeModel(this);
    functionsProxyModel_ = new QSortFilterProxyModel(this);
    functionsProxyModel_->setSourceModel(functionsModel_);
    functionsProxyModel_->setFilterKeyColumn(0); // Filter by Function Name column
    functionsProxyModel_->setFilterCaseSensitivity(Qt::CaseInsensitive);

    functionsTable_->setModel(functionsProxyModel_);
    functionsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    functionsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    functionsTable_->horizontalHeader()->setStretchLastSection(true);
    functionsTable_->verticalHeader()->setVisible(false);
    functionsTable_->setAlternatingRowColors(true);
    connect(functionsTable_, &QTableView::clicked, this, &DecompilerPage::onFunctionClicked);
    connect(functionsTable_, &QTableView::doubleClicked, this, &DecompilerPage::onFunctionClicked);

    funcLayout->addWidget(functionsTable_, 1);
    splitter->addWidget(funcWidget);

    // --- Right Panel: Detail Tabs with Single-Function Focus Bar ---
    auto rightContainer = new QWidget(this);
    auto rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);

    // Function Focus Banner
    focusBannerWidget_ = new QWidget(this);
    focusBannerWidget_->setStyleSheet(
        "background-color: #202923; border: 1px solid #b3e878; border-radius: 5px; padding: 4px 8px;"
    );
    auto focusBannerLayout = new QHBoxLayout(focusBannerWidget_);
    focusBannerLayout->setContentsMargins(8, 4, 8, 4);

    focusLabel_ = new QLabel(this);
    focusLabel_->setStyleSheet("font-weight: 600; color: #b3e878; font-size: 12px;");

    clearFocusButton_ = new QPushButton(QIcon(":/icons/clear.svg"), tr("Show Full Module"), this);
    clearFocusButton_->setObjectName("focusButton");
    clearFocusButton_->setFixedHeight(26);
    connect(clearFocusButton_, &QPushButton::clicked, this, &DecompilerPage::onClearFunctionFocus);

    focusBannerLayout->addWidget(focusLabel_, 1);
    focusBannerLayout->addWidget(clearFocusButton_);
    focusBannerWidget_->setVisible(false);
    rightLayout->addWidget(focusBannerWidget_);

    // Tabs
    detailTabs_ = new QTabWidget(this);

    // Tab 1: C Pseudocode Editor (with C Syntax Highlighter)
    pseudocodeEditor_ = new CodeEditor(this);
    pseudocodeEditor_->setReadOnly(true);
    pseudocodeEditor_->setLanguage(EditorLanguage::C);
    detailTabs_->addTab(pseudocodeEditor_, QIcon(":/icons/code.svg"), tr("C Pseudocode"));

    // Tab 2: Disassembly Editor (with ASM Syntax Highlighter)
    disassemblyEditor_ = new CodeEditor(this);
    disassemblyEditor_->setReadOnly(true);
    disassemblyEditor_->setLanguage(EditorLanguage::Assembly);
    detailTabs_->addTab(disassemblyEditor_, QIcon(":/icons/asm.svg"), tr("Assembly View"));

    // Tab 3: Decoded Instructions Table
    instructionsTable_ = new QTableView(this);
    instructionsModel_ = new InstructionTableModel(this);
    instructionsTable_->setModel(instructionsModel_);
    instructionsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    instructionsTable_->horizontalHeader()->setStretchLastSection(true);
    instructionsTable_->verticalHeader()->setDefaultSectionSize(22);
    instructionsTable_->setAlternatingRowColors(true);
    detailTabs_->addTab(instructionsTable_, QIcon(":/icons/instruction.svg"), tr("Decoded Instructions"));

    // Tab 4: Strings Table
    stringsTable_ = new QTableView(this);
    stringsModel_ = new StringsTableModel(this);
    stringsTable_->setModel(stringsModel_);
    stringsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    stringsTable_->horizontalHeader()->setStretchLastSection(true);
    stringsTable_->setAlternatingRowColors(true);
    detailTabs_->addTab(stringsTable_, QIcon(":/icons/string.svg"), tr("Extracted Strings"));

    // Tab 5: Hex View
    hexView_ = new HexViewWidget(this);
    detailTabs_->addTab(hexView_, QIcon(":/icons/hex.svg"), tr("Hex View"));

    rightLayout->addWidget(detailTabs_, 1);
    splitter->addWidget(rightContainer);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    mainLayout->addWidget(splitter, 1);
}

void DecompilerPage::setTheme(EditorTheme theme) {
    currentTheme_ = theme;
    pseudocodeEditor_->setTheme(theme);
    disassemblyEditor_->setTheme(theme);

    if (theme == EditorTheme::Midnight) {
        focusBannerWidget_->setStyleSheet(
            "background-color: #1c2b40; border: 1px solid #7ecdf3; border-radius: 5px; padding: 4px 8px;"
        );
        focusLabel_->setStyleSheet("font-weight: 600; color: #7ecdf3; font-size: 12px;");
    } else if (theme == EditorTheme::Light) {
        focusBannerWidget_->setStyleSheet(
            "background-color: #edf2e8; border: 1px solid #426d22; border-radius: 5px; padding: 4px 8px;"
        );
        focusLabel_->setStyleSheet("font-weight: 600; color: #426d22; font-size: 12px;");
    } else {
        focusBannerWidget_->setStyleSheet(
            "background-color: #202923; border: 1px solid #b3e878; border-radius: 5px; padding: 4px 8px;"
        );
        focusLabel_->setStyleSheet("font-weight: 600; color: #b3e878; font-size: 12px;");
    }
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
    fileInfoLabel_->setStyleSheet("color: #e7eee9; font-weight: bold;");

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

    // Generate and cache full module pseudocode and disassembly
    fullPseudocode_ = PseudocodeGenerator::generate(currentParsedQvm_, currentFunctions_);
    fullDisassembly_ = Exporter::generateDisassembly(currentParsedQvm_);

    displayFullModule();

    // Set hex view
    std::vector<uint8_t> allBytes = currentParsedQvm_.sections.code;
    allBytes.insert(allBytes.end(), currentParsedQvm_.sections.data.begin(), currentParsedQvm_.sections.data.end());
    allBytes.insert(allBytes.end(), currentParsedQvm_.sections.lit.begin(), currentParsedQvm_.sections.lit.end());
    hexView_->setData(allBytes);

    exportAsmButton_->setEnabled(true);
    exportPseudoButton_->setEnabled(true);
}

void DecompilerPage::onFunctionSearchChanged(const QString& text) {
    if (functionsProxyModel_) {
        functionsProxyModel_->setFilterFixedString(text);
    }
}

void DecompilerPage::onFunctionClicked(const QModelIndex& index) {
    if (!index.isValid()) return;

    QModelIndex srcIndex = functionsProxyModel_->mapToSource(index);
    const auto* func = functionsModel_->functionAt(srcIndex.row());
    if (func) {
        displayFunction(*func);
    }
}

void DecompilerPage::displayFunction(const FunctionInfo& func) {
    currentSelectedFunc_ = &func;

    // Show Focus Banner
    focusLabel_->setText(
        QString("▶ Function Focus: %1  |  Range: 0x%2 - 0x%3  |  Instructions: %4 (idx %5 - %6)  |  Stack: %7 B  |  Hash: 0x%8")
            .arg(QString::fromStdString(func.name))
            .arg(func.startOffset, 0, 16)
            .arg(func.endOffset, 0, 16)
            .arg(func.endIndex - func.startIndex + 1)
            .arg(func.startIndex)
            .arg(func.endIndex)
            .arg(func.stackFrameSize)
            .arg(func.hash, 0, 16)
    );
    focusBannerWidget_->setVisible(true);

    // 1. C Pseudocode: Show ONLY this single function!
    std::string funcC = PseudocodeGenerator::generateSingleFunction(currentParsedQvm_, func, currentFunctions_);
    pseudocodeEditor_->setPlainText(QString::fromStdString(funcC));

    // 2. Assembly View: Show ONLY this single function's assembly!
    std::string funcAsm = Exporter::generateFunctionDisassembly(currentParsedQvm_, func);
    disassemblyEditor_->setPlainText(QString::fromStdString(funcAsm));

    // 3. Instructions Table: Filter ONLY this single function's instructions!
    instructionsModel_->setFilterRange(static_cast<int>(func.startIndex), static_cast<int>(func.endIndex));
}

void DecompilerPage::displayFullModule() {
    currentSelectedFunc_ = nullptr;
    focusBannerWidget_->setVisible(false);

    // Restore full module pseudocode and disassembly
    pseudocodeEditor_->setPlainText(QString::fromStdString(fullPseudocode_));
    disassemblyEditor_->setPlainText(QString::fromStdString(fullDisassembly_));

    // Clear instruction filter to show all instructions
    instructionsModel_->clearFilter();
}

void DecompilerPage::onClearFunctionFocus() {
    displayFullModule();
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
