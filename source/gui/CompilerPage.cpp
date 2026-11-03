#include "source/gui/CompilerPage.hpp"
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QShortcut>
#include <QSplitter>
#include <QVBoxLayout>

namespace qvm::gui {

namespace {

const char* const kDefaultVmMainTemplate =
R"(#include "bg_lib.h"

// Forward declarations
void printf(const char* fmt, ...);
int fibonacci(int n);

/*
================
vmMain

The entry-point for Quake III QVM bytecode execution.
Control passes into this function when the engine calls the VM.
================
*/
int vmMain(int command, int arg0, int arg1, int arg2, int arg3, int arg4,
           int arg5, int arg6, int arg7, int arg8, int arg9, int arg10,
           int arg11)
{
    if (command == 0) {
        // Command 0: VM Init
        printf("^2[QVM Editor]^7 Module loaded successfully!\n");
        printf("^3[QVM Editor]^7 fibonacci(7) = %i\n", fibonacci(7));
    } else {
        printf("^1[QVM Editor]^7 Unknown engine command: %i\n", command);
    }

    return 0;
}

void printf(const char* fmt, ...)
{
    va_list argptr;
    char text[1024];

    va_start(argptr, fmt);
    vsprintf(text, fmt, argptr);
    va_end(argptr);

    trap_Printf(text);
}

int fibonacci(int n)
{
    if (n <= 1)
        return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}
)";

} // anonymous namespace

// ==============================================================================
// CompileWorker Implementation (Background Thread)
// ==============================================================================

CompileWorker::CompileWorker(CompilerConfig config, std::string sourceCode, QObject* parent)
    : QThread(parent), config_(std::move(config)), sourceCode_(std::move(sourceCode)) {}

void CompileWorker::run() {
    auto progress = [this](int current, int total, std::string_view msg) {
        emit progressUpdated(current, total, QString::fromUtf8(msg.data(), static_cast<qsizetype>(msg.size())));
    };

    auto log = [this](std::string_view line, bool isErr) {
        emit logReceived(QString::fromUtf8(line.data(), static_cast<qsizetype>(line.size())), isErr);
    };

    Result<BuildReport> result = sourceCode_.empty()
        ? Compiler::compile(config_, progress, log)
        : Compiler::compileSourceCode(sourceCode_, config_, progress, log);

    if (result.hasError()) {
        emit finished(false, QString::fromStdString(result.error().message()), QString());
    } else {
        const auto& report = result.value();
        QString summary = QString("QVM compiled successfully (%1 bytes) in %2 seconds")
                              .arg(report.qvmFileSize)
                              .arg(report.durationSeconds, 0, 'f', 2);
        emit finished(true, summary, QString::fromStdString(report.outputQvmPath.string()));
    }
}

// ==============================================================================
// CompilerPage Implementation
// ==============================================================================

CompilerPage::CompilerPage(QWidget* parent) : QWidget(parent) {
    setupUi();
    onLoadStarterTemplate();
    updateCliCommandPreview();
}

