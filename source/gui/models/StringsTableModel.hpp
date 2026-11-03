#pragma once

#include "qvm/decompiler/StringExtractor.hpp"
#include <QAbstractTableModel>
#include <vector>

namespace qvm::gui {

class StringsTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColOffset = 0,
        ColSection,
        ColLength,
        ColContent,
        ColCount
    };

    explicit StringsTableModel(QObject* parent = nullptr);

    void setStrings(std::vector<ExtractedString> strings);
    void clear();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    std::vector<ExtractedString> strings_;
};

} // namespace qvm::gui
