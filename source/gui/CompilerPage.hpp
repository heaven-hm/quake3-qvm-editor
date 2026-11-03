#pragma once

#include "qvm/compiler/BuildPipeline.hpp"
#include "qvm/compiler/Compiler.hpp"
#include "qvm/compiler/CompilerConfig.hpp"
#include "source/gui/widgets/CodeEditor.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QThread>
#include <QWidget>

namespace qvm::gui {

class CompileWorker : public QThread {
    Q_OBJECT

public:
    explicit CompileWorker(CompilerConfig config, std::string sourceCode = "", QObject* parent = nullptr);
    void run() override;

signals:
    void progressUpdated(int current, int total, QString message);
    void logReceived(QString message, bool isError);
    void finished(bool success, QString summary, QString qvmPath);

private:
    CompilerConfig config_;
    std::string sourceCode_;
};

class CompilerPage : public QWidget {
    Q_OBJECT

public:
    explicit CompilerPage(QWidget* parent = nullptr);

    void setSourceDirectory(const QString& path);
    void setTheme(EditorTheme theme);

signals:
    void compilationSucceeded(const QString& qvmFilePath);

private slots:
    void onBrowseSource();
    void onBrowseOutput();
    void onBrowseToolchain();
    void onStartCompileDirectory();
    void onStartCompileEditor();
    void onOpenCFile();
    void onSaveCFile();
    void onLoadStarterTemplate();
    void onCopyCliCommand();
    void updateCliCommandPreview();
    void onProgressUpdated(int current, int total, QString message);
    void onLogReceived(QString message, bool isError);
    void onCompilationFinished(bool success, QString summary, QString qvmPath);

private:
    void setupUi();
    void setupEditorTab(QWidget* parent);
    void setupDirectoryTab(QWidget* parent);

    QTabWidget* modeTabs_ = nullptr;

    // --- Interactive C Editor Tab UI ---
    CodeEditor* cCodeEditor_ = nullptr;
    QLineEdit* editorOutputNameEdit_ = nullptr;
    QLineEdit* editorOutputDirEdit_ = nullptr;
    QLineEdit* editorDefinesEdit_ = nullptr;
    QLineEdit* editorIncludesEdit_ = nullptr;
    QCheckBox* editorDebugCheck_ = nullptr;
    QCheckBox* editorVerboseCheck_ = nullptr;
    QLineEdit* cliCommandPreviewEdit_ = nullptr;
    QPushButton* compileEditorBtn_ = nullptr;

    // --- Directory Batch Build Tab UI ---
    QComboBox* moduleCombo_ = nullptr;
    QLineEdit* sourceDirEdit_ = nullptr;
    QLineEdit* outputDirEdit_ = nullptr;
    QLineEdit* outputNameEdit_ = nullptr;
    QLineEdit* toolchainDirEdit_ = nullptr;
    QCheckBox* debugSymbolsCheck_ = nullptr;
    QCheckBox* verboseCheck_ = nullptr;
    QCheckBox* cleanIntermediateCheck_ = nullptr;
    QPushButton* compileDirBtn_ = nullptr;

    // --- Shared Progress & Logs UI ---
    QProgressBar* progressBar_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    QTabWidget* logTabs_ = nullptr;
    QPlainTextEdit* allLogsEdit_ = nullptr;
    QPlainTextEdit* lccLogsEdit_ = nullptr;
    QPlainTextEdit* q3asmLogsEdit_ = nullptr;
    QPlainTextEdit* errorLogsEdit_ = nullptr;

    EditorTheme currentTheme_{EditorTheme::Dark};
};

} // namespace qvm::gui
