cmake_minimum_required(VERSION 3.16)

project(Fetakuvaaja VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Gui Qml Quick Core Widgets)

qt_standard_project_setup(REQUIRES 6.8)

qt6_add_executable(appFetakuvaaja
    main.cpp
)

qt6_add_qml_module(appFetakuvaaja
    URI Fetakuvaaja
    VERSION 1.0
    RESOURCES
        Main.qml
    SOURCES
        backend.cpp
        screenrecord.cpp
        screenrecord.h
        backend.h
)

set_target_properties(appFetakuvaaja PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)
find_package(Qt6 REQUIRED COMPONENTS Core)

target_link_libraries(appFetakuvaaja
    PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml d3d11 dxgi Qt6::Widgets mfplat mfuuid mfreadwrite wmcodecdspuuid
)

target_compile_definitions(appFetakuvaaja 
    PRIVATE              
    WIN32_LEAN_AND_MEAN

)

add_compile_definitions(NOMINMAX)


include(GNUInstallDirs)
install(TARGETS appFetakuvaaja
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
