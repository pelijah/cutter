#pragma once

#include <QTextEdit>
#include <QPlainTextEdit>
#include <QGridLayout>
#include <QJsonObject>
#include <memory>

#include "Cutter.h"
#include "CutterDockWidget.h"

class MainWindow;

namespace Ui {
class RegistersWidget;
}

class RegistersWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit RegistersWidget(MainWindow *main);
    ~RegistersWidget();

private slots:
    void updateContents();
    void setRegisterGrid();

private:
    std::unique_ptr<Ui::RegistersWidget> ui;
    QGridLayout *registerLayout = new QGridLayout;
    int numCols = 2;
    int registerLen = 0;
};