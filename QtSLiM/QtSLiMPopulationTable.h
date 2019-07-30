#ifndef QTSLIMPOPULATIONTABLE_H
#define QTSLIMPOPULATIONTABLE_H

#include <QObject>
#include <QAbstractTableModel>
#include <QHeaderView>

class QPainter;


class QtSLiMPopulationTableModel : public QAbstractTableModel
{
    Q_OBJECT    
    
public:
    QtSLiMPopulationTableModel(QObject *parent = nullptr);
    virtual ~QtSLiMPopulationTableModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void reloadTable(void);
};

class QtSLiMPopulationTableHeaderView : public QHeaderView
{
    Q_OBJECT
    
    QIcon *icon_cloning_rate = nullptr;
    QIcon *icon_selfing_rate = nullptr;
    QIcon *icon_sex_ratio = nullptr;
    QIcon *icon_female_symbol = nullptr;
    QIcon *icon_male_symbol = nullptr;
    
public:
    QtSLiMPopulationTableHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);
    virtual ~QtSLiMPopulationTableHeaderView() override;
    
    virtual void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
};


#endif // QTSLIMPOPULATIONTABLE_H









































