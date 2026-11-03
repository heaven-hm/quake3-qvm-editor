#pragma once

#include "qvm/format/QvmInstruction.hpp"
#include <QAbstractTableModel>
#include <vector>

namespace qvm::gui {

class InstructionTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColIndex = 0,
        ColOffset,
        ColOpcode,
        ColOperandDec,
        ColOperandHex,
        ColComment,
        ColCount
    };

    explicit InstructionTableModel(QObject* parent = nullptr);

    void setInstructions(std::vector<QvmInstruction> instructions);
    void setFilterRange(int startRow, int endRow);
    void clearFilter();
    void clear();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    const QvmInstruction* instructionAt(int row) const;

private:
    std::vector<QvmInstruction> instructions_;
    int filterStart_{-1};
    int filterEnd_{-1};
};

} // namespace qvm::gui
