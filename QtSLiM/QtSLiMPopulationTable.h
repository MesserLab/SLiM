#ifndef QTSLIMPOPULATIONTABLE_H
#define QTSLIMPOPULATIONTABLE_H

#include <QObject>
#include <QAbstractTableModel>


class QtSLiMPopulationTableModel : public QAbstractTableModel
{
    Q_OBJECT    
    
public:
    QtSLiMPopulationTableModel(QObject *parent = 0);
    virtual ~QtSLiMPopulationTableModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void reloadTable(void);
};

#endif // QTSLIMPOPULATIONTABLE_H









































