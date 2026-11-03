#include "source/gui/MainWindow.hpp"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Quake 3 QVM Studio");
    app.setOrganizationName("QVM Studio");

    qvm::gui::MainWindow window;
    window.show();

    if (argc > 1) {
        QString arg = QString::fromLocal8Bit(argv[1]);
        if (arg.endsWith(".qvm", Qt::CaseInsensitive)) {
            window.openQvmFile(arg);
        }
    }

    return app.exec();
}
