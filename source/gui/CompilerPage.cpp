#include "source/gui/CompilerPage.hpp"
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

namespace qvm::gui {

CompileWorker::CompileWorker(CompilerConfig config, QObject* parent)
    : QThread(parent), config_(std::move(config)) {}

void CompileWorker::run() {
    auto progress = [this](int current, int total, std::string_view msg) {
        emit progressUpdated(current, total, QString::fromUtf8(msg.data(), static_cast<qsizetype>(msg.size())));
    };

    auto log = [this](std::string_view line, bool isErr) {
        emit logReceived(QString::fromUtf8(line.data(), static_cast<qsizetype>(line.size())), isErr);
    };

    auto res = BuildPipeline::execute(config_, progress, log);
    if (res.hasError()) {
        emit finished(false, QString::fromStdString(res.error().message()), QString());
    } else {
        const auto& report = res.value();
        QString summary = QString("Built in %1 seconds. File size: %2 bytes")
            .arg(report.durationSeconds, 0, 'f', 2)
            .arg(report.qvmFileSize);
        emit finished(true, summary, QString::fromStdString(report.outputQvmPath.string()));
    }
}

CompilerPage::CompilerPage(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void CompilerPage::setupUi() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // Configuration Group
    auto configGroup = new QGroupBox(tr("Build Configuration"), this);
    auto formLayout = new QFormLayout(configGroup);
    formLayout->setSpacing(8);

    // Module Combo
    moduleCombo_ = new QComboBox(this);
    moduleCombo_->addItem(tr("qagame (Server Game Logic)"), "qagame");
    moduleCombo_->addItem(tr("cgame (Client Game & Rendering)"), "cgame");
    moduleCombo_->addItem(tr("ui (Quake III Arena Menu & UI)"), "ui");
    moduleCombo_->addItem(tr("custom (Custom Mod Source)"), "custom");
    formLayout->addRow(tr("Target Module:"), moduleCombo_);

    // Source Directory Picker
    auto sourceLayout = new QHBoxLayout();
    sourceDirEdit_ = new QLineEdit(this);
    sourceDirEdit_->setPlaceholderText(tr("Select folder containing Quake III C source code..."));
    auto browseSourceBtn = new QPushButton(QIcon(":/icons/open.svg"), tr("Browse..."), this);
    connect(browseSourceBtn, &QPushButton::clicked, this, &CompilerPage::onBrowseSource);
    sourceLayout->addWidget(sourceDirEdit_);
    sourceLayout->addWidget(browseSourceBtn);
    formLayout->addRow(tr("Source Directory:"), sourceLayout);

    // Output Directory Picker
    auto outputLayout = new QHBoxLayout();
    outputDirEdit_ = new QLineEdit(this);
    outputDirEdit_->setPlaceholderText(tr("Select destination build folder (default: source/build)..."));
    auto browseOutputBtn = new QPushButton(QIcon(":/icons/open.svg"), tr("Browse..."), this);
    connect(browseOutputBtn, &QPushButton::clicked, this, &CompilerPage::onBrowseOutput);
    outputLayout->addWidget(outputDirEdit_);
    outputLayout->addWidget(browseOutputBtn);
    formLayout->addRow(tr("Output Directory:"), outputLayout);

    // Output File Name
    outputNameEdit_ = new QLineEdit(this);
    outputNameEdit_->setPlaceholderText(tr("Leave blank for default (e.g. qagame.qvm)"));
    formLayout->addRow(tr("Output Name:"), outputNameEdit_);

    // Toolchain Directory
    auto toolchainLayout = new QHBoxLayout();
    toolchainDirEdit_ = new QLineEdit(this);
    toolchainDirEdit_->setPlaceholderText(tr("Auto-detected by default (or select custom toolchain directory)"));
    auto browseToolchainBtn = new QPushButton(QIcon(":/icons/open.svg"), tr("Browse..."), this);
    connect(browseToolchainBtn, &QPushButton::clicked, this, &CompilerPage::onBrowseToolchain);
    toolchainLayout->addWidget(toolchainDirEdit_);
    toolchainLayout->addWidget(browseToolchainBtn);
    formLayout->addRow(tr("Compiler Toolchain:"), toolchainLayout);

    // Checkboxes
    auto checkLayout = new QHBoxLayout();
    debugSymbolsCheck_ = new QCheckBox(tr("Include Debug Symbols (-Wf-g)"), this);
    verboseCheck_ = new QCheckBox(tr("Verbose Q3ASM Output (-v)"), this);
    cleanIntermediateCheck_ = new QCheckBox(tr("Clean Intermediate Assembly (.asm)"), this);
    cleanIntermediateCheck_->setChecked(false);
    checkLayout->addWidget(debugSymbolsCheck_);
    checkLayout->addWidget(verboseCheck_);
    checkLayout->addWidget(cleanIntermediateCheck_);
    checkLayout->addStretch();
    formLayout->addRow(tr("Options:"), checkLayout);

    mainLayout->addWidget(configGroup);

    // Action & Status Bar
    auto actionLayout = new QHBoxLayout();
    compileButton_ = new QPushButton(QIcon(":/icons/compiler.svg"), tr("Compile Quake III QVM"), this);
    compileButton_->setProperty("primary", true);
    compileButton_->setFixedHeight(36);
    connect(compileButton_, &QPushButton::clicked, this, &CompilerPage::onStartCompile);

    statusLabel_ = new QLabel(tr("Ready to compile."), this);
    statusLabel_->setStyleSheet("color: #9cad9f; font-weight: 500;");

    actionLayout->addWidget(compileButton_);
    actionLayout->addWidget(statusLabel_, 1);
    mainLayout->addLayout(actionLayout);

    // Progress Bar
    progressBar_ = new QProgressBar(this);
    progressBar_->setFixedHeight(18);
    progressBar_->setTextVisible(true);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    progressBar_->setVisible(false);
    mainLayout->addWidget(progressBar_);

    // Log Output Tabs
    logTabs_ = new QTabWidget(this);

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

    logTabs_->addTab(allLogsEdit_, QIcon(":/icons/editor.svg"), tr("Build Output"));
    logTabs_->addTab(lccLogsEdit_, QIcon(":/icons/code.svg"), tr("LCC Compiler"));
    logTabs_->addTab(q3asmLogsEdit_, QIcon(":/icons/asm.svg"), tr("Q3ASM Assembler"));
    logTabs_->addTab(errorLogsEdit_, QIcon(":/icons/info.svg"), tr("Errors & Warnings"));

    mainLayout->addWidget(logTabs_, 1);
}

void CompilerPage::setSourceDirectory(const QString& path) {
    sourceDirEdit_->setText(path);
}

void CompilerPage::onBrowseSource() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Source Directory"), sourceDirEdit_->text());
    if (!dir.isEmpty()) {
        sourceDirEdit_->setText(dir);
    }
}

