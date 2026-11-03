#pragma once

#include "qvm/compiler/BuildPipeline.hpp"
#include "qvm/compiler/CompilerConfig.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QThread>
#include <QWidget>

namespace qvm::gui {

class CompileWorker : public QThread {
    Q_OBJECT

public:
    explicit CompileWorker(CompilerConfig config, QObject* parent = nullptr);
    void run() override;

signals:
    void progressUpdated(int current, int total, QString message);
    void logReceived(QString message, bool isError);
    void finished(bool success, QString summary, QString qvmPath);

private:
    CompilerConfig config_;
};

class CompilerPage : public QWidget {
    Q_OBJECT

public:
    explicit CompilerPage(QWidget* parent = nullptr);

    void setSourceDirectory(const QString& path);

signals:
    void compilationSucceeded(const QString& qvmFilePath);

private slots:
    void onBrowseSource();
    void onBrowseOutput();
    void onBrowseToolchain();
    void onStartCompile();
    void onProgressUpdated(int current, int total, QString message);
    void onLogReceived(QString message, bool isError);
    void onCompilationFinished(bool success, QString summary, QString qvmPath);

private:
    void setupUi();

    QComboBox* moduleCombo_ = nullptr;
    QLineEdit* sourceDirEdit_ = nullptr;
    QLineEdit* outputDirEdit_ = nullptr;
    QLineEdit* outputNameEdit_ = nullptr;
    QLineEdit* toolchainDirEdit_ = nullptr;

    QCheckBox* debugSymbolsCheck_ = nullptr;
    QCheckBox* verboseCheck_ = nullptr;
    QCheckBox* cleanIntermediateCheck_ = nullptr;

    QPushButton* compileButton_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    QTabWidget* logTabs_ = nullptr;
    QPlainTextEdit* allLogsEdit_ = nullptr;
    QPlainTextEdit* lccLogsEdit_ = nullptr;
    QPlainTextEdit* q3asmLogsEdit_ = nullptr;
    QPlainTextEdit* errorLogsEdit_ = nullptr;

    CompileWorker* currentWorker_ = nullptr;
};

} // namespace qvm::gui
