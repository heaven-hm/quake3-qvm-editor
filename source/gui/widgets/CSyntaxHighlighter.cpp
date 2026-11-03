#include "source/gui/widgets/CSyntaxHighlighter.hpp"

namespace qvm::gui {

CSyntaxHighlighter::CSyntaxHighlighter(QTextDocument* parent, EditorTheme theme)
    : QSyntaxHighlighter(parent), currentTheme_(theme) {
    setupRules();
}

void CSyntaxHighlighter::setTheme(EditorTheme theme) {
    currentTheme_ = theme;
    setupRules();
    rehighlight();
}

void CSyntaxHighlighter::setupRules() {
    highlightingRules_.clear();

    QColor keywordColor, typeColor, syscallColor, preprocessorColor;
    QColor commentColor, stringColor, numberColor, functionColor;

    if (currentTheme_ == EditorTheme::Light) {
        keywordColor = QColor(14, 88, 175);       // Deep blue
        typeColor = QColor(38, 127, 124);         // Teal
        syscallColor = QColor(50, 110, 20);       // Dark forest
        preprocessorColor = QColor(145, 40, 140); // Magenta
        commentColor = QColor(100, 116, 94);      // Muted green
        stringColor = QColor(180, 50, 30);        // Crimson
        numberColor = QColor(120, 80, 20);        // Amber
        functionColor = QColor(120, 40, 160);     // Purple
    } else if (currentTheme_ == EditorTheme::Midnight) {
        keywordColor = QColor(92, 170, 242);      // Cyan blue
        typeColor = QColor(126, 205, 243);         // Neon electric cyan
        syscallColor = QColor(140, 220, 254);     // Sky cyan
        preprocessorColor = QColor(197, 134, 192);// Purple
        commentColor = QColor(130, 153, 182);     // Slate
        stringColor = QColor(206, 145, 120);      // Orange-peach
        numberColor = QColor(181, 206, 168);      // Pale green
        functionColor = QColor(220, 220, 170);    // Warm yellow
    } else {
        // Dark theme (with neon lime accents)
        keywordColor = QColor(86, 156, 214);      // VS code blue
        typeColor = QColor(78, 201, 176);         // Teal
        syscallColor = QColor(179, 232, 120);     // Neon lime
        preprocessorColor = QColor(197, 134, 192);// Violet
        commentColor = QColor(117, 134, 121);     // Muted olive
        stringColor = QColor(206, 145, 120);      // Salmon
        numberColor = QColor(181, 206, 168);      // Pale green
        functionColor = QColor(220, 220, 170);    // Warm yellow
    }

    keywordFormat_.setForeground(keywordColor);
    keywordFormat_.setFontWeight(QFont::Bold);

    const QString keywordPatterns[] = {
        QStringLiteral("\\bif\\b"), QStringLiteral("\\belse\\b"), QStringLiteral("\\bwhile\\b"),
        QStringLiteral("\\bfor\\b"), QStringLiteral("\\bdo\\b"), QStringLiteral("\\bswitch\\b"),
        QStringLiteral("\\bcase\\b"), QStringLiteral("\\bdefault\\b"), QStringLiteral("\\bbreak\\b"),
        QStringLiteral("\\bcontinue\\b"), QStringLiteral("\\breturn\\b"), QStringLiteral("\\bgoto\\b"),
        QStringLiteral("\\bsizeof\\b"), QStringLiteral("\\btypedef\\b"), QStringLiteral("\\bstruct\\b"),
        QStringLiteral("\\bunion\\b"), QStringLiteral("\\benum\\b"), QStringLiteral("\\bstatic\\b"),
        QStringLiteral("\\bextern\\b"), QStringLiteral("\\bconst\\b"), QStringLiteral("\\bvolatile\\b"),
        QStringLiteral("\\bregister\\b"), QStringLiteral("\\bNULL\\b")
    };

    for (const auto& pattern : keywordPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat_;
        highlightingRules_.push_back(rule);
    }

    // Types
    typeFormat_.setForeground(typeColor);
    typeFormat_.setFontWeight(QFont::DemiBold);
    const QString typePatterns[] = {
        QStringLiteral("\\bint\\b"), QStringLiteral("\\bvoid\\b"), QStringLiteral("\\bchar\\b"),
        QStringLiteral("\\bshort\\b"), QStringLiteral("\\blong\\b"), QStringLiteral("\\bfloat\\b"),
        QStringLiteral("\\bdouble\\b"), QStringLiteral("\\bsigned\\b"), QStringLiteral("\\bunsigned\\b"),
        QStringLiteral("\\buint8_t\\b"), QStringLiteral("\\bint8_t\\b"), QStringLiteral("\\buint16_t\\b"),
        QStringLiteral("\\bint16_t\\b"), QStringLiteral("\\buint32_t\\b"), QStringLiteral("\\bint32_t\\b"),
        QStringLiteral("\\buint64_t\\b"), QStringLiteral("\\bint64_t\\b"), QStringLiteral("\\bvec3_t\\b"),
        QStringLiteral("\\bvec2_t\\b"), QStringLiteral("\\bvmMain\\b")
    };
    for (const auto& pattern : typePatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = typeFormat_;
        highlightingRules_.push_back(rule);
    }

    // Quake III Engine Syscalls & functions
    syscallFormat_.setForeground(syscallColor);
    syscallFormat_.setFontWeight(QFont::Bold);
    HighlightingRule syscallRule;
    syscallRule.pattern = QRegularExpression(QStringLiteral("\\btrap_[A-Za-z0-9_]+\\b|\\bfunc_[0-9a-fA-F]+\\b"));
    syscallRule.format = syscallFormat_;
    highlightingRules_.push_back(syscallRule);

    // Preprocessor directives
    preprocessorFormat_.setForeground(preprocessorColor);
    preprocessorFormat_.setFontWeight(QFont::Bold);
    HighlightingRule prepRule;
    prepRule.pattern = QRegularExpression(QStringLiteral("#[A-Za-z_]+"));
    prepRule.format = preprocessorFormat_;
    highlightingRules_.push_back(prepRule);

    // Function calls / declarations
    functionFormat_.setForeground(functionColor);
    HighlightingRule funcRule;
    funcRule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()"));
    funcRule.format = functionFormat_;
    highlightingRules_.push_back(funcRule);

    // Numbers (decimal, hex, float)
    numberFormat_.setForeground(numberColor);
    HighlightingRule numRule;
    numRule.pattern = QRegularExpression(QStringLiteral("\\b0x[0-9a-fA-F]+\\b|\\b[0-9]+\\.?[0-9]*f?\\b"));
    numRule.format = numberFormat_;
    highlightingRules_.push_back(numRule);

    // String literals
    quotationFormat_.setForeground(stringColor);
    HighlightingRule strRule;
    strRule.pattern = QRegularExpression(QStringLiteral("\".*?\"|'.*?'"));
    strRule.format = quotationFormat_;
    highlightingRules_.push_back(strRule);

    // Single-line comments
    singleLineCommentFormat_.setForeground(commentColor);
    singleLineCommentFormat_.setFontItalic(true);
    HighlightingRule commentRule;
    commentRule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
    commentRule.format = singleLineCommentFormat_;
    highlightingRules_.push_back(commentRule);

    // Multi-line comments
    multiLineCommentFormat_.setForeground(commentColor);
    multiLineCommentFormat_.setFontItalic(true);
    commentStartExpression_ = QRegularExpression(QStringLiteral("/\\*"));
    commentEndExpression_ = QRegularExpression(QStringLiteral("\\*/"));
}

void CSyntaxHighlighter::highlightBlock(const QString& text) {
    for (const auto& rule : highlightingRules_) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(static_cast<int>(match.capturedStart()),
                      static_cast<int>(match.capturedLength()),
                      rule.format);
        }
    }

    // Multi-line comments handling
    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1) {
        startIndex = static_cast<int>(text.indexOf(commentStartExpression_));
    }

    while (startIndex >= 0) {
        QRegularExpressionMatch match = commentEndExpression_.match(text, startIndex);
        int endIndex = static_cast<int>(match.capturedStart());
        int commentLength = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + static_cast<int>(match.capturedLength());
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat_);
        startIndex = static_cast<int>(text.indexOf(commentStartExpression_, startIndex + commentLength));
    }
}

} // namespace qvm::gui
