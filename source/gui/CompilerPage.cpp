#include "source/gui/CompilerPage.hpp"
#include "qvm/compiler/Compiler.hpp"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QMessageBox>
#include <QMimeData>
#include <QShortcut>
#include <QSplitter>
#include <QVBoxLayout>

namespace qvm::gui {

CompileWorker::CompileWorker(CompilerConfig config, std::string sourceCode, QObject* parent)
    : QThread(parent), config_(std::move(config)), sourceCode_(std::move(sourceCode)) {}

void CompileWorker::run() {
    auto progress = [this](int current, int total, std::string_view msg) {
        emit progressUpdated(current, total, QString::fromUtf8(msg.data(), msg.size()));
    };
    auto log = [this](std::string_view line, bool isErr) {
        emit logReceived(QString::fromUtf8(line.data(), line.size()), isErr);
    };

    if (!sourceCode_.empty()) {
        // In-memory compile from the editor buffer.
        auto res = Compiler::compileSourceCode(sourceCode_, config_, progress, log);
        if (res.hasError()) {
            emit finished(false, QString::fromStdString(res.error().message()), QString());
        } else {
            const auto& r = res.value();
            emit finished(true,
                          QString("Compiled %1 (%2 bytes)").arg(QFileInfo(QString::fromStdString(r.outputQvmPath.string())).fileName()).arg(r.qvmFileSize),
                          QString::fromStdString(r.outputQvmPath.string()));
        }
    } else {
        // Directory/file-based compile.
        auto res = Compiler::compile(config_, progress, log);
        if (res.hasError()) {
            emit finished(false, QString::fromStdString(res.error().message()), QString());
        } else {
            const auto& r = res.value();
            emit finished(true,
                          QString("Compiled %1 (%2 bytes)").arg(QFileInfo(QString::fromStdString(r.outputQvmPath.string())).fileName()).arg(r.qvmFileSize),
                          QString::fromStdString(r.outputQvmPath.string()));
        }
    }
}

RunWorker::RunWorker(QString qvmPath, qvm::RuntimeOptions opts, QObject* parent)
    : QThread(parent), qvmPath_(std::move(qvmPath)), opts_(std::move(opts)) {}

void RunWorker::run() {
    auto onOutput = [this](std::string_view line, bool isError) {
        emit outputReceived(QString::fromUtf8(line.data(), line.size()), isError);
    };
    auto res = qvm::QvmRuntime::run(qvmPath_.toStdString(), opts_, onOutput);
    if (!res.hasValue()) {
        emit finished(false, QString::fromStdString(res.error().message()));
    } else {
        const auto& r = res.value();
        for (const auto& traceLine : r.trace) {
            emit outputReceived(QString("TRACE  %1").arg(QString::fromStdString(traceLine)), false);
        }
        emit finished(r.success,
                      r.success ? QString("VM exited OK (%1 syscalls, %2 ms)").arg(r.syscallCount).arg((int)r.durationMs)
                                : QString("QVM terminated: %1").arg(QString::fromStdString(r.error)));
    }
}

// ==============================================================================
// CompilerPage Implementation
// ==============================================================================

const char* const kDefaultVmMainTemplate =
R"(// Compatibility for newer ioquake-derived SDKs whose bg_lib.h expects
// these optional compiler annotations to be declared by q_shared.h first.
#ifndef Q_PRINTF_FUNC
#define Q_PRINTF_FUNC(fmt, va)
#endif
#ifndef Q_SCANF_FUNC
#define Q_SCANF_FUNC(fmt, va)
#endif

#include "bg_lib.h"

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
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    // trap_Print is available in the qagame, cgame and ui syscall tables.
    // (trap_Printf only exists for qagame, so trap_Print keeps this template
    //  linkable against whichever module is selected.)
    trap_Print(text);
}

int fibonacci(int n)
{
    if (n <= 1)
        return n;
    return    fibonacci(n - 1) + fibonacci(n - 2);
}
)";

// ==============================================================================
// CompilerPage Implementation
// ==============================================================================

