cmake_minimum_required(VERSION 3.28)
project(MagicOnLinux)

find_package(SDL2 REQUIRED)
add_compile_options(-Wall -Wextra -Wpedantic -Wno-multichar)

file(GLOB_RECURSE m68k m68k/*.c m68k/*.h)
file(GLOB_RECURSE sources main.cpp src/*.cpp inc/*.h)

add_executable(MagiCLinux ${sources} ${m68k})
target_include_directories(MagiCLinux PUBLIC inc m68k)
target_link_libraries(MagiCLinux PUBLIC SDL2)
