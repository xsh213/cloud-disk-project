QT += core widgets network

CONFIG += c++17

TARGET = LANDiskClient
TEMPLATE = app

INCLUDEPATH += $$PWD/Utils
INCLUDEPATH += $$PWD/Communication

HEADERS += \
    LANDiskClient.h \
    Utils/Config.h \
    Communication/HttpClient.h \
    Communication/ApiClient.h \
    Utils/FileUtil.h \
    Utils/HashUtil.h

SOURCES += \
    main.cpp \
    LANDiskClient.cpp \
    Communication/HttpClient.cpp \
    Communication/ApiClient.cpp \
    Utils/FileUtil.cpp \
    Utils/HashUtil.cpp

FORMS += \
    LANDiskClient.ui

RESOURCES += \
    LANDiskClient.qrc

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000