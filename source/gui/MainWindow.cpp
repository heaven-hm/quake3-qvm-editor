#include "source/gui/MainWindow.hpp"
#include "qvm/common/Version.hpp"
#include <QApplication>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QStyle>

namespace qvm::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    setupMenus();
    applyDarkTheme();

    setWindowTitle(QString("%1 v%2").arg(kAppName.data()).arg(kVersion.data()));
    resize(1200, 800);
}

void MainWindow::setupUi() {
    stackedWidget_ = new QStackedWidget(this);

    compilerPage_ = new CompilerPage(this);
    decompilerPage_ = new DecompilerPage(this);

    stackedWidget_->addWidget(compilerPage_);
    stackedWidget_->addWidget(decompilerPage_);

    setCentralWidget(stackedWidget_);

    connect(compilerPage_, &CompilerPage::compilationSucceeded,
            this, &MainWindow::onCompilationSucceeded);

    // Main navigation toolbar
    auto navToolBar = addToolBar(tr("Navigation"));
    navToolBar->setMovable(false);
    navToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    actionCompiler_ = navToolBar->addAction(
        style()->standardIcon(QStyle::SP_DriveCDIcon), tr("QVM Compiler"));
    actionCompiler_->setCheckable(true);
    actionCompiler_->setChecked(true);
    connect(actionCompiler_, &QAction::triggered, this, &MainWindow::onShowCompiler);

    actionDecompiler_ = navToolBar->addAction(
        style()->standardIcon(QStyle::SP_FileDialogContentsView), tr("QVM Decompiler & Disassembler"));
    actionDecompiler_->setCheckable(true);
    connect(actionDecompiler_, &QAction::triggered, this, &MainWindow::onShowDecompiler);

    statusBar()->showMessage(tr("Quake 3 QVM Studio ready."));
}

void MainWindow::setupMenus() {
    auto fileMenu = menuBar()->addMenu(tr("&File"));
    auto exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    auto viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(actionCompiler_);
    viewMenu->addAction(actionDecompiler_);

    auto helpMenu = menuBar()->addMenu(tr("&Help"));
    actionAbout_ = helpMenu->addAction(tr("&About Quake 3 QVM Studio"), this, &MainWindow::onAbout);
}

void MainWindow::onShowCompiler() {
    actionCompiler_->setChecked(true);
    actionDecompiler_->setChecked(false);
    stackedWidget_->setCurrentWidget(compilerPage_);
    statusBar()->showMessage(tr("Mode: Compiler (C Source -> QVM)"));
}

void MainWindow::onShowDecompiler() {
    actionCompiler_->setChecked(false);
    actionDecompiler_->setChecked(true);
    stackedWidget_->setCurrentWidget(decompilerPage_);
    statusBar()->showMessage(tr("Mode: Decompiler (QVM -> Assembly & C Pseudocode)"));
}

void MainWindow::onCompilationSucceeded(const QString& qvmFilePath) {
    auto reply = QMessageBox::question(
        this, tr("Compilation Complete"),
        tr("QVM generated successfully!\n\nWould you like to open it in the decompiler now?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

    if (reply == QMessageBox::Yes) {
        onShowDecompiler();
        decompilerPage_->loadQvmFile(qvmFilePath);
    }
}

void MainWindow::onAbout() {
    QMessageBox::about(
        this, tr("About Quake 3 QVM Studio"),
        QString("<h3>%1 v%2</h3>"
                "<p>A dedicated native C++20 and Qt 6 desktop application for compiling, "
                "inspecting, disassembling, and decompiling Quake III Arena virtual machine bytecode (.qvm).</p>"
                "<h4>Toolchain Integration:</h4>"
                "<ul>"
                "<li><b>LCC Compiler:</b> Fraser & Hanson / id Software Quake III bytecode compiler.</li>"
                "<li><b>Q3ASM:</b> id Software Quake III bytecode assembler.</li>"
                "<li><b>Decompiler Engine:</b> Native C++20 bytecode parser, control-flow reconstructor, and expression stack analyzer.</li>"
                "</ul>"
                "<p>Quake III Arena is copyright &copy; 1999-2005 Id Software, Inc. and licensed under GPLv2.</p>")
            .arg(kAppName.data()).arg(kVersion.data()));
}

void MainWindow::applyDarkTheme() {
    setStyleSheet(R"(
        QMainWindow {
            background-color: #0f172a;
            color: #f8fafc;
        }
        QWidget {
            background-color: #0f172a;
            color: #f8fafc;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
            font-size: 13px;
        }
        QGroupBox {
            border: 1px solid #334155;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 16px;
            font-weight: bold;
            color: #38bdf8;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 4px;
        }
        QLineEdit, QComboBox {
            background-color: #1e293b;
            border: 1px solid #475569;
            border-radius: 4px;
            padding: 6px 10px;
            color: #f1f5f9;
        }
        QLineEdit:focus, QComboBox:focus {
            border: 1px solid #38bdf8;
        }
        QPushButton {
            background-color: #1e293b;
            border: 1px solid #475569;
            border-radius: 4px;
            padding: 6px 14px;
            color: #f1f5f9;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: #334155;
            border-color: #64748b;
        }
        QPushButton:pressed {
            background-color: #0f172a;
        }
        QToolBar {
            background-color: #1e293b;
            border-bottom: 1px solid #334155;
            padding: 4px;
            spacing: 6px;
        }
        QToolButton {
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 4px;
            padding: 6px 12px;
            color: #cbd5e1;
            font-weight: bold;
        }
        QToolButton:checked {
            background-color: #0284c7;
            color: white;
        }
        QToolButton:hover:!checked {
            background-color: #334155;
        }
        QTabWidget::pane {
            border: 1px solid #334155;
            background-color: #1e293b;
            border-radius: 4px;
        }
        QTabBar::tab {
            background-color: #0f172a;
            color: #94a3b8;
            padding: 8px 16px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #1e293b;
            color: #38bdf8;
            font-weight: bold;
        }
        QTableView {
            background-color: #1e293b;
            border: 1px solid #334155;
            gridline-color: #334155;
            selection-background-color: #0369a1;
            selection-color: white;
            color: #f1f5f9;
        }
        QHeaderView::section {
            background-color: #0f172a;
            color: #94a3b8;
            padding: 6px;
            border: 1px solid #334155;
            font-weight: bold;
        }
        QPlainTextEdit {
            background-color: #111827;
            border: 1px solid #334155;
            color: #e2e8f0;
            selection-background-color: #0284c7;
        }
        QProgressBar {
            border: 1px solid #334155;
            border-radius: 4px;
            text-align: center;
            background-color: #1e293b;
            color: white;
        }
        QProgressBar::chunk {
            background-color: #0284c7;
            border-radius: 3px;
        }
        QStatusBar {
            background-color: #0f172a;
            border-top: 1px solid #334155;
            color: #94a3b8;
        }
    )");
}

} // namespace qvm::gui
