#pragma once

#include <QMainWindow>
#include <QFileSystemModel>
#include <QTreeView>
#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QLineEdit>
#include <QStatusBar>
#include <QProgressBar>
#include <QMenu>
#include <QAction>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onUploadClicked();
    void onNewFolderClicked();
    void onDeleteClicked();
    void onRefreshClicked();
    void onUpClicked();
    void onFileDoubleClicked(const QModelIndex& index);
    void onCurrentPathChanged(const QModelIndex& index);
    void onCustomContextMenuRequested(const QPoint& pos);

private:
    void setupUI();
    void updateStatusBar();
    QString getCurrentPath();
    bool deleteFileOrFolder(const QString& path);
    bool renameFileOrFolder(const QString& oldPath, const QString& newName);

private:
    QFileSystemModel* fileModel;
    QTreeView* treeView;
    QListView* listView;
    QPushButton* uploadBtn;
    QPushButton* newFolderBtn;
    QPushButton* deleteBtn;
    QPushButton* refreshBtn;
    QPushButton* upBtn;
    QLineEdit* pathLineEdit;
    QLabel* statusLabel;
    QProgressBar* progressBar;
    QString currentPath;
    QString rootPath;
};
