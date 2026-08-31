#include "MainWindow.h"
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , rootPath(QDir::homePath() + "/Documents/LocalNetDisk")
{
    QDir dir;
    if (!dir.exists(rootPath)) {
        dir.mkpath(rootPath);
    }

    setupUI();

    setWindowTitle("局域网网盘");
    resize(1024, 768);

    fileModel = new QFileSystemModel(this);
    fileModel->setRootPath(rootPath);
    fileModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);

    treeView->setModel(fileModel);
    listView->setModel(fileModel);

    treeView->hideColumn(1);
    treeView->hideColumn(2);
    treeView->hideColumn(3);

    QModelIndex rootIndex = fileModel->index(rootPath);
    treeView->setRootIndex(rootIndex);
    listView->setRootIndex(rootIndex);
    currentPath = rootPath;
    pathLineEdit->setText(rootPath);

    connect(treeView, &QTreeView::clicked, this, &MainWindow::onCurrentPathChanged);
    connect(listView, &QListView::doubleClicked, this, &MainWindow::onFileDoubleClicked);
    connect(treeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onCustomContextMenuRequested);
    connect(listView, &QListView::customContextMenuRequested, this, &MainWindow::onCustomContextMenuRequested);

    connect(uploadBtn, &QPushButton::clicked, this, &MainWindow::onUploadClicked);
    connect(newFolderBtn, &QPushButton::clicked, this, &MainWindow::onNewFolderClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &MainWindow::onDeleteClicked);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(upBtn, &QPushButton::clicked, this, &MainWindow::onUpClicked);

    connect(pathLineEdit, &QLineEdit::returnPressed, [this]() {
        QString path = pathLineEdit->text();
        QDir dir(path);
        if (dir.exists()) {
            QModelIndex index = fileModel->index(path);
            treeView->setRootIndex(index);
            listView->setRootIndex(index);
            currentPath = path;
            updateStatusBar();
        }
        else {
            QMessageBox::warning(this, "错误", "路径不存在！");
        }
        });

    updateStatusBar();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(5);

    QHBoxLayout* toolLayout = new QHBoxLayout();
    toolLayout->setSpacing(10);

    pathLineEdit = new QLineEdit();
    pathLineEdit->setPlaceholderText("请输入路径...");
    pathLineEdit->setMinimumHeight(30);
    toolLayout->addWidget(pathLineEdit, 1);

    uploadBtn = new QPushButton("上传");
    uploadBtn->setMinimumHeight(30);
    uploadBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; border-radius: 5px; padding: 5px 15px; }");

    newFolderBtn = new QPushButton("新建文件夹");
    newFolderBtn->setMinimumHeight(30);
    newFolderBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; border-radius: 5px; padding: 5px 15px; }");

    deleteBtn = new QPushButton("删除");
    deleteBtn->setMinimumHeight(30);
    deleteBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; border-radius: 5px; padding: 5px 15px; }");

    refreshBtn = new QPushButton("刷新");
    refreshBtn->setMinimumHeight(30);
    refreshBtn->setStyleSheet("QPushButton { background-color: #FF9800; color: white; border-radius: 5px; padding: 5px 15px; }");

    upBtn = new QPushButton("⬆ 向上");
    upBtn->setMinimumHeight(30);
    upBtn->setStyleSheet("QPushButton { background-color: #607D8B; color: white; border-radius: 5px; padding: 5px 15px; }");

    toolLayout->addWidget(uploadBtn);
    toolLayout->addWidget(newFolderBtn);
    toolLayout->addWidget(deleteBtn);
    toolLayout->addWidget(refreshBtn);
    toolLayout->addWidget(upBtn);

    mainLayout->addLayout(toolLayout);

    QSplitter* splitter = new QSplitter(Qt::Horizontal);

    treeView = new QTreeView();
    treeView->setMinimumWidth(200);
    treeView->setMaximumWidth(300);
    treeView->setHeaderHidden(true);
    treeView->setIndentation(15);
    treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    listView = new QListView();
    listView->setViewMode(QListView::IconMode);
    listView->setIconSize(QSize(64, 64));
    listView->setGridSize(QSize(80, 80));
    listView->setResizeMode(QListView::Adjust);
    listView->setMovement(QListView::Static);
    listView->setContextMenuPolicy(Qt::CustomContextMenu);

    splitter->addWidget(treeView);
    splitter->addWidget(listView);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);

    statusLabel = new QLabel("就绪");
    progressBar = new QProgressBar();
    progressBar->setMaximumWidth(150);
    progressBar->setVisible(false);

    statusBar()->addWidget(statusLabel);
    statusBar()->addPermanentWidget(progressBar);
}

