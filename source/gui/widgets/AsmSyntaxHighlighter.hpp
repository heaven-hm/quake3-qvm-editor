#pragma once

#include "source/gui/widgets/CSyntaxHighlighter.hpp"
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <vector>

namespace qvm::gui {

class AsmSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit AsmSyntaxHighlighter(QTextDocument* parent = nullptr, EditorTheme theme = EditorTheme::Dark);

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

    QTextCharFormat opcodeFormat_;
    QTextCharFormat labelFormat_;
    QTextCharFormat commentFormat_;
    QTextCharFormat numberFormat_;
    QTextCharFormat stringFormat_;
    QTextCharFormat directiveFormat_;

    EditorTheme currentTheme_{EditorTheme::Dark};
};

} // namespace qvm::gui
