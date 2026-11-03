#include "source/gui/MainWindow.hpp"
#include "qvm/common/Version.hpp"
#include <QApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QWidget>

namespace qvm::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowIcon(QIcon(":/icons/brand.svg"));
    setupUi();
    setupMenus();
    applyTheme(EditorTheme::Dark);

    setWindowTitle(QString("%1 v%2 — Quake III QVM Studio").arg(kAppName.data()).arg(kVersion.data()));
    resize(1300, 850);
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

    // --- Modern Top Navigation Toolbar ---
    auto navToolBar = addToolBar(tr("Main Navigation"));
    navToolBar->setMovable(false);
    navToolBar->setFloatable(false);
    navToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // Brand widget (Icon + Title + Subtitle)
    auto brandWidget = new QWidget(this);
    auto brandLayout = new QHBoxLayout(brandWidget);
    brandLayout->setContentsMargins(4, 2, 16, 2);
    brandLayout->setSpacing(8);

    auto brandIcon = new QLabel(this);
    brandIcon->setPixmap(QIcon(":/icons/brand.svg").pixmap(24, 24));

    auto brandTitle = new QLabel(tr("<b>QUAKE 3</b> <span style='color:#b3e878; font-weight:700;'>QVM STUDIO</span>"), this);
    brandTitle->setStyleSheet("font-size: 13px;");

    brandLayout->addWidget(brandIcon);
    brandLayout->addWidget(brandTitle);
    navToolBar->addWidget(brandWidget);

    navToolBar->addSeparator();

    // Mode Buttons
    actionCompiler_ = navToolBar->addAction(
        QIcon(":/icons/compiler.svg"), tr(" QVM Compiler"));
    actionCompiler_->setCheckable(true);
    actionCompiler_->setChecked(true);
    connect(actionCompiler_, &QAction::triggered, this, &MainWindow::onShowCompiler);

    actionDecompiler_ = navToolBar->addAction(
        QIcon(":/icons/decompiler.svg"), tr(" QVM Decompiler & Disassembler"));
    actionDecompiler_->setCheckable(true);
    connect(actionDecompiler_, &QAction::triggered, this, &MainWindow::onShowDecompiler);

    // Spacer to push right-hand widgets to the right
    auto spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    navToolBar->addWidget(spacer);

    // Live Engine Status Pill
    statusPill_ = new QLabel(tr("● Native Engine Ready"), this);
    statusPill_->setStyleSheet(
        "color: #b3e878; font-size: 11px; font-weight: 600; padding: 4px 10px; "
        "background-color: #171e19; border: 1px solid #303b33; border-radius: 12px; margin-right: 8px;"
    );
    navToolBar->addWidget(statusPill_);

    // Theme Selector
    auto themeLabel = new QLabel(tr(" Theme: "), this);
    themeLabel->setStyleSheet("color: #758679; font-size: 11px; font-weight: 500;");
    navToolBar->addWidget(themeLabel);

    themeCombo_ = new QComboBox(this);
    themeCombo_->addItem(QIcon(":/icons/theme.svg"), tr("Dark (Tactical)"), static_cast<int>(EditorTheme::Dark));
    themeCombo_->addItem(QIcon(":/icons/theme.svg"), tr("Midnight (Navy)"), static_cast<int>(EditorTheme::Midnight));
    themeCombo_->addItem(QIcon(":/icons/theme.svg"), tr("Light"), static_cast<int>(EditorTheme::Light));
    themeCombo_->setFixedWidth(145);
    connect(themeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onThemeChanged);
    navToolBar->addWidget(themeCombo_);

    statusBar()->showMessage(tr("Quake 3 QVM Studio ready. Select Compiler or Decompiler mode."));
}

void MainWindow::setupMenus() {
    auto fileMenu = menuBar()->addMenu(tr("&File"));
    auto exitAction = fileMenu->addAction(QIcon(":/icons/clear.svg"), tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    auto viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(actionCompiler_);
    viewMenu->addAction(actionDecompiler_);

    auto helpMenu = menuBar()->addMenu(tr("&Help"));
    actionAbout_ = helpMenu->addAction(QIcon(":/icons/info.svg"), tr("&About Quake 3 QVM Studio"), this, &MainWindow::onAbout);
}

void MainWindow::onShowCompiler() {
    actionCompiler_->setChecked(true);
    actionDecompiler_->setChecked(false);
    stackedWidget_->setCurrentWidget(compilerPage_);
    statusBar()->showMessage(tr("Mode: QVM Compiler (C Source -> Bytecode .qvm)"));
}

void MainWindow::onShowDecompiler() {
    actionCompiler_->setChecked(false);
    actionDecompiler_->setChecked(true);
    stackedWidget_->setCurrentWidget(decompilerPage_);
    statusBar()->showMessage(tr("Mode: QVM Decompiler (Bytecode .qvm -> Assembly & C Pseudocode)"));
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

void MainWindow::onThemeChanged(int index) {
    auto theme = static_cast<EditorTheme>(themeCombo_->itemData(index).toInt());
    applyTheme(theme);
}

void MainWindow::applyTheme(EditorTheme theme) {
    currentTheme_ = theme;

    QString qssPath;
    if (theme == EditorTheme::Light) {
        qssPath = ":/styles/light.qss";
        if (statusPill_) {
            statusPill_->setStyleSheet(
                "color: #426d22; font-size: 11px; font-weight: 600; padding: 4px 10px; "
                "background-color: #ffffff; border: 1px solid #d0d9ca; border-radius: 12px; margin-right: 8px;"
            );
        }
    } else if (theme == EditorTheme::Midnight) {
        qssPath = ":/styles/midnight.qss";
        if (statusPill_) {
            statusPill_->setStyleSheet(
                "color: #7ecdf3; font-size: 11px; font-weight: 600; padding: 4px 10px; "
                "background-color: #131d2d; border: 1px solid #2b3d57; border-radius: 12px; margin-right: 8px;"
            );
        }
    } else {
        qssPath = ":/styles/dark.qss";
        if (statusPill_) {
            statusPill_->setStyleSheet(
                "color: #b3e878; font-size: 11px; font-weight: 600; padding: 4px 10px; "
                "background-color: #171e19; border: 1px solid #303b33; border-radius: 12px; margin-right: 8px;"
            );
        }
    }

    QFile file(qssPath);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        qApp->setStyleSheet(QLatin1String(file.readAll()));
    }

    if (decompilerPage_) {
        decompilerPage_->setTheme(theme);
    }
}

void MainWindow::onAbout() {
    QMessageBox::about(
        this, tr("About Quake 3 QVM Studio"),
        QString("<h3>%1 v%2</h3>"
                "<p>A dedicated, modern native C++20 and Qt 6 desktop application for compiling, "
                "inspecting, disassembling, and decompiling Quake III Arena virtual machine bytecode (.qvm).</p>"
                "<h4>Toolchain Integration:</h4>"
                "<ul>"
                "<li><b>LCC Compiler:</b> Fraser & Hanson / id Software Quake III bytecode compiler.</li>"
                "<li><b>Q3ASM Assembler:</b> id Software QVM assembler.</li>"
                "<li><b>C Syntax Highlighter:</b> Real-time token highlighting for pseudocode and source.</li>"
                "<li><b>Single-Function Focus:</b> Direct inspection of isolated function ASM and pseudocode.</li>"
                "</ul>"
                "<p><i>Inspired by project-igi-qvm-editor-web design patterns.</i></p>")
            .arg(kAppName.data()).arg(kVersion.data()));
}

} // namespace qvm::gui
