#pragma once

#include "qvm/decompiler/FunctionAnalyzer.hpp"
#include <QAbstractTableModel>
#include <vector>

namespace qvm::gui {

class FunctionsTreeModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColName = 0,
        ColStartOffset,
        ColEndOffset,
        ColInstructionCount,
        ColStackFrame,
        ColHash,
        ColStatus,
        ColCount
    };

    explicit FunctionsTreeModel(QObject* parent = nullptr);

    void setFunctions(std::vector<FunctionInfo> functions);
    void clear();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    const FunctionInfo* functionAt(int row) const;

private:
    std::vector<FunctionInfo> functions_;
};

} // namespace qvm::gui
