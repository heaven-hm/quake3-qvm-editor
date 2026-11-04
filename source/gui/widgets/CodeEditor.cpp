#include "source/gui/widgets/CodeEditor.hpp"
#include <QFontDatabase>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QRegularExpression>
#include <QTextBlock>

namespace qvm::gui {

CodeEditor::CodeEditor(QWidget* parent) : QPlainTextEdit(parent) {
    lineNumberArea_ = new LineNumberArea(this);

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
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

void CodeEditor::keyPressEvent(QKeyEvent* event) {
    if (event->matches(QKeySequence::Find)) {
        findText(false);
        return;
    }
    if (event->matches(QKeySequence::Replace)) {
        findText(true);
        return;
    }
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_L) {
        goToLine();
        return;
    }
    if (event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_Slash) {
        toggleLineComment();
        return;
    }

    static const QHash<QChar, QChar> pairs{{'(', ')'}, {'[', ']'}, {'{', '}'}, {'\"', '\"'}, {'\'', '\''}};
    const QString text = event->text();
    if (!isReadOnly() && text.size() == 1 && pairs.contains(text.front())) {
        QTextCursor cursor = textCursor();
        const QChar closing = pairs.value(text.front());
        if (cursor.hasSelection()) {
            const QString selected = cursor.selectedText();
            cursor.insertText(text + selected + closing);
        } else {
            cursor.insertText(text + closing);
            cursor.movePosition(QTextCursor::Left);
        }
        setTextCursor(cursor);
        return;
    }
    if (!isReadOnly() && text == "\n") {
        QTextCursor cursor = textCursor();
        const QString prior = cursor.block().text();
        QString indent;
        for (const QChar ch : prior) {
            if (ch == ' ' || ch == '\t') indent += ch;
            else break;
        }
        if (prior.trimmed().endsWith('{')) indent += "    ";
        cursor.insertText("\n" + indent);
        setTextCursor(cursor);
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void CodeEditor::toggleLineComment() {
    if (isReadOnly()) return;
    QTextCursor cursor = textCursor();
    const int first = cursor.selectionStart();
    const int last = cursor.selectionEnd();
    cursor.beginEditBlock();
    cursor.setPosition(first);
    const QString prefix = currentLang_ == EditorLanguage::Assembly ? "; " : "// ";
    const bool uncomment = cursor.block().text().trimmed().startsWith(prefix.trimmed());
    do {
        cursor.movePosition(QTextCursor::StartOfBlock);
        const QString line = cursor.block().text();
        const int content = line.indexOf(QRegularExpression("\\S"));
        if (content >= 0) {
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, content);
            if (uncomment && line.mid(content).startsWith(prefix.trimmed())) {
                cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, prefix.trimmed().size());
                if (cursor.selectedText().endsWith(' ') && line.mid(content).startsWith(prefix)) {
                    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
                }
                cursor.removeSelectedText();
            } else if (!uncomment) {
                cursor.insertText(prefix);
            }
        }
    } while (cursor.movePosition(QTextCursor::NextBlock) && cursor.position() <= last);
    cursor.endEditBlock();
}

void CodeEditor::findText(bool replace) {
    bool ok = false;
    const QString needle = QInputDialog::getText(this, replace ? tr("Replace") : tr("Find"),
                                                  tr("Find:"), QLineEdit::Normal, textCursor().selectedText(), &ok);
    if (!ok || needle.isEmpty()) return;
    if (replace) {
        const QString replacement = QInputDialog::getText(this, tr("Replace"), tr("Replace with:"),
                                                           QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        QTextCursor cursor(document());
        cursor.beginEditBlock();
        while (true) {
            cursor = document()->find(needle, cursor);
            if (cursor.isNull()) break;
            cursor.insertText(replacement);
        }
        cursor.endEditBlock();
        return;
    }
    if (!find(needle) && !textCursor().atStart()) {
        moveCursor(QTextCursor::Start);
        find(needle);
    }
}

void CodeEditor::goToLine() {
    bool ok = false;
    const int line = QInputDialog::getInt(this, tr("Go to Line"), tr("Line:"),
                                          textCursor().blockNumber() + 1, 1, blockCount(), 1, &ok);
    if (!ok) return;
    QTextCursor cursor(document()->findBlockByNumber(line - 1));
    setTextCursor(cursor);
    centerCursor();
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
