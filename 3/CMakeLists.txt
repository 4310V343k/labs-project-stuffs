cmake_minimum_required(VERSION 3.14)
project(bignums LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── GMP (only for number generation) ───────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(GMP REQUIRED gmp)
if(NOT GMP_FOUND)
    message(FATAL_ERROR "GMP not found. Install it with: sudo apt install libgmp-dev")
endif()

# ── FTXUI ──────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v6.1.9
)
FetchContent_MakeAvailable(ftxui)

# ── Executable ─────────────────────────────────────────────────────────────
add_executable(bignums
    src/main.cpp
    src/bignum.cpp
    src/generator.cpp
)

target_include_directories(bignums PRIVATE src ${GMP_INCLUDE_DIRS})
target_link_libraries(bignums
    PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
    ${GMP_LIBRARIES}
)
target_compile_options(bignums PRIVATE -Wall -Wextra)
