
#ifndef JUPYTERWIDGET_H
#define JUPYTERWIDGET_H

#ifdef CUTTER_ENABLE_JUPYTER

#include <memory>

#include "utils/JupyterConnection.h"
#include "CutterDockWidget.h"

#include <QAbstractButton>

#include "CutterDockWidget.h"
#include "common/JupyterConnection.h"

namespace Ui {
class JupyterWidget;
}

class JupyterWebView;
class QTabWidget;
class MainWindow;

class JupyterWidget : public QDockWidget
{
    Q_OBJECT

public:
    JupyterWidget(MainWindow *main);
    ~JupyterWidget();

#ifdef CUTTER_ENABLE_QTWEBENGINE
    JupyterWebView *createNewTab();
#endif

private slots:
    void urlReceived(const QString &url);
    void creationFailed();

    void openHomeTab();
    void tabCloseRequested(int index);

private:
    std::unique_ptr<Ui::JupyterWidget> ui;

    QAbstractButton *homeButton;

    void removeTab(int index);
    void clearTabs();
};

#ifdef CUTTER_ENABLE_QTWEBENGINE

#include <QWebEngineView>

class JupyterWebView : public QWebEngineView
{
    Q_OBJECT

public:
    JupyterWebView(JupyterWidget *mainWidget, QWidget *parent = nullptr);

    void setTabWidget(QTabWidget *tabWidget);

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;

private slots:
    void onTitleChanged(const QString &title);

private:
    JupyterWidget *mainWidget;
    QTabWidget *tabWidget;

    void updateTitle();
};

#endif

#endif

#endif //JUPYTERWIDGET_H
