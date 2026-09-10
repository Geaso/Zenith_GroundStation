cmake_minimum_required(VERSION 3.16)

project(ZenithGroundStationQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network SerialPort Quick3D)

qt_standard_project_setup(REQUIRES 6.5)

set(APP_SOURCES
    src/main.cpp
    src/AppState.cpp
    src/AppState.h
    src/TelemetryStore.cpp
    src/TelemetryStore.h
    src/ZenithProtocolClient.cpp
    src/ZenithProtocolClient.h
    src/Lr24RadioProtocol.cpp
    src/Lr24RadioProtocol.h
    src/RadioPairingModel.cpp
    src/RadioPairingModel.h
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
    src/ZenithMavlinkCodec.cpp
    src/ZenithMavlinkCodec.h
    src/ManualMoveCommand.h
    src/GridMapImageProvider.h
    src/VoxelInstanceTable.h
    src/TrailInstanceTable.h
    src/PlannedPathInstanceTable.h
    resources/qml.qrc
)

qt_add_executable(${PROJECT_NAME} WIN32 MANUAL_FINALIZATION ${APP_SOURCES})

# QML 走模块化，触发 qmlcachegen 把每个 .qml 编成字节码单元。
# 注意：模块内的类型靠 "import ZenithUI 1.0" 解析，不要再用 import "components" 这类目录导入，
# 因为下面剥掉 raw 资源后 qrc 里已经没有目录结构了。
qt_add_qml_module(${PROJECT_NAME}
    URI ZenithUI
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/RadioPairingDialog.qml
        qml/components/InfoCard.qml
        qml/components/MetricTile.qml
        qml/components/SectionTitle.qml
        qml/components/PrimaryButton.qml
        qml/components/StatusPill.qml
        qml/pages/OverviewPage.qml
        qml/pages/ModulesPage.qml
        qml/pages/ScriptsPage.qml
        qml/pages/ParamsPage.qml
        qml/pages/MapPage.qml
)

target_include_directories(${PROJECT_NAME} PRIVATE src include)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE third_party)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Network
        Qt6::SerialPort
        Qt6::Quick3D
)

# ---------------------------------------------------------------------------
# 闭源加固
# ---------------------------------------------------------------------------
# qt_add_qml_module() 在生成字节码之外，还会无条件把 QML 源码原样打进一份
# "<target>_raw_qml_N" 资源（见 Qt6QmlMacros.cmake 中的 qt6_add_resources 调用）。
# 开源版 Qt 没有提供关闭它的开关，只能在这里把这份资源从目标源文件里摘掉。
# 摘掉后 exe 内不再含任何 QML 明文，运行时由 qmlcachegen 的字节码单元接管。
get_target_property(_zenith_srcs ${PROJECT_NAME} SOURCES)
set(_zenith_kept "")
foreach(_s IN LISTS _zenith_srcs)
    if(_s MATCHES "raw_qml")
        message(STATUS "[closed-source] 剥离 QML 源码资源: ${_s}")
    else()
        list(APPEND _zenith_kept "${_s}")
    endif()
endforeach()
set_property(TARGET ${PROJECT_NAME} PROPERTY SOURCES "${_zenith_kept}")

# Release 下隐藏符号并 strip，缩小可逆向的信息面
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${PROJECT_NAME} PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)
    target_link_options(${PROJECT_NAME} PRIVATE -s)
endif()

# 任务清单外置：编译后拷到 exe 同级，保证开发运行与客户分发的加载路径一致
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config/script_actions.json"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config/script_actions.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config/radio_pairings.json"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config/radio_pairings.json"
    COMMENT "拷贝任务清单与数传配对表"
)

qt_finalize_executable(${PROJECT_NAME})

include(CTest)
# Explicit manual integration target; never contacts a remote endpoint in CTest.
qt_add_executable(ZenithMavlinkProbe
    tools/mavlink_probe.cpp
    src/ZenithMavlinkCodec.cpp src/ZenithMavlinkCodec.h
    src/ZenithProtocolClient.cpp src/ZenithProtocolClient.h
    src/Lr24RadioProtocol.cpp src/Lr24RadioProtocol.h
    src/TelemetryStore.cpp src/TelemetryStore.h
)
target_include_directories(ZenithMavlinkProbe PRIVATE src include)
target_include_directories(ZenithMavlinkProbe SYSTEM PRIVATE third_party)
target_link_libraries(ZenithMavlinkProbe PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::SerialPort)

