cmake_minimum_required(VERSION 3.10)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-server/src)
set(PROJECT_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../cpp-server/include)

enable_testing()

set(
    SOURCES
    unit/DBTest.cpp
    unit/IDBTest.cpp
    unit/ScenarioParserTest.cpp
    unit/ScenarioHandlerTest.cpp
    unit/JSONHandlerTest.cpp
    main.cpp
    ${PROJECT_SOURCE_DIR}/ASTNodes.cpp
    ${PROJECT_SOURCE_DIR}/DataBase.cpp
    ${PROJECT_SOURCE_DIR}/ScenarioParser.cpp
    ${PROJECT_SOURCE_DIR}/ScenarioHandler.cpp
)

set(
    HEADERS
    ${PROJECT_HEADER_DIR}/JSONHandler.hpp
    ${PROJECT_HEADER_DIR}/ASTNodes.hpp
    ${PROJECT_HEADER_DIR}/DataBase.hpp
    ${PROJECT_HEADER_DIR}/ScenarioParser.hpp
    ${PROJECT_HEADER_DIR}/ScenarioHandler.hpp
)

add_executable(unit_tests
    ${HEADERS} ${SOURCES}
)

target_include_directories(unit_tests PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_HEADER_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(unit_tests
    gtest_main
    gmock
    sqlite3
    ${CMAKE_THREAD_LIBS_INIT}
)

# # Интеграционные тесты
# add_executable(integration_tests
#     integration/MqttDatabaseIntegrationTest.cpp
#     integration/ScenarioMqttIntegrationTest.cpp
#     integration/WebSocketMqttIntegrationTest.cpp
#     integration/DiagnosticsIntegrationTest.cpp
#     main.cpp
# )

# target_include_directories(integration_tests PRIVATE
#     ${PROJECT_SOURCE_DIR}
#     ${PROJECT_SOURCE_DIR}/database
#     ${PROJECT_SOURCE_DIR}/mqtt
#     ${PROJECT_SOURCE_DIR}/websocket
#     ${PROJECT_SOURCE_DIR}/scenario-handler
#     ${CMAKE_CURRENT_BINARY_DIR}
# )

# target_link_libraries(integration_tests
#     gtest_main
#     gmock
#     sqlite3
#     ${CMAKE_THREAD_LIBS_INIT}
# )

# Добавление тестов в CTest
include(GoogleTest)
# gtest_discover_tests(unit_tests)
# gtest_discover_tests(integration_tests)