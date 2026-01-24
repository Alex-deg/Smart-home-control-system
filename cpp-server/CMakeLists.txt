cmake_minimum_required(VERSION 3.10)

project(SmartHomeSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(
    HEADER
    ./include/crow_all.h
    ./src/database/DataBase.h
    ./src/api/api.h
    ./src/mqtt/MQTTClient.h
)

set(
    SOURCE
    ./src/database/DataBase.cpp
    ./src/main.cpp
    ./src/api/api.cpp
    ./src/mqtt/MQTTClient.cpp
)

find_package(SQLite3 REQUIRED)

find_library(MOSQUITTO_LIB NAMES mosquitto)
if (NOT MOSQUITTO_LIB)
    message(WARNING "Mosquitto library not found. Trying to find with pkg-config...")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
    set(MOSQUITTO_LIB ${MOSQUITTO_LIBRARIES})
endif()

add_executable(SmartHomeSystem ${HEADER} ${SOURCE})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    SQLite::SQLite3
    ${MOSQUITTO_LIB}
    pthread
    ${CMAKE_THREAD_LIBS_INIT}
)

# if (SQLite3_FOUND)
#     target_link_libraries(SmartHomeSystem PRIVATE SQLite::SQLite3)
# else()
#     message(FATAL_ERROR "SQLite3 not found!")
# endif()