void CompilerPage::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 12, 16, 16);
    mainLayout->setSpacing(10);

    modeTabs_ = new QTabWidget(this);

    // Tab 1: Interactive C Editor
    auto editorWidget = new QWidget(this);
    setupEditorTab(editorWidget);
    modeTabs_->addTab(editorWidget, QIcon(":/icons/code.svg"), tr("Interactive C Editor"));

    // Tab 2: Project Directory Build
    auto dirWidget = new QWidget(this);
    setupDirectoryTab(dirWidget);
    modeTabs_->addTab(dirWidget, QIcon(":/icons/open.svg"), tr("Project Directory Build"));

    mainLayout->addWidget(modeTabs_, 1);

    // --- Bottom Shared Console & Progress Bar ---
    auto bottomGroup = new QGroupBox(tr("Build Logs & Toolchain Messages"), this);
    auto bottomLayout = new QVBoxLayout(bottomGroup);
    bottomLayout->setContentsMargins(8, 8, 8, 8);
    bottomLayout->setSpacing(6);

    progressBar_ = new QProgressBar(this);
    progressBar_->setFixedHeight(18);
    progressBar_->setTextVisible(true);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);
    bottomLayout->addWidget(progressBar_);

    statusLabel_ = new QLabel(tr("Ready to compile."), this);
    statusLabel_->setStyleSheet("color: #9cad9f; font-weight: 500; font-size: 11px;");
    bottomLayout->addWidget(statusLabel_);

    logTabs_ = new QTabWidget(this);
    logTabs_->setFixedHeight(160);

    allLogsEdit_ = new QPlainTextEdit(this);
    allLogsEdit_->setReadOnly(true);
    allLogsEdit_->setFont(QFont("Menlo", 10));

    lccLogsEdit_ = new QPlainTextEdit(this);
    lccLogsEdit_->setReadOnly(true);
    lccLogsEdit_->setFont(QFont("Menlo", 10));

    q3asmLogsEdit_ = new QPlainTextEdit(this);
    q3asmLogsEdit_->setReadOnly(true);
    q3asmLogsEdit_->setFont(QFont("Menlo", 10));

    errorLogsEdit_ = new QPlainTextEdit(this);
    errorLogsEdit_->setReadOnly(true);
    errorLogsEdit_->setFont(QFont("Menlo", 10));

    logTabs_->addTab(allLogsEdit_, QIcon(":/icons/editor.svg"), tr("All Output"));
    logTabs_->addTab(lccLogsEdit_, QIcon(":/icons/code.svg"), tr("LCC Output"));
    logTabs_->addTab(q3asmLogsEdit_, QIcon(":/icons/asm.svg"), tr("Q3ASM Output"));
    logTabs_->addTab(errorLogsEdit_, QIcon(":/icons/info.svg"), tr("Errors & Warnings"));
    bottomLayout->addWidget(logTabs_);

    mainLayout->addWidget(bottomGroup);
}

