cmake_minimum_required(VERSION 3.25)
project(MagicOnLinux)

# Find SDL2
if(APPLE)
    # On macOS, use pkg-config to find SDL2
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
else()
    find_package(SDL2 REQUIRED)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wno-multichar)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fPIC)
endif()

file(GLOB_RECURSE m68k src/m68k/*.c src/m68k/*.h)
file(GLOB_RECURSE sources main.cpp src/*.cpp inc/*.h)

add_executable(magic-on-linux ${sources} ${m68k})
target_include_directories(magic-on-linux PUBLIC inc src/m68k ${SDL2_INCLUDE_DIRS})
if(APPLE)
    target_link_directories(magic-on-linux PUBLIC ${SDL2_LIBRARY_DIRS})
endif()
target_link_libraries(magic-on-linux PUBLIC ${SDL2_LIBRARIES})

if(APPLE)
    add_compile_definitions(
        DEFAULT_ATARI_ROOT="~/Documents/MAGIC_C"
        DEFAULT_CONFIG="~/Library/Preferences/magiclinux.conf"
        DEFAULT_EDITOR="open -e"
    )
else()
    add_compile_definitions(
        DEFAULT_ATARI_ROOT="~/Documents/MAGIC_C"
        DEFAULT_CONFIG="~/.config/magiclinux.conf"
        DEFAULT_EDITOR="xdg-open"
)
endif()

if(NOT APPLE)
# Installs the executable to /usr/local/bin/
# For e.g. "/opt/bin/": cmake .. -DCMAKE_INSTALL_PREFIX:PATH=/opt ..
#                       make install
install(TARGETS magic-on-linux)
install(FILES assets/magic-on-linux.desktop DESTINATION share/applications)
install(FILES assets/magic-on-linux.png DESTINATION share/icons/hicolor/512x512/apps)
endif()