void MainWindow::updateStatusBar()
{
    QDir dir(currentPath);
    int fileCount = dir.entryList(QDir::Files).count();
    int folderCount = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).count();
    statusLabel->setText(QString("路径: %1 | 文件: %2 | 文件夹: %3")
        .arg(currentPath).arg(fileCount).arg(folderCount));
}

QString MainWindow::getCurrentPath()
{
    QModelIndex index = listView->rootIndex();
    if (index.isValid()) {
        return fileModel->filePath(index);
    }
    return rootPath;
}

void MainWindow::onUploadClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "选择要上传的文件", QDir::homePath(), "所有文件 (*.*)");
    if (files.isEmpty()) return;

    QString targetPath = getCurrentPath();
    progressBar->setVisible(true);
    progressBar->setRange(0, files.size());

    for (int i = 0; i < files.size(); ++i) {
        QString sourceFile = files[i];
        QFileInfo info(sourceFile);
        QString destFile = targetPath + "/" + info.fileName();

        if (QFile::exists(destFile)) {
            QMessageBox::StandardButton reply = QMessageBox::question(this,
                "文件已存在",
                QString("文件 %1 已存在，是否覆盖？").arg(info.fileName()),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::No) continue;
        }

        if (QFile::copy(sourceFile, destFile)) {
            statusBar()->showMessage(QString("已上传: %1").arg(info.fileName()), 2000);
        }
        else {
            QMessageBox::warning(this, "错误", QString("上传文件 %1 失败！").arg(info.fileName()));
        }
        progressBar->setValue(i + 1);
    }

    progressBar->setVisible(false);
    onRefreshClicked();
}

void MainWindow::onNewFolderClicked()
{
    bool ok;
    QString folderName = QInputDialog::getText(this, "新建文件夹", "请输入文件夹名称:", QLineEdit::Normal, "新建文件夹", &ok);
    if (ok && !folderName.isEmpty()) {
        QString newFolderPath = getCurrentPath() + "/" + folderName;
        QDir dir;
        if (dir.mkdir(newFolderPath)) {
            statusBar()->showMessage(QString("已创建文件夹: %1").arg(folderName), 2000);
            onRefreshClicked();
        }
        else {
            QMessageBox::warning(this, "错误", "创建文件夹失败！");
        }
    }
}

void MainWindow::onDeleteClicked()
{
    QModelIndex index = listView->currentIndex();
    if (!index.isValid()) {
        QMessageBox::information(this, "提示", "请先选择要删除的文件或文件夹！");
        return;
    }

    QString filePath = fileModel->filePath(index);
    QFileInfo info(filePath);

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "确认删除",
        QString("确定要删除 %1 吗？").arg(info.fileName()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (deleteFileOrFolder(filePath)) {
            statusBar()->showMessage(QString("已删除: %1").arg(info.fileName()), 2000);
            onRefreshClicked();
        }
    }
}

bool MainWindow::deleteFileOrFolder(const QString& path)
{
    QFileInfo info(path);
    if (info.isDir()) {
        QDir dir(path);
        return dir.removeRecursively();
    }
    else {
        return QFile::remove(path);
    }
}

void MainWindow::onRefreshClicked()
{
    QModelIndex currentIndex = treeView->rootIndex();
    if (currentIndex.isValid()) {
        QString path = fileModel->filePath(currentIndex);
        QModelIndex newIndex = fileModel->index(path);
        treeView->setRootIndex(newIndex);
        listView->setRootIndex(newIndex);
        updateStatusBar();
    }
}

void MainWindow::onUpClicked()
{
    QDir dir(currentPath);
    if (dir.cdUp()) {
        QString parentPath = dir.absolutePath();
        QModelIndex parentIndex = fileModel->index(parentPath);
        if (parentIndex.isValid()) {
            treeView->setRootIndex(parentIndex);
            listView->setRootIndex(parentIndex);
            currentPath = parentPath;
            pathLineEdit->setText(parentPath);
            updateStatusBar();
        }
    }
    else {
        statusBar()->showMessage("已在根目录", 2000);
    }
}