CompilerPage::CompilerPage(QWidget* parent) : QWidget(parent) {
    setAcceptDrops(true);
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

    auto logsToolbar = new QHBoxLayout();
    auto clearLogs = new QPushButton(QIcon(":/icons/clear.svg"), tr("Clear Logs"), this);
    auto exportLogs = new QPushButton(QIcon(":/icons/export.svg"), tr("Save Logs..."), this);
    connect(clearLogs, &QPushButton::clicked, this, &CompilerPage::onClearLogs);
    connect(exportLogs, &QPushButton::clicked, this, &CompilerPage::onExportLogs);
    logsToolbar->addStretch();
    logsToolbar->addWidget(clearLogs);
    logsToolbar->addWidget(exportLogs);
    bottomLayout->addLayout(logsToolbar);

    logTabs_ = new QTabWidget(this);
    logTabs_->setFixedHeight(160);

    allLogsEdit_ = new QPlainTextEdit(this);
    allLogsEdit_->setReadOnly(true);
    allLogsEdit_->setFont(QFont("Menlo", 10));
    allLogsEdit_->document()->setMaximumBlockCount(4000);

    lccLogsEdit_ = new QPlainTextEdit(this);
    lccLogsEdit_->setReadOnly(true);
    lccLogsEdit_->setFont(QFont("Menlo", 10));
    lccLogsEdit_->document()->setMaximumBlockCount(4000);

    q3asmLogsEdit_ = new QPlainTextEdit(this);
    q3asmLogsEdit_->setReadOnly(true);
    q3asmLogsEdit_->setFont(QFont("Menlo", 10));
    q3asmLogsEdit_->document()->setMaximumBlockCount(4000);

    errorLogsEdit_ = new QPlainTextEdit(this);
    errorLogsEdit_->setReadOnly(true);
    errorLogsEdit_->setFont(QFont("Menlo", 10));
    errorLogsEdit_->document()->setMaximumBlockCount(4000);

    logTabs_->addTab(allLogsEdit_, QIcon(":/icons/editor.svg"), tr("All Output"));
    logTabs_->addTab(lccLogsEdit_, QIcon(":/icons/code.svg"), tr("LCC Output"));
    logTabs_->addTab(q3asmLogsEdit_, QIcon(":/icons/asm.svg"), tr("Q3ASM Output"));
    logTabs_->addTab(errorLogsEdit_, QIcon(":/icons/info.svg"), tr("Errors & Warnings"));

    vmOutputEdit_ = new QPlainTextEdit(this);
    vmOutputEdit_->setReadOnly(true);
    vmOutputEdit_->setFont(QFont("Menlo", 10));
    vmOutputEdit_->document()->setMaximumBlockCount(4000);
    logTabs_->addTab(vmOutputEdit_, QIcon(":/icons/play.svg"), tr("VM Output"));

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

    runBtn_ = new QPushButton(QIcon(":/icons/play.svg"), tr("Run QVM (F5)"), this);
    runBtn_->setFixedHeight(32);
    connect(runBtn_, &QPushButton::clicked, this, &CompilerPage::onStartRun);

    // Ctrl+Enter / Cmd+Enter Shortcut
    auto shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(shortcut, &QShortcut::activated, this, &CompilerPage::onStartCompileEditor);

    // F5 -> Run QVM
    auto runShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(runShortcut, &QShortcut::activated, this, &CompilerPage::onStartRun);

    barLayout->addWidget(openBtn);
    barLayout->addWidget(saveBtn);
    barLayout->addWidget(templateBtn);
    barLayout->addWidget(copyCmdBtn);
    barLayout->addStretch();
    barLayout->addWidget(compileEditorBtn_);
    barLayout->addWidget(runBtn_);
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

    editorModuleCombo_ = new QComboBox(this);
    editorModuleCombo_->addItem(tr("qagame (Server Game Logic)"), "qagame");
    editorModuleCombo_->addItem(tr("cgame (Client Game & Rendering)"), "cgame");
    editorModuleCombo_->addItem(tr("ui (Quake III Arena Menu & UI)"), "ui");
    editorModuleCombo_->addItem(tr("custom (Standalone Mod)"), "custom");
    connect(editorModuleCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CompilerPage::onEditorModuleChanged);
    formLayout->addRow(tr("Target Module:"), editorModuleCombo_);

    editorMappingLabel_ = new QLabel(this);
    editorMappingLabel_->setWordWrap(true);
    editorMappingLabel_->setStyleSheet("color: #7ecdf3; font-family: monospace; font-size: 11px;");
    formLayout->addRow(tr("Enforced Mapping:"), editorMappingLabel_);

    editorOutputNameEdit_ = new QLineEdit("qagame.qvm", this);
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

    auto filesLayout = new QHBoxLayout();
    selectedFilesEdit_ = new QLineEdit(this);
    selectedFilesEdit_->setPlaceholderText(tr("Selected source files (.c/.asm) to compile (leave blank for module defaults)..."));
    browseSelectedFilesBtn_ = new QPushButton(QIcon(":/icons/open.svg"), tr("Select Files..."), this);
    browseSelectedFilesBtn_->setToolTip(tr("Select specific .c/.asm source files and dependencies to compile"));
    connect(browseSelectedFilesBtn_, &QPushButton::clicked, this, [this]() {
        QString dir = sourceDirEdit_->text();
        QStringList files = QFileDialog::getOpenFileNames(
            this, tr("Select Source Files to Compile"), dir, tr("C/ASM Files (*.c *.asm);;All Files (*.*)"));
        if (!files.isEmpty()) {
            selectedFilesEdit_->setText(files.join("; "));
        }
    });
    filesLayout->addWidget(selectedFilesEdit_);
    filesLayout->addWidget(browseSelectedFilesBtn_);
    formLayout->addRow(tr("Source Files:"), filesLayout);

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
        openSourceFile(path);
    }
}

