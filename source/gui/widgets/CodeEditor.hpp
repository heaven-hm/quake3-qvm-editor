#pragma once

#include <QPlainTextEdit>
#include <QWidget>

namespace qvm::gui {

class LineNumberArea;

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit CodeEditor(QWidget* parent = nullptr);

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
