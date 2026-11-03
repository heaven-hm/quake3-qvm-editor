#pragma once

#include "source/gui/widgets/CSyntaxHighlighter.hpp"
#include "source/gui/widgets/AsmSyntaxHighlighter.hpp"
#include <QPlainTextEdit>
#include <QWidget>
#include <memory>

namespace qvm::gui {

enum class EditorLanguage {
    None,
    C,
    Assembly
};

class LineNumberArea;

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit CodeEditor(QWidget* parent = nullptr);

    void setLanguage(EditorLanguage lang);
    void setTheme(EditorTheme theme);

    void lineNumberAreaPaintEvent(QPaintEvent* event);
    int lineNumberAreaWidth();

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect& rect, int dy);

private:
    QWidget* lineNumberArea_ = nullptr;
    EditorLanguage currentLang_{EditorLanguage::None};
    EditorTheme currentTheme_{EditorTheme::Dark};

    std::unique_ptr<CSyntaxHighlighter> cHighlighter_;
    std::unique_ptr<AsmSyntaxHighlighter> asmHighlighter_;
};

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditor* editor) : QWidget(editor), codeEditor_(editor) {}

    QSize sizeHint() const override {
        return QSize(codeEditor_->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        codeEditor_->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor* codeEditor_;
};

} // namespace qvm::gui
