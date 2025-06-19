cmake_minimum_required(VERSION 3.16)
project(vlc-wayland-player)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt6 COMPONENTS Gui REQUIRED)
# find_package(VLC REQUIRED)

set(VLC_INSTALL_PATH "/home/abdo/vlc/build-lib/install")
set(ENV{PKG_CONFIG_PATH} "${VLC_INSTALL_PATH}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

set(VLC_LIB_PATH "${VLC_INSTALL_PATH}/lib")
set(VLC_INCLUDE_PATH "${VLC_INSTALL_PATH}/include")


include_directories(${VLC_INCLUDE_PATH})
link_directories(${VLC_LIB_PATH})

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(VLC REQUIRED libvlc)
find_package(Wayland REQUIRED)

include_directories(
    ${VLC_INCLUDE_DIRS}
)

add_executable(${PROJECT_NAME}
    main.cpp
    vlc_player.h
)

target_link_libraries(${PROJECT_NAME}
    Qt6::Gui
    ${VLC_LIBRARIES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${VLC_INCLUDE_DIRS}
)

# Add runtime environment variables
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_ENVIRONMENT "LD_LIBRARY_PATH=${VLC_INSTALL_PATH}/lib"
)