if(BUILD_TESTING)
    add_test(NAME qml_module_load COMMAND ZenithGroundStationQt --check-ui)
    set_tests_properties(qml_module_load PROPERTIES
        TIMEOUT 15
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=$<TARGET_FILE_DIR:Qt6::Core>/../plugins")
    qt_add_executable(MavlinkGroundStationTests
        tests/test_mavlink_groundstation.cpp
        src/CommandDispatcher.cpp src/CommandDispatcher.h
        src/ParamStore.cpp src/ParamStore.h
        src/ZenithMavlinkCodec.cpp src/ZenithMavlinkCodec.h
        src/ZenithProtocolClient.cpp src/ZenithProtocolClient.h
        src/Lr24RadioProtocol.cpp src/Lr24RadioProtocol.h
        src/TelemetryStore.cpp src/TelemetryStore.h
    )
    target_include_directories(MavlinkGroundStationTests PRIVATE src include)
    target_include_directories(MavlinkGroundStationTests SYSTEM PRIVATE third_party)
    target_link_libraries(MavlinkGroundStationTests PRIVATE Qt6::Core Qt6::Gui Qt6::Network Qt6::SerialPort)
    add_test(NAME mavlink_groundstation COMMAND MavlinkGroundStationTests)

    qt_add_executable(ScriptActionModelTests
        tests/test_script_action_model.cpp
        src/ScriptActionModel.cpp
        src/ScriptActionModel.h
    )
    target_include_directories(ScriptActionModelTests PRIVATE src)
    target_link_libraries(ScriptActionModelTests PRIVATE Qt6::Core)
    add_test(NAME script_action_model_crud COMMAND ScriptActionModelTests)

    qt_add_executable(TelemetryStabilityTests
        tests/test_telemetry_stability.cpp
        src/TelemetryStore.cpp
        src/TelemetryStore.h
    )
    target_include_directories(TelemetryStabilityTests PRIVATE src)
    target_link_libraries(TelemetryStabilityTests PRIVATE Qt6::Core Qt6::Gui)
    add_test(NAME telemetry_stability COMMAND TelemetryStabilityTests)

    qt_add_executable(VoxelInstanceTableTests
        tests/test_voxel_instance_table.cpp
        src/VoxelInstanceTable.h
        src/TelemetryStore.cpp
        src/TelemetryStore.h
        tests/legacy/LegacyFrameCodec.cpp
        tests/legacy/LegacyMsgPack.h
    )
    target_include_directories(VoxelInstanceTableTests PRIVATE src tests/legacy)
    target_link_libraries(VoxelInstanceTableTests PRIVATE Qt6::Core Qt6::Gui Qt6::Quick3D Qt6::Network Qt6::SerialPort)
    add_test(NAME voxel_instance_table COMMAND VoxelInstanceTableTests)
    add_test(NAME voxelmap_legacy_fixture COMMAND VoxelInstanceTableTests
        --wire-fixture "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/voxelmap_frame.bin" 3874)
    set_tests_properties(voxel_instance_table voxelmap_legacy_fixture PROPERTIES
        ENVIRONMENT "QT_FORCE_STDERR_LOGGING=1")
    set_tests_properties(voxelmap_legacy_fixture PROPERTIES LABELS "legacy")

    qt_add_executable(Lr24RadioProtocolTests
        tests/test_lr24_radio_protocol.cpp
        src/Lr24RadioProtocol.cpp
        src/Lr24RadioProtocol.h
    )
    target_include_directories(Lr24RadioProtocolTests PRIVATE src)
    target_link_libraries(Lr24RadioProtocolTests PRIVATE Qt6::Core)
    add_test(NAME lr24_radio_protocol COMMAND Lr24RadioProtocolTests)

    qt_add_executable(RadioPairingModelTests
        tests/test_radio_pairing_model.cpp
        src/RadioPairingModel.cpp
        src/RadioPairingModel.h
    )
    target_include_directories(RadioPairingModelTests PRIVATE src)
    target_link_libraries(RadioPairingModelTests PRIVATE Qt6::Core)
    add_test(NAME radio_pairing_model COMMAND RadioPairingModelTests)
endif()
