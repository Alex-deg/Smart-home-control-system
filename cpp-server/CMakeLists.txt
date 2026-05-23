cmake_minimum_required(VERSION 3.10)

project(SmartHomeSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system thread)

set(
    HEADER
    include/api.hpp
    include/ASTNodes.hpp
    include/DataBase.hpp
    include/JSONHandler.hpp
    include/MQTTClient.hpp
    include/ScenarioHandler.hpp
    include/ScenarioParser.hpp
    include/ws.hpp
)

set(
    SOURCE
    src/api.hpp
    src/ASTNodes.hpp
    src/DataBase.hpp
    src/MQTTClient.hpp
    src/ScenarioHandler.hpp
    src/ScenarioParser.hpp
    src/ws.hpp
    main.cpp
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
    ${Boost_LIBRARIES}
    pthread
    ${CMAKE_THREAD_LIBS_INIT}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${Boost_INCLUDE_DIRS})