#include "source/gui/models/FunctionsTreeModel.hpp"
#include <QFont>
#include <iomanip>
#include <sstream>

namespace qvm::gui {

FunctionsTreeModel::FunctionsTreeModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void FunctionsTreeModel::setFunctions(std::vector<FunctionInfo> functions) {
    beginResetModel();
    functions_ = std::move(functions);
    endResetModel();
}

void FunctionsTreeModel::clear() {
    beginResetModel();
    functions_.clear();
    endResetModel();
}

int FunctionsTreeModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(functions_.size());
}

int FunctionsTreeModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant FunctionsTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= static_cast<int>(functions_.size())) {
        return {};
    }

    const auto& func = functions_[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColName:
                return QString::fromStdString(func.name);
            case ColStartOffset: {
                std::stringstream ss;
                ss << "0x" << std::setw(6) << std::setfill('0') << std::hex << func.startOffset;
                return QString::fromStdString(ss.str());
            }
            case ColEndOffset: {
                std::stringstream ss;
                ss << "0x" << std::setw(6) << std::setfill('0') << std::hex << func.endOffset;
                return QString::fromStdString(ss.str());
            }
            case ColInstructionCount:
                return QString::number(func.endIndex >= func.startIndex ? (func.endIndex - func.startIndex + 1) : 0);
            case ColStackFrame:
                return QString::number(func.stackFrameSize) + " B";
            case ColHash: {
                std::stringstream ss;
                ss << "0x" << std::hex << func.hash;
                return QString::fromStdString(ss.str());
            }
            case ColStatus:
                return func.isKnown ? QString("Identified") : QString("Inferred");
            default:
                break;
        }
    } else if (role == Qt::FontRole) {
        QFont font("Menlo");
        font.setStyleHint(QFont::Monospace);
        return font;
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColStartOffset || index.column() == ColEndOffset ||
            index.column() == ColInstructionCount || index.column() == ColStackFrame ||
            index.column() == ColHash) {
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }

    return {};
}

QVariant FunctionsTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case ColName:             return tr("Function Name");
            case ColStartOffset:      return tr("Start Offset");
            case ColEndOffset:        return tr("End Offset");
            case ColInstructionCount: return tr("Instructions");
            case ColStackFrame:       return tr("Frame Size");
            case ColHash:             return tr("Bytecode Hash");
            case ColStatus:           return tr("Status");
            default: break;
        }
    }
    return {};
}

const FunctionInfo* FunctionsTreeModel::functionAt(int row) const {
    if (row >= 0 && row < static_cast<int>(functions_.size())) {
        return &functions_[row];
    }
    return nullptr;
}

} // namespace qvm::gui
