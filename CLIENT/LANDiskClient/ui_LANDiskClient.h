/********************************************************************************
** Form generated from reading UI file 'LANDiskClient.ui'
**
** Created by: Qt User Interface Compiler version 6.11.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LANDISKCLIENT_H
#define UI_LANDISKCLIENT_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_LANDiskClientClass
{
public:
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QWidget *centralWidget;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *LANDiskClientClass)
    {
        if (LANDiskClientClass->objectName().isEmpty())
            LANDiskClientClass->setObjectName("LANDiskClientClass");
        LANDiskClientClass->resize(600, 400);
        menuBar = new QMenuBar(LANDiskClientClass);
        menuBar->setObjectName("menuBar");
        LANDiskClientClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(LANDiskClientClass);
        mainToolBar->setObjectName("mainToolBar");
        LANDiskClientClass->addToolBar(mainToolBar);
        centralWidget = new QWidget(LANDiskClientClass);
        centralWidget->setObjectName("centralWidget");
        LANDiskClientClass->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(LANDiskClientClass);
        statusBar->setObjectName("statusBar");
        LANDiskClientClass->setStatusBar(statusBar);

        retranslateUi(LANDiskClientClass);

        QMetaObject::connectSlotsByName(LANDiskClientClass);
    } // setupUi

    void retranslateUi(QMainWindow *LANDiskClientClass)
    {
        LANDiskClientClass->setWindowTitle(QCoreApplication::translate("LANDiskClientClass", "LANDiskClient", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LANDiskClientClass: public Ui_LANDiskClientClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LANDISKCLIENT_H
