#pragma once

#include "source/gui/CompilerPage.hpp"
#include "source/gui/DecompilerPage.hpp"
#include <QAction>
#include <QMainWindow>
#include <QStackedWidget>
#include <QToolBar>

namespace qvm::gui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onShowCompiler();
    void onShowDecompiler();
    void onCompilationSucceeded(const QString& qvmFilePath);
    void onAbout();

private:
    void setupUi();
    void setupMenus();
    void applyDarkTheme();

    QStackedWidget* stackedWidget_ = nullptr;
    CompilerPage* compilerPage_ = nullptr;
    DecompilerPage* decompilerPage_ = nullptr;

    QAction* actionCompiler_ = nullptr;
    QAction* actionDecompiler_ = nullptr;
    QAction* actionAbout_ = nullptr;
};

} // namespace qvm::gui
