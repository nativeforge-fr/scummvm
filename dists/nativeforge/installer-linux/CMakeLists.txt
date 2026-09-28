cmake_minimum_required(VERSION 3.16)
project(versailles-installer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(versailles-installer main.cpp)
target_link_libraries(versailles-installer PRIVATE Qt6::Widgets)

install(TARGETS versailles-installer RUNTIME DESTINATION bin)
