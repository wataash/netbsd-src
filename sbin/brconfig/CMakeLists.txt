cmake_minimum_required(VERSION 3.8)
project(brconfig)

set(CMAKE_VERBOSE_MAKEFILE on)

add_executable(brconfig brconfig.c)
# target_link_libraries(brconfig dummy)
# target_include_directories(brconfig PRIVATE ${PROJECT_SOURCE_DIR}/../dest.i386/usr/include)