void CompilerPage::setupEditorTab(QWidget* parent) {
    auto layout = new QVBoxLayout(parent);
    layout->setContentsMargins(4, 8, 4, 4);
    layout->setSpacing(8);

    // Editor Action Toolbar
    auto barLayout = new QHBoxLayout();
    barLayout->setSpacing(8);

    auto openBtn = new QPushButton(QIcon(":/icons/open.svg"), tr("Open C..."), this);
    connect(openBtn, &QPushButton::clicked, this, &CompilerPage::onOpenCFile);

    auto saveBtn = new QPushButton(QIcon(":/icons/export.svg"), tr("Save C..."), this);
    connect(saveBtn, &QPushButton::clicked, this, &CompilerPage::onSaveCFile);

    auto templateBtn = new QPushButton(QIcon(":/icons/editor.svg"), tr("Starter Template"), this);
    connect(templateBtn, &QPushButton::clicked, this, &CompilerPage::onLoadStarterTemplate);

    auto copyCmdBtn = new QPushButton(QIcon(":/icons/copy.svg"), tr("Copy CLI Command"), this);
    connect(copyCmdBtn, &QPushButton::clicked, this, &CompilerPage::onCopyCliCommand);

    compileEditorBtn_ = new QPushButton(QIcon(":/icons/compiler.svg"), tr("Compile Code to QVM (Ctrl+Enter)"), this);
    compileEditorBtn_->setProperty("primary", true);
    compileEditorBtn_->setFixedHeight(32);
    connect(compileEditorBtn_, &QPushButton::clicked, this, &CompilerPage::onStartCompileEditor);

    // Ctrl+Enter / Cmd+Enter Shortcut
    auto shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(shortcut, &QShortcut::activated, this, &CompilerPage::onStartCompileEditor);

    barLayout->addWidget(openBtn);
    barLayout->addWidget(saveBtn);
    barLayout->addWidget(templateBtn);
    barLayout->addWidget(copyCmdBtn);
    barLayout->addStretch();
    barLayout->addWidget(compileEditorBtn_);
    layout->addLayout(barLayout);

    // Splitter: Editor on Left/Center, Build Flags on Right
    auto splitter = new QSplitter(Qt::Horizontal, parent);
    splitter->setChildrenCollapsible(false);

    // Left: C Code Editor
    cCodeEditor_ = new CodeEditor(this);
    cCodeEditor_->setLanguage(EditorLanguage::C);
    splitter->addWidget(cCodeEditor_);

    // Right: Command Options Card
    auto optionsGroup = new QGroupBox(tr("Command & Build Options"), this);
    auto formLayout = new QFormLayout(optionsGroup);
    formLayout->setContentsMargins(10, 10, 10, 10);
    formLayout->setSpacing(8);

    editorOutputNameEdit_ = new QLineEdit("custom.qvm", this);
    connect(editorOutputNameEdit_, &QLineEdit::textChanged, this, &CompilerPage::updateCliCommandPreview);
    formLayout->addRow(tr("Output Name:"), editorOutputNameEdit_);

    auto outDirLayout = new QHBoxLayout();
    editorOutputDirEdit_ = new QLineEdit("build/out", this);
    connect(editorOutputDirEdit_, &QLineEdit::textChanged, this, &CompilerPage::updateCliCommandPreview);
    browseEditorOutputBtn_ = new QPushButton(QIcon(":/icons/open.svg"), tr("Browse..."), this);
    connect(browseEditorOutputBtn_, &QPushButton::clicked, this, &CompilerPage::onBrowseEditorOutputDir);
    outDirLayout->addWidget(editorOutputDirEdit_);
    outDirLayout->addWidget(browseEditorOutputBtn_);
    formLayout->addRow(tr("Output Directory:"), outDirLayout);

    auto sdkLayout = new QHBoxLayout();
    editorSdkDirEdit_ = new QLineEdit(this);
    editorSdkDirEdit_->setPlaceholderText(tr("Quake 3 SDK or code directory (optional)..."));
    connect(editorSdkDirEdit_, &QLineEdit::textChanged, this, &CompilerPage::onSdkDirChanged);
    browseSdkBtn_ = new QPushButton(QIcon(":/icons/open.svg"), tr("Select Quake 3 SDK..."), this);
    browseSdkBtn_->setToolTip(tr("Select Quake 3 SDK or code directory to resolve all libraries & headers (qcommon, cgame, game, ui)"));
    connect(browseSdkBtn_, &QPushButton::clicked, this, &CompilerPage::onBrowseSdk);
    sdkLayout->addWidget(editorSdkDirEdit_);
    sdkLayout->addWidget(browseSdkBtn_);
    formLayout->addRow(tr("Quake 3 SDK:"), sdkLayout);

    editorDefinesEdit_ = new QLineEdit("-DQ3_VM", this);
    editorDefinesEdit_->setPlaceholderText(tr("-DDEBUG -DQ3_VM"));
    connect(editorDefinesEdit_, &QLineEdit::textChanged, this, &CompilerPage::updateCliCommandPreview);
    formLayout->addRow(tr("Defines:"), editorDefinesEdit_);

    editorIncludesEdit_ = new QLineEdit(this);
    editorIncludesEdit_->setPlaceholderText(tr("-I./include -I../common"));
    connect(editorIncludesEdit_, &QLineEdit::textChanged, this, &CompilerPage::updateCliCommandPreview);
    formLayout->addRow(tr("Includes:"), editorIncludesEdit_);

    editorDebugCheck_ = new QCheckBox(tr("Include Debug Symbols (-Wf-g)"), this);
    connect(editorDebugCheck_, &QCheckBox::toggled, this, &CompilerPage::updateCliCommandPreview);
    formLayout->addRow(tr("Debug:"), editorDebugCheck_);

    editorVerboseCheck_ = new QCheckBox(tr("Verbose Q3ASM Output (-v)"), this);
    connect(editorVerboseCheck_, &QCheckBox::toggled, this, &CompilerPage::updateCliCommandPreview);
    formLayout->addRow(tr("Verbose:"), editorVerboseCheck_);

    // Live CLI Command Preview
    cliCommandPreviewEdit_ = new QLineEdit(this);
    cliCommandPreviewEdit_->setReadOnly(true);
    cliCommandPreviewEdit_->setStyleSheet("color: #b3e878; font-family: monospace; font-size: 11px;");
    formLayout->addRow(tr("CLI Command:"), cliCommandPreviewEdit_);

    splitter->addWidget(optionsGroup);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    layout->addWidget(splitter, 1);
}

