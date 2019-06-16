cmake_minimum_required(VERSION 3.10.0)
project (riscv_simulator)


set(CMAKE_FIND_ROOT_PATH  /usr/lib32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "-m32")


add_executable(riscv_simulator.out main.cpp elf.cpp instruction.cpp memory.cpp pipeline.cpp syscall.cpp tomasulo.cpp tomasulo_2.cpp)
