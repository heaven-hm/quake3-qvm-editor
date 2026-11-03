#pragma once

#include <QPlainTextEdit>
#include <vector>

namespace qvm::gui {

class HexViewWidget : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit HexViewWidget(QWidget* parent = nullptr);

    void setData(const std::vector<uint8_t>& data, size_t maxBytes = 65536);
    void clearData();
};

} // namespace qvm::gui
