#pragma once

#include <QWidget>
#include <QMainWindow>

class QMenu;
class QAction;
class QTextEdit;
class QLabel;

class DemoWidget : public QMainWindow
{
    Q_OBJECT

public:
    explicit DemoWidget(QWidget *parent = nullptr);

private slots:
    void onOpen();
    void onInsertEquation();
    void onInsertMultiEquations();
    void onShowDependencyGraph();
    void onShowEquationInspector();
    void onShowEquationResultInspector() {}

private:
    void createMenus();
    void createActions();
    
    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *viewMenu;
    
    QAction *openAction;
    QAction *exitAction;
    QAction *insertEquationAction;
    QAction *insertMultiEquationsAction;
    QAction *dependencyGraphAction;
    QAction *equationInspectorAction;
    QAction *equationResultInspectorAction;
};
