#pragma once

#include "source/gui/CompilerPage.hpp"
#include "source/gui/DecompilerPage.hpp"
#include "source/gui/widgets/CSyntaxHighlighter.hpp"
#include <QAction>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>
#include <QToolBar>

namespace qvm::gui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    void openQvmFile(const QString& filePath);

public slots:
    void applyTheme(EditorTheme theme);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onShowCompiler();
    void onShowDecompiler();
    void onCompilationSucceeded(const QString& qvmFilePath);
    void onAbout();
    void onThemeChanged(int index);

private:
    void setupUi();
    void setupMenus();

    QStackedWidget* stackedWidget_ = nullptr;
    CompilerPage* compilerPage_ = nullptr;
    DecompilerPage* decompilerPage_ = nullptr;

    QAction* actionCompiler_ = nullptr;
    QAction* actionDecompiler_ = nullptr;
    QAction* actionAbout_ = nullptr;

    QComboBox* themeCombo_ = nullptr;
    QLabel* statusPill_ = nullptr;
    EditorTheme currentTheme_{EditorTheme::Light};
};

} // namespace qvm::gui