void CompilerPage::setupDirectoryTab(QWidget* parent) {
    auto layout = new QVBoxLayout(parent);
    layout->setContentsMargins(4, 8, 4, 4);
    layout->setSpacing(10);

    auto configGroup = new QGroupBox(tr("Project Workspace Configuration"), parent);
    auto formLayout = new QFormLayout(configGroup);
    formLayout->setSpacing(8);

    moduleCombo_ = new QComboBox(this);
    moduleCombo_->addItem(tr("qagame (Server Game Logic)"), "qagame");
    moduleCombo_->addItem(tr("cgame (Client Game & Rendering)"), "cgame");
    moduleCombo_->addItem(tr("ui (Quake III Arena Menu & UI)"), "ui");
    moduleCombo_->addItem(tr("custom (Custom Mod Source)"), "custom");
    formLayout->addRow(tr("Target Module:"), moduleCombo_);

    auto sourceLayout = new QHBoxLayout();
    sourceDirEdit_ = new QLineEdit(this);
    sourceDirEdit_->setPlaceholderText(tr("Select folder containing Quake III C source code..."));
    auto browseSourceBtn = new QPushButton(QIcon(":/icons/open.svg"), tr("Browse..."), this);
    connect(browseSourceBtn, &QPushButton::clicked, this, &CompilerPage::onBrowseSource);
    sourceLayout->addWidget(sourceDirEdit_);
    sourceLayout->addWidget(browseSourceBtn);
    formLayout->addRow(tr("Source Directory:"), sourceLayout);

    auto sdkDirLayout = new QHBoxLayout();
    dirSdkDirEdit_ = new QLineEdit(this);
    dirSdkDirEdit_->setPlaceholderText(tr("Quake 3 SDK or code directory (optional)..."));
    browseDirSdkBtn_ = new QPushButton(QIcon(":/icons/open.svg"), tr("Select Quake 3 SDK..."), this);
    browseDirSdkBtn_->setToolTip(tr("Select Quake 3 SDK or code directory to resolve all libraries & headers (qcommon, cgame, game, ui)"));
    connect(browseDirSdkBtn_, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select Quake 3 SDK or Code Directory"), dirSdkDirEdit_->text());
        if (!dir.isEmpty()) {
            dirSdkDirEdit_->setText(dir);
            if (editorSdkDirEdit_ && editorSdkDirEdit_->text().isEmpty()) {
                editorSdkDirEdit_->setText(dir);
            }
        }
    });
    sdkDirLayout->addWidget(dirSdkDirEdit_);
    sdkDirLayout->addWidget(browseDirSdkBtn_);
    formLayout->addRow(tr("Quake 3 SDK:"), sdkDirLayout);

    auto outputLayout = new QHBoxLayout();
    outputDirEdit_ = new QLineEdit(this);
    outputDirEdit_->setPlaceholderText(tr("Select destination build folder (default: source/build)..."));
    auto browseOutputBtn = new QPushButton(QIcon(":/icons/open.svg"), tr("Browse..."), this);
    connect(browseOutputBtn, &QPushButton::clicked, this, &CompilerPage::onBrowseOutput);
    outputLayout->addWidget(outputDirEdit_);
    outputLayout->addWidget(browseOutputBtn);
    formLayout->addRow(tr("Output Directory:"), outputLayout);

    outputNameEdit_ = new QLineEdit(this);
    outputNameEdit_->setPlaceholderText(tr("Leave blank for default (e.g. qagame.qvm)"));
    formLayout->addRow(tr("Output Name:"), outputNameEdit_);

    auto toolchainLayout = new QHBoxLayout();
    toolchainDirEdit_ = new QLineEdit(this);
    toolchainDirEdit_->setPlaceholderText(tr("Auto-detected by default (or select custom toolchain directory)"));
    auto browseToolchainBtn = new QPushButton(QIcon(":/icons/open.svg"), tr("Browse..."), this);
    connect(browseToolchainBtn, &QPushButton::clicked, this, &CompilerPage::onBrowseToolchain);
    toolchainLayout->addWidget(toolchainDirEdit_);
    toolchainLayout->addWidget(browseToolchainBtn);
    formLayout->addRow(tr("Compiler Toolchain:"), toolchainLayout);

    auto checkLayout = new QHBoxLayout();
    debugSymbolsCheck_ = new QCheckBox(tr("Include Debug Symbols (-Wf-g)"), this);
    verboseCheck_ = new QCheckBox(tr("Verbose Q3ASM Output (-v)"), this);
    cleanIntermediateCheck_ = new QCheckBox(tr("Clean Intermediate Assembly (.asm)"), this);
    checkLayout->addWidget(debugSymbolsCheck_);
    checkLayout->addWidget(verboseCheck_);
    checkLayout->addWidget(cleanIntermediateCheck_);
    checkLayout->addStretch();
    formLayout->addRow(tr("Options:"), checkLayout);

    layout->addWidget(configGroup);

    auto actionLayout = new QHBoxLayout();
    compileDirBtn_ = new QPushButton(QIcon(":/icons/compiler.svg"), tr("Compile Project Directory to QVM"), this);
    compileDirBtn_->setProperty("primary", true);
    compileDirBtn_->setFixedHeight(34);
    connect(compileDirBtn_, &QPushButton::clicked, this, &CompilerPage::onStartCompileDirectory);
    actionLayout->addWidget(compileDirBtn_);
    actionLayout->addStretch();
    layout->addLayout(actionLayout);
    layout->addStretch();
}

