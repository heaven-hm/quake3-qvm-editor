#include "source/gui/widgets/CodeEditor.hpp"
#include <QPainter>
#include <QTextBlock>

namespace qvm::gui {

CodeEditor::CodeEditor(QWidget* parent) : QPlainTextEdit(parent) {
    lineNumberArea_ = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    QFont font("SF Mono");
    if (!font.exactMatch()) {
        font.setFamily("Menlo");
    }
    if (!font.exactMatch()) {
        font.setFamily("Consolas");
    }
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(11);
    setFont(font);

    setLineWrapMode(QPlainTextEdit::NoWrap);
}

void CodeEditor::setLanguage(EditorLanguage lang) {
    currentLang_ = lang;
    cHighlighter_.reset();
    asmHighlighter_.reset();

    if (lang == EditorLanguage::C) {
        cHighlighter_ = std::make_unique<CSyntaxHighlighter>(document(), currentTheme_);
    } else if (lang == EditorLanguage::Assembly) {
        asmHighlighter_ = std::make_unique<AsmSyntaxHighlighter>(document(), currentTheme_);
    }
}

void CodeEditor::setTheme(EditorTheme theme) {
    currentTheme_ = theme;
    if (cHighlighter_) {
        cHighlighter_->setTheme(theme);
    }
    if (asmHighlighter_) {
        asmHighlighter_->setTheme(theme);
    }
    lineNumberArea_->update();
    highlightCurrentLine();
}

int CodeEditor::lineNumberAreaWidth() {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 18 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect& rect, int dy) {
    if (dy) {
        lineNumberArea_->scroll(0, dy);
    } else {
        lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
    }

    if (rect.contains(viewport()->rect())) {
        updateLineNumberAreaWidth(0);
    }
}

void CodeEditor::resizeEvent(QResizeEvent* event) {
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    lineNumberArea_->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine() {
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;
        QColor lineColor;
        if (currentTheme_ == EditorTheme::Light) {
            lineColor = QColor(0, 0, 0, 15);
        } else if (currentTheme_ == EditorTheme::Midnight) {
            lineColor = QColor(126, 205, 243, 20);
        } else {
            lineColor = QColor(179, 232, 120, 20);
        }
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent* event) {
    QPainter painter(lineNumberArea_);
    QColor bgColor;
    QColor textColor;

    if (currentTheme_ == EditorTheme::Light) {
        bgColor = QColor(241, 244, 239);
        textColor = QColor(100, 116, 94);
    } else if (currentTheme_ == EditorTheme::Midnight) {
        bgColor = QColor(19, 29, 45);
        textColor = QColor(130, 153, 182);
    } else {
        bgColor = QColor(23, 30, 25);
        textColor = QColor(117, 134, 121);
    }

    painter.fillRect(event->rect(), bgColor);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(textColor);
            painter.drawText(0, top, lineNumberArea_->width() - 8, fontMetrics().height(),
                             Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

} // namespace qvm::gui