void CompilerPage::openSourceFile(const QString& path) {
    QFile file(path);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("Open Source Failed"), tr("Cannot open %1").arg(path));
        return;
    }
    cCodeEditor_->setPlainText(QString::fromUtf8(file.readAll()));
    cCodeEditor_->document()->setModified(false);
    currentSourcePath_ = path;
    editorOutputNameEdit_->setText(QFileInfo(path).baseName() + ".qvm");
    lastSourceDir_ = QFileInfo(path).absolutePath();
    appendLog(allLogsEdit_, tr("Opened source: %1").arg(path));
    updateCliCommandPreview();
}

void CompilerPage::onSaveCFile() {
    QString path = currentSourcePath_;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this, tr("Save C Source File"), "g_main.c", tr("C Source Files (*.c);;All Files (*.*)"));
    }
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QFile::WriteOnly | QFile::Text)) {
            file.write(cCodeEditor_->toPlainText().toUtf8());
            currentSourcePath_ = path;
            cCodeEditor_->document()->setModified(false);
            appendLog(allLogsEdit_, tr("Saved source: %1").arg(path));
        }
    }
}

void CompilerPage::onLoadStarterTemplate() {
    cCodeEditor_->setPlainText(QString::fromUtf8(kDefaultVmMainTemplate));
    // Re-apply the active target module mapping (auto-selects output + syscall).
    onEditorModuleChanged(editorModuleCombo_ ? editorModuleCombo_->currentIndex() : 0);
}

void CompilerPage::onEditorModuleChanged(int /*index*/) {
    if (!editorModuleCombo_) {
        return;
    }

    const ModuleType module =
        stringToModuleType(editorModuleCombo_->currentData().toString().toStdString());
    const ModuleMapping mapping = ModuleConfig::getModuleMapping(module);

    // Enforce the fixed per-module mapping: one source, one syscall table, one output.
    editorOutputNameEdit_->setText(QString::fromStdString(mapping.outputFile));

    if (editorMappingLabel_) {
        editorMappingLabel_->setText(
            tr("source: %1    syscall: %2    output: %3")
                .arg(QString::fromStdString(mapping.sourceFile))
                .arg(QString::fromStdString(mapping.syscallFile))
                .arg(QString::fromStdString(mapping.outputFile)));
    }

    updateCliCommandPreview();
}

