cmake_minimum_required(VERSION 3.28)
project(MagicOnLinux)

find_package(SDL2 REQUIRED)
add_compile_options(-Wall -Wextra -Wpedantic -Wno-multichar)

file(GLOB_RECURSE m68k src/m68k/*.c src/m68k/*.h)
file(GLOB_RECURSE sources main.cpp src/*.cpp inc/*.h)

add_executable(magic-on-linux ${sources} ${m68k})
target_include_directories(magic-on-linux PUBLIC inc src/m68k)
target_link_libraries(magic-on-linux PUBLIC SDL2)