void CompilerPage::setTheme(EditorTheme theme) {
    currentTheme_ = theme;
    if (cCodeEditor_) {
        cCodeEditor_->setTheme(theme);
    }
}

void CompilerPage::onOpenCFile() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open C Source File"), QString(), tr("C Source Files (*.c *.h);;All Files (*.*)"));
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            cCodeEditor_->setPlainText(QString::fromUtf8(file.readAll()));
            editorOutputNameEdit_->setText(QFileInfo(path).baseName() + ".qvm");
            updateCliCommandPreview();
        }
    }
}

void CompilerPage::onSaveCFile() {
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save C Source File"), "g_main.c", tr("C Source Files (*.c);;All Files (*.*)"));
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QFile::WriteOnly | QFile::Text)) {
            file.write(cCodeEditor_->toPlainText().toUtf8());
        }
    }
}

void CompilerPage::onLoadStarterTemplate() {
    cCodeEditor_->setPlainText(QString::fromUtf8(kDefaultVmMainTemplate));
    editorOutputNameEdit_->setText("custom.qvm");
    updateCliCommandPreview();
}

void CompilerPage::updateCliCommandPreview() {
    QString cmd = "q3vm compile --source ./source --output " + editorOutputDirEdit_->text();
    if (!editorOutputNameEdit_->text().isEmpty()) {
        cmd += " --name " + editorOutputNameEdit_->text();
    }
    if (editorSdkDirEdit_ && !editorSdkDirEdit_->text().trimmed().isEmpty()) {
        cmd += " --sdk \"" + editorSdkDirEdit_->text().trimmed() + "\"";
    }
    if (editorDebugCheck_->isChecked()) {
        cmd += " --debug";
    }
    if (editorVerboseCheck_->isChecked()) {
        cmd += " --verbose";
    }
    if (!editorDefinesEdit_->text().isEmpty()) {
        cmd += " " + editorDefinesEdit_->text();
    }
    if (!editorIncludesEdit_->text().isEmpty()) {
        cmd += " " + editorIncludesEdit_->text();
    }
    cliCommandPreviewEdit_->setText(cmd);
}

void CompilerPage::onCopyCliCommand() {
    QApplication::clipboard()->setText(cliCommandPreviewEdit_->text());
    statusLabel_->setText(tr("CLI command copied to clipboard."));
}

void CompilerPage::onBrowseSource() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Quake III C Source Directory"), sourceDirEdit_->text());
    if (!dir.isEmpty()) {
        sourceDirEdit_->setText(dir);
        if (outputDirEdit_->text().isEmpty()) {
            outputDirEdit_->setText(dir + "/build");
        }
    }
}

void CompilerPage::onBrowseOutput() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Output Directory"), outputDirEdit_->text());
    if (!dir.isEmpty()) {
        outputDirEdit_->setText(dir);
    }
}

