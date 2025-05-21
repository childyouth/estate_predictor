cmake_minimum_required(VERSION 3.22)

project(
    estate_parser
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-g")

include_directories(include)
file(GLOB SOURCES "src/*.cpp")

find_package(Boost 1.74 COMPONENTS fiber system REQUIRED)
add_executable(estate_parser ${SOURCES})

target_link_libraries(estate_parser Boost::fiber)


# add_executable(autoecho autoecho.cpp yield.hpp round_robin.hpp)
# find_package(Boost 1.74 COMPONENTS fiber system REQUIRED)
# target_link_libraries(autoecho Boost::fiber)