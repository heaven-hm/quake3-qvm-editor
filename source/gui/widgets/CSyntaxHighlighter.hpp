#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <vector>

namespace qvm::gui {

enum class EditorTheme {
    Dark,
    Midnight,
    Light
};

class CSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit CSyntaxHighlighter(QTextDocument* parent = nullptr, EditorTheme theme = EditorTheme::Dark);

    void setTheme(EditorTheme theme);

protected:
    void highlightBlock(const QString& text) override;

private:
    void setupRules();

    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    std::vector<HighlightingRule> highlightingRules_;

    QRegularExpression commentStartExpression_;
    QRegularExpression commentEndExpression_;

    QTextCharFormat keywordFormat_;
    QTextCharFormat typeFormat_;
    QTextCharFormat syscallFormat_;
    QTextCharFormat preprocessorFormat_;
    QTextCharFormat singleLineCommentFormat_;
    QTextCharFormat multiLineCommentFormat_;
    QTextCharFormat quotationFormat_;
    QTextCharFormat numberFormat_;
    QTextCharFormat functionFormat_;

    EditorTheme currentTheme_{EditorTheme::Dark};
};

} // namespace qvm::gui
