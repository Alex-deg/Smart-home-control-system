cmake_minimum_required(VERSION 3.10)

project(SmartHomeSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(
    HEADER
    ./include/crow_all.h
    ./src/database/DataBase.h
    ./src/api/api.h
)

set(
    SOURCE
    ./src/database/DataBase.cpp
    ./src/main.cpp
    ./src/api/api.cpp
)

find_package(SQLite3 REQUIRED)

add_executable(SmartHomeSystem ${HEADER} ${SOURCE})

if (SQLite3_FOUND)
    target_link_libraries(SmartHomeSystem PRIVATE SQLite::SQLite3)
else()
    message(FATAL_ERROR "SQLite3 not found!")
endif()