void CompilerPage::onBrowseToolchain() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Custom Toolchain Directory"), toolchainDirEdit_->text());
    if (!dir.isEmpty()) {
        toolchainDirEdit_->setText(dir);
    }
}

void CompilerPage::onBrowseEditorOutputDir() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Output Directory"), editorOutputDirEdit_->text());
    if (!dir.isEmpty()) {
        editorOutputDirEdit_->setText(dir);
        updateCliCommandPreview();
    }
}

void CompilerPage::onBrowseSdk() {
    QString current = editorSdkDirEdit_->text();
    if (current.isEmpty() && dirSdkDirEdit_) {
        current = dirSdkDirEdit_->text();
    }
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Quake III SDK or Code Directory"), current);
    if (!dir.isEmpty()) {
        editorSdkDirEdit_->setText(dir);
        if (dirSdkDirEdit_ && dirSdkDirEdit_->text().isEmpty()) {
            dirSdkDirEdit_->setText(dir);
        }
        onSdkDirChanged(dir);
    }
}

void CompilerPage::onSdkDirChanged(const QString& sdkPath) {
    if (!sdkPath.trimmed().isEmpty()) {
        statusLabel_->setText(tr("Quake III SDK set: %1 (qcommon, cgame, game, ui auto-resolved)").arg(sdkPath));
    }
    updateCliCommandPreview();
}

void CompilerPage::onStartCompileEditor() {
    QString sourceCode = cCodeEditor_->toPlainText();
    if (sourceCode.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Empty Source Code"), tr("Please write or paste C source code into the editor."));
        return;
    }

    CompilerConfig config;
    config.module = ModuleType::Custom;
    config.outputDir = editorOutputDirEdit_->text().toStdString();
    config.outputName = editorOutputNameEdit_->text().toStdString();
    if (editorSdkDirEdit_ && !editorSdkDirEdit_->text().trimmed().isEmpty()) {
        config.sdkDir = editorSdkDirEdit_->text().trimmed().toStdString();
    }
    config.debugSymbols = editorDebugCheck_->isChecked();
    config.verbose = editorVerboseCheck_->isChecked();

    // Parse defines
    QString defs = editorDefinesEdit_->text();
    for (const auto& d : defs.split(' ', Qt::SkipEmptyParts)) {
        config.preprocessorDefines.push_back(d.toStdString());
    }

    // Parse includes
    QString incs = editorIncludesEdit_->text().trimmed();
    QString currentInc;
    bool inQuotes = false;
    QChar quoteChar;
    QStringList incTokens;
    for (int i = 0; i < incs.length(); ++i) {
        QChar c = incs[i];
        if ((c == '"' || c == '\'') && !inQuotes) {
            inQuotes = true;
            quoteChar = c;
        } else if (inQuotes && c == quoteChar) {
            inQuotes = false;
        } else if (!inQuotes && (c == ' ' || c == ';' || c == '\t')) {
            if (!currentInc.isEmpty()) {
                incTokens.append(currentInc);
                currentInc.clear();
            }
        } else {
            currentInc.append(c);
        }
    }
    if (!currentInc.isEmpty()) {
        incTokens.append(currentInc);
    }

    for (QString inc : incTokens) {
        inc = inc.trimmed();
        if (inc.startsWith("-I")) {
            inc = inc.mid(2).trimmed();
        }
        if ((inc.startsWith('"') && inc.endsWith('"')) || (inc.startsWith('\'') && inc.endsWith('\''))) {
            inc = inc.mid(1, inc.length() - 2);
        }
        QFileInfo fi(inc);
        if (fi.isFile()) {
            inc = fi.dir().absolutePath();
        }
        if (!inc.isEmpty()) {
            config.includePaths.push_back(inc.toStdString());
        }
    }

    // Reset console
    allLogsEdit_->clear();
    lccLogsEdit_->clear();
    q3asmLogsEdit_->clear();
    errorLogsEdit_->clear();

    progressBar_->setVisible(true);
    progressBar_->setValue(0);
    compileEditorBtn_->setEnabled(false);
    statusLabel_->setText(tr("Compiling C code from editor..."));

    auto* worker = new CompileWorker(config, sourceCode.toStdString(), this);
    connect(worker, &CompileWorker::progressUpdated, this, &CompilerPage::onProgressUpdated);
    connect(worker, &CompileWorker::logReceived, this, &CompilerPage::onLogReceived);
    connect(worker, &CompileWorker::finished, this, &CompilerPage::onCompilationFinished);
    connect(worker, &CompileWorker::finished, worker, &QObject::deleteLater);
    worker->start();
}

