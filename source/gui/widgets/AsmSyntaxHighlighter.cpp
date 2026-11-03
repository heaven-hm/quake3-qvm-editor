#include "source/gui/widgets/AsmSyntaxHighlighter.hpp"

namespace qvm::gui {

AsmSyntaxHighlighter::AsmSyntaxHighlighter(QTextDocument* parent, EditorTheme theme)
    : QSyntaxHighlighter(parent), currentTheme_(theme) {
    setupRules();
}

void AsmSyntaxHighlighter::setTheme(EditorTheme theme) {
    currentTheme_ = theme;
    setupRules();
    rehighlight();
}

void AsmSyntaxHighlighter::setupRules() {
    highlightingRules_.clear();

    QColor opcodeColor, labelColor, commentColor, numberColor, stringColor, directiveColor;

    if (currentTheme_ == EditorTheme::Light) {
        opcodeColor = QColor(14, 88, 175);
        labelColor = QColor(145, 40, 140);
        commentColor = QColor(100, 116, 94);
        numberColor = QColor(120, 80, 20);
        stringColor = QColor(180, 50, 30);
        directiveColor = QColor(50, 110, 20);
    } else if (currentTheme_ == EditorTheme::Midnight) {
        opcodeColor = QColor(126, 205, 243);
        labelColor = QColor(220, 220, 170);
        commentColor = QColor(130, 153, 182);
        numberColor = QColor(181, 206, 168);
        stringColor = QColor(206, 145, 120);
        directiveColor = QColor(197, 134, 192);
    } else {
        opcodeColor = QColor(179, 232, 120); // Neon lime
        labelColor = QColor(220, 220, 170);  // Yellow/gold
        commentColor = QColor(117, 134, 121);// Olive/slate
        numberColor = QColor(181, 206, 168); // Soft green
        stringColor = QColor(206, 145, 120); // Salmon
        directiveColor = QColor(86, 156, 214);// Blue
    }

    opcodeFormat_.setForeground(opcodeColor);
    opcodeFormat_.setFontWeight(QFont::Bold);

    const QString opcodes[] = {
        QStringLiteral("\\benter\\b"), QStringLiteral("\\bleave\\b"), QStringLiteral("\\bcall\\b"),
        QStringLiteral("\\bpush\\b"), QStringLiteral("\\bpop\\b"), QStringLiteral("\\bconst\\b"),
        QStringLiteral("\\blocal\\b"), QStringLiteral("\\bjump\\b"), QStringLiteral("\\beq\\b"),
        QStringLiteral("\\bne\\b"), QStringLiteral("\\blti\\b"), QStringLiteral("\\blei\\b"),
        QStringLiteral("\\bgti\\b"), QStringLiteral("\\bgei\\b"), QStringLiteral("\\bltu\\b"),
        QStringLiteral("\\bleu\\b"), QStringLiteral("\\bgtu\\b"), QStringLiteral("\\bgeu\\b"),
        QStringLiteral("\\beqf\\b"), QStringLiteral("\\bnef\\b"), QStringLiteral("\\bltf\\b"),
        QStringLiteral("\\blef\\b"), QStringLiteral("\\bgtf\\b"), QStringLiteral("\\bgef\\b"),
        QStringLiteral("\\bload1\\b"), QStringLiteral("\\bload2\\b"), QStringLiteral("\\bload4\\b"),
        QStringLiteral("\\bstore1\\b"), QStringLiteral("\\bstore2\\b"), QStringLiteral("\\bstore4\\b"),
        QStringLiteral("\\barg\\b"), QStringLiteral("\\bblock_copy\\b"), QStringLiteral("\\bsex8\\b"),
        QStringLiteral("\\bsex16\\b"), QStringLiteral("\\bnegi\\b"), QStringLiteral("\\badd\\b"),
        QStringLiteral("\\bsub\\b"), QStringLiteral("\\bdivi\\b"), QStringLiteral("\\bdivu\\b"),
        QStringLiteral("\\bmodi\\b"), QStringLiteral("\\bmodu\\b"), QStringLiteral("\\bmul\\b"),
        QStringLiteral("\\bband\\b"), QStringLiteral("\\bbor\\b"), QStringLiteral("\\bbxor\\b"),
        QStringLiteral("\\bbcom\\b"), QStringLiteral("\\blsh\\b"), QStringLiteral("\\brshi\\b"),
        QStringLiteral("\\brshu\\b"), QStringLiteral("\\bnegf\\b"), QStringLiteral("\\baddf\\b"),
        QStringLiteral("\\bsubf\\b"), QStringLiteral("\\bdivf\\b"), QStringLiteral("\\bmulf\\b"),
        QStringLiteral("\\bcvif\\b"), QStringLiteral("\\bcvfi\\b")
    };

    for (const auto& op : opcodes) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(op);
        rule.format = opcodeFormat_;
        highlightingRules_.push_back(rule);
    }

    // Directives
    directiveFormat_.setForeground(directiveColor);
    directiveFormat_.setFontWeight(QFont::DemiBold);
    const QString directives[] = {
        QStringLiteral("\\bcode\\b"), QStringLiteral("\\bdata\\b"), QStringLiteral("\\blit\\b"),
        QStringLiteral("\\bbss\\b"), QStringLiteral("\\bequ\\b"), QStringLiteral("\\balign\\b"),
        QStringLiteral("\\bexport\\b"), QStringLiteral("\\bimport\\b")
    };
    for (const auto& dir : directives) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(dir);
        rule.format = directiveFormat_;
        highlightingRules_.push_back(rule);
    }

    // Labels
    labelFormat_.setForeground(labelColor);
    labelFormat_.setFontWeight(QFont::Bold);
    HighlightingRule labelRule;
    labelRule.pattern = QRegularExpression(QStringLiteral("^[A-Za-z0-9_]+:|loc_[0-9a-fA-F]+:"));
    labelRule.format = labelFormat_;
    highlightingRules_.push_back(labelRule);

    // Numbers
    numberFormat_.setForeground(numberColor);
    HighlightingRule numRule;
    numRule.pattern = QRegularExpression(QStringLiteral("\\b0x[0-9a-fA-F]+\\b|\\b-?[0-9]+\\b"));
    numRule.format = numberFormat_;
    highlightingRules_.push_back(numRule);

    // Strings
    stringFormat_.setForeground(stringColor);
    HighlightingRule strRule;
    strRule.pattern = QRegularExpression(QStringLiteral("\".*?\""));
    strRule.format = stringFormat_;
    highlightingRules_.push_back(strRule);

    // Comments
    commentFormat_.setForeground(commentColor);
    commentFormat_.setFontItalic(true);
    HighlightingRule commentRule;
    commentRule.pattern = QRegularExpression(QStringLiteral(";[^\n]*"));
    commentRule.format = commentFormat_;
    highlightingRules_.push_back(commentRule);
}

void AsmSyntaxHighlighter::highlightBlock(const QString& text) {
    for (const auto& rule : highlightingRules_) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(static_cast<int>(match.capturedStart()),
                      static_cast<int>(match.capturedLength()),
                      rule.format);
        }
    }
}

} // namespace qvm::gui