void CompilerPage::updateCliCommandPreview() {
    QString cmd = "q3vm compile --source ./source";
    if (editorModuleCombo_) {
        cmd += " --module " + editorModuleCombo_->currentData().toString();
    }
    cmd += " --output " + editorOutputDirEdit_->text();
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
    config.module = stringToModuleType(editorModuleCombo_->currentData().toString().toStdString());
    config.linkFullModule = (config.module != ModuleType::Custom);
    config.outputDir = editorOutputDirEdit_->text().toStdString();
    config.outputName = editorOutputNameEdit_->text().toStdString();
    if (editorSdkDirEdit_ && !editorSdkDirEdit_->text().trimmed().isEmpty()) {
        config.sdkDir = editorSdkDirEdit_->text().trimmed().toStdString();
    }
    config.debugSymbols = editorDebugCheck_->isChecked();
    config.verbose = editorVerboseCheck_->isChecked();

    // Prefer the directory of the last opened C file so a module source (e.g.
    // code/cgame.c) resolves its module tree and matching headers first.
    if (!lastSourceDir_.isEmpty()) {
        config.includePaths.push_back(lastSourceDir_.toStdString());
    }

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

    QString filesStr = selectedFilesEdit_ ? selectedFilesEdit_->text().trimmed() : "";
    if (!filesStr.isEmpty()) {
        for (const auto& token : filesStr.split(QRegularExpression("[;,\\s]+"), Qt::SkipEmptyParts)) {
            config.selectedFiles.push_back(token.toStdString());
        }
    }

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
    appendLog(allLogsEdit_, message);

    if (message.contains("Compiling") || message.contains("cpp:") || message.contains("lcc")) {
        appendLog(lccLogsEdit_, message);
    } else if (message.contains("Assembling") || message.contains("q3asm") || message.contains("Pass")) {
        appendLog(q3asmLogsEdit_, message);
    }

    if (isError || message.contains("error:", Qt::CaseInsensitive) || message.contains("warning:", Qt::CaseInsensitive)) {
        appendLog(errorLogsEdit_, message);
    }
}

void CompilerPage::onCompilationFinished(bool success, QString summary, QString qvmPath) {
    progressBar_->setVisible(false);
    compileEditorBtn_->setEnabled(true);
    compileDirBtn_->setEnabled(true);
    const QString statusText = summary.simplified();
    statusLabel_->setText(statusText.size() > 240 ? statusText.left(237) + "..." : statusText);
    statusLabel_->setToolTip(summary);

    if (success) {
        lastQvmPath_ = qvmPath;
        allLogsEdit_->appendPlainText("\n=== BUILD SUCCEEDED ===\n" + summary);
        emit compilationSucceeded(qvmPath);
    } else {
        allLogsEdit_->appendPlainText("\n=== BUILD FAILED ===\n" + summary);
        QMessageBox errorBox(QMessageBox::Critical, tr("Build Failed"),
                             tr("Compilation failed. Full diagnostics are available in "
                                "Build Logs → Errors & Warnings."),
                             QMessageBox::Ok, this);
        errorBox.setDetailedText(summary);
        if (!qEnvironmentVariableIsSet("QVM_NONINTERACTIVE_TEST")) {
            errorBox.exec();
        }
    }
}

