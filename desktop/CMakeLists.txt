cmake_minimum_required(VERSION 3.16)
project(krisp_desktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets Test)
find_package(GTest REQUIRED)

add_library(krisp_core STATIC
    src/deepgram_protocol.cpp
    src/protocol.cpp
    src/transcript_model.cpp
)
target_include_directories(krisp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(krisp_core PUBLIC Qt6::Core)

add_executable(krisp_desktop
    src/main.cpp
    src/ws_server.cpp
    src/deepgram_client.cpp
    src/main_window.cpp
)
target_link_libraries(krisp_desktop PRIVATE
    krisp_core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)

enable_testing()
add_executable(krisp_tests
    tests/main.cpp
    tests/test_deepgram_protocol.cpp
    tests/test_protocol.cpp
    tests/test_transcript_model.cpp
    tests/test_stream_routing.cpp
)
target_link_libraries(krisp_tests PRIVATE
    krisp_core
    Qt6::Test
    GTest::gtest
    GTest::gmock
)
add_test(NAME krisp_tests COMMAND krisp_tests)

install(TARGETS krisp_desktop RUNTIME DESTINATION bin)
