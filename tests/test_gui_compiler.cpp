#include "source/gui/CompilerPage.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QThread>

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <gtest/gtest.h>

namespace {

QApplication& application() {
    static int argc = 1;
    static char appName[] = "q3vm_gui_tests";
    static char* argv[] = {appName, nullptr};
    static QApplication instance(argc, argv);
    return instance;
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

} // namespace

TEST(GuiCompilerPage, CompilesStarterTemplateWithSelectedSdk) {
    application();
    qvm::gui::CompilerPage page;

    const auto source = readTextFile("tests/fixtures/editor_starter_template.c");
    ASSERT_FALSE(source.empty());

    auto* editor = page.findChild<qvm::gui::CodeEditor*>();
    ASSERT_NE(editor, nullptr);
    editor->setPlainText(QString::fromStdString(source));

    std::vector<QLineEdit*> sdkFields;
    QLineEdit* outputField = nullptr;
    for (auto* field : page.findChildren<QLineEdit*>()) {
        if (field->placeholderText().startsWith("Quake 3 SDK")) {
            sdkFields.push_back(field);
        }
        if (outputField == nullptr && field->text() == "build/out") {
            outputField = field;
        }
    }
    ASSERT_FALSE(sdkFields.empty());
    ASSERT_NE(outputField, nullptr);

    const auto outputDir = std::filesystem::path("build/test_gui_editor_compile");
    std::filesystem::remove_all(outputDir);
    const char* configuredSdk = std::getenv("QVM_GUI_TEST_SDK");
    const QString sdkPath = configuredSdk != nullptr && *configuredSdk != '\0'
        ? QString::fromUtf8(configuredSdk)
        : QStringLiteral("third_party/quake3/code");
    for (auto* sdkField : sdkFields) {
        sdkField->setText(sdkPath);
    }
    outputField->setText(QString::fromStdString(outputDir.string()));

    bool succeeded = false;
    QString outputPath;
    QObject::connect(&page, &qvm::gui::CompilerPage::compilationSucceeded,
                     [&succeeded, &outputPath](const QString& path) {
                         succeeded = true;
                         outputPath = path;
                     });

    ASSERT_TRUE(QMetaObject::invokeMethod(&page, "onStartCompileEditor", Qt::DirectConnection));

    QElapsedTimer timer;
    timer.start();
    while (!succeeded && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(10);
    }

    QString logs;
    for (auto* log : page.findChildren<QPlainTextEdit*>()) {
        logs += log->toPlainText();
    }
    EXPECT_TRUE(succeeded) << logs.toStdString();
    EXPECT_TRUE(std::filesystem::is_regular_file(outputPath.toStdString()));
    std::filesystem::remove_all(outputDir);
}