void MainWindow::onFileDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    QString filePath = fileModel->filePath(index);
    QFileInfo info(filePath);

    if (info.isDir()) {
        treeView->setRootIndex(index);
        listView->setRootIndex(index);
        currentPath = filePath;
        pathLineEdit->setText(filePath);
        updateStatusBar();
    }
    else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    }
}

void MainWindow::onCurrentPathChanged(const QModelIndex& index)
{
    if (!index.isValid()) return;

    QString path = fileModel->filePath(index);
    QFileInfo info(path);

    if (info.isDir()) {
        listView->setRootIndex(index);
        currentPath = path;
        pathLineEdit->setText(path);
        updateStatusBar();
    }
}

void MainWindow::onCustomContextMenuRequested(const QPoint& pos)
{
    QWidget* sender = qobject_cast<QWidget*>(this->sender());
    if (!sender) return;

    QModelIndex index;
    if (sender == treeView) {
        index = treeView->indexAt(pos);
    }
    else if (sender == listView) {
        index = listView->indexAt(pos);
    }

    QMenu menu(this);

    if (!index.isValid()) {
        QAction* newFolderAction = new QAction("新建文件夹", this);
        QAction* uploadAction = new QAction("上传文件", this);
        QAction* refreshAction = new QAction("刷新", this);

        connect(newFolderAction, &QAction::triggered, this, &MainWindow::onNewFolderClicked);
        connect(uploadAction, &QAction::triggered, this, &MainWindow::onUploadClicked);
        connect(refreshAction, &QAction::triggered, this, &MainWindow::onRefreshClicked);

        menu.addAction(newFolderAction);
        menu.addAction(uploadAction);
        menu.addSeparator();
        menu.addAction(refreshAction);
        menu.exec(sender->mapToGlobal(pos));
        return;
    }

    QAction* openAction = new QAction("打开", this);
    QAction* deleteAction = new QAction("删除", this);
    QAction* renameAction = new QAction("重命名", this);
    QAction* uploadAction = new QAction("上传文件", this);
    QAction* newFolderAction = new QAction("新建文件夹", this);
    QAction* refreshAction = new QAction("刷新", this);

    connect(openAction, &QAction::triggered, [this, index]() {
        onFileDoubleClicked(index);
        });
    connect(deleteAction, &QAction::triggered, [this, index]() {
        QString path = fileModel->filePath(index);
        QFileInfo info(path);
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "确认删除",
            QString("确定要删除 %1 吗？").arg(info.fileName()),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            if (deleteFileOrFolder(path)) {
                statusBar()->showMessage(QString("已删除: %1").arg(info.fileName()), 2000);
                onRefreshClicked();
            }
        }
        });
    connect(renameAction, &QAction::triggered, [this, index]() {
        QString oldPath = fileModel->filePath(index);
        QFileInfo info(oldPath);
        bool ok;
        QString newName = QInputDialog::getText(this,
            "重命名",
            "请输入新名称:",
            QLineEdit::Normal,
            info.fileName(),
            &ok);
        if (ok && !newName.isEmpty()) {
            if (renameFileOrFolder(oldPath, newName)) {
                statusBar()->showMessage(QString("已重命名为: %1").arg(newName), 2000);
                onRefreshClicked();
            }
        }
        });
    connect(uploadAction, &QAction::triggered, this, &MainWindow::onUploadClicked);
    connect(newFolderAction, &QAction::triggered, this, &MainWindow::onNewFolderClicked);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::onRefreshClicked);

    menu.addAction(openAction);
    menu.addSeparator();
    menu.addAction(deleteAction);
    menu.addAction(renameAction);
    menu.addSeparator();
    menu.addAction(uploadAction);
    menu.addAction(newFolderAction);
    menu.addSeparator();
    menu.addAction(refreshAction);

    menu.exec(sender->mapToGlobal(pos));
}

bool MainWindow::renameFileOrFolder(const QString& oldPath, const QString& newName)
{
    QFileInfo info(oldPath);
    QString newPath = info.absolutePath() + "/" + newName;

    if (QFile::exists(newPath)) {
        QMessageBox::warning(this, "错误", "同名文件或文件夹已存在！");
        return false;
    }

    QDir dir;
    return dir.rename(oldPath, newPath);
}