void CompilerPage::onStartRun() {
    // Prefer the QVM path produced by the most recent successful compile in this
    // page; fall back to the editor output dir + name if no compile has happened.
    QString qvmPath;
    if (!lastQvmPath_.isEmpty()) {
        qvmPath = lastQvmPath_;
    } else {
        QString outDir = editorOutputDirEdit_->text().trimmed();
        QString outName = editorOutputNameEdit_->text().trimmed();
        if (outDir.isEmpty() || outName.isEmpty()) {
            QMessageBox::warning(this, tr("No QVM to Run"),
                tr("Compile a QVM first, or set the editor output directory and output name."));
            return;
        }
        QDir dir(outDir);
        if (!dir.exists()) {
            QMessageBox::warning(this, tr("Output Directory Missing"),
                tr("The editor output directory does not exist: %1").arg(outDir));
            return;
        }
        qvmPath = dir.filePath(outName);
    }

    if (!QFileInfo::exists(qvmPath)) {
        QMessageBox::warning(this, tr("QVM File Missing"),
            tr("Cannot find QVM file: %1").arg(qvmPath));
        return;
    }

    vmOutputEdit_->clear();
    vmOutputEdit_->appendPlainText(tr("Running %1...").arg(QFileInfo(qvmPath).fileName()));
    logTabs_->setCurrentWidget(vmOutputEdit_);
    vmOutputEdit_->setFocus();
    runBtn_->setEnabled(false);
    statusLabel_->setText(tr("Running %1...").arg(QFileInfo(qvmPath).fileName()));

    qvm::RuntimeOptions opts;
    opts.module = qvm::runtimeModuleFromString(
        editorModuleCombo_->currentData().toString().toStdString());

    auto* worker = new RunWorker(qvmPath, opts, this);
    connect(worker, &RunWorker::outputReceived, this, &CompilerPage::onRunOutput);
    connect(worker, &RunWorker::finished, this, &CompilerPage::onRunFinished);
    connect(worker, &RunWorker::finished, worker, &QObject::deleteLater);
    worker->start();
}

void CompilerPage::onRunOutput(QString line, bool isError) {
    appendLog(vmOutputEdit_, line);
    if (isError) {
        appendLog(errorLogsEdit_, line);
    }
}

void CompilerPage::onClearLogs() {
    for (auto* log : {allLogsEdit_, lccLogsEdit_, q3asmLogsEdit_, errorLogsEdit_, vmOutputEdit_}) log->clear();
    statusLabel_->setText(tr("Logs cleared."));
}

void CompilerPage::onExportLogs() {
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Logs"), "qvm-editor.log",
                                                      tr("Log Files (*.log *.txt);;All Files (*.*)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::critical(this, tr("Save Logs Failed"), tr("Cannot write %1").arg(path));
        return;
    }
    file.write(allLogsEdit_->toPlainText().toUtf8());
    file.write("\n\n=== VM OUTPUT ===\n");
    file.write(vmOutputEdit_->toPlainText().toUtf8());
    statusLabel_->setText(tr("Logs saved to %1").arg(path));
}

void CompilerPage::appendLog(QPlainTextEdit* destination, const QString& message) {
    destination->appendPlainText(QDateTime::currentDateTime().toString("HH:mm:ss.zzz") + "  " + message);
}

void CompilerPage::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasUrls()) return event->ignore();
    const QString path = event->mimeData()->urls().value(0).toLocalFile();
    if (path.endsWith(".c", Qt::CaseInsensitive) || path.endsWith(".h", Qt::CaseInsensitive)) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void CompilerPage::dropEvent(QDropEvent* event) {
    const QString path = event->mimeData()->urls().value(0).toLocalFile();
    if (!path.isEmpty()) {
        openSourceFile(path);
        event->acceptProposedAction();
    }
}

void CompilerPage::onRunFinished(bool success, QString summary) {
    vmOutputEdit_->appendPlainText("\n=== VM FINISHED ===\n" + summary);
    statusLabel_->setText(summary);
    runBtn_->setEnabled(true);
    logTabs_->setCurrentWidget(vmOutputEdit_);

    if (!success) {
        QMessageBox::warning(this, tr("VM Execution Issue"), summary);
    }
}

void CompilerPage::setSourceDirectory(const QString& path) {
    sourceDirEdit_->setText(path);
    if (outputDirEdit_->text().isEmpty()) {
        outputDirEdit_->setText(path + "/build");
    }
}

} // namespace qvm::gui