void CompilerPage::onBrowseOutput() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"), outputDirEdit_->text());
    if (!dir.isEmpty()) {
        outputDirEdit_->setText(dir);
    }
}

void CompilerPage::onBrowseToolchain() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Toolchain Directory"), toolchainDirEdit_->text());
    if (!dir.isEmpty()) {
        toolchainDirEdit_->setText(dir);
    }
}

void CompilerPage::onStartCompile() {
    QString sourceDir = sourceDirEdit_->text().trimmed();
    if (sourceDir.isEmpty()) {
        QMessageBox::warning(this, tr("Source Directory Missing"),
                             tr("Please select a valid source directory containing Quake III C files."));
        return;
    }

    CompilerConfig config;
    config.module = stringToModuleType(moduleCombo_->currentData().toString().toStdString());
    config.sourceDir = sourceDir.toStdString();
    if (!outputDirEdit_->text().trimmed().isEmpty()) {
        config.outputDir = outputDirEdit_->text().trimmed().toStdString();
    }
    if (!outputNameEdit_->text().trimmed().isEmpty()) {
        config.outputName = outputNameEdit_->text().trimmed().toStdString();
    }
    if (!toolchainDirEdit_->text().trimmed().isEmpty()) {
        config.toolchainDir = toolchainDirEdit_->text().trimmed().toStdString();
    }
    config.debugSymbols = debugSymbolsCheck_->isChecked();
    config.verbose = verboseCheck_->isChecked();
    config.cleanIntermediate = cleanIntermediateCheck_->isChecked();

    allLogsEdit_->clear();
    lccLogsEdit_->clear();
    q3asmLogsEdit_->clear();
    errorLogsEdit_->clear();

    compileButton_->setEnabled(false);
    progressBar_->setVisible(true);
    progressBar_->setValue(0);
    statusLabel_->setText(tr("Compiling QVM..."));
    statusLabel_->setStyleSheet("color: #38bdf8; font-weight: bold;");

    currentWorker_ = new CompileWorker(config, this);
    connect(currentWorker_, &CompileWorker::progressUpdated, this, &CompilerPage::onProgressUpdated);
    connect(currentWorker_, &CompileWorker::logReceived, this, &CompilerPage::onLogReceived);
    connect(currentWorker_, &CompileWorker::finished, this, &CompilerPage::onCompilationFinished);
    currentWorker_->start();
}

void CompilerPage::onProgressUpdated(int current, int total, QString message) {
    if (total > 0) {
        int pct = (current * 100) / total;
        progressBar_->setValue(pct);
    }
    statusLabel_->setText(message);
}

void CompilerPage::onLogReceived(QString message, bool isError) {
    allLogsEdit_->appendPlainText(message);
    if (message.contains("Stage 1") || message.contains("LCC") || message.contains(".c")) {
        lccLogsEdit_->appendPlainText(message);
    }
    if (message.contains("Stage 2") || message.contains("Q3ASM") || message.contains(".asm")) {
        q3asmLogsEdit_->appendPlainText(message);
    }
    if (isError || message.contains("error", Qt::CaseInsensitive) || message.contains("warning", Qt::CaseInsensitive)) {
        errorLogsEdit_->appendPlainText(message);
    }
}

void CompilerPage::onCompilationFinished(bool success, QString summary, QString qvmPath) {
    compileButton_->setEnabled(true);
    progressBar_->setValue(100);

    if (success) {
        statusLabel_->setText(tr("SUCCESS: ") + summary);
        statusLabel_->setStyleSheet("color: #4ade80; font-weight: bold;");
        emit compilationSucceeded(qvmPath);
    } else {
        statusLabel_->setText(tr("FAILED: ") + summary);
        statusLabel_->setStyleSheet("color: #f87171; font-weight: bold;");
        logTabs_->setCurrentWidget(errorLogsEdit_);
        QMessageBox::critical(this, tr("Compilation Error"), summary);
    }

    if (currentWorker_) {
        currentWorker_->deleteLater();
        currentWorker_ = nullptr;
    }
}

} // namespace qvm::gui
