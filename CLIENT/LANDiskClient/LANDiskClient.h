#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_LANDiskClient.h"

class LANDiskClient : public QMainWindow
{
    Q_OBJECT

public:
    LANDiskClient(QWidget *parent = nullptr);
    ~LANDiskClient();

private:
    Ui::LANDiskClientClass ui;
};

