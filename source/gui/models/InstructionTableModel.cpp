#include "source/gui/models/InstructionTableModel.hpp"
#include <QFont>
#include <iomanip>
#include <sstream>

namespace qvm::gui {

InstructionTableModel::InstructionTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void InstructionTableModel::setInstructions(std::vector<QvmInstruction> instructions) {
    beginResetModel();
    instructions_ = std::move(instructions);
    filterStart_ = -1;
    filterEnd_ = -1;
    endResetModel();
}

void InstructionTableModel::setFilterRange(int startRow, int endRow) {
    beginResetModel();
    filterStart_ = qBound(0, startRow, static_cast<int>(instructions_.size()) - 1);
    filterEnd_ = qBound(filterStart_, endRow, static_cast<int>(instructions_.size()) - 1);
    endResetModel();
}

void InstructionTableModel::clearFilter() {
    beginResetModel();
    filterStart_ = -1;
    filterEnd_ = -1;
    endResetModel();
}

void InstructionTableModel::clear() {
    beginResetModel();
    instructions_.clear();
    filterStart_ = -1;
    filterEnd_ = -1;
    endResetModel();
}

int InstructionTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    if (instructions_.empty()) return 0;
    if (filterStart_ >= 0 && filterEnd_ >= filterStart_) {
        return (filterEnd_ - filterStart_ + 1);
    }
    return static_cast<int>(instructions_.size());
}

int InstructionTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant InstructionTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};

    int realRow = index.row();
    if (filterStart_ >= 0) {
        realRow += filterStart_;
    }

    if (realRow < 0 || realRow >= static_cast<int>(instructions_.size())) {
        return {};
    }

    const auto& instr = instructions_[realRow];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case ColIndex:
                return QString::number(instr.index);
            case ColOffset: {
                std::stringstream ss;
                ss << "0x" << std::setw(6) << std::setfill('0') << std::hex << instr.offset;
                return QString::fromStdString(ss.str());
            }
            case ColOpcode:
                return QString::fromUtf8(instr.opcodeName().data(), static_cast<qsizetype>(instr.opcodeName().size()));
            case ColOperandDec:
                if (instr.operandSize > 0) return QString::number(instr.operand);
                return QString("-");
            case ColOperandHex:
                if (instr.operandSize > 0) {
                    std::stringstream ss;
                    ss << "0x" << std::hex << instr.operand;
                    return QString::fromStdString(ss.str());
                }
                return QString("-");
            case ColComment:
                return QString::fromStdString(instr.comment);
            default:
                break;
        }
    } else if (role == Qt::FontRole) {
        QFont font("Menlo");
        font.setStyleHint(QFont::Monospace);
        return font;
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColIndex || index.column() == ColOffset ||
            index.column() == ColOperandDec || index.column() == ColOperandHex) {
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }

    return {};
}

QVariant InstructionTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case ColIndex:      return tr("Index");
            case ColOffset:     return tr("Offset");
            case ColOpcode:     return tr("Opcode");
            case ColOperandDec: return tr("Operand (Dec)");
            case ColOperandHex: return tr("Operand (Hex)");
            case ColComment:    return tr("Comment / Target");
            default: break;
        }
    }
    return {};
}

const QvmInstruction* InstructionTableModel::instructionAt(int row) const {
    int realRow = row;
    if (filterStart_ >= 0) {
        realRow += filterStart_;
    }
    if (realRow >= 0 && realRow < static_cast<int>(instructions_.size())) {
        return &instructions_[realRow];
    }
    return nullptr;
}

} // namespace qvm::gui
