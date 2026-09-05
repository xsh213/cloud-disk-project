cmake_minimum_required(VERSION 3.21)

project(LANCloudDiskServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)

add_executable(LANCloudDiskServer
    src/main.cpp
    src/FileManager.cpp
    src/StorageEngine.cpp
    include/FileManager.h
    include/StorageEngine.h
)

target_include_directories(LANCloudDiskServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(LANCloudDiskServer PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
)
