#pragma once

#include <memory>

#include "Cutter.h"
#include "CutterDockWidget.h"

#include <QAbstractListModel>
#include <QSortFilterProxyModel>

class MainWindow;
class QTreeWidget;
class MemoryMapWidget;

namespace Ui {
class MemoryMapWidget;
}


class MainWindow;
class QTreeWidgetItem;


class MemoryMapModel: public QAbstractListModel
{
    Q_OBJECT

    friend MemoryMapWidget;

private:
    QList<MemoryMapDescription> *memoryMaps;

public:
    enum Column { AddrStartColumn = 0, AddrEndColumn, NameColumn, PermColumn, ColumnCount };
    enum Role { MemoryDescriptionRole = Qt::UserRole };

    MemoryMapModel(QList<MemoryMapDescription> *memoryMaps, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
};



class MemoryProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    MemoryProxyModel(MemoryMapModel *sourceModel, QObject *parent = nullptr);

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
};



class MemoryMapWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit MemoryMapWidget(MainWindow *main);
    ~MemoryMapWidget();

private slots:
    void on_memoryTreeView_doubleClicked(const QModelIndex &index);

    void refreshMemoryMap();

private:
    std::unique_ptr<Ui::MemoryMapWidget> ui;

    MemoryMapModel *memoryModel;
    MemoryProxyModel *memoryProxyModel;
    QList<MemoryMapDescription> memoryMaps;

    void setScrollMode();
};
