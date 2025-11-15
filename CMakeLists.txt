cmake_minimum_required(VERSION 4.0)
project(nmtest)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LIB_NAME nmtestlib)
add_library(${LIB_NAME})
target_sources(${LIB_NAME}
        PUBLIC
        FILE_SET CXX_MODULES
        FILES
        src/nmtest.ixx
)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIB_NAME})