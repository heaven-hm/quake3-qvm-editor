#include "source/gui/widgets/HexViewWidget.hpp"
#include <QFont>
#include <iomanip>
#include <sstream>

namespace qvm::gui {

HexViewWidget::HexViewWidget(QWidget* parent) : QPlainTextEdit(parent) {
    setReadOnly(true);
    QFont font("Menlo");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(10);
    setFont(font);
    setLineWrapMode(QPlainTextEdit::NoWrap);
}

void HexViewWidget::clearData() {
    clear();
}

void HexViewWidget::setData(const std::vector<uint8_t>& data, size_t maxBytes) {
    clear();
    if (data.empty()) return;

    size_t count = std::min(data.size(), maxBytes);
    std::stringstream ss;

    ss << " OFFSET    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII\n";
    ss << " --------  -----------------------------------------------   ----------------\n";

    for (size_t i = 0; i < count; i += 16) {
        // Offset
        ss << " " << std::hex << std::setw(8) << std::setfill('0') << i << "  ";

        // Hex bytes
        for (size_t j = 0; j < 16; ++j) {
            if (j == 8) ss << " ";
            if (i + j < count) {
                ss << std::hex << std::setw(2) << std::setfill('0')
                   << static_cast<int>(data[i + j]) << " ";
            } else {
                ss << "   ";
            }
        }

        ss << "  ";

        // ASCII chars
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < count) {
                uint8_t b = data[i + j];
                if (std::isprint(b)) {
                    ss << static_cast<char>(b);
                } else {
                    ss << ".";
                }
            } else {
                ss << " ";
            }
        }

        ss << "\n";
    }

    if (data.size() > maxBytes) {
        ss << "\n... (" << (data.size() - maxBytes) << " additional bytes truncated for display) ...\n";
    }

    setPlainText(QString::fromStdString(ss.str()));
}

} // namespace qvm::gui