void CompilerPage::onStartCompileDirectory() {
    if (sourceDirEdit_->text().isEmpty()) {
        QMessageBox::warning(this, tr("Missing Source Directory"), tr("Please select a source directory to compile."));
        return;
    }

    CompilerConfig config;
    config.module = stringToModuleType(moduleCombo_->currentData().toString().toStdString());
    config.sourceDir = sourceDirEdit_->text().toStdString();
    config.outputDir = outputDirEdit_->text().toStdString();
    config.outputName = outputNameEdit_->text().toStdString();
    if (dirSdkDirEdit_ && !dirSdkDirEdit_->text().trimmed().isEmpty()) {
        config.sdkDir = dirSdkDirEdit_->text().trimmed().toStdString();
    }
    config.toolchainDir = toolchainDirEdit_->text().toStdString();
    config.debugSymbols = debugSymbolsCheck_->isChecked();
    config.verbose = verboseCheck_->isChecked();
    config.cleanIntermediate = cleanIntermediateCheck_->isChecked();

    allLogsEdit_->clear();
    lccLogsEdit_->clear();
    q3asmLogsEdit_->clear();
    errorLogsEdit_->clear();

    progressBar_->setVisible(true);
    progressBar_->setValue(0);
    compileDirBtn_->setEnabled(false);
    statusLabel_->setText(tr("Compiling project directory..."));

    auto* worker = new CompileWorker(config, "", this);
    connect(worker, &CompileWorker::progressUpdated, this, &CompilerPage::onProgressUpdated);
    connect(worker, &CompileWorker::logReceived, this, &CompilerPage::onLogReceived);
    connect(worker, &CompileWorker::finished, this, &CompilerPage::onCompilationFinished);
    connect(worker, &CompileWorker::finished, worker, &QObject::deleteLater);
    worker->start();
}

void CompilerPage::onProgressUpdated(int current, int total, QString message) {
    if (total > 0) {
        int pct = qBound(0, static_cast<int>((current * 100) / total), 100);
        progressBar_->setValue(pct);
    }
    statusLabel_->setText(message);
    allLogsEdit_->appendPlainText(message);
}

void CompilerPage::onLogReceived(QString message, bool isError) {
    allLogsEdit_->appendPlainText(message);

    if (message.contains("Compiling") || message.contains("cpp:") || message.contains("lcc")) {
        lccLogsEdit_->appendPlainText(message);
    } else if (message.contains("Assembling") || message.contains("q3asm") || message.contains("Pass")) {
        q3asmLogsEdit_->appendPlainText(message);
    }

    if (isError || message.contains("error:", Qt::CaseInsensitive) || message.contains("warning:", Qt::CaseInsensitive)) {
        errorLogsEdit_->appendPlainText(message);
    }
}

void CompilerPage::onCompilationFinished(bool success, QString summary, QString qvmPath) {
    progressBar_->setVisible(false);
    compileEditorBtn_->setEnabled(true);
    compileDirBtn_->setEnabled(true);
    statusLabel_->setText(summary);

    if (success) {
        allLogsEdit_->appendPlainText("\n=== BUILD SUCCEEDED ===\n" + summary);
        emit compilationSucceeded(qvmPath);
    } else {
        allLogsEdit_->appendPlainText("\n=== BUILD FAILED ===\n" + summary);
        QMessageBox::critical(this, tr("Build Failed"), summary);
    }
}

void CompilerPage::setSourceDirectory(const QString& path) {
    sourceDirEdit_->setText(path);
    if (outputDirEdit_->text().isEmpty()) {
        outputDirEdit_->setText(path + "/build");
    }
}

} // namespace qvm::gui
