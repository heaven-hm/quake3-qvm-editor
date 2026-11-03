#include "source/gui/models/StringsTableModel.hpp"
#include <QFont>
#include <iomanip>
#include <sstream>

namespace qvm::gui {

StringsTableModel::StringsTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void StringsTableModel::setStrings(std::vector<ExtractedString> strings) {
    beginResetModel();
    strings_ = std::move(strings);
    endResetModel();
}

void StringsTableModel::clear() {
    beginResetModel();
    strings_.clear();
    endResetModel();
}

int StringsTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(strings_.size());
}

int StringsTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant StringsTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(strings_.size())) {
        return {};
    }

    const auto& item = strings_[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColOffset: {
                std::stringstream ss;
                ss << "0x" << std::setw(6) << std::setfill('0') << std::hex << item.offset;
                return QString::fromStdString(ss.str());
            }
            case ColSection:
                return item.inLitSection ? QString("LIT (Strings)") : QString("DATA");
            case ColLength:
                return QString::number(item.text.size());
            case ColContent:
                return QString::fromStdString(item.text);
            default:
                break;
        }
    } else if (role == Qt::FontRole) {
        QFont font("Menlo");
        font.setStyleHint(QFont::Monospace);
        return font;
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColOffset || index.column() == ColLength) {
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }

    return {};
}

QVariant StringsTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case ColOffset:  return tr("Offset");
            case ColSection: return tr("Section");
            case ColLength:  return tr("Length");
            case ColContent: return tr("String Literal");
            default: break;
        }
    }
    return {};
}

} // namespace qvm::gui
