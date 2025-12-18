cmake_minimum_required(VERSION 3.20)
project(hft_strategy_simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Helpful defaults for local dev
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(hft_core
    src/order_book.cpp
)

target_include_directories(hft_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(hft_sim
    apps/sim_main.cpp
)

target_link_libraries(hft_sim PRIVATE hft_core)

