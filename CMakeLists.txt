cmake_minimum_required(VERSION 3.16)

project(ZenithGroundStationQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)

set(APP_SOURCES
    src/main.cpp
    src/AppState.cpp
    src/AppState.h
    src/TelemetryStore.cpp
    src/TelemetryStore.h
    src/ZenithProtocolClient.cpp
    src/ZenithProtocolClient.h
    src/CommandDispatcher.cpp
    src/CommandDispatcher.h
    src/ZenithProtocolProfile.cpp
    src/ZenithProtocolProfile.h
    src/ScriptActionModel.cpp
    src/ScriptActionModel.h
    src/FlightRecorder.cpp
    src/FlightRecorder.h
    src/ParamStore.cpp
    src/ParamStore.h
    src/ZenithProtocol.h
    resources/qml.qrc
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        WIN32
        MANUAL_FINALIZATION
        ${APP_SOURCES}
    )
else()
    add_executable(${PROJECT_NAME}
        WIN32
        ${APP_SOURCES}
    )
endif()

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::QuickControls2
        Qt${QT_VERSION_MAJOR}::Network
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()
