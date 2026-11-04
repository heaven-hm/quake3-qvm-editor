#include "source/gui/MainWindow.hpp"
#include "qvm/common/Version.hpp"
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QSettings>
#include <QMimeData>
#include <QWidget>

namespace qvm::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowIcon(QIcon(":/app_icon.png"));
    setupUi();
    setupMenus();

    QSettings settings;
    const auto savedTheme = static_cast<EditorTheme>(settings.value("ui/theme", static_cast<int>(EditorTheme::Light)).toInt());
    const int themeIndex = themeCombo_->findData(static_cast<int>(savedTheme));
    themeCombo_->setCurrentIndex(themeIndex >= 0 ? themeIndex : themeCombo_->findData(static_cast<int>(EditorTheme::Light)));
    applyTheme(savedTheme);
    setAcceptDrops(true);

    setWindowTitle(QString("%1 v%2 — Quake III QVM Editor").arg(kAppName.data()).arg(kVersion.data()));
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

    auto brandTitle = new QLabel(tr("<b>QUAKE 3</b> <span style='color:#b3e878; font-weight:700;'>QVM EDITOR</span>"), this);
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

    statusBar()->showMessage(tr("Quake 3 QVM Editor ready. Select Compiler or Decompiler mode."));
}

void MainWindow::setupMenus() {
    auto fileMenu = menuBar()->addMenu(tr("&File"));
    auto exitAction = fileMenu->addAction(QIcon(":/icons/clear.svg"), tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);

    auto viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(actionCompiler_);
    viewMenu->addAction(actionDecompiler_);

    auto helpMenu = menuBar()->addMenu(tr("&Help"));
    actionAbout_ = helpMenu->addAction(QIcon(":/icons/info.svg"), tr("&About Quake 3 QVM Editor"), this, &MainWindow::onAbout);
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

void MainWindow::openQvmFile(const QString& filePath) {
    if (decompilerPage_) {
        onShowDecompiler();
        decompilerPage_->loadQvmFile(filePath);
    }
}

void MainWindow::onCompilationSucceeded(const QString& qvmFilePath) {
    // Keep the compiler visible after a build so the Run action and its VM
    // output remain immediately available.  The user can open the generated
    // file in the decompiler explicitly through File/Open or by launching it.
    statusBar()->showMessage(tr("Compiled %1. Run it with F5, or open it in the decompiler.")
                                 .arg(QFileInfo(qvmFilePath).fileName()));
}

void MainWindow::onThemeChanged(int index) {
    auto theme = static_cast<EditorTheme>(themeCombo_->itemData(index).toInt());
    applyTheme(theme);
    QSettings().setValue("ui/theme", static_cast<int>(theme));
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (!event->mimeData()->hasUrls()) return event->ignore();
    const QString path = event->mimeData()->urls().value(0).toLocalFile();
    if (path.endsWith(".qvm", Qt::CaseInsensitive) || path.endsWith(".c", Qt::CaseInsensitive) ||
        path.endsWith(".h", Qt::CaseInsensitive)) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QString path = event->mimeData()->urls().value(0).toLocalFile();
    if (path.endsWith(".qvm", Qt::CaseInsensitive)) {
        openQvmFile(path);
    } else if (path.endsWith(".c", Qt::CaseInsensitive) || path.endsWith(".h", Qt::CaseInsensitive)) {
        onShowCompiler();
        compilerPage_->openSourceFile(path);
    } else {
        return event->ignore();
    }
    event->acceptProposedAction();
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

    if (compilerPage_) {
        compilerPage_->setTheme(theme);
    }
    if (decompilerPage_) {
        decompilerPage_->setTheme(theme);
    }
}

void MainWindow::onAbout() {
    QMessageBox::about(
        this, tr("About Quake 3 QVM Editor"),
        QString("<h3>%1 v%2</h3>"
                "<p>A dedicated, modern native C++20 and Qt 6 desktop application for compiling, "
                "inspecting, disassembling, and decompiling Quake III Arena virtual machine bytecode (.qvm).</p>"
                "<h4>Toolchain Integration:</h4>"
                "<ul>"
                "<li><b>LCC Compiler:</b> Fraser & Hanson / id Software Quake III bytecode compiler.</li>"
                "<li><b>Q3ASM Assembler:</b> id Software QVM assembler.</li>"
                "<li><b>C Syntax Highlighter:</b> Real-time token highlighting for pseudocode and source.</li>"
                "<li><b>Single-Function Focus:</b> Direct inspection of isolated function ASM and pseudocode.</li>"
                "</ul>")
            .arg(kAppName.data()).arg(kVersion.data()));
}

} // namespace qvm